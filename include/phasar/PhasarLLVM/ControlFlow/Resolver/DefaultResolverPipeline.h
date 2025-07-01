/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DEFAULTRESOLVERPIPELINE_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DEFAULTRESOLVERPIPELINE_H

#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverBase.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"

namespace psr {
class LLVMProjectIRDB;
class LLVMVFTableProvider;
class DIBasedTypeHierarchy;
enum class CallGraphAnalysisType;

[[nodiscard]] GenericResolver createDefaultResolverPipeline(
    CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
    const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
    LLVMAliasInfoRef PT = nullptr);
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DEFAULTRESOLVERPIPELINE_H
