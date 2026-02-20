#include <stateup/stateup.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

using namespace stateup::logic;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void section(const char* title) {
    std::cout << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
    std::cout << "  " << title << "\n";
    std::cout << "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━\n";
}

// ---------------------------------------------------------------------------
// DEMO 1: Graph reachability (transitive closure)
// ---------------------------------------------------------------------------
// Computes all (from, to) pairs reachable via any number of hops.
// Classic Datalog rule:  reachable(A,C) :- reachable(A,B), edge(B,C).

void demo_reachability() {
    section("DEMO 1: Graph Reachability (Transitive Closure)");

    // Directed graph:
    //   1 --> 2 --> 3 --> 5
    //         |
    //         v
    //         4 --> 6

    struct Edge {
        int from, to;
        bool operator<(const Edge& o) const {
            return from < o.from || (from == o.from && to < o.to);
        }
        bool operator==(const Edge& o) const { return from == o.from && to == o.to; }
    };

    Iteration<Edge> iter;
    auto* edges     = iter.variable();
    auto* reachable = iter.variable();

    std::vector<Edge> base = {{1,2},{2,3},{2,4},{3,5},{4,6}};
    edges->insert_slice(std::span<const Edge>(base));
    reachable->insert_slice(std::span<const Edge>(base));

    std::cout << "  Base edges:  ";
    for (auto& e : base) std::cout << e.from << "->" << e.to << "  ";
    std::cout << "\n";

    // Fixpoint: first iter.changed() processes the seed, then join_into
    // derives new pairs using recent(reachable) × stable(edges) each round.
    while (iter.changed()) {
        join_into<Edge, Edge, Edge, int>(
            *reachable, *edges, reachable,
            [](const Edge& e) { return e.to; },
            [](const Edge& e) { return e.from; },
            [](const Edge& r, const Edge& e) -> Edge { return {r.from, e.to}; }
        );
    }

    auto result = reachable->complete();

    std::cout << "  Iterations:  " << iter.current_iteration() << "\n";
    std::cout << "  Reachable pairs (" << result.size() << "):\n";
    for (const auto& e : result.elements())
        std::cout << "    " << e.from << " --> " << e.to << "\n";
}

// ---------------------------------------------------------------------------
// DEMO 2: RBAC — Role-based access control
// ---------------------------------------------------------------------------
// Derives effective permissions from role hierarchies and direct assignments.
// Rules:
//   has_perm(U,P) :- has_role(U,R), role_perm(R,P).
//   has_role(U,R) :- has_role(U,R2), role_inherits(R2,R).

