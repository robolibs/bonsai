#pragma once
#include <any>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bonsai::tree {

    class Blackboard {
      public:
        struct Event {
            enum class Type { Set, Get, Remove, Clear, ScopePushed, ScopePopped };

            Type type;
            std::string key;
            std::type_index valueType;
            bool success;
            size_t scopeDepth;
        };

        using Observer = std::function<void(const Event &)>;

        class ScopeToken {
          public:
            ScopeToken(Blackboard *owner, size_t depth);
            ScopeToken(const ScopeToken &) = delete;
            ScopeToken &operator=(const ScopeToken &) = delete;
            ScopeToken(ScopeToken &&other) noexcept;
            ScopeToken &operator=(ScopeToken &&other) noexcept;
            ~ScopeToken();

            void release();

          private:
            Blackboard *owner_ = nullptr;
            size_t depth_ = 0;
            bool active_ = false;
        };

        template <typename T> inline void set(const std::string &key, T value) {
            Observer observerCopy;
            Event event{Event::Type::Set, key, std::type_index(typeid(T)), true, 0};
            {
                std::lock_guard<std::mutex> lock(mutex_);
                Entry entry{std::make_any<T>(std::move(value)), std::type_index(typeid(T))};
                scopes_.back()[key] = std::move(entry);
                observerCopy = observer_;
                event.scopeDepth = scopes_.size() - 1;
            }
            notify(observerCopy, event);
        }

        template <typename T> inline std::optional<T> get(const std::string &key) const {
            Observer observerCopy;
            Event event{Event::Type::Get, key, std::type_index(typeid(T)), false, 0};
            std::optional<T> result;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                observerCopy = observer_;
                auto [entry, depth] = findEntry(key);
                event.scopeDepth = depth;
                if (!entry) {
                    // Keep failure event with requested type
                } else if (entry->type != std::type_index(typeid(T))) {
                    event.valueType = std::type_index(typeid(T));
                } else {
                    try {
                        result = std::any_cast<T>(entry->value);
                        event.valueType = entry->type;
                        event.success = true;
                    } catch (const std::bad_any_cast &) {
                        event.valueType = entry->type;
                        event.success = false;
                    }
                }
            }
            notify(observerCopy, event);
            return result;
        }

        inline bool has(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return findEntry(key).first != nullptr;
        }

        inline void remove(const std::string &key) {
            Observer observerCopy;
            Event event{Event::Type::Remove, key, std::type_index(typeid(void)), false, 0};
            {
                std::lock_guard<std::mutex> lock(mutex_);
                auto &current = scopes_.back();
                event.success = current.erase(key) > 0;
                event.scopeDepth = scopes_.size() - 1;
                observerCopy = observer_;
            }
            notify(observerCopy, event);
        }

        inline void clear() {
            Observer observerCopy;
            Event event{Event::Type::Clear, "", std::type_index(typeid(void)), true, 0};
            {
                std::lock_guard<std::mutex> lock(mutex_);
                scopes_.assign(1, {});
                observerCopy = observer_;
            }
            notify(observerCopy, event);
        }

        ScopeToken pushScope();
        void popScope();

        void setObserver(Observer observer);

        // FIX: Add key enumeration
        inline std::vector<std::string> getAllKeys() const {
            std::lock_guard<std::mutex> lock(mutex_);
            std::unordered_set<std::string> allKeys;
            for (const auto &scope : scopes_) {
                for (const auto &[key, entry] : scope) {
                    allKeys.insert(key);
                }
            }
            return std::vector<std::string>(allKeys.begin(), allKeys.end());
        }

        // FIX: Add type info for a key
        inline std::optional<std::type_index> getType(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto [entry, depth] = findEntry(key);
            if (entry) {
                return entry->type;
            }
            return std::nullopt;
        }

      private:
        struct Entry {
            Entry() : value(), type(typeid(void)) {}
            Entry(std::any v, std::type_index t) : value(std::move(v)), type(t) {}
            std::any value;
            std::type_index type;
        };

        inline std::pair<Entry *, size_t> findEntry(const std::string &key) const {
            for (size_t depth = scopes_.size(); depth > 0; --depth) {
                auto &scope = scopes_[depth - 1];
                auto it = scope.find(key);
                if (it != scope.end()) {
                    return {const_cast<Entry *>(&it->second), depth - 1};
                }
            }
            return {nullptr, scopes_.size()};
        }

        inline void notify(const Observer &observer, const Event &event) const {
            // FIX: Only notify on successful operations to improve performance
            if (observer && (event.success || event.type == Event::Type::Set || event.type == Event::Type::Clear ||
                             event.type == Event::Type::ScopePushed || event.type == Event::Type::ScopePopped)) {
                observer(event);
            }
        }

        mutable std::mutex mutex_;
        std::vector<std::unordered_map<std::string, Entry>> scopes_{{}};
        Observer observer_;
    };

    inline Blackboard::ScopeToken::ScopeToken(Blackboard *owner, size_t depth)
        : owner_(owner), depth_(depth), active_(owner != nullptr) {}

    inline Blackboard::ScopeToken::ScopeToken(ScopeToken &&other) noexcept { *this = std::move(other); }

    inline Blackboard::ScopeToken &Blackboard::ScopeToken::operator=(ScopeToken &&other) noexcept {
        if (this != &other) {
            release();
            owner_ = other.owner_;
            depth_ = other.depth_;
            active_ = other.active_;
            other.owner_ = nullptr;
            other.active_ = false;
        }
        return *this;
    }

    inline Blackboard::ScopeToken::~ScopeToken() { release(); }

    inline void Blackboard::ScopeToken::release() {
        if (active_ && owner_) {
            owner_->popScope();
            active_ = false;
        }
    }

    inline Blackboard::ScopeToken Blackboard::pushScope() {
        Observer observerCopy;
        size_t depth = 0;
        Event event{Event::Type::ScopePushed, "", std::type_index(typeid(void)), true, 0};
        {
            std::lock_guard<std::mutex> lock(mutex_);
            scopes_.emplace_back();
            depth = scopes_.size() - 1;
            event.scopeDepth = depth;
            observerCopy = observer_;
        }
        notify(observerCopy, event);
        return ScopeToken(this, depth);
    }

    inline void Blackboard::popScope() {
        Observer observerCopy;
        Event event{Event::Type::ScopePopped, "", std::type_index(typeid(void)), true, 0};
        bool shouldNotify = false;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (scopes_.size() <= 1)
                return;
            scopes_.pop_back();
            event.scopeDepth = scopes_.size() - 1;
            observerCopy = observer_;
            shouldNotify = true;
        }
        if (shouldNotify)
            notify(observerCopy, event);
    }

    inline void Blackboard::setObserver(Observer observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observer_ = std::move(observer);
    }

} // namespace bonsai::tree
