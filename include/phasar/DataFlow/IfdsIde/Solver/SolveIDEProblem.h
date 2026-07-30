/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * SolveIDEProblem.h
 *
 *  Created on: 29.07.2026
 *      Author: mxHuber
 */

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_SOLVE_IDE_PROBLEM_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_SOLVE_IDE_PROBLEM_H

#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"
#include "phasar/DataFlow/IfdsIde/Solver/ParallelizedIDESolver.h"

namespace psr {

template <typename AnalysisDomainTy, typename Container>
static OwningSolverResults<typename AnalysisDomainTy::n_t,
                           typename AnalysisDomainTy::d_t,
                           typename AnalysisDomainTy::l_t>
solveIDEProblem(IDETabulationProblem<AnalysisDomainTy, Container> &Problem,
                const ICFG auto &ICF, size_t NumOfThreads = 1) {
  // IDESolver is faster than the ParallelizedIDESolver with one thread
  if (NumOfThreads == 1) {
    IDESolver Solver(&Problem, &ICF);
    SimpleTimer SolveTimer = SimpleTimer();
    Solver.solve();
    llvm::outs() << "\n\n\nIDESolver solve() time: " << SolveTimer.elapsed()
                 << "\n\n\n";
    return Solver.consumeSolverResults();
  }

  ParallelizedIDESolver<AnalysisDomainTy, Container> Solver(&Problem, &ICF,
                                                            NumOfThreads);
  SimpleTimer SolveTimer = SimpleTimer();
  Solver.solve();
  llvm::outs() << "\n\n\nParallelizedIDESolver solve() time: "
               << SolveTimer.elapsed() << "\n\n\n";

  return Solver.consumeSolverResults();
}

} // namespace psr

#endif
