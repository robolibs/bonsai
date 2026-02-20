#pragma once

#include "relation.hpp"
#include <algorithm>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace stateup::logic {

    // -------------------------------------------------------------------------
    // Variable<Tuple>
    // -------------------------------------------------------------------------
    //
    // The semi-naive evaluation delta structure (ports Zodd's Variable(Tuple)).
    //
    // Three layers:
    //   stable_  — all facts confirmed in previous iterations (list of Relations)
    //   recent_  — the delta from the last changed() call (used by join operators)
    //   to_add_  — facts queued for the next changed() step
    //
    // Typical fixpoint loop:
    //   auto* v = iter.variable();
    //   v->insert_slice(seed_facts);
    //   while (iter.changed()) {
    //       // read from v->recent() and v->stable()
    //       // write new derived facts via v->insert_slice(...)
    //   }
    //   auto result = v->complete();

    template <TupleLike Tuple> class Variable {
      public:
        using RelList = std::vector<Relation<Tuple>>;

        explicit Variable(const ExecutionContext &ctx = {}) : ctx_(ctx) {}

        // ---- Insertion ------------------------------------------------------

        void insert(const Tuple &t) {
            std::vector<Tuple> v{t};
            to_add_.push_back(Relation<Tuple>::from_slice(std::move(v), ctx_));
        }

        void insert_slice(std::span<const Tuple> ts) {
            if (ts.empty())
                return;
            std::vector<Tuple> v(ts.begin(), ts.end());
            to_add_.push_back(Relation<Tuple>::from_slice(std::move(v), ctx_));
        }

        void insert_relation(Relation<Tuple> r) {
            if (!r.empty())
                to_add_.push_back(std::move(r));
        }

        // ---- Semi-naive step ------------------------------------------------

        // Advance one iteration step.
        // Returns true if new facts were derived (recent_ is non-empty).
        bool changed() {
            if (to_add_.empty()) {
                recent_ = Relation<Tuple>::empty_relation();
                return false;
            }

            // Step 1: merge all to_add_ into one candidate relation
            Relation<Tuple> candidate = to_add_[0];
            for (std::size_t i = 1; i < to_add_.size(); ++i)
                candidate = Relation<Tuple>::merge(candidate, to_add_[i], ctx_);
            to_add_.clear();

            // Step 2: set-difference: candidate minus stable_merged()
            const auto &stbl = stable_merged();
            auto se = stbl.begin();
            auto send = stbl.end();

            std::vector<Tuple> fresh;
            fresh.reserve(candidate.size());

            for (const auto &t : candidate.elements()) {
                // Gallop-advance se to first element >= t
                se = gallop_lower_bound(se, send, t);
                if (se == send || !(*se == t))
                    fresh.push_back(t);
            }

            if (fresh.empty()) {
                recent_ = Relation<Tuple>::empty_relation();
                return false;
            }

            // fresh is already sorted (subset of sorted candidate)
            recent_ = Relation<Tuple>::from_slice(std::move(fresh), ctx_);
            stable_.push_back(recent_);
            stable_dirty_ = true;
            return true;
        }

        // ---- Accessors ------------------------------------------------------

        const Relation<Tuple> &recent() const { return recent_; }

        // Returns a merged view of all stable layers (lazily cached).
        const Relation<Tuple> &stable() const { return stable_merged(); }

        std::size_t total_size() const {
            std::size_t n = recent_.size();
            for (const auto &r : stable_)
                n += r.size();
            for (const auto &r : to_add_)
                n += r.size();
            return n;
        }

        // ---- Finalize -------------------------------------------------------

        // Merge everything into a single Relation and return it.
        Relation<Tuple> complete() {
            // Drain to_add_ first
            if (!to_add_.empty()) {
                Relation<Tuple> candidate = to_add_[0];
                for (std::size_t i = 1; i < to_add_.size(); ++i)
                    candidate = Relation<Tuple>::merge(candidate, to_add_[i], ctx_);
                to_add_.clear();
                stable_.push_back(std::move(candidate));
                stable_dirty_ = true;
            }
            return stable_merged();
        }

        // ---- Reset ----------------------------------------------------------

        void reset() {
            stable_.clear();
            recent_ = Relation<Tuple>::empty_relation();
            to_add_.clear();
            stable_cache_.reset();
            stable_dirty_ = false;
        }

      private:
        // Gallop-accelerated lower_bound on raw pointers.
        // Phase 1: doubling window — find [lo, hi) containing the answer.
        // Phase 2: binary search in that window.
        static const Tuple *gallop_lower_bound(const Tuple *begin, const Tuple *end, const Tuple &key) {
            if (begin >= end)
                return end;
            const Tuple *lo = begin;
            const Tuple *hi = begin;
            std::ptrdiff_t step = 1;
            while (hi < end && *hi < key) {
                lo = hi;
                hi += step;
                step *= 2;
            }
            if (hi > end)
                hi = end;
            return std::lower_bound(lo, hi, key);
        }

        const Relation<Tuple> &stable_merged() const {
            if (!stable_dirty_ && stable_cache_.has_value())
                return *stable_cache_;
            if (stable_.empty()) {
                stable_cache_ = Relation<Tuple>::empty_relation();
                stable_dirty_ = false;
                return *stable_cache_;
            }
            Relation<Tuple> merged = stable_[0];
            for (std::size_t i = 1; i < stable_.size(); ++i)
                merged = Relation<Tuple>::merge(merged, stable_[i], ctx_);
            stable_cache_ = std::move(merged);
            stable_dirty_ = false;
            return *stable_cache_;
        }

        RelList stable_;
        Relation<Tuple> recent_;
        RelList to_add_;
        mutable std::optional<Relation<Tuple>> stable_cache_;
        mutable bool stable_dirty_ = false;
        ExecutionContext ctx_;
    };

} // namespace stateup::logic
