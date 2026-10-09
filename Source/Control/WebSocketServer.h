#pragma once

// DSD Control Protocol — WebSocket Server (Stage C)
//
// Wraps IXWebSocket's WebSocketServer:
//   - Listens on specified port (default 9100)
//   - Handles connection events and incoming frames
//   - Provides text message dispatch and broadcast facilities
//   - Winsock initialization and cleanup on Windows

#include <ixwebsocket/IXWebSocketServer.h>
#include <string>
#include <memory>
#include <functional>
#include <atomic>

namespace dsd::control
{
    class WebSocketServer
    {
    public:
        using MessageCallback = std::function<void(std::shared_ptr<ix::ConnectionState> state,
                                                   ix::WebSocket& ws,
                                                   const ix::WebSocketMessagePtr& msg)>;

        WebSocketServer();
        ~WebSocketServer();

        bool start(int port = 9100, const std::string& host = "0.0.0.0");
        void stop();
        bool isRunning() const noexcept { return isRunning_.load(); }
        int getPort() const noexcept { return port_; }

        void setMessageCallback(MessageCallback cb);

        bool sendText(ix::WebSocket& ws, const std::string& text);
        bool sendText(const std::shared_ptr<ix::WebSocket>& ws, const std::string& text);
        size_t getBufferedAmount(const std::shared_ptr<ix::WebSocket>& ws);
        void broadcast(const std::vector<std::shared_ptr<ix::WebSocket>>& clients, const std::string& text);

        std::set<std::shared_ptr<ix::WebSocket>> getClients();

    private:
        std::unique_ptr<ix::WebSocketServer> server_;
        MessageCallback messageCallback_;
        int port_ { 9100 };
        std::atomic<bool> isRunning_ { false };
    };

} // namespace dsd::control
