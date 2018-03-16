/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * IFDSTabulationProblem.h
 *
 *  Created on: 04.08.2016
 *      Author: pdschbrt
 */

#ifndef ANALYSIS_IFDS_IDE_IFDSTABULATIONPROBLEM_H_
#define ANALYSIS_IFDS_IDE_IFDSTABULATIONPROBLEM_H_

#include "../control_flow/ICFG.h"
#include "FlowFunctions.h"
#include "SolverConfiguration.h"
#include <map>
#include <set>
#include <string>
#include <type_traits>

using namespace std;

template <typename N, typename D, typename M, typename I>
class IFDSTabulationProblem : public FlowFunctions<N, D, M> {
public:
  SolverConfiguration solver_config;
  virtual ~IFDSTabulationProblem() = default;
  virtual I interproceduralCFG() = 0;
  virtual map<N, set<D>> initialSeeds() = 0;
  virtual D zeroValue() = 0;
  virtual bool isZeroValue(D d) const = 0;
  virtual string DtoString(D d) = 0;
  virtual string NtoString(N n) = 0;
  virtual string MtoString(M m) = 0;
  void setSolverConfiguration(SolverConfiguration conf) {
    solver_config = conf;
  }
  SolverConfiguration getSolverConfiguration() { return solver_config; }
};

#endif /* ANALYSIS_IFDS_IDE_IFDSTABULATIONPROBLEM_HH_ */
