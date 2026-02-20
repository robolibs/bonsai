#pragma once

#include "join.hpp"
#include "variable.hpp"
#include <algorithm>
#include <memory>
#include <ranges>
#include <span>
#include <vector>

namespace stateup::logic {

    // =========================================================================
    // Leaper<Tuple, Val> — virtual base for leapfrog trie join strategies
    // =========================================================================
    //
    // A Leaper constrains what Val values are valid for a given prefix Tuple.
    // Used by extend_into to perform multi-way leapfrog trie joins.
    //
    // Virtual dispatch is chosen (over std::function) because:
    //   - Leapers are stored heterogeneously (vector<unique_ptr<Leaper>>)
    //   - clone() is required per parallel chunk (each chunk needs its own state)
    //   - The hot path (propose/intersect in tight loop) benefits from
    //     predictable virtual dispatch (same concrete type per slot)

    template <TupleLike Tuple, typename Val> class Leaper {
      public:
        virtual ~Leaper() = default;

        // Count candidate values for the given prefix.
        // Used to sort leapers cheapest-first (ascending count).
        virtual std::size_t count(const Tuple &prefix) const = 0;

        // Write the smallest candidate Val for this prefix into *val.
        // Returns false if no candidates exist.
        virtual bool propose(const Tuple &prefix, Val *val) const = 0;

        // Advance *val to the first value >= *val that this leaper accepts.
        // Returns false if no such value exists.
        virtual bool intersect(const Tuple &prefix, Val *val) const = 0;

        // Deep-copy for parallel chunk processing.
        virtual std::unique_ptr<Leaper<Tuple, Val>> clone() const = 0;
    };

    // =========================================================================
    // ExtendWith<Tuple, Key, Val>
    // =========================================================================
    //
    // Semi-join extend: for a given prefix Tuple, proposes Val values from a
    // source Relation<SourceTuple> where key_ext(source_tuple) matches
    // prefix_key_ext(prefix).
    //
    // SourceTuple is a user-defined struct with Key and Val fields.

    template <TupleLike Tuple, typename Key, typename Val, TupleLike SourceTuple, typename PrefixKeyExt,
              typename SrcKeyExt, typename SrcValExt>
    requires KeyExtractorFor<Tuple, Key, PrefixKeyExt> && KeyExtractorFor<SourceTuple, Key, SrcKeyExt>
    class ExtendWith final : public Leaper<Tuple, Val> {
      public:
        ExtendWith(const Relation<SourceTuple> &source, PrefixKeyExt prefix_key_ext, SrcKeyExt src_key_ext,
                   SrcValExt src_val_ext)
            : source_(source), prefix_key_ext_(prefix_key_ext), src_key_ext_(src_key_ext), src_val_ext_(src_val_ext) {}

        std::size_t count(const Tuple &prefix) const override {
            Key k = prefix_key_ext_(prefix);
            auto [f, l] = find_key_range(source_.elements(), k, src_key_ext_);
            return static_cast<std::size_t>(l - f);
        }

        bool propose(const Tuple &prefix, Val *val) const override {
            Key k = prefix_key_ext_(prefix);
            auto [f, l] = find_key_range(source_.elements(), k, src_key_ext_);
            if (f == l)
                return false;
            *val = src_val_ext_(*f);
            return true;
        }

        bool intersect(const Tuple &prefix, Val *val) const override {
            Key k = prefix_key_ext_(prefix);
            auto [f, l] = find_key_range(source_.elements(), k, src_key_ext_);
            if (f == l)
                return false;
            // Find first src element with val >= *val
            auto it =
                std::lower_bound(f, l, *val, [&](const SourceTuple &t, const Val &v) { return src_val_ext_(t) < v; });
            if (it == l)
                return false;
            *val = src_val_ext_(*it);
            return true;
        }

        std::unique_ptr<Leaper<Tuple, Val>> clone() const override { return std::make_unique<ExtendWith>(*this); }

      private:
        Relation<SourceTuple> source_;
        PrefixKeyExt prefix_key_ext_;
        SrcKeyExt src_key_ext_;
        SrcValExt src_val_ext_;
    };

    // CTAD deduction guide for ExtendWith.
    // Deduces Key from PrefixKeyExt return type, Val from SrcValExt return type.
    template <TupleLike Tuple, TupleLike SourceTuple, typename PrefixKeyExt, typename SrcKeyExt, typename SrcValExt>
    ExtendWith(const Relation<SourceTuple> &, PrefixKeyExt, SrcKeyExt, SrcValExt)
        -> ExtendWith<Tuple, std::invoke_result_t<PrefixKeyExt, const Tuple &>,
                      std::invoke_result_t<SrcValExt, const SourceTuple &>, SourceTuple, PrefixKeyExt, SrcKeyExt,
                      SrcValExt>;