void demo_rbac() {
    section("DEMO 2: RBAC — Role Hierarchy & Permission Inference");

    // Use integer IDs for users, roles, permissions
    // Users:  1=alice, 2=bob, 3=charlie
    // Roles:  10=viewer, 20=editor, 30=admin
    // Perms:  100=read, 200=write, 300=delete, 400=manage

    struct Pair {
        int a, b;
        bool operator<(const Pair& o) const {
            return a < o.a || (a == o.a && b < o.b);
        }
        bool operator==(const Pair& o) const { return a == o.a && b == o.b; }
    };

    // Direct role assignments: user -> role
    auto user_role = Relation<Pair>::from_slice({
        {1, 20},  // alice   -> editor
        {2, 10},  // bob     -> viewer
        {3, 30},  // charlie -> admin
    });

    // Role inheritance: child_role -> parent_role
    auto role_inherits = Relation<Pair>::from_slice({
        {20, 10},  // editor  inherits viewer
        {30, 20},  // admin   inherits editor (and transitively viewer)
    });

    // Role -> permission assignments
    auto role_perm = Relation<Pair>::from_slice({
        {10, 100},  // viewer: read
        {20, 200},  // editor: write
        {30, 300},  // admin:  delete
        {30, 400},  // admin:  manage
    });

    // Step 1: derive all effective (user, role) pairs via role inheritance.
    // Rule: has_role(U,R) :- has_role(U,R2), role_inherits(R2,R).
    Iteration<Pair> role_iter;
    auto* eff_roles  = role_iter.variable();  // (user, role) — grows each round
    auto* inherit_var = role_iter.variable(); // (child, parent) — seeded once

    eff_roles->insert_relation(user_role);
    inherit_var->insert_relation(role_inherits);

    while (role_iter.changed()) {
        // join: eff_roles.b (current role) == inherit_var.a (child role)
        // emit (user, parent_role)
        join_into<Pair, Pair, Pair, int>(
            *eff_roles, *inherit_var, eff_roles,
            [](const Pair& p) { return p.b; },  // key: role in eff_roles
            [](const Pair& p) { return p.a; },  // key: child role in inherits
            [](const Pair& ur, const Pair& ri) -> Pair { return {ur.a, ri.b}; }
        );
    }

    auto all_roles = eff_roles->complete();

    // Step 2: join effective roles with role_perm to get (user, permission).
    Iteration<Pair> perm_iter;
    auto* roles_var = perm_iter.variable();  // (user, role)
    auto* rperm_var = perm_iter.variable();  // (role, perm)
    auto* out_var   = perm_iter.variable();  // (user, perm)

    roles_var->insert_relation(all_roles);
    rperm_var->insert_relation(role_perm);

    while (perm_iter.changed()) {
        // join: roles_var.b (role) == rperm_var.a (role) → emit (user, perm)
        join_into<Pair, Pair, Pair, int>(
            *roles_var, *rperm_var, out_var,
            [](const Pair& p) { return p.b; },  // role from user->role
            [](const Pair& p) { return p.a; },  // role from role->perm
            [](const Pair& ur, const Pair& rp) -> Pair { return {ur.a, rp.b}; }
        );
    }

    auto all_perms = out_var->complete();

    // Only keep user->permission pairs
    std::vector<Pair> user_perms;
    for (const auto& p : all_perms.elements())
        if (p.a >= 1 && p.a <= 3)
            user_perms.push_back(p);
    std::sort(user_perms.begin(), user_perms.end());

    auto user_name = [](int u) -> std::string {
        switch(u) { case 1: return "alice"; case 2: return "bob"; case 3: return "charlie"; }
        return "?";
    };
    auto perm_name = [](int p) -> std::string {
        switch(p) { case 100: return "read"; case 200: return "write";
                    case 300: return "delete"; case 400: return "manage"; }
        return "?";
    };

    std::cout << "  Effective permissions:\n";
    for (const auto& p : user_perms)
        std::cout << "    " << user_name(p.a) << " can " << perm_name(p.b) << "\n";
}

// ---------------------------------------------------------------------------
// DEMO 3: Package dependency resolution
// ---------------------------------------------------------------------------
// Finds all transitive dependencies of a package.
// Rule:  transitive_dep(A,C) :- transitive_dep(A,B), direct_dep(B,C).

void demo_dependencies() {
    section("DEMO 3: Package Dependency Resolution");

    struct Dep {
        int pkg, dep;
        bool operator<(const Dep& o) const {
            return pkg < o.pkg || (pkg == o.pkg && dep < o.dep);
        }
        bool operator==(const Dep& o) const { return pkg == o.pkg && dep == o.dep; }
    };

    // Packages: 1=app, 2=web-framework, 3=http-client, 4=json, 5=ssl, 6=crypto
    std::vector<Dep> direct = {
        {1, 2},  // app          -> web-framework
        {1, 3},  // app          -> http-client
        {2, 3},  // web-framework-> http-client
        {2, 4},  // web-framework-> json
        {3, 5},  // http-client  -> ssl
        {5, 6},  // ssl          -> crypto
    };

    auto pkg_name = [](int p) -> std::string {
        switch(p) {
            case 1: return "app";
            case 2: return "web-framework";
            case 3: return "http-client";
            case 4: return "json";
            case 5: return "ssl";
            case 6: return "crypto";
        }
        return "?";
    };

    Iteration<Dep> iter;
    auto* deps      = iter.variable();
    auto* transitive = iter.variable();

    deps->insert_slice(std::span<const Dep>(direct));
    transitive->insert_slice(std::span<const Dep>(direct));

    while (iter.changed()) {
        join_into<Dep, Dep, Dep, int>(
            *transitive, *deps, transitive,
            [](const Dep& d) { return d.dep; },
            [](const Dep& d) { return d.pkg; },
            [](const Dep& t, const Dep& d) -> Dep { return {t.pkg, d.dep}; }
        );
    }

    // Show all dependencies of "app" (pkg=1)
    auto result = transitive->complete();
    std::cout << "  Direct dependencies (input):\n";
    for (const auto& d : direct)
        std::cout << "    " << pkg_name(d.pkg) << " -> " << pkg_name(d.dep) << "\n";

    std::cout << "  All transitive deps of 'app':\n";
    for (const auto& d : result.elements())
        if (d.pkg == 1)
            std::cout << "    app -> " << pkg_name(d.dep) << "\n";
}

