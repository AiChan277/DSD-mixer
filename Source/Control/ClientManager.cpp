#include "Control/ClientManager.h"
#include "Control/DSDCPTypes.h"
#include <algorithm>

namespace dsd::control
{
    ClientManager::ClientManager() = default;

    void ClientManager::setPin(const std::string& pin)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        currentPin_ = pin;
    }

    std::string ClientManager::getPin() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return currentPin_;
    }

    bool ClientManager::verifyPin(const std::string& pin) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (currentPin_.empty())
            return true; // No PIN required if empty
        return pin == currentPin_;
    }

    std::shared_ptr<ClientSession> ClientManager::registerClient(const std::string& connectionId,
                                                                  std::shared_ptr<ix::WebSocket> ws,
                                                                  const std::string& name,
                                                                  const std::string& type,
                                                                  const std::string& pin)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check if already registered
        for (auto& c : clients_)
        {
            if (c->connectionId == connectionId)
            {
                c->name = name;
                c->type = type;
                c->authenticated = currentPin_.empty() || (pin == currentPin_);
                c->webSocket = ws;
                return c;
            }
        }

        auto session = std::make_shared<ClientSession>();
        session->connectionId = connectionId;
        session->clientId = "c_" + std::to_string(nextClientId_++);
        session->name = name.empty() ? "Remote Client" : name;
        session->type = type.empty() ? "control_surface" : type;
        session->authenticated = currentPin_.empty() || (pin == currentPin_);
        session->webSocket = ws;

        // Default subscriptions
        session->subscriptions.insert("channel.state");
        session->subscriptions.insert("output.state");
        session->subscriptions.insert("route.state");
        session->meteringSubscribed = false;
        session->meterIntervalMs = 100;

        clients_.push_back(session);
        return session;
    }

    void ClientManager::unregisterClient(const std::string& connectionId)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
            [&](const std::shared_ptr<ClientSession>& c) {
                return c->connectionId == connectionId;
            }), clients_.end());
    }

    std::shared_ptr<ClientSession> ClientManager::getClient(const std::string& connectionId) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& c : clients_)
        {
            if (c->connectionId == connectionId)
                return c;
        }
        return nullptr;
    }

    void ClientManager::updateSubscriptions(const std::string& connectionId,
                                            const std::vector<std::string>& eventCategories,
                                            bool meteringEnabled,
                                            int meterIntervalMs)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& c : clients_)
        {
            if (c->connectionId == connectionId)
            {
                if (!eventCategories.empty())
                {
                    c->subscriptions.clear();
                    for (const auto& cat : eventCategories)
                        c->subscriptions.insert(cat);
                }
                c->meteringSubscribed = meteringEnabled;
                c->meterIntervalMs = std::max(DSDCP_METER_MIN_INTERVAL_MS, meterIntervalMs);
                break;
            }
        }
    }

    std::vector<std::shared_ptr<ClientSession>> ClientManager::getAuthenticatedClients() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::shared_ptr<ClientSession>> result;
        for (const auto& c : clients_)
        {
            if (c->authenticated)
                result.push_back(c);
        }
        return result;
    }

    std::vector<std::shared_ptr<ClientSession>> ClientManager::getAllClients() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return clients_;
    }

    int ClientManager::getConnectedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<int>(clients_.size());
    }

    int ClientManager::getAuthenticatedCount() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        for (const auto& c : clients_)
        {
            if (c->authenticated)
                ++count;
        }
        return count;
    }

    bool ClientManager::hasAnyMeteringSubscriber() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto& c : clients_)
        {
            if (c->authenticated && c->meteringSubscribed)
                return true;
        }
        return false;
    }

    void ClientManager::clear()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        clients_.clear();
    }

} // namespace dsd::control
