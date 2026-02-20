#pragma once

#include "context.hpp"
#include "relation.hpp"
#include <algorithm>
#include <concepts>
#include <functional>
#include <mutex>
#include <ranges>
#include <span>
#include <vector>

namespace stateup::logic {

    // =========================================================================
    // aggregate — group-by aggregation
    // =========================================================================
    //
    // Groups input tuples by Key (via key_ext), extracts an AggVal per tuple
    // (via val_ext), and folds each group using fold_fn starting from identity.
    //
    // Parallel strategy (ports Zodd's aggregate.zig):
    //   1. Parallel extraction: divide input into 256-item chunks, each worker
    //      builds a local vector<pair<Key,AggVal>> — no locking needed.
    //   2. Merge all local vectors into one.
    //   3. Sort merged vector by Key using std::ranges::sort (pdqsort).
    //   4. Serial fold pass: accumulate consecutive equal-key runs.
    //
    // Returns a vector of {Key, AggVal} pairs, one per group, sorted by Key.

    template <TupleLike Tuple, typename Key, typename AggVal, typename KeyExtractor, typename ValExtractor,
              typename FoldFn>
    requires KeyExtractorFor<Tuple, Key, KeyExtractor> && std::invocable<ValExtractor, const Tuple &> &&
             std::same_as<std::invoke_result_t<ValExtractor, const Tuple &>, AggVal> &&
             std::invocable<FoldFn, AggVal, AggVal> &&
             std::same_as<std::invoke_result_t<FoldFn, AggVal, AggVal>, AggVal> && std::totally_ordered<Key>
    std::vector<std::pair<Key, AggVal>> aggregate(std::span<const Tuple> input, KeyExtractor key_ext,
                                                  ValExtractor val_ext, FoldFn fold_fn, AggVal identity,
                                                  const ExecutionContext &ctx = {}) {

        if (input.empty())
            return {};

        constexpr std::size_t kChunkSize = 256;
        const std::size_t n = input.size();
        const std::size_t num_chunks = (n + kChunkSize - 1) / kChunkSize;

        // Step 1: extract (key, val) pairs — parallel if available
        std::vector<std::vector<std::pair<Key, AggVal>>> local_vecs(num_chunks);

        auto extract_chunk = [&](std::size_t ci) {
            const std::size_t lo = ci * kChunkSize;
            const std::size_t hi = std::min(lo + kChunkSize, n);
            local_vecs[ci].reserve(hi - lo);
            for (std::size_t i = lo; i < hi; ++i)
                local_vecs[ci].emplace_back(key_ext(input[i]), val_ext(input[i]));
        };

        if (ctx.has_parallel() && num_chunks > 1) {
            ctx.pool()->bulk(extract_chunk, num_chunks);
        } else {
            for (std::size_t ci = 0; ci < num_chunks; ++ci)
                extract_chunk(ci);
        }

        // Step 2: merge all local vectors
        std::vector<std::pair<Key, AggVal>> pairs;
        pairs.reserve(n);
        for (auto &lv : local_vecs)
            for (auto &p : lv)
                pairs.push_back(std::move(p));

        // Step 3: sort by key
        std::ranges::sort(pairs, [](const auto &a, const auto &b) { return a.first < b.first; });

        // Step 4: fold consecutive equal-key runs
        std::vector<std::pair<Key, AggVal>> result;
        result.reserve(pairs.size()); // upper bound

        std::size_t i = 0;
        while (i < pairs.size()) {
            Key cur_key = pairs[i].first;
            AggVal acc = identity;
            while (i < pairs.size() && pairs[i].first == cur_key) {
                acc = fold_fn(acc, pairs[i].second);
                ++i;
            }
            result.emplace_back(cur_key, std::move(acc));
        }

        return result;
    }

} // namespace stateup::logic