// ---------------------------------------------------------------------------
// DEMO 4: Aggregate — total cost per category
// ---------------------------------------------------------------------------

void demo_aggregate() {
    section("DEMO 4: Aggregate — Total Cost Per Category");

    struct Item {
        int category, cost;
        bool operator<(const Item& o) const {
            return category < o.category || (category == o.category && cost < o.cost);
        }
        bool operator==(const Item& o) const {
            return category == o.category && cost == o.cost;
        }
    };

    std::vector<Item> items = {
        {1, 50}, {1, 120}, {1, 30},   // compute: 200
        {2, 80}, {2, 90},              // storage: 170
        {3, 15}, {3, 25}, {3, 60},    // network: 100
    };

    auto totals = aggregate<Item, int, int>(
        std::span<const Item>(items),
        [](const Item& i) { return i.category; },
        [](const Item& i) { return i.cost; },
        [](int a, int b) { return a + b; },
        0
    );

    auto cat_name = [](int c) -> std::string {
        switch(c) { case 1: return "compute"; case 2: return "storage"; case 3: return "network"; }
        return "?";
    };

    std::cout << "  Cost totals:\n";
    for (const auto& [cat, total] : totals)
        std::cout << "    " << cat_name(cat) << ": $" << total << "\n";
}

// ---------------------------------------------------------------------------
// DEMO 5: SecondaryIndex — fast lookup
// ---------------------------------------------------------------------------

void demo_index() {
    section("DEMO 5: SecondaryIndex — Point & Range Lookup");

    struct Log {
        int node, event_id;
        bool operator<(const Log& o) const {
            return node < o.node || (node == o.node && event_id < o.event_id);
        }
        bool operator==(const Log& o) const {
            return node == o.node && event_id == o.event_id;
        }
    };

    auto logs = Relation<Log>::from_slice({
        {1, 101}, {1, 102}, {1, 103},
        {2, 201}, {2, 202},
        {3, 301},
        {4, 401}, {4, 402}, {4, 403}, {4, 404},
    });

    auto log_key = [](const Log& l) { return l.node; };
    SecondaryIndex idx(logs, log_key);

    // Point lookup
    std::cout << "  Events on node 1: ";
    for (const auto& l : idx.get(1)) std::cout << l.event_id << " ";
    std::cout << "\n";

    std::cout << "  Events on node 4: ";
    for (const auto& l : idx.get(4)) std::cout << l.event_id << " ";
    std::cout << "\n";

    // Range query
    std::cout << "  Events on nodes 2-3:\n";
    for (const auto& span : idx.get_range(2, 3))
        for (const auto& l : span)
            std::cout << "    node=" << l.node << " event=" << l.event_id << "\n";

    std::cout << "  Total indexed: " << idx.size() << " entries, "
              << idx.num_keys() << " nodes\n";
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════╗\n";
    std::cout << "║     stateup::logic  —  Datalog Engine Demo          ║\n";
    std::cout << "║     Semi-naive evaluation · C++20 port of Zodd      ║\n";
    std::cout << "╚══════════════════════════════════════════════════════╝\n";

    demo_reachability();
    demo_rbac();
    demo_dependencies();
    demo_aggregate();
    demo_index();

    std::cout << "\n  Done.\n\n";
    return 0;
}
