#pragma once
#include <any>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <typeindex>
#include <utility>
#include <unordered_map>
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
            std::lock_guard<std::mutex> lock(mutex_);
            Entry entry{std::make_any<T>(std::move(value)), std::type_index(typeid(T))};
            scopes_.back()[key] = std::move(entry);
            notify({Event::Type::Set, key, std::type_index(typeid(T)), true, scopes_.size() - 1});
        }

        template <typename T> inline std::optional<T> get(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto [entry, depth] = findEntry(key);
            if (!entry) {
                notify({Event::Type::Get, key, std::type_index(typeid(T)), false, depth});
                return std::nullopt;
            }

            if (entry->type != std::type_index(typeid(T))) {
                notify({Event::Type::Get, key, std::type_index(typeid(T)), false, depth});
                return std::nullopt;
            }

            try {
                notify({Event::Type::Get, key, entry->type, true, depth});
                return std::any_cast<T>(entry->value);
            } catch (const std::bad_any_cast &) {
                notify({Event::Type::Get, key, entry->type, false, depth});
                return std::nullopt;
            }
        }

        inline bool has(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return findEntry(key).first != nullptr;
        }

        inline void remove(const std::string &key) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto &current = scopes_.back();
            bool removed = current.erase(key) > 0;
            notify({Event::Type::Remove, key, std::type_index(typeid(void)), removed, scopes_.size() - 1});
        }

        inline void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            scopes_.assign(1, {});
            notify({Event::Type::Clear, "", std::type_index(typeid(void)), true, 0});
        }

        ScopeToken pushScope();
        void popScope();

        void setObserver(Observer observer);

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

        inline void notify(const Event &event) const {
            if (observer_)
                observer_(event);
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
        std::lock_guard<std::mutex> lock(mutex_);
        scopes_.emplace_back();
        notify({Event::Type::ScopePushed, "", std::type_index(typeid(void)), true, scopes_.size() - 1});
        return ScopeToken(this, scopes_.size() - 1);
    }

    inline void Blackboard::popScope() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (scopes_.size() <= 1)
            return;
        scopes_.pop_back();
        notify({Event::Type::ScopePopped, "", std::type_index(typeid(void)), true, scopes_.size() - 1});
    }

    inline void Blackboard::setObserver(Observer observer) {
        std::lock_guard<std::mutex> lock(mutex_);
        observer_ = std::move(observer);
    }

} // namespace bonsai::tree
