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

/// Create a default resolver pipeline similar to the old Resolver::create().
///
/// \param Ty The CallGraphAnalysisType that tells, which resolver(s) should be
/// created
/// \param IRDB A non-null pointer to the currently analyzed project
/// \param VTP A non-null pointer to vtable information for the current project
/// \param TH A pointer to a pre-computed type-hierarchy. Needs to be non-null,
/// iff the requested CallGraphAnalysisType required type-hierarchy information,
/// i.e., for CHA/RTA.
/// \param PT A reference to pre-computed alias information. Needs to be
/// non-null, iff the requested CallGraphAnalysisType required alias
/// information, i.e., for OTF.
/// \returns A resolver pipeline that reflects the requested
/// CallGraphAnalysisType.
[[nodiscard]] LLVMGenericResolver createDefaultResolverPipeline(
    CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
    const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
    LLVMAliasInfoRef PT = nullptr);
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_RESOLVER_DEFAULTRESOLVERPIPELINE_H
