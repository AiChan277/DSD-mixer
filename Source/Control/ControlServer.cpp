#include "Control/ControlServer.h"
#include "Audio/AudioEngine.h"

namespace dsd::control
{
    ControlServer::ControlServer(AudioEngine& engine, int port)
        : engine_(engine)
        , port_(port)
        , commandDispatcher_(engine)
        , stateObserver_(engine, 30)
    {
        stateObserver_.addListener(this);

        webSocketServer_.setMessageCallback(
            [this](std::shared_ptr<ix::ConnectionState> state,
                   ix::WebSocket& ws,
                   const ix::WebSocketMessagePtr& msg)
            {
                onClientMessage(state, ws, msg);
            });
    }

    ControlServer::~ControlServer()
    {
        stop();
        stateObserver_.removeListener(this);
    }

    bool ControlServer::start(int port, const std::string& host)
    {
        port_ = port;
        if (!webSocketServer_.start(port_, host))
            return false;

        stateObserver_.start();
        return true;
    }

    void ControlServer::stop()
    {
        stateObserver_.stop();
        webSocketServer_.stop();
        clientManager_.clear();
    }

    bool ControlServer::isRunning() const
    {
        return webSocketServer_.isRunning();
    }

    int ControlServer::getPort() const
    {
        return webSocketServer_.getPort();
    }

    void ControlServer::setPin(const juce::String& pin)
    {
        clientManager_.setPin(pin.toStdString());
    }

    juce::String ControlServer::getPin() const
    {
        return juce::String(clientManager_.getPin());
    }

    int ControlServer::getConnectedClientCount() const
    {
        return clientManager_.getConnectedCount();
    }

    int ControlServer::getAuthenticatedClientCount() const
    {
        return clientManager_.getAuthenticatedCount();
    }

