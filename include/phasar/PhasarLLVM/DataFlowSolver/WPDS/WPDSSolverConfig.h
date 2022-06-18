/******************************************************************************
 * Copyright (c) 2019 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_DATAFLOWSOLVER_WPDS_WPDSSOLVERCONFIG_H
#define PHASAR_PHASARLLVM_DATAFLOWSOLVER_WPDS_WPDSSOLVERCONFIG_H

#include "phasar/PhasarLLVM/DataFlowSolver/WPDS/WPDSOptions.h"

namespace llvm {
class raw_ostream;
}

namespace psr {

struct WPDSSolverConfig {
  WPDSSolverConfig() = default;
  WPDSSolverConfig(bool RecordWitnesses, WPDSSearchDirection Direction,
                   WPDSType WPDSTy);
  ~WPDSSolverConfig() = default;
  WPDSSolverConfig(const WPDSSolverConfig &) = default;
  WPDSSolverConfig &operator=(const WPDSSolverConfig &) = default;
  WPDSSolverConfig(WPDSSolverConfig &&) = default;
  WPDSSolverConfig &operator=(WPDSSolverConfig &&) = default;
  bool RecordWitnesses = false;
  WPDSSearchDirection Direction = WPDSSearchDirection::FORWARD;
  WPDSType WPDSType = WPDSType::FWPDS;
  friend llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                                       const WPDSSolverConfig &SC);
};

} // namespace psr

#endif
