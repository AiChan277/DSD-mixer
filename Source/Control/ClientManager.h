#pragma once

// DSD Control Protocol — Client Manager (Stage C)
//
// Manages connected remote clients:
//   - Connection registration and lifecycle
//   - 6-digit PIN authentication
//   - Client capability and subscription management
//   - Thread-safe tracking of client WebSocket instances

#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <mutex>

namespace ix
{
    class WebSocket;
}

namespace dsd::control
{
    struct ClientSession
    {
        std::string connectionId;
        std::string clientId;       // Assigned server ID e.g. "c_1"
        std::string name;           // e.g. "Samsung A9"
        std::string type;           // e.g. "control_surface"
        bool authenticated { false };
        std::set<std::string> subscriptions; // e.g. {"channel.state", "output.state", "route.state"}
        bool meteringSubscribed { false };
        int meterIntervalMs { 100 };
        std::weak_ptr<ix::WebSocket> webSocket;

        bool isSubscribedTo(const std::string& eventType) const
        {
            return subscriptions.find(eventType) != subscriptions.end();
        }
    };

    class ClientManager
    {
    public:
        ClientManager();
        ~ClientManager() = default;

        // PIN pairing
        void setPin(const std::string& pin);
        std::string getPin() const;
        bool verifyPin(const std::string& pin) const;

        // Client registration
        std::shared_ptr<ClientSession> registerClient(const std::string& connectionId,
                                                      std::shared_ptr<ix::WebSocket> ws,
                                                      const std::string& name,
                                                      const std::string& type,
                                                      const std::string& pin);

        void unregisterClient(const std::string& connectionId);

        std::shared_ptr<ClientSession> getClient(const std::string& connectionId) const;

        // Subscriptions
        void updateSubscriptions(const std::string& connectionId,
                                 const std::vector<std::string>& eventCategories,
                                 bool meteringEnabled,
                                 int meterIntervalMs);

        // Queries
        std::vector<std::shared_ptr<ClientSession>> getAuthenticatedClients() const;
        std::vector<std::shared_ptr<ClientSession>> getAllClients() const;
        int getConnectedCount() const;
        int getAuthenticatedCount() const;
        bool hasAnyMeteringSubscriber() const;

        void clear();

    private:
        mutable std::mutex mutex_;
        std::string currentPin_ { "123456" };
        uint64_t nextClientId_ { 1 };
        std::vector<std::shared_ptr<ClientSession>> clients_;
    };

} // namespace dsd::control