    // Factory helper: make_extend_with<Tuple>(source, prefix_key_ext, src_key_ext, src_val_ext)
    // Fully deduces all template parameters from the arguments.
    template <TupleLike Tuple, TupleLike SourceTuple, typename PrefixKeyExt, typename SrcKeyExt, typename SrcValExt>
    auto make_extend_with(const Relation<SourceTuple> &source, PrefixKeyExt prefix_key_ext, SrcKeyExt src_key_ext,
                          SrcValExt src_val_ext) {
        using Key = std::invoke_result_t<PrefixKeyExt, const Tuple &>;
        using Val = std::invoke_result_t<SrcValExt, const SourceTuple &>;
        return ExtendWith<Tuple, Key, Val, SourceTuple, PrefixKeyExt, SrcKeyExt, SrcValExt>(source, prefix_key_ext,
                                                                                            src_key_ext, src_val_ext);
    }

    // =========================================================================
    // FilterAnti<Tuple, Key, Val, SourceTuple, ...>
    // =========================================================================
    //
    // Anti-join filter: count() returns 0 if the (key, val) pair is found in
    // the source, effectively blocking that value from being emitted.
    // Returns SIZE_MAX (maxInt) if not found (unlimited candidates).

    template <TupleLike Tuple, typename Key, typename Val, TupleLike SourceTuple, typename PrefixKeyExt,
              typename SrcKeyExt, typename SrcValExt>
    requires KeyExtractorFor<Tuple, Key, PrefixKeyExt> && KeyExtractorFor<SourceTuple, Key, SrcKeyExt>
    class FilterAnti final : public Leaper<Tuple, Val> {
      public:
        FilterAnti(const Relation<SourceTuple> &source, PrefixKeyExt prefix_key_ext, SrcKeyExt src_key_ext,
                   SrcValExt src_val_ext)
            : source_(source), prefix_key_ext_(prefix_key_ext), src_key_ext_(src_key_ext), src_val_ext_(src_val_ext) {}

        std::size_t count(const Tuple &prefix) const override {
            // Return 0 if any matching (key, val) exists — blocks this leaper
            // from producing candidates. Return SIZE_MAX if no match (pass-through).
            Key k = prefix_key_ext_(prefix);
            auto [f, l] = find_key_range(source_.elements(), k, src_key_ext_);
            return (f == l) ? std::numeric_limits<std::size_t>::max() : 0;
        }

        bool propose(const Tuple & /*prefix*/, Val * /*val*/) const override {
            // FilterAnti never proposes; it only intersects (filters).
            return false;
        }

        bool intersect(const Tuple &prefix, Val *val) const override {
            Key k = prefix_key_ext_(prefix);
            auto [f, l] = find_key_range(source_.elements(), k, src_key_ext_);
            if (f == l)
                return true; // no exclusions — pass through
            // Find the first excluded val >= *val
            auto it =
                std::lower_bound(f, l, *val, [&](const SourceTuple &t, const Val &v) { return src_val_ext_(t) < v; });
            if (it == l)
                return true;
            if (src_val_ext_(*it) == *val)
                return false; // this val is excluded
            // *val is not excluded; pass through
            return true;
        }

        std::unique_ptr<Leaper<Tuple, Val>> clone() const override { return std::make_unique<FilterAnti>(*this); }

      private:
        Relation<SourceTuple> source_;
        PrefixKeyExt prefix_key_ext_;
        SrcKeyExt src_key_ext_;
        SrcValExt src_val_ext_;
    };

    // =========================================================================
    // ExtendAnti<Tuple, Key, Val, SourceTuple, ExcludeTuple, ...>
    // =========================================================================
    //
    // Extend that excludes values already present in an exclude relation.
    // Proposes values from base_source, skips any that appear in exclude_source.

    template <TupleLike Tuple, typename Key, typename Val, TupleLike SourceTuple, TupleLike ExcludeTuple,
              typename PrefixKeyExt, typename SrcKeyExt, typename SrcValExt, typename ExclKeyExt, typename ExclValExt>
    requires KeyExtractorFor<Tuple, Key, PrefixKeyExt> && KeyExtractorFor<SourceTuple, Key, SrcKeyExt> &&
             KeyExtractorFor<ExcludeTuple, Key, ExclKeyExt>
    class ExtendAnti final : public Leaper<Tuple, Val> {
      public:
        ExtendAnti(const Relation<SourceTuple> &base_source, const Relation<ExcludeTuple> &exclude_source,
                   PrefixKeyExt prefix_key_ext, SrcKeyExt src_key_ext, SrcValExt src_val_ext, ExclKeyExt excl_key_ext,
                   ExclValExt excl_val_ext)
            : base_source_(base_source), exclude_source_(exclude_source), prefix_key_ext_(prefix_key_ext),
              src_key_ext_(src_key_ext), src_val_ext_(src_val_ext), excl_key_ext_(excl_key_ext),
              excl_val_ext_(excl_val_ext) {}

