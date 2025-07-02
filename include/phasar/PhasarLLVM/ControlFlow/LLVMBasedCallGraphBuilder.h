/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDCALLGRAPHBUILDER_H
#define PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDCALLGRAPHBUILDER_H

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraph.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverBase.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/Utils/Soundness.h"

namespace psr {
class LLVMProjectIRDB;
enum class CallGraphAnalysisType;
class DIBasedTypeHierarchy;
class LLVMVFTableProvider;

/// Builds a call-graph using the provided information.
///
/// \param IRDB A reference to the IRDB containing the code under analysis.
/// \param CGType The CallGraphAnalysisType that tells, which algorithm should
/// be used to create the call-graph.
/// \param EntryPoints A set of functions, where the analysis should start. The
/// resulting call-graph will only contain functions that are (transitively)
/// reachable from the entry points.
/// \param TH A reference to a pre-computed type-hierarchy.
/// \param VTP A reference to vtable information for the current project
/// \param PT A reference to pre-computed alias information. If nullptr, a
/// default alias-info will be constructed on-demand, provided the CGType
/// requires alias information.
/// \param S Configure, how well the analysis should try to compute a sound
/// over-approximation of the real runtime call-graph. Higher soundness levels
/// usually have negative impact on the precision of the resulting call-graph,
/// i.e, a more sound call-graph may have more spurious call-edges.
[[nodiscard]] LLVMBasedCallGraph
buildLLVMBasedCallGraph(LLVMProjectIRDB &IRDB, CallGraphAnalysisType CGType,
                        llvm::ArrayRef<const llvm::Function *> EntryPoints,
                        DIBasedTypeHierarchy &TH, LLVMVFTableProvider &VTP,
                        LLVMAliasInfoRef PT = nullptr,
                        Soundness S = Soundness::Soundy);

/// Builds a call-graph using the provided information.
///
/// \param IRDB A reference to the IRDB containing the code under analysis.
/// \param CGResolver A resolver pipeline that should be used to resolve
/// call-targets of indirect calls
/// \param EntryPoints A set of functions, where the analysis should start. The
/// resulting call-graph will only contain functions that are (transitively)
/// reachable from the entry points.
/// \param S Configure, how well the analysis should try to compute a sound
/// over-approximation of the real runtime call-graph. Higher soundness levels
/// usually have negative impact on the precision of the resulting call-graph,
/// i.e, a more sound call-graph may have more spurious call-edges.
[[nodiscard]] LLVMBasedCallGraph
buildLLVMBasedCallGraph(const LLVMProjectIRDB &IRDB,
                        GenericResolverRef CGResolver,
                        llvm::ArrayRef<const llvm::Function *> EntryPoints,
                        Soundness S = Soundness::Soundy);

/// Builds a call-graph using the provided information.
///
/// \param IRDB A reference to the IRDB containing the code under analysis.
/// \param CGType The CallGraphAnalysisType that tells, which algorithm should
/// be used to create the call-graph.
/// \param EntryPoints A set of function-names, identifying the functions where
/// the analysis should start. The resulting call-graph will only contain
/// functions that are (transitively) reachable from the entry points.
/// \param TH A reference to a pre-computed type-hierarchy.
/// \param VTP A reference to vtable information for the current project
/// \param PT A reference to pre-computed alias information. If nullptr, a
/// default alias-info will be constructed on-demand, provided the CGType
/// requires alias information.
/// \param S Configure, how well the analysis should try to compute a sound
/// over-approximation of the real runtime call-graph. Higher soundness levels
/// usually have negative impact on the precision of the resulting call-graph,
/// i.e, a more sound call-graph may have more spurious call-edges.
[[nodiscard]] LLVMBasedCallGraph
buildLLVMBasedCallGraph(LLVMProjectIRDB &IRDB, CallGraphAnalysisType CGType,
                        llvm::ArrayRef<std::string> EntryPoints,
                        DIBasedTypeHierarchy &TH, LLVMVFTableProvider &VTP,
                        LLVMAliasInfoRef PT = nullptr,
                        Soundness S = Soundness::Soundy);

/// Builds a call-graph using the provided information.
///
/// \param IRDB A reference to the IRDB containing the code under analysis.
/// \param CGResolver A resolver pipeline that should be used to resolve
/// call-targets of indirect calls
/// \param EntryPoints A set of function-names, identifying the functions where
/// the analysis should start. The resulting call-graph will only contain
/// functions that are (transitively) reachable from the entry points.
/// \param S Configure, how well the analysis should try to compute a sound
/// over-approximation of the real runtime call-graph. Higher soundness levels
/// usually have negative impact on the precision of the resulting call-graph,
/// i.e, a more sound call-graph may have more spurious call-edges.
[[nodiscard]] LLVMBasedCallGraph buildLLVMBasedCallGraph(
    const LLVMProjectIRDB &IRDB, GenericResolverRef CGResolver,
    llvm::ArrayRef<std::string> EntryPoints, Soundness S = Soundness::Soundy);
} // namespace psr

#endif // PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDCALLGRAPHBUILDER_H
