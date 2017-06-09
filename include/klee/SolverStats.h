//===-- SolverStats.h -------------------------------------------*- C++ -*-===//
//
//                     The KLEE Symbolic Virtual Machine
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//

#ifndef KLEE_SOLVERSTATS_H
#define KLEE_SOLVERSTATS_H

#include "klee/Statistic.h"

namespace klee {
namespace stats {

  extern Statistic cexCacheTime;
  extern Statistic queries;
  extern Statistic queriesInvalid;
  extern Statistic queriesValid;
  extern Statistic queryCacheHits;
  extern Statistic queryCacheMisses;
  extern Statistic queryCexCacheHits;
  extern Statistic queryCexCacheMisses;
  extern Statistic queryConstructTime;
  extern Statistic queryConstructs;
  extern Statistic queryCounterexamples;
  extern Statistic queryTime;

#ifdef DEBUG
  extern Statistic arrayHashTime;
#endif

  extern Statistic allocations;
  extern Statistic coveredInstructions;
  extern Statistic falseBranches;
  extern Statistic forkTime;
  extern Statistic forks;
  extern Statistic instructionRealTime;
  extern Statistic instructionTime;
  extern Statistic instructions;
  extern Statistic minDistToReturn;
  extern Statistic minDistToUncovered;
  extern Statistic reachableUncovered;
  extern Statistic resolveTime;
  extern Statistic solverTime;
  extern Statistic states;
  extern Statistic trueBranches;
  extern Statistic uncoveredInstructions;
}
}

#endif
