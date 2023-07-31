/******************************************************************************
 * Copyright (c) 2023 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Maximilian Leo Huber and others
 *****************************************************************************/

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERDATA_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERDATA_H

#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"

#include "nlohmann/json.hpp"

namespace psr {
class IDESolverData {
public:
  [[nodiscard]] IDESolverData(const psr::LLVMProjectIRDB &IRDB,
                              const nlohmann::json &Config) noexcept;

  inline void addJumpFunctions(const std::vector<std::string> &Functions) {
    JumpFunctions = Functions;
  }

  [[nodiscard]] inline const std::vector<std::string> &
  getAllJumpFunctions() const {
    return JumpFunctions;
  }

private:
  std::vector<std::string> JumpFunctions;
};

} // namespace psr

#endif // PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERDATA_H