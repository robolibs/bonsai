#include <stateup/stateup.hpp>
#include <doctest/doctest.h>
#include <algorithm>
#include <string>
#include <vector>

using namespace stateup::logic;

// ---------------------------------------------------------------------------
// Shared tuple types used across tests
// ---------------------------------------------------------------------------

struct Edge {
    int from, to;
    bool operator<(const Edge& o) const {
        return from < o.from || (from == o.from && to < o.to);
    }
    bool operator==(const Edge& o) const { return from == o.from && to == o.to; }
};

struct Triple {
    int a, b, c;
    bool operator<(const Triple& o) const {
        if (a != o.a) return a < o.a;
        if (b != o.b) return b < o.b;
        return c < o.c;
    }
    bool operator==(const Triple& o) const { return a == o.a && b == o.b && c == o.c; }
};

// ---------------------------------------------------------------------------
// TEST: Relation
// ---------------------------------------------------------------------------

TEST_CASE("Relation basic operations") {
    SUBCASE("empty relation") {
        auto r = Relation<Edge>::empty_relation();
        CHECK(r.empty());
        CHECK(r.size() == 0);
    }

    SUBCASE("from_slice sorts and deduplicates") {
        std::vector<Edge> data = {{3,4},{1,2},{2,3},{1,2},{3,4}};
        auto r = Relation<Edge>::from_slice(data);
        CHECK(r.size() == 3);
        auto elems = r.elements();
        CHECK(elems[0] == Edge{1,2});
        CHECK(elems[1] == Edge{2,3});
        CHECK(elems[2] == Edge{3,4});
    }

    SUBCASE("from_slice single element") {
        auto r = Relation<Edge>::from_slice({{5,6}});
        CHECK(r.size() == 1);
        CHECK(r.elements()[0] == Edge{5,6});
    }

    SUBCASE("merge two relations deduplicates") {
        auto a = Relation<Edge>::from_slice({{1,2},{2,3}});
        auto b = Relation<Edge>::from_slice({{2,3},{3,4}});
        auto m = Relation<Edge>::merge(a, b);
        CHECK(m.size() == 3);
        auto elems = m.elements();
        CHECK(elems[0] == Edge{1,2});
        CHECK(elems[1] == Edge{2,3});
        CHECK(elems[2] == Edge{3,4});
    }

    SUBCASE("merge with empty") {
        auto a = Relation<Edge>::from_slice({{1,2}});
        auto m = Relation<Edge>::merge(a, Relation<Edge>::empty_relation());
        CHECK(m.size() == 1);
    }

    SUBCASE("save and load round-trip") {
        auto r = Relation<Edge>::from_slice({{1,2},{3,4},{5,6}});
        std::vector<std::byte> buf;
        r.save(buf);
        auto r2 = Relation<Edge>::load(std::span<const std::byte>(buf));
        REQUIRE(r2.size() == 3);
        CHECK(r2.elements()[0] == Edge{1,2});
        CHECK(r2.elements()[2] == Edge{5,6});
    }

    SUBCASE("iteration with begin/end") {
        auto r = Relation<Edge>::from_slice({{1,2},{2,3}});
        int count = 0;
        for (const auto& e : r.elements()) { (void)e; ++count; }
        CHECK(count == 2);
    }
}

// ---------------------------------------------------------------------------
// TEST: Variable
// ---------------------------------------------------------------------------

TEST_CASE("Variable semi-naive step") {
    SUBCASE("insert and changed produces recent") {
        Variable<Edge> v;
        v.insert({1, 2});
        v.insert({2, 3});
        CHECK(v.changed());
        CHECK(v.recent().size() == 2);
        CHECK(v.stable().size() == 2);
    }

    SUBCASE("duplicate facts not re-added") {
        Variable<Edge> v;
        v.insert({1, 2});
        CHECK(v.changed()); // first step: {1,2} is new

        v.insert({1, 2}); // already in stable
        CHECK_FALSE(v.changed()); // nothing new
        CHECK(v.recent().empty());
    }

    SUBCASE("changed returns false when no to_add") {
        Variable<Edge> v;
        CHECK_FALSE(v.changed());
    }

    SUBCASE("complete merges everything") {
        Variable<Edge> v;
        v.insert({1, 2});
        v.changed();
        v.insert({3, 4});
        v.changed();
        auto result = v.complete();
        CHECK(result.size() == 2);
    }

    SUBCASE("reset clears all state") {
        Variable<Edge> v;
        v.insert({1, 2});
        v.changed();
        v.reset();
        CHECK(v.recent().empty());
        CHECK(v.stable().empty());
        CHECK(v.total_size() == 0);
    }

    SUBCASE("insert_slice") {
        Variable<Edge> v;
        std::vector<Edge> edges = {{1,2},{2,3},{3,4}};
        v.insert_slice(std::span<const Edge>(edges));
        CHECK(v.changed());
        CHECK(v.recent().size() == 3);
    }
}

// ---------------------------------------------------------------------------
// TEST: Iteration (fixpoint)
// ---------------------------------------------------------------------------

TEST_CASE("Iteration fixpoint") {
    SUBCASE("counter increments") {
        Iteration<Edge> iter;
        auto* v = iter.variable();
        v->insert({1, 2});
        CHECK(iter.current_iteration() == 0);
        CHECK(iter.changed());
        CHECK(iter.current_iteration() == 1);
    }

    SUBCASE("max_iterations guard") {
        Iteration<Edge> iter({}, 2);
        auto* v = iter.variable();
        v->insert({1, 2});
        iter.changed(); // iter 1
        v->insert({2, 3});
        iter.changed(); // iter 2
        v->insert({3, 4});
        CHECK_FALSE(iter.changed()); // blocked by max
    }

    SUBCASE("reset restores iteration count") {
        Iteration<Edge> iter;
        auto* v = iter.variable();
        v->insert({1, 2});
        iter.changed();
        iter.reset();
        CHECK(iter.current_iteration() == 0);
    }
}

// ---------------------------------------------------------------------------
// TEST: Transitive closure via join_into
// ---------------------------------------------------------------------------

TEST_CASE("Transitive closure with join_into") {
    // Graph: 1->2, 2->3, 3->4
    // Reachable: 1->2, 1->3, 1->4, 2->3, 2->4, 3->4

    Iteration<Edge> iter;
    auto* edges    = iter.variable();
    auto* reachable = iter.variable();

    std::vector<Edge> base = {{1,2},{2,3},{3,4}};
    edges->insert_slice(std::span<const Edge>(base));
    reachable->insert_slice(std::span<const Edge>(base));

    // First iter.changed() inside the loop processes the seed inserts (recent_=base).
    // join_into then derives new pairs using recent(reachable) × stable(edges).
    while (iter.changed()) {
        join_into<Edge, Edge, Edge, int>(
            *reachable, *edges, reachable,
            [](const Edge& e) { return e.to; },   // key from reachable: .to
            [](const Edge& e) { return e.from; },  // key from edges:     .from
            [](const Edge& r, const Edge& e) -> Edge { return {r.from, e.to}; }
        );
    }

    auto result = reachable->complete();
    CHECK(result.size() == 6);

    // Check specific reachability
    auto elems = result.elements();
    auto has = [&](int f, int t) {
        return std::ranges::any_of(elems, [&](const Edge& e){
            return e.from == f && e.to == t;
        });
    };
    CHECK(has(1, 2));
    CHECK(has(1, 3));
    CHECK(has(1, 4));
    CHECK(has(2, 3));
    CHECK(has(2, 4));
    CHECK(has(3, 4));
}

// ---------------------------------------------------------------------------
// TEST: join_anti
// ---------------------------------------------------------------------------

