#pragma once
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace stateup::tree {

    // Event data that can be passed with events
    class EventData {
      public:
        virtual ~EventData() = default;
    };

    using EventDataPtr = std::shared_ptr<EventData>;

    // Event bus for publish/subscribe between nodes
    class EventBus {
      public:
        using EventCallback = std::function<void(const EventDataPtr &)>;
        using SubscriptionId = size_t;

        // Subscribe to an event
        SubscriptionId subscribe(const std::string &eventName, EventCallback callback) {
            SubscriptionId id = nextSubscriptionId_++;
            subscriptions_[eventName].push_back({id, std::move(callback)});
            return id;
        }

        // Unsubscribe from an event
        void unsubscribe(const std::string &eventName, SubscriptionId id) {
            auto it = subscriptions_.find(eventName);
            if (it != subscriptions_.end()) {
                auto &callbacks = it->second;
                callbacks.erase(std::remove_if(callbacks.begin(), callbacks.end(),
                                               [id](const Subscription &sub) { return sub.id == id; }),
                                callbacks.end());
            }
        }

        // Publish an event
        void publish(const std::string &eventName, const EventDataPtr &data = nullptr) {
            auto it = subscriptions_.find(eventName);
            if (it != subscriptions_.end()) {
                // Copy the callback list to avoid issues if callbacks modify subscriptions
                auto callbacks = it->second;
                for (const auto &sub : callbacks) {
                    sub.callback(data);
                }
            }
        }

        // Clear all subscriptions for an event
        void clearEvent(const std::string &eventName) { subscriptions_.erase(eventName); }

        // Clear all subscriptions
        void clearAll() { subscriptions_.clear(); }

      private:
        struct Subscription {
            SubscriptionId id;
            EventCallback callback;
        };

        std::unordered_map<std::string, std::vector<Subscription>> subscriptions_;
        SubscriptionId nextSubscriptionId_ = 0;
    };

} // namespace stateup::tree
