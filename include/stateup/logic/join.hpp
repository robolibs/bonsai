#pragma once

#include "variable.hpp"
#include <algorithm>
#include <concepts>
#include <functional>
#include <span>
#include <vector>

namespace stateup::logic {

    // =========================================================================
    // Gallop search utilities
    // =========================================================================
    //
    // Gallop search (exponential + binary) finds the first position >= key
    // in O(log k) where k is the distance from begin to the result.
    // Outperforms std::lower_bound when the target is close to the start.

    // Find first position >= key in [begin, end).
    // Compare must be a strict-weak-order (defaults to std::less<>).
    template <typename It, typename T, typename Compare = std::less<>>
    It gallop_lower_bound(It begin, It end, const T &key, Compare cmp = {}) {
        if (begin == end)
            return end;
        It lo = begin;
        std::size_t step = 1;
        // Phase 1: exponential steps
        while (lo != end && cmp(*lo, key)) {
            It next = lo;
            for (std::size_t i = 0; i < step && next != end; ++i)
                ++next;
            if (next == end || !cmp(*next, key)) {
                // Binary search in [lo, next)
                return std::lower_bound(lo, next, key, cmp);
            }
            lo = next;
            step *= 2;
        }
        return std::lower_bound(begin, lo, key, cmp);
    }

    // Find first position > key in [begin, end) (upper_bound variant).
    template <typename It, typename T, typename Compare = std::less<>>
    It gallop_upper_bound(It begin, It end, const T &key, Compare cmp = {}) {
        if (begin == end)
            return end;
        It lo = begin;
        std::size_t step = 1;
        while (lo != end && !cmp(key, *lo)) {
            It next = lo;
            for (std::size_t i = 0; i < step && next != end; ++i)
                ++next;
            if (next == end || cmp(key, *next)) {
                return std::upper_bound(lo, next, key, cmp);
            }
            lo = next;
            step *= 2;
        }
        return std::upper_bound(begin, lo, key, cmp);
    }

    // Find the range [first, last) of elements in sorted span where
    // key_extractor(*it) == key. Uses gallop for both ends.
    template <TupleLike Tuple, typename Key, typename KeyExtractor>
    requires KeyExtractorFor<Tuple, Key, KeyExtractor>
    std::pair<const Tuple *, const Tuple *> find_key_range(std::span<const Tuple> data, const Key &key,
                                                           KeyExtractor &&extractor) {
        if (data.empty())
            return {data.data(), data.data()};

        // Lower bound: first element where key_extractor(t) >= key
        auto lo_cmp = [&](const Tuple &t, const Key &k) { return extractor(t) < k; };
        auto hi_cmp = [&](const Key &k, const Tuple &t) { return k < extractor(t); };

        auto *first = gallop_lower_bound(data.data(), data.data() + data.size(), key, lo_cmp);
        if (first == data.data() + data.size() || extractor(*first) != key)
            return {first, first};

        auto *last = gallop_upper_bound(first, data.data() + data.size(), key, hi_cmp);
        return {first, last};
    }

    // =========================================================================
    // Internal sort-merge join helper
    // =========================================================================

    namespace detail {

        // Sort-merge join over two sorted spans sharing a common Key prefix.
        // For each matching pair (t1, t2) where key_ext1(t1) == key_ext2(t2),
        // calls combine(t1, t2) and pushes the result into out.
        template <TupleLike T1, TupleLike T2, TupleLike Result, typename Key, typename KeyExt1, typename KeyExt2,
                  typename Combiner>
        void merge_join(std::span<const T1> left, std::span<const T2> right, std::vector<Result> &out, KeyExt1 key_ext1,
                        KeyExt2 key_ext2, Combiner combine) {
            if (left.empty() || right.empty())
                return;

            const T1 *li = left.data();
            const T1 *le = left.data() + left.size();
            const T2 *ri = right.data();
            const T2 *re = right.data() + right.size();

            while (li != le && ri != re) {
                Key lk = key_ext1(*li);
                Key rk = key_ext2(*ri);

                if (lk < rk) {
                    li = gallop_lower_bound(li, le, rk, [&](const T1 &t, const Key &k) { return key_ext1(t) < k; });
                } else if (rk < lk) {
                    ri = gallop_lower_bound(ri, re, lk, [&](const T2 &t, const Key &k) { return key_ext2(t) < k; });
                } else {
                    // Keys match — find the full extent of the match on both sides
                    const T1 *lend = li;
                    while (lend != le && key_ext1(*lend) == lk)
                        ++lend;
                    const T2 *rend = ri;
                    while (rend != re && key_ext2(*rend) == rk)
                        ++rend;

                    for (const T1 *a = li; a != lend; ++a)
                        for (const T2 *b = ri; b != rend; ++b)
                            out.push_back(combine(*a, *b));

                    li = lend;
                    ri = rend;
                }
            }
        }

    } // namespace detail

