#pragma once

// DSD Control Protocol — Control Server (Stage C)
//
// The central coordinator for remote control surfaces:
//   - Owns and manages WebSocketServer and ClientManager
//   - Connects CommandDispatcher for inbound command execution
//   - Listens to StateObserver to broadcast state deltas and metering
//   - Dispatches remote commands to the JUCE message thread
//   - Manages client authentication, protocol handshakes, and full sync

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "Control/DSDCPTypes.h"
#include "Control/DSDCPSerializer.h"
#include "Control/CommandDispatcher.h"
#include "Control/StateObserver.h"
#include "Control/ClientManager.h"
#include "Control/WebSocketServer.h"
#include <memory>

namespace dsd
{
    class AudioEngine;
}

namespace dsd::control
{
    class ControlServer : public StateObserver::Listener
    {
    public:
        // engine: the underlying audio engine (sole source of truth)
        explicit ControlServer(AudioEngine& engine, int port = DSDCP_DEFAULT_PORT);
        ~ControlServer() override;

        // Server lifecycle
        bool start(int port = DSDCP_DEFAULT_PORT, const std::string& host = "0.0.0.0");
        void stop();
        bool isRunning() const;
        int getPort() const;

        // PIN Pairing
        void setPin(const juce::String& pin);
        juce::String getPin() const;

        // Client metrics
        int getConnectedClientCount() const;
        int getAuthenticatedClientCount() const;
        std::vector<std::shared_ptr<ClientSession>> getAllClients() const { return clientManager_.getAllClients(); }

        // Accessors to subsystems
        ClientManager& getClientManager() noexcept { return clientManager_; }
        StateObserver& getStateObserver() noexcept { return stateObserver_; }
        CommandDispatcher& getCommandDispatcher() noexcept { return commandDispatcher_; }
        WebSocketServer& getWebSocketServer() noexcept { return webSocketServer_; }

        // StateObserver::Listener implementation
        void onStateEvents(const std::vector<juce::String>& events) override;
        void onMeterEvent(const juce::String& meterJson) override;

    private:
        void onClientMessage(std::shared_ptr<ix::ConnectionState> connectionState,
                             ix::WebSocket& webSocket,
                             const ix::WebSocketMessagePtr& msg);

        void handleHello(const std::string& connectionId,
                         ix::WebSocket& ws,
                         const ParseResult& frame);

        void handleSubscribe(const std::string& connectionId,
                             ix::WebSocket& ws,
                             const ParseResult& frame);

        void handleCommand(const std::string& connectionId,
                           ix::WebSocket& ws,
                           const ParseResult& frame);

        void handlePing(ix::WebSocket& ws,
                        const ParseResult& frame);

        void handleGoodbye(const std::string& connectionId,
                           ix::WebSocket& ws,
                           const ParseResult& frame);

        void broadcastEvent(const juce::String& eventJson, const std::string& category);

        AudioEngine& engine_;
        int port_ { DSDCP_DEFAULT_PORT };

        CommandDispatcher commandDispatcher_;
        StateObserver stateObserver_;
        ClientManager clientManager_;
        WebSocketServer webSocketServer_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ControlServer)
    };

} // namespace dsd::control
