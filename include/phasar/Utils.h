/******************************************************************************
 * Copyright (c) 2023 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_H
#define PHASAR_UTILS_H

#include "phasar/Utils/AdjacencyList.h"
#include "phasar/Utils/AnalysisPrinterBase.h"
#include "phasar/Utils/AnalysisProperties.h"
#include "phasar/Utils/Average.h"
#include "phasar/Utils/BitVectorSet.h"
#include "phasar/Utils/BoxedPointer.h"
#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/ChronoUtils.h"
#include "phasar/Utils/DFAMinimizer.h"
#include "phasar/Utils/DOTGraph.h"
#include "phasar/Utils/DebugOutput.h"
#include "phasar/Utils/DefaultAnalysisPrinter.h"
#include "phasar/Utils/DefaultValue.h"
#include "phasar/Utils/EmptyBaseOptimizationUtils.h"
#include "phasar/Utils/EnumFlags.h"
#include "phasar/Utils/EquivalenceClassMap.h"
#include "phasar/Utils/ErrorHandling.h"
#include "phasar/Utils/GraphTraits.h"
#include "phasar/Utils/IO.h"
#include "phasar/Utils/InitPhasar.h"
#include "phasar/Utils/IotaIterator.h"
#include "phasar/Utils/JoinLattice.h"
#include "phasar/Utils/Macros.h"
#include "phasar/Utils/MaybeUniquePtr.h"
#include "phasar/Utils/NullAnalysisPrinter.h"
#include "phasar/Utils/Nullable.h"
#include "phasar/Utils/OnTheFlyAnalysisPrinter.h"
#include "phasar/Utils/PAMM.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/PointerUtils.h"
#include "phasar/Utils/Printer.h"
#include "phasar/Utils/RepeatIterator.h"
#include "phasar/Utils/SemiRing.h"
#include "phasar/Utils/Soundness.h"
#include "phasar/Utils/StableVector.h"
#include "phasar/Utils/Table.h"
#include "phasar/Utils/TableWrappers.h"
#include "phasar/Utils/Timer.h"
#include "phasar/Utils/TypeTraits.h"
#include "phasar/Utils/Utilities.h"

#endif // PHASAR_UTILS_H
