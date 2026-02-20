#pragma once

// stateup::logic — Datalog / deductive reasoning engine
// C++20 port of Zodd (https://github.com/joincivil/zodd)
//
// Semi-naive evaluation over immutable sorted relations.
// Provides: fixpoint iteration, binary joins, leapfrog trie joins,
//           group-by aggregation, and secondary indexes.
//
// Namespace: stateup::logic  (alias: su::logic)

#include "stateup/logic/aggregate.hpp"
#include "stateup/logic/context.hpp"
#include "stateup/logic/extend.hpp"
#include "stateup/logic/index.hpp"
#include "stateup/logic/iteration.hpp"
#include "stateup/logic/join.hpp"
#include "stateup/logic/relation.hpp"
#include "stateup/logic/variable.hpp"
