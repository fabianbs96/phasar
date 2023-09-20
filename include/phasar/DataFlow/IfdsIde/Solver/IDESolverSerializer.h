/******************************************************************************
 * Copyright (c) 2023 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Maximilian Leo Huber and others
 *****************************************************************************/

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERSERIALIZER_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERSERIALIZER_H

#include "IDESolverData.h"

namespace psr {
class IDESolverSerializer {
private:
  void serializeJumpFunctions();
  void serializeWorkLists();
  void serializeEndsummaries();
  void serializeIncomingTab();

  void deserializeJumpFunctions();
  void deserializeWorkLists();
  void deserializeEndsummaries();
  void deserializeIncomingTab();

public:
  IDESolverSerializer();

  void serializeIDESolverData();
  IDESolverData deserializeIDESolverData();
};
} // namespace psr

#endif // PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERSERIALIZER_H
