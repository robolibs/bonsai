#pragma once
#include <any>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace bonsai::tree {

    class Blackboard {
      public:
        template <typename T> inline void set(const std::string &key, T value) {
            std::lock_guard<std::mutex> lock(mutex_);
            data_[key] = std::make_any<T>(std::move(value));
        }

        template <typename T> inline std::optional<T> get(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = data_.find(key);
            if (it != data_.end()) {
                try {
                    return std::any_cast<T>(it->second);
                } catch (...) {
                    return std::nullopt;
                }
            }
            return std::nullopt;
        }

        inline bool has(const std::string &key) const {
            std::lock_guard<std::mutex> lock(mutex_);
            return data_.find(key) != data_.end();
        }

        inline void remove(const std::string &key) {
            std::lock_guard<std::mutex> lock(mutex_);
            data_.erase(key);
        }

        inline void clear() {
            std::lock_guard<std::mutex> lock(mutex_);
            data_.clear();
        }

      private:
        mutable std::mutex mutex_;
        std::unordered_map<std::string, std::any> data_;
    };

} // namespace bonsai::tree
