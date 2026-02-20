#pragma once

#include "variable.hpp"
#include <cstddef>
#include <memory>
#include <vector>

namespace stateup::logic {

    // -------------------------------------------------------------------------
    // Iteration<Tuple>
    // -------------------------------------------------------------------------
    //
    // Manages a collection of Variable<Tuple> instances and drives the
    // semi-naive fixpoint loop (ports Zodd's Iteration(Tuple)).
    //
    // Usage:
    //   Iteration<Edge> iter(ctx);
    //   auto* reachable = iter.variable();
    //   reachable->insert_slice(base_edges);
    //
    //   while (iter.changed()) {
    //       join_into(*reachable, *edges, reachable, ...);
    //   }
    //   auto result = reachable->complete();

    template <TupleLike Tuple> class Iteration {
      public:
        static constexpr std::size_t kDefaultMaxIterations = 1'000'000;

        explicit Iteration(const ExecutionContext &ctx = {}, std::size_t max_iterations = kDefaultMaxIterations)
            : ctx_(ctx), max_iterations_(max_iterations) {}

        // Allocate a new Variable managed by this Iteration.
        // The returned pointer is valid for the lifetime of this Iteration.
        Variable<Tuple> *variable() {
            variables_.push_back(std::make_unique<Variable<Tuple>>(ctx_));
            return variables_.back().get();
        }

        // Run one semi-naive step across all variables.
        // Returns true if any variable produced new facts.
        // Increments the iteration counter; returns false (and stops) if
        // max_iterations is reached.
        bool changed() {
            if (current_iteration_ >= max_iterations_)
                return false;
            ++current_iteration_;

            bool any_changed = false;
            for (auto &v : variables_)
                if (v->changed())
                    any_changed = true;
            return any_changed;
        }

        // Reset all variables and the iteration counter.
        void reset() {
            for (auto &v : variables_)
                v->reset();
            current_iteration_ = 0;
        }

        // ---- Accessors ------------------------------------------------------

        std::size_t current_iteration() const { return current_iteration_; }
        std::size_t max_iterations() const { return max_iterations_; }

      private:
        using VarList = std::vector<std::unique_ptr<Variable<Tuple>>>;

        VarList variables_;
        ExecutionContext ctx_;
        std::size_t max_iterations_;
        std::size_t current_iteration_ = 0;
    };

} // namespace stateup::logic