TEST_CASE("join_anti excludes matched tuples") {
    // left = {1->2, 2->3, 3->4}, right = {2->3}
    // anti-join on .from: keep left tuples whose .from has no match in right
    // right has .from=2, so {2->3} is excluded; {1->2} and {3->4} pass

    Variable<Edge> left, right, output;
    std::vector<Edge> ldata = {{1,2},{2,3},{3,4}};
    std::vector<Edge> rdata = {{2,3}};
    left.insert_slice(std::span<const Edge>(ldata));
    right.insert_slice(std::span<const Edge>(rdata));
    left.changed();
    right.changed();

    join_anti<Edge, Edge, int>(
        left, right, &output,
        [](const Edge& e) { return e.from; },
        [](const Edge& e) { return e.from; }
    );
    output.changed();

    auto result = output.complete();
    CHECK(result.size() == 2);
    auto elems = result.elements();
    auto has = [&](int f, int t) {
        return std::ranges::any_of(elems, [&](const Edge& e){
            return e.from == f && e.to == t;
        });
    };
    CHECK(has(1, 2));
    CHECK(has(3, 4));
    CHECK_FALSE(has(2, 3));
}

// ---------------------------------------------------------------------------
// TEST: gallop_lower_bound
// ---------------------------------------------------------------------------

TEST_CASE("gallop_lower_bound") {
    std::vector<int> v = {1, 3, 5, 7, 9, 11, 13};

    SUBCASE("finds exact value") {
        auto it = gallop_lower_bound(v.begin(), v.end(), 7);
        REQUIRE(it != v.end());
        CHECK(*it == 7);
    }

    SUBCASE("finds insertion point for missing value") {
        auto it = gallop_lower_bound(v.begin(), v.end(), 6);
        REQUIRE(it != v.end());
        CHECK(*it == 7);
    }

    SUBCASE("returns begin for value smaller than all") {
        auto it = gallop_lower_bound(v.begin(), v.end(), 0);
        CHECK(it == v.begin());
    }

    SUBCASE("returns end for value larger than all") {
        auto it = gallop_lower_bound(v.begin(), v.end(), 100);
        CHECK(it == v.end());
    }

    SUBCASE("matches std::lower_bound on random positions") {
        for (int target : {1, 2, 5, 8, 13, 14}) {
            auto g = gallop_lower_bound(v.begin(), v.end(), target);
            auto s = std::lower_bound(v.begin(), v.end(), target);
            CHECK(g == s);
        }
    }
}

// ---------------------------------------------------------------------------
// TEST: aggregate
// ---------------------------------------------------------------------------

TEST_CASE("aggregate group-by fold") {
    struct Record {
        int dept;
        int salary;
        bool operator<(const Record& o) const {
            return dept < o.dept || (dept == o.dept && salary < o.salary);
        }
        bool operator==(const Record& o) const {
            return dept == o.dept && salary == o.salary;
        }
    };

    std::vector<Record> data = {
        {1, 100}, {1, 200}, {2, 300}, {2, 400}, {2, 500}, {3, 150}
    };

    auto result = aggregate<Record, int, int>(
        std::span<const Record>(data),
        [](const Record& r) { return r.dept; },
        [](const Record& r) { return r.salary; },
        [](int a, int b) { return a + b; },
        0
    );

    REQUIRE(result.size() == 3);
    CHECK(result[0].first == 1);  CHECK(result[0].second == 300);
    CHECK(result[1].first == 2);  CHECK(result[1].second == 1200);
    CHECK(result[2].first == 3);  CHECK(result[2].second == 150);
}

// ---------------------------------------------------------------------------
// TEST: SecondaryIndex
// ---------------------------------------------------------------------------