    void ControlServer::onClientMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                                         ix::WebSocket& webSocket,
                                         const ix::WebSocketMessagePtr& msg)
    {
        if (!connectionState)
            return;

        const std::string connId = connectionState->getId();

        if (msg->type == ix::WebSocketMessageType::Open)
        {
            // Client connected; waiting for hello handshake
            return;
        }
        else if (msg->type == ix::WebSocketMessageType::Close)
        {
            clientManager_.unregisterClient(connId);
            return;
        }
        else if (msg->type == ix::WebSocketMessageType::Message)
        {
            auto frame = parseFrame(juce::String(msg->str));
            if (frame.type == MessageType::Unknown)
            {
                webSocketServer_.sendText(webSocket,
                    serialiseError(ErrorCode::UnknownMethod,
                                   frame.parseError.isNotEmpty() ? frame.parseError : "Invalid DSDCP message").toStdString());
                return;
            }

            switch (frame.type)
            {
                case MessageType::Hello:
                    handleHello(connId, webSocket, frame);
                    break;
                case MessageType::Subscribe:
                    handleSubscribe(connId, webSocket, frame);
                    break;
                case MessageType::Command:
                    handleCommand(connId, webSocket, frame);
                    break;
                case MessageType::Ping:
                    handlePing(webSocket, frame);
                    break;
                case MessageType::Goodbye:
                    handleGoodbye(connId, webSocket, frame);
                    break;
                default:
                    webSocketServer_.sendText(webSocket,
                        serialiseError(ErrorCode::UnknownMethod, "Unsupported message type").toStdString());
                    break;
            }
        }
    }

    void ControlServer::handleHello(const std::string& connectionId,
                                    ix::WebSocket& ws,
                                    const ParseResult& frame)
    {
        // Protocol name & version check
        if (!frame.root.hasProperty("protocol") || frame.root["protocol"].toString() != "DSDCP")
        {
            webSocketServer_.sendText(ws,
                serialiseError(ErrorCode::VersionMismatch, "Invalid protocol; expected 'DSDCP'").toStdString());
            return;
        }

        if (!frame.root.hasProperty("version") || static_cast<int>(frame.root["version"]) != DSDCP_VERSION)
        {
            webSocketServer_.sendText(ws,
                serialiseError(ErrorCode::VersionMismatch, "Unsupported protocol version").toStdString());
            return;
        }

        // Extract client info
        std::string clientName = "Remote Client";
        std::string clientType = "control_surface";
        if (frame.root.hasProperty("client") && frame.root["client"].isObject())
        {
            const auto& c = frame.root["client"];
            if (c.hasProperty("name")) clientName = c["name"].toString().toStdString();
            if (c.hasProperty("type")) clientType = c["type"].toString().toStdString();
        }

        // Check PIN Authentication
        std::string pin;
        if (frame.root.hasProperty("auth") && frame.root["auth"].isObject())
        {
            if (frame.root["auth"].hasProperty("pin"))
                pin = frame.root["auth"]["pin"].toString().toStdString();
        }

        if (!clientManager_.verifyPin(pin))
        {
            webSocketServer_.sendText(ws,
                serialiseError(ErrorCode::NotAuthenticated, "Invalid or missing PIN pairing").toStdString());
            return;
        }

        // Locate std::shared_ptr<ix::WebSocket> from active clients set
        std::shared_ptr<ix::WebSocket> wsPtr;
        for (const auto& c : webSocketServer_.getClients())
        {
            if (c.get() == &ws)
            {
                wsPtr = c;
                break;
            }
        }

        auto session = clientManager_.registerClient(connectionId, wsPtr, clientName, clientType, pin);

        // Send welcome
        auto capObj = std::make_unique<juce::DynamicObject>();
        capObj->setProperty("channels", NUM_CHANNELS_LEVEL1);
        capObj->setProperty("outputs",  NUM_OUTPUT_BUSES_LEVEL1);

        juce::Array<juce::var> matrixDim;
        matrixDim.add(NUM_CHANNELS_LEVEL1);
        matrixDim.add(NUM_OUTPUT_BUSES_LEVEL1);
        capObj->setProperty("routingMatrix", matrixDim);

        auto welcomeJson = serialiseWelcome(session->clientId, juce::var(capObj.release()));
        webSocketServer_.sendText(ws, welcomeJson.toStdString());

        // Immediately push initial full sync snapshot
        auto syncJson = stateObserver_.getFullSyncJson();
        webSocketServer_.sendText(ws, syncJson.toStdString());
    }

    void ControlServer::handleSubscribe(const std::string& connectionId,
                                        ix::WebSocket& ws,
                                        const ParseResult& frame)
    {
        auto client = clientManager_.getClient(connectionId);
        if (!client || !client->authenticated)
        {
            webSocketServer_.sendText(ws,
                serialiseError(ErrorCode::NotAuthenticated, "Client not authenticated").toStdString());
            return;
        }

        std::vector<std::string> categories;
        if (frame.root.hasProperty("events") && frame.root["events"].isArray())
        {
            const auto* arr = frame.root["events"].getArray();
            if (arr != nullptr)
            {
                for (const auto& item : *arr)
                    categories.push_back(item.toString().toStdString());
            }
        }

        bool metering = client->meteringSubscribed;
        int meterInterval = client->meterIntervalMs;
        if (frame.root.hasProperty("meters") && frame.root["meters"].isObject())
        {
            const auto& m = frame.root["meters"];
            if (m.hasProperty("enabled"))
                metering = static_cast<bool>(m["enabled"]);
            if (m.hasProperty("intervalMs"))
                meterInterval = static_cast<int>(m["intervalMs"]);
        }

        clientManager_.updateSubscriptions(connectionId, categories, metering, meterInterval);

        // Enable or disable StateObserver metering based on collective subscriber interest
        stateObserver_.setMeteringEnabled(clientManager_.hasAnyMeteringSubscriber());
    }

    void ControlServer::handleCommand(const std::string& connectionId,
                                      ix::WebSocket& ws,
                                      const ParseResult& frame)
    {
        auto client = clientManager_.getClient(connectionId);
        if (!client || !client->authenticated)
        {
            webSocketServer_.sendText(ws,
                serialiseError(ErrorCode::NotAuthenticated, "Client not authenticated").toStdString());
            return;
        }

        // v1 restriction: remote session.load / session.save are NOT allowed.
        // Checked before parsing so clients get PermissionDenied rather than a
        // generic unknown-method error.
        if (frame.root.hasProperty("method"))
        {
            const auto m = frame.root["method"].toString();
            if (m == "session.load" || m == "session.save")
            {
                webSocketServer_.sendText(ws,
                    serialiseResponse(DSDCPResponse::fail(frame.root["id"].toString(), ErrorCode::PermissionDenied,
                                                         "Remote session load/save is not allowed in v1")).toStdString());
                return;
            }
        }

        DSDCPCommand cmd;
        auto extractErr = extractCommand(frame, cmd);
        if (extractErr.has_value())
        {
            webSocketServer_.sendText(ws, serialiseResponse(*extractErr).toStdString());
            return;
        }

        // Dispatch command execution to JUCE message thread
        auto executeAndReply = [this, connectionId, cmd]()
        {
            auto resp = commandDispatcher_.execute(cmd);
            auto respJson = serialiseResponse(resp).toStdString();

            auto c = clientManager_.getClient(connectionId);
            if (c)
            {
                if (auto wsLocked = c->webSocket.lock())
                {
                    webSocketServer_.sendText(wsLocked, respJson);
                }
            }

            // Immediately poll and broadcast state changes resulting from the command
            stateObserver_.pollState();
        };

        if (juce::MessageManager::getInstanceWithoutCreating() == nullptr ||
            juce::MessageManager::getInstance()->isThisTheMessageThread())
        {
            executeAndReply();
        }
        else
        {
            juce::MessageManager::callAsync(executeAndReply);
        }
    }

    void ControlServer::handlePing(ix::WebSocket& ws, const ParseResult& frame)
    {
        juce::int64 ts = 0;
        if (frame.root.hasProperty("timestamp"))
            ts = static_cast<juce::int64>(frame.root["timestamp"]);

        webSocketServer_.sendText(ws, serialisePong(ts).toStdString());
    }

    void ControlServer::handleGoodbye(const std::string& connectionId,
                                      ix::WebSocket& ws,
                                      const ParseResult& /*frame*/)
    {
        clientManager_.unregisterClient(connectionId);
        ws.close();
    }

    void ControlServer::broadcastEvent(const juce::String& eventJson, const std::string& category)
    {
        const std::string text = eventJson.toStdString();
        auto authClients = clientManager_.getAuthenticatedClients();

        for (const auto& c : authClients)
        {
            if (category.empty() || c->isSubscribedTo(category))
            {
                if (auto ws = c->webSocket.lock())
                {
                    webSocketServer_.sendText(ws, text);
                }
            }
        }
    }

    void ControlServer::onStateEvents(const std::vector<juce::String>& events)
    {
        for (const auto& ev : events)
        {
            std::string cat = "channel.state";
            if (ev.contains("\"output.state\""))
                cat = "output.state";
            else if (ev.contains("\"route.state\""))
                cat = "route.state";

            broadcastEvent(ev, cat);
        }
    }

    void ControlServer::onMeterEvent(const juce::String& meterJson)
    {
        const std::string text = meterJson.toStdString();
        auto authClients = clientManager_.getAuthenticatedClients();

        for (const auto& c : authClients)
        {
            if (c->meteringSubscribed)
            {
                if (auto ws = c->webSocket.lock())
                {
                    // If client's transmit buffer exceeds 32KB, it is experiencing network lag.
                    // Drop superseded meter frames to avoid unbounded queue growth and latency buildup.
                    if (webSocketServer_.getBufferedAmount(ws) < 32768)
                    {
                        webSocketServer_.sendText(ws, text);
                    }
                }
            }
        }
    }

} // namespace dsd::control
