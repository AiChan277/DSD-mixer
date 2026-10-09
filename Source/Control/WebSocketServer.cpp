#include "Control/WebSocketServer.h"
#include <ixwebsocket/IXNetSystem.h>
#include <iostream>

namespace dsd::control
{
    WebSocketServer::WebSocketServer()
    {
        ix::initNetSystem();
    }

    WebSocketServer::~WebSocketServer()
    {
        stop();
        ix::uninitNetSystem();
    }

    bool WebSocketServer::start(int port, const std::string& host)
    {
        stop();

        port_ = port;
        server_ = std::make_unique<ix::WebSocketServer>(port, host);
        server_->disablePerMessageDeflate();

        if (messageCallback_)
        {
            server_->setOnClientMessageCallback(messageCallback_);
        }

        auto res = server_->listen();
        if (!res.first)
        {
            server_.reset();
            isRunning_.store(false);
            return false;
        }

        server_->start();
        isRunning_.store(true);
        return true;
    }

    void WebSocketServer::stop()
    {
        if (server_)
        {
            server_->stop();
            server_.reset();
        }
        isRunning_.store(false);
    }

    void WebSocketServer::setMessageCallback(MessageCallback cb)
    {
        messageCallback_ = std::move(cb);
        if (server_)
        {
            server_->setOnClientMessageCallback(messageCallback_);
        }
    }

    bool WebSocketServer::sendText(ix::WebSocket& ws, const std::string& text)
    {
        auto info = ws.sendText(text);
        return info.success;
    }

    bool WebSocketServer::sendText(const std::shared_ptr<ix::WebSocket>& ws, const std::string& text)
    {
        if (ws && ws->getReadyState() == ix::ReadyState::Open)
        {
            auto info = ws->sendText(text);
            return info.success;
        }
        return false;
    }

    size_t WebSocketServer::getBufferedAmount(const std::shared_ptr<ix::WebSocket>& ws)
    {
        if (ws)
            return ws->bufferedAmount();
        return 0;
    }

    void WebSocketServer::broadcast(const std::vector<std::shared_ptr<ix::WebSocket>>& clients, const std::string& text)
    {
        for (const auto& ws : clients)
        {
            sendText(ws, text);
        }
    }

    std::set<std::shared_ptr<ix::WebSocket>> WebSocketServer::getClients()
    {
        if (server_)
        {
            return server_->getClients();
        }
        return {};
    }

} // namespace dsd::control
