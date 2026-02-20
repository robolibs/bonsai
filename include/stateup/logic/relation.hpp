#pragma once

#include "context.hpp"
#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <cstring>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace stateup::logic {

    // -------------------------------------------------------------------------
    // Concepts
    // -------------------------------------------------------------------------

    // A TupleLike type must be lexicographically orderable and trivially copyable.
    // Users define operator< and operator== (or use C++20 <=> to derive them).
    template <typename T>
    concept TupleLike = requires(T a, T b) {
        { a < b } -> std::convertible_to<bool>;
        { a == b } -> std::convertible_to<bool>;
    } && std::is_trivially_copyable_v<T> && std::is_aggregate_v<T>;

    // A KeyExtractor maps Tuple -> Key.
    template <typename Tuple, typename Key, typename Extractor>
    concept KeyExtractorFor = TupleLike<Tuple> && std::invocable<Extractor, const Tuple &> &&
                              std::same_as<std::invoke_result_t<Extractor, const Tuple &>, Key>;

    // -------------------------------------------------------------------------
    // Relation<Tuple>
    // -------------------------------------------------------------------------

    // Immutable, sorted, deduplicated set of tuples.
    // Uses shared_ptr<const vector<Tuple>> for value semantics with cheap copies.
    // All construction goes through factory methods (from_slice, merge, load).
    template <TupleLike Tuple> class Relation {
      public:
        using value_type = Tuple;
        using const_iterator = const Tuple *;

        // ---- Factory methods ------------------------------------------------

        // Build a relation from an unsorted, possibly duplicate slice.
        // Sorts (parallel if ctx.has_parallel(), 2048-item chunks) then deduplicates.
        static Relation from_slice(std::vector<Tuple> data, const ExecutionContext &ctx = {}) {
            if (data.empty())
                return empty_relation();

            if (ctx.has_parallel() && data.size() > 2048) {
                auto *pool = ctx.pool();
                const std::size_t chunk = 2048;
                const std::size_t n = data.size();
                const std::size_t nchunks = (n + chunk - 1) / chunk;

                pool->bulk(
                    [&](std::size_t i) {
                        const std::size_t lo = i * chunk;
                        const std::size_t hi = std::min(lo + chunk, n);
                        std::sort(data.begin() + lo, data.begin() + hi);
                    },
                    nchunks);

                // k-way merge the sorted chunks
                std::vector<Tuple> merged;
                merged.reserve(n);
                // Iterative pair-wise merge (simple; good enough for moderate k)
                std::vector<std::size_t> starts(nchunks), ends(nchunks);
                for (std::size_t i = 0; i < nchunks; ++i) {
                    starts[i] = i * chunk;
                    ends[i] = std::min(starts[i] + chunk, n);
                }
                // Use std::merge repeatedly (tournament merge would be O(N log k)
                // but this is simpler and data is already chunked)
                std::vector<Tuple> tmp;
                tmp.reserve(n);
                std::vector<Tuple> *src = &data;
                // Pair-wise reduction
                std::vector<Tuple> buf;
                buf.reserve(n);
                std::size_t lo = 0;
                while (lo + chunk < n) {
                    std::size_t mid = lo + chunk;
                    std::size_t hi2 = std::min(mid + chunk, n);
                    std::merge(src->begin() + lo, src->begin() + mid, src->begin() + mid, src->begin() + hi2,
                               std::back_inserter(buf));
                    lo = hi2;
                }
                // Append any remainder not merged above
                for (std::size_t i = lo; i < n; ++i)
                    buf.push_back((*src)[i]);
                data = std::move(buf);
            } else {
                std::sort(data.begin(), data.end());
            }

            deduplicate(data);
            return Relation(std::make_shared<std::vector<Tuple>>(std::move(data)));
        }

        // Two-pointer merge of two already-sorted Relations with deduplication.
        static Relation merge(const Relation &a, const Relation &b, const ExecutionContext & /*ctx*/ = {}) {
            if (a.empty())
                return b;
            if (b.empty())
                return a;

            auto ae = a.elements();
            auto be = b.elements();
            std::vector<Tuple> result;
            result.reserve(ae.size() + be.size());

            std::size_t i = 0, j = 0;
            while (i < ae.size() && j < be.size()) {
                if (ae[i] < be[j]) {
                    result.push_back(ae[i++]);
                } else if (be[j] < ae[i]) {
                    result.push_back(be[j++]);
                } else {
                    // Equal: keep one, advance both
                    result.push_back(ae[i++]);
                    ++j;
                }
            }
            while (i < ae.size())
                result.push_back(ae[i++]);
            while (j < be.size())
                result.push_back(be[j++]);

            return Relation(std::make_shared<std::vector<Tuple>>(std::move(result)));
        }

        // ---- Serialization --------------------------------------------------

        // Binary format: magic "STLREL\0\0" (8 bytes) + uint64 count + raw Tuple array.
        void save(std::vector<std::byte> &out) const {
            const char magic[8] = {'S', 'T', 'L', 'R', 'E', 'L', '\0', '\0'};
            const auto prev = out.size();
            out.resize(prev + 8 + 8 + size() * sizeof(Tuple));
            std::memcpy(out.data() + prev, magic, 8);
            uint64_t n = static_cast<uint64_t>(size());
            std::memcpy(out.data() + prev + 8, &n, 8);
            if (!empty())
                std::memcpy(out.data() + prev + 16, elements().data(), size() * sizeof(Tuple));
        }

        static Relation load(std::span<const std::byte> in) {
            if (in.size() < 16)
                throw std::runtime_error("Relation::load: buffer too small");
            const char expected[8] = {'S', 'T', 'L', 'R', 'E', 'L', '\0', '\0'};
            if (std::memcmp(in.data(), expected, 8) != 0)
                throw std::runtime_error("Relation::load: bad magic");
            uint64_t n;
            std::memcpy(&n, in.data() + 8, 8);
            if (in.size() < 16 + n * sizeof(Tuple))
                throw std::runtime_error("Relation::load: truncated data");
            std::vector<Tuple> data(n);
            if (n > 0)
                std::memcpy(data.data(), in.data() + 16, n * sizeof(Tuple));
            return Relation(std::make_shared<std::vector<Tuple>>(std::move(data)));
        }

        // ---- Element access -------------------------------------------------

        std::span<const Tuple> elements() const {
            if (!data_)
                return {};
            return std::span<const Tuple>(*data_);
        }

        std::size_t size() const { return data_ ? data_->size() : 0; }

        bool empty() const { return size() == 0; }

        const_iterator begin() const { return data_ ? data_->data() : nullptr; }

        const_iterator end() const { return data_ ? data_->data() + data_->size() : nullptr; }

        // ---- Singleton empty ------------------------------------------------

        static const Relation &empty_relation() {
            static const Relation kEmpty{};
            return kEmpty;
        }

        // ---- Default ctor (empty) -------------------------------------------
        Relation() = default;

      private:
        explicit Relation(std::shared_ptr<std::vector<Tuple>> data) : data_(std::move(data)) {}

        // Remove consecutive duplicates in a sorted vector (in-place).
        static void deduplicate(std::vector<Tuple> &v) {
            if (v.empty())
                return;
            std::size_t write = 0;
            for (std::size_t read = 1; read < v.size(); ++read) {
                if (!(v[read] == v[write]))
                    v[++write] = v[read];
            }
            v.resize(write + 1);
        }

        std::shared_ptr<const std::vector<Tuple>> data_;
    };

} // namespace stateup::logic