    // =========================================================================
    // join_into — semi-naive binary join
    // =========================================================================
    //
    // Joins Variable<T1> and Variable<T2> on a shared Key.
    // Follows semi-naive pattern: processes three passes so only new facts
    // derived from the current iteration's delta are emitted:
    //   Pass 1: stable(left) × recent(right)
    //   Pass 2: recent(left) × stable(right)
    //   Pass 3: recent(left) × recent(right)
    //
    // Results are inserted into *output.
    // Parallel execution (if ctx.has_parallel()): one task per pass
    // (3 tasks submitted to the thread pool, synchronized before return).

    template <TupleLike T1, TupleLike T2, TupleLike Result, typename Key, typename KeyExt1, typename KeyExt2,
              typename Combiner>
    requires KeyExtractorFor<T1, Key, KeyExt1> && KeyExtractorFor<T2, Key, KeyExt2> &&
             std::invocable<Combiner, const T1 &, const T2 &> &&
             std::same_as<std::invoke_result_t<Combiner, const T1 &, const T2 &>, Result>
    void join_into(Variable<T1> &left, Variable<T2> &right, Variable<Result> *output, KeyExt1 key_ext1,
                   KeyExt2 key_ext2, Combiner combine, const ExecutionContext &ctx = {}) {

        auto run_pass = [&](std::span<const T1> l, std::span<const T2> r) {
            std::vector<Result> buf;
            detail::merge_join<T1, T2, Result, Key>(l, r, buf, key_ext1, key_ext2, combine);
            if (!buf.empty())
                output->insert_slice(std::span<const Result>(buf));
        };

        auto sl = left.stable().elements();
        auto rl = left.recent().elements();
        auto sr = right.stable().elements();
        auto rr = right.recent().elements();

        if (ctx.has_parallel()) {
            auto *pool = ctx.pool();
            std::vector<std::vector<Result>> results(3);
            pool->bulk(
                [&](std::size_t i) {
                    switch (i) {
                    case 0:
                        detail::merge_join<T1, T2, Result, Key>(sl, rr, results[0], key_ext1, key_ext2, combine);
                        break;
                    case 1:
                        detail::merge_join<T1, T2, Result, Key>(rl, sr, results[1], key_ext1, key_ext2, combine);
                        break;
                    case 2:
                        detail::merge_join<T1, T2, Result, Key>(rl, rr, results[2], key_ext1, key_ext2, combine);
                        break;
                    }
                },
                3);
            for (auto &r : results)
                if (!r.empty())
                    output->insert_slice(std::span<const Result>(r));
        } else {
            run_pass(sl, rr);
            run_pass(rl, sr);
            run_pass(rl, rr);
        }
    }

    // =========================================================================
    // join_anti — anti-join
    // =========================================================================
    //
    // Emits tuples from left's recent delta for which NO matching tuple exists
    // in right (either stable or recent). Results inserted into *output.

    template <TupleLike T1, TupleLike T2, typename Key, typename KeyExt1, typename KeyExt2>
    requires KeyExtractorFor<T1, Key, KeyExt1> && KeyExtractorFor<T2, Key, KeyExt2>
    void join_anti(Variable<T1> &left, Variable<T2> &right, Variable<T1> *output, KeyExt1 key_ext1, KeyExt2 key_ext2,
                   const ExecutionContext & /*ctx*/ = {}) {

        auto process = [&](std::span<const T1> source) {
            auto sr = right.stable().elements();
            auto rr = right.recent().elements();
            std::vector<T1> out;

            for (const auto &t : source) {
                Key k = key_ext1(t);
                // Check if key exists in right.stable or right.recent
                auto has_key = [&](std::span<const T2> span) {
                    auto it = std::lower_bound(span.begin(), span.end(), k,
                                               [&](const T2 &t2, const Key &key) { return key_ext2(t2) < key; });
                    return it != span.end() && key_ext2(*it) == k;
                };
                if (!has_key(sr) && !has_key(rr))
                    out.push_back(t);
            }
            if (!out.empty())
                output->insert_slice(std::span<const T1>(out));
        };

        process(left.recent().elements());
    }

} // namespace stateup::logic