TEST_CASE("SecondaryIndex point and range lookup") {
    struct KV {
        int key, val;
        bool operator<(const KV& o) const {
            return key < o.key || (key == o.key && val < o.val);
        }
        bool operator==(const KV& o) const { return key == o.key && val == o.val; }
    };

    auto r = Relation<KV>::from_slice({{1,10},{1,20},{2,30},{3,40},{3,50},{3,60}});
    auto key_ext = [](const KV& kv) { return kv.key; };
    SecondaryIndex idx(r, key_ext);

    SUBCASE("point lookup existing key") {
        auto span = idx.get(1);
        CHECK(span.size() == 2);
        CHECK(span[0].val == 10);
        CHECK(span[1].val == 20);
    }

    SUBCASE("point lookup missing key") {
        auto span = idx.get(99);
        CHECK(span.empty());
    }

    SUBCASE("range query") {
        auto spans = idx.get_range(1, 2);
        CHECK(spans.size() == 2); // keys 1 and 2
    }

    SUBCASE("range query out of bounds") {
        auto spans = idx.get_range(10, 20);
        CHECK(spans.empty());
    }

    SUBCASE("size") {
        CHECK(idx.size() == 6);
        CHECK(idx.num_keys() == 3);
    }

    SUBCASE("insert new tuple") {
        idx.insert({4, 70});
        auto span = idx.get(4);
        CHECK(span.size() == 1);
        CHECK(span[0].val == 70);
    }
}

// ---------------------------------------------------------------------------
// TEST: find_key_range
// ---------------------------------------------------------------------------

TEST_CASE("find_key_range") {
    struct KV {
        int k, v;
        bool operator<(const KV& o) const {
            return k < o.k || (k == o.k && v < o.v);
        }
        bool operator==(const KV& o) const { return k == o.k && v == o.v; }
    };

    auto r = Relation<KV>::from_slice({{1,10},{1,20},{2,30},{3,40}});
    auto elems = r.elements();

    SUBCASE("finds matching range") {
        auto [f, l] = find_key_range(elems, 1, [](const KV& kv){ return kv.k; });
        CHECK(l - f == 2);
        CHECK(f->v == 10);
    }

    SUBCASE("empty range for missing key") {
        auto [f, l] = find_key_range(elems, 99, [](const KV& kv){ return kv.k; });
        CHECK(f == l);
    }
}

// ---------------------------------------------------------------------------
// TEST: ExtendWith leapfrog join
// ---------------------------------------------------------------------------

TEST_CASE("ExtendWith leapfrog extend_into") {
    // source: pairs (key, val) — edges indexed by source node
    struct KV {
        int k, v;
        bool operator<(const KV& o) const {
            return k < o.k || (k == o.k && v < o.v);
        }
        bool operator==(const KV& o) const { return k == o.k && v == o.v; }
    };

    // prefix tuples: single int (the "from" node we want to extend)
    struct Node {
        int id;
        bool operator<(const Node& o) const { return id < o.id; }
        bool operator==(const Node& o) const { return id == o.id; }
    };

    // Edges: 1->2, 1->3, 2->4
    auto edges = Relation<KV>::from_slice({{1,2},{1,3},{2,4}});

    // Query: for each node in {1, 2}, find reachable nodes
    auto src = Relation<Node>::from_slice({{1},{2}});
    Variable<Node> output;

    auto leaper = make_extend_with<Node>(edges,
        [](const Node& n) { return n.id; },
        [](const KV& kv)  { return kv.k; },
        [](const KV& kv)  { return kv.v; });

    std::vector<Leaper<Node, int>*> leapers = {&leaper};

    extend_into(src, std::span<Leaper<Node,int>* const>(leapers), &output,
                [](const Node& /*n*/, int val) -> Node { return {val}; });
    output.changed();

    auto result = output.complete();
    // From node 1: reach 2, 3. From node 2: reach 4. Total = 3 unique nodes.
    CHECK(result.size() == 3);
    auto elems = result.elements();
    auto has = [&](int id) {
        return std::ranges::any_of(elems, [&](const Node& n){ return n.id == id; });
    };
    CHECK(has(2));
    CHECK(has(3));
    CHECK(has(4));
}
