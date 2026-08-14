/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * IDESolver.h
 *
 *  Created on: 14.08.2026
 *      Author: mxHuber
 */

#pragma once

#include "phasar/Utils/PAMMMacros.h"

namespace psr::detail {
struct IDESolverPerf {
  PAMM_CATEGORY(IDESolver);

  // NOLINTBEGIN
  PAMM_COUNTER(Genfacts, Core);
  PAMM_COUNTER(Killfacts, Core);
  PAMM_COUNTER(Summaryreuse, Core);
  PAMM_COUNTER(IntraPathEdges, Core);
  PAMM_COUNTER(InterPathEdges, Core);
  PAMM_COUNTER(FFQueries, Full);
  PAMM_COUNTER(EFQueries, Full);
  PAMM_COUNTER(ValuePropagation, Full);
  PAMM_COUNTER(ValueComputation, Full);
  PAMM_COUNTER(SpecialSummaryFF_Application, Full);
  PAMM_COUNTER(SpecialSummaryEF_Queries, Full);
  PAMM_COUNTER(JumpFnConstruction, Full);
  PAMM_COUNTER(ProcessCall, Full);
  PAMM_COUNTER(ProcessNormal, Full);
  PAMM_COUNTER(ProcessExit, Full);

  PAMM_HISTOGRAM(DataFlowFacts, Full);
  PAMM_HISTOGRAM(PointsTo, Full);

  PAMM_TIMER(DFAPhase1, Full);
  PAMM_TIMER(DFAPhase2, Full);
  // NOLINTEND
};
} // namespace psr::detail
