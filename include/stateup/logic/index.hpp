#pragma once

#include "relation.hpp"
#include <concepts>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace stateup::logic {

    // =========================================================================
    // SecondaryIndex<Tuple, Key, KeyExtractor, KeyCompare>
    // =========================================================================
    //
    // Secondary index over a Relation, keyed by KeyExtractor(tuple).
    // Backs the index with std::map<Key, vector<Tuple>> (red-black tree),
    // providing O(log N) point lookups and O(log N + K) range queries.
    //
    // This ports Zodd's index.zig (BTreeMap with configurable branching factor).
    // std::map is the idiomatic C++ equivalent for in-memory secondary indexes.

    template <TupleLike Tuple, typename Key, typename KeyExtractor, typename KeyCompare = std::less<Key>>
    requires KeyExtractorFor<Tuple, Key, KeyExtractor>
    class SecondaryIndex {
      public:
        // Build index by scanning the full Relation.
        explicit SecondaryIndex(const Relation<Tuple> &source, KeyExtractor key_ext, KeyCompare key_cmp = {})
            : key_ext_(key_ext), key_cmp_(key_cmp), index_(key_cmp) {
            for (const auto &t : source.elements()) {
                index_[key_ext_(t)].push_back(t);
                ++total_size_;
            }
        }

        // ---- Point lookup ---------------------------------------------------

        // Returns a span of all tuples with exactly this key.
        // Returns empty span if key not found.
        std::span<const Tuple> get(const Key &key) const {
            auto it = index_.find(key);
            if (it == index_.end())
                return {};
            return std::span<const Tuple>(it->second);
        }

        // ---- Range query ----------------------------------------------------

        // Returns a vector of spans for all tuples with key in [lo, hi] (inclusive).
        std::vector<std::span<const Tuple>> get_range(const Key &lo, const Key &hi) const {
            std::vector<std::span<const Tuple>> result;
            auto it = index_.lower_bound(lo);
            auto end = index_.upper_bound(hi);
            for (; it != end; ++it)
                result.emplace_back(std::span<const Tuple>(it->second));
            return result;
        }

        // ---- Insertion ------------------------------------------------------

        // Insert a single tuple into the index.
        void insert(const Tuple &t) {
            auto &bucket = index_[key_ext_(t)];
            // Maintain sorted order within bucket
            auto pos = std::lower_bound(bucket.begin(), bucket.end(), t);
            if (pos == bucket.end() || !(*pos == t)) {
                bucket.insert(pos, t);
                ++total_size_;
            }
        }

        void insert_slice(std::span<const Tuple> ts) {
            for (const auto &t : ts)
                insert(t);
        }

        // ---- Accessors ------------------------------------------------------

        bool empty() const { return total_size_ == 0; }
        std::size_t size() const { return total_size_; }
        std::size_t num_keys() const { return index_.size(); }

      private:
        using BucketMap = std::map<Key, std::vector<Tuple>, KeyCompare>;

        KeyExtractor key_ext_;
        KeyCompare key_cmp_;
        BucketMap index_;
        std::size_t total_size_ = 0;
    };

    // CTAD deduction guide: deduce Key from KeyExtractor's return type.
    // Allows: SecondaryIndex idx(relation, [](const T& t){ return t.key; });
    template <TupleLike Tuple, typename KeyExtractor>
    SecondaryIndex(const Relation<Tuple> &, KeyExtractor)
        -> SecondaryIndex<Tuple, std::invoke_result_t<KeyExtractor, const Tuple &>, KeyExtractor>;

} // namespace stateup::logic
