/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * LLVMBasedICFG.h
 *
 *  Created on: 09.09.2016
 *      Author: pdschbrt
 */

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDICFG_H_
#define PHASAR_PHASARLLVM_CONTROLFLOW_LLVMBASEDICFG_H_

#include "phasar/ControlFlow/CallGraph.h"
#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/ControlFlow/ICFGBase.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/Utils/LLVMBasedContainerConfig.h"
#include "phasar/Utils/MaybeUniquePtr.h"
#include "phasar/Utils/MemoryResource.h"
#include "phasar/Utils/Soundness.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Value.h"
#include "llvm/Support/raw_ostream.h"

#include "nlohmann/json.hpp"

#include <memory>

namespace psr {
class LLVMTypeHierarchy;
class LLVMProjectIRDB;

class LLVMBasedICFG;
template <> struct CFGTraits<LLVMBasedICFG> : CFGTraits<LLVMBasedCFG> {};

class LLVMBasedICFG : public LLVMBasedCFG, public ICFGBase<LLVMBasedICFG> {
  friend ICFGBase;

  struct Builder;

public:
  static constexpr llvm::StringLiteral GlobalCRuntimeModelName =
      "__psrCRuntimeGlobalCtorsModel";

  /// Constructs the ICFG based on the given IRDB and the entry-points using a
  /// fixpoint iteration. This may take a long time.
  ///
  /// \param IRDB The IR code where the ICFG should be based on. Must not be
  /// nullptr.
  /// \param CGType The analysis kind to use for call-graph resolution
  /// \param EntryPoints The names of the functions to start with when
  /// incrementally building up the ICFG. For whole-program analysis of an
  /// executable use {"main"}.
  /// \param TH The type-hierarchy implementation to use. Will be constructed
  /// on-the-fly if nullptr, but required
  /// \param PT The points-to implementation to use. Will be constructed
  /// on-the-fly if nullptr, but required
  /// \param S The soundness level to expect from the analysis. Currently unused
  /// \param IncludeGlobals Properly include global constructors/destructors
  /// into the ICFG, if true. Requires to generate artificial functions into the
  /// IRDB. True by default
  explicit LLVMBasedICFG(LLVMProjectIRDB *IRDB, CallGraphAnalysisType CGType,
                         llvm::ArrayRef<std::string> EntryPoints = {},
                         LLVMTypeHierarchy *TH = nullptr,
                         LLVMAliasInfoRef PT = nullptr,
                         Soundness S = Soundness::Soundy,
                         bool IncludeGlobals = true);

  /// Creates an ICFG with an already given call-graph
  explicit LLVMBasedICFG(CallGraph<n_t, f_t> CG, LLVMProjectIRDB *IRDB,
                         LLVMTypeHierarchy *TH = nullptr);

  explicit LLVMBasedICFG(LLVMProjectIRDB *IRDB,
                         const nlohmann::json &SerializedCG,
                         LLVMTypeHierarchy *TH = nullptr);

  ~LLVMBasedICFG() = default;

  LLVMBasedICFG(const LLVMBasedICFG &) = delete;
  LLVMBasedICFG &operator=(const LLVMBasedICFG &) = delete;

  LLVMBasedICFG(LLVMBasedICFG &&) noexcept = default;
  LLVMBasedICFG &operator=(LLVMBasedICFG &&) noexcept = default;

  /// Exports the whole ICFG (not only the call-graph) as DOT.
  ///
  /// \param WithSourceCodeInfo If true, not only contains the LLVM instructions
  /// as labels, but source-code information as well (e.g. function name, line
  /// no, col no, src-line).
  [[nodiscard]] std::string
  exportICFGAsDot(bool WithSourceCodeInfo = true) const;
  /// Similar to exportICFGAsDot, but exports the ICFG as JSON instead
  [[nodiscard]] nlohmann::json
  exportICFGAsJson(bool WithSourceCodeInfo = true) const;

  [[nodiscard]] size_t getNumVertexFunctions() const noexcept {
    return CG.viewCallGraph().getNumVertexFunctions();
  }

  /// Returns all functions from the underlying IRDB that are part of the ICFG,
  /// i.e. that are reachable from the entry-points
  [[nodiscard]] auto getAllVertexFunctions() const noexcept {
    return CG.viewCallGraph().getAllVertexFunctions();
  }

  /// Gets the underlying IRDB
  [[nodiscard]] LLVMProjectIRDB *getIRDB() const noexcept { return IRDB; }

  using CFGBase::print;
  using ICFGBase::print;

  using CFGBase::getAsJson;
  using ICFGBase::getAsJson;

  [[nodiscard]] size_t getNumOfEdges() const noexcept { return NumEdges; }
  [[nodiscard]] size_t getTotalRuntimeEdgesAdded() const noexcept {
    return TotalRuntimeEdgesAdded;
  }

  [[nodiscard]] bool addEdgeToICFG(const llvm::Instruction *CallSite,
                                   const llvm::Function *CallTarget);

  [[nodiscard]] auto getUnsoundCallSites() {
    return llvm::make_range(UnsoundCallSites.begin(), UnsoundCallSites.end());
  }

  [[nodiscard]] size_t getNumUnsoundCallSites() const noexcept {
    return UnsoundCallSites.size();
  }

private:
  [[nodiscard]] FunctionRange getAllFunctionsImpl() const;
  [[nodiscard]] f_t getFunctionImpl(llvm::StringRef Fun) const;

  [[nodiscard]] bool isIndirectFunctionCallImpl(n_t Inst) const;
  [[nodiscard]] bool isVirtualFunctionCallImpl(n_t Inst) const;
  [[nodiscard]] std::vector<n_t> allNonCallStartNodesImpl() const;
  [[nodiscard]] llvm::SmallVector<n_t> getCallsFromWithinImpl(f_t Fun) const;
  [[nodiscard]] llvm::SmallVector<n_t, 2>
  getReturnSitesOfCallAtImpl(n_t Inst) const;
  void printImpl(llvm::raw_ostream &OS) const;
  [[nodiscard]] nlohmann::json getAsJsonImpl() const;
  [[nodiscard]] const CallGraph<n_t, f_t> &getCallGraphImpl() const noexcept {
    return CG.viewCallGraph();
  }

  [[nodiscard]] llvm::Function *buildCRuntimeGlobalCtorsDtorsModel(
      llvm::Module &M, llvm::ArrayRef<llvm::Function *> UserEntryPoints);

  // ---

  CallGraphBuilder<const llvm::Instruction *, const llvm::Function *> CG;
  llvm::DenseSet<const llvm::Instruction *> UnsoundCallSites;
  LLVMProjectIRDB *IRDB = nullptr;
  MaybeUniquePtr<LLVMTypeHierarchy, true> TH;
  size_t NumEdges = 0;
  size_t TotalRuntimeEdgesAdded = 0;
};
} // namespace psr

#endif