        std::size_t count(const Tuple &prefix) const override {
            Key k = prefix_key_ext_(prefix);
            auto [bf, bl] = find_key_range(base_source_.elements(), k, src_key_ext_);
            auto [ef, el] = find_key_range(exclude_source_.elements(), k, excl_key_ext_);
            // Conservative estimate: base count minus exclude count (lower bound)
            std::size_t base_cnt = static_cast<std::size_t>(bl - bf);
            std::size_t excl_cnt = static_cast<std::size_t>(el - ef);
            return (base_cnt > excl_cnt) ? base_cnt - excl_cnt : 0;
        }

        bool propose(const Tuple &prefix, Val *val) const override {
            Key k = prefix_key_ext_(prefix);
            auto [bf, bl] = find_key_range(base_source_.elements(), k, src_key_ext_);
            auto [ef, el] = find_key_range(exclude_source_.elements(), k, excl_key_ext_);
            if (bf == bl)
                return false;
            // Find the first base val not present in exclude
            for (auto it = bf; it != bl; ++it) {
                Val v = src_val_ext_(*it);
                auto eit = std::lower_bound(
                    ef, el, v, [&](const ExcludeTuple &t, const Val &vv) { return excl_val_ext_(t) < vv; });
                if (eit == el || excl_val_ext_(*eit) != v) {
                    *val = v;
                    return true;
                }
            }
            return false;
        }

        bool intersect(const Tuple &prefix, Val *val) const override {
            Key k = prefix_key_ext_(prefix);
            auto [bf, bl] = find_key_range(base_source_.elements(), k, src_key_ext_);
            auto [ef, el] = find_key_range(exclude_source_.elements(), k, excl_key_ext_);
            if (bf == bl)
                return false;
            // Advance to first base val >= *val, not in exclude
            auto bit =
                std::lower_bound(bf, bl, *val, [&](const SourceTuple &t, const Val &v) { return src_val_ext_(t) < v; });
            for (; bit != bl; ++bit) {
                Val v = src_val_ext_(*bit);
                auto eit = std::lower_bound(
                    ef, el, v, [&](const ExcludeTuple &t, const Val &vv) { return excl_val_ext_(t) < vv; });
                if (eit == el || excl_val_ext_(*eit) != v) {
                    *val = v;
                    return true;
                }
            }
            return false;
        }

        std::unique_ptr<Leaper<Tuple, Val>> clone() const override { return std::make_unique<ExtendAnti>(*this); }

      private:
        Relation<SourceTuple> base_source_;
        Relation<ExcludeTuple> exclude_source_;
        PrefixKeyExt prefix_key_ext_;
        SrcKeyExt src_key_ext_;
        SrcValExt src_val_ext_;
        ExclKeyExt excl_key_ext_;
        ExclValExt excl_val_ext_;
    };

    // =========================================================================
    // extend_into — parallel leapfrog trie join
    // =========================================================================
    //
    // For each tuple in source, uses the leapers to propose and intersect
    // candidate Val values, emitting combine(tuple, val) for each agreed value.
    //
    // Parallel: processes kLeapfrogChunkSize=128 tuples per task via pool->bulk.
    // Each chunk clones all leapers (thread-local state).
    //
    // Val must be std::incrementable so we can advance past emitted values.
    // For non-incrementable types, use the next_fn overload below.

    inline constexpr std::size_t kLeapfrogChunkSize = 128;

    template <TupleLike SourceTuple, TupleLike OutputTuple, typename Val, typename Combiner>
    requires std::invocable<Combiner, const SourceTuple &, const Val &> &&
             std::same_as<std::invoke_result_t<Combiner, const SourceTuple &, const Val &>, OutputTuple> &&
             std::incrementable<Val>
    void extend_into(const Relation<SourceTuple> &source, std::span<Leaper<SourceTuple, Val> *const> leapers,
                     Variable<OutputTuple> *output, Combiner combine, const ExecutionContext &ctx = {}) {

        auto process_chunk = [&](std::span<const SourceTuple> chunk,
                                 std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> &local_leapers,
                                 std::vector<OutputTuple> &results) {
            for (const auto &prefix : chunk) {
                // Sort leapers by count ascending (cheapest first)
                std::sort(local_leapers.begin(), local_leapers.end(),
                          [&](const auto &a, const auto &b) { return a->count(prefix) < b->count(prefix); });

                Val val{};
                if (!local_leapers[0]->propose(prefix, &val))
                    continue;

                // Leapfrog: intersect all leapers repeatedly
                for (;;) {
                    bool all_accept = true;
                    for (auto &lp : local_leapers) {
                        if (!lp->intersect(prefix, &val)) {
                            all_accept = false;
                            break;
                        }
                    }
                    if (!all_accept)
                        break;
                    results.push_back(combine(prefix, val));
                    ++val; // advance to next candidate
                }
            }
        };

        auto elems = source.elements();
        const std::size_t n = elems.size();
        const std::size_t num_chunks = (n + kLeapfrogChunkSize - 1) / kLeapfrogChunkSize;

        if (ctx.has_parallel() && num_chunks > 1) {
            std::vector<std::vector<OutputTuple>> chunk_results(num_chunks);

            ctx.pool()->bulk(
                [&](std::size_t ci) {
                    const std::size_t lo = ci * kLeapfrogChunkSize;
                    const std::size_t hi = std::min(lo + kLeapfrogChunkSize, n);
                    auto chunk = elems.subspan(lo, hi - lo);

                    // Clone leapers for this chunk (thread-local)
                    std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> local;
                    local.reserve(leapers.size());
                    for (auto *lp : leapers)
                        local.push_back(lp->clone());

                    process_chunk(chunk, local, chunk_results[ci]);
                },
                num_chunks);

            for (auto &r : chunk_results)
                if (!r.empty())
                    output->insert_slice(std::span<const OutputTuple>(r));
        } else {
            // Single-threaded: no cloning needed
            std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> local;
            local.reserve(leapers.size());
            for (auto *lp : leapers)
                local.push_back(lp->clone());

            std::vector<OutputTuple> results;
            process_chunk(elems, local, results);
            if (!results.empty())
                output->insert_slice(std::span<const OutputTuple>(results));
        }
    }

    // Overload with explicit next_fn for non-incrementable Val types.
    template <TupleLike SourceTuple, TupleLike OutputTuple, typename Val, typename Combiner, typename NextFn>
    requires std::invocable<Combiner, const SourceTuple &, const Val &> &&
             std::same_as<std::invoke_result_t<Combiner, const SourceTuple &, const Val &>, OutputTuple> &&
             std::invocable<NextFn, Val> && std::same_as<std::invoke_result_t<NextFn, Val>, Val>
    void extend_into(const Relation<SourceTuple> &source, std::span<Leaper<SourceTuple, Val> *const> leapers,
                     Variable<OutputTuple> *output, Combiner combine, NextFn next_fn,
                     const ExecutionContext &ctx = {}) {

        auto process_chunk = [&](std::span<const SourceTuple> chunk,
                                 std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> &local_leapers,
                                 std::vector<OutputTuple> &results) {
            for (const auto &prefix : chunk) {
                std::sort(local_leapers.begin(), local_leapers.end(),
                          [&](const auto &a, const auto &b) { return a->count(prefix) < b->count(prefix); });

                Val val{};
                if (!local_leapers[0]->propose(prefix, &val))
                    continue;

                for (;;) {
                    bool all_accept = true;
                    for (auto &lp : local_leapers) {
                        if (!lp->intersect(prefix, &val)) {
                            all_accept = false;
                            break;
                        }
                    }
                    if (!all_accept)
                        break;
                    results.push_back(combine(prefix, val));
                    val = next_fn(val);
                }
            }
        };

        auto elems = source.elements();
        const std::size_t n = elems.size();
        const std::size_t num_chunks = (n + kLeapfrogChunkSize - 1) / kLeapfrogChunkSize;

        if (ctx.has_parallel() && num_chunks > 1) {
            std::vector<std::vector<OutputTuple>> chunk_results(num_chunks);
            ctx.pool()->bulk(
                [&](std::size_t ci) {
                    const std::size_t lo = ci * kLeapfrogChunkSize;
                    const std::size_t hi = std::min(lo + kLeapfrogChunkSize, n);
                    auto chunk = elems.subspan(lo, hi - lo);

                    std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> local;
                    local.reserve(leapers.size());
                    for (auto *lp : leapers)
                        local.push_back(lp->clone());

                    process_chunk(chunk, local, chunk_results[ci]);
                },
                num_chunks);
            for (auto &r : chunk_results)
                if (!r.empty())
                    output->insert_slice(std::span<const OutputTuple>(r));
        } else {
            std::vector<std::unique_ptr<Leaper<SourceTuple, Val>>> local;
            local.reserve(leapers.size());
            for (auto *lp : leapers)
                local.push_back(lp->clone());

            std::vector<OutputTuple> results;
            process_chunk(elems, local, results);
            if (!results.empty())
                output->insert_slice(std::span<const OutputTuple>(results));
        }
    }

} // namespace stateup::logic
