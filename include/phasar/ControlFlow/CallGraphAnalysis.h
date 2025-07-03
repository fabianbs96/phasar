/******************************************************************************
 * Copyright (c) 2025 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_CONTROLFLOW_CALLGRAPHANALYSIS_H
#define PHASAR_CONTROLFLOW_CALLGRAPHANALYSIS_H

#include "phasar/ControlFlow/CFGBase.h"
#include "phasar/ControlFlow/CallGraph.h"
#include "phasar/ControlFlow/Resolver/Resolver.h"
#include "phasar/DB/ProjectIRDBBase.h"
#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/NonNullPtr.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/Printer.h"
#include "phasar/Utils/Soundness.h"

namespace psr {

template <typename DB, typename CFG, typename ResolverT>
class CallGraphAnalysis {
public:
  using Traits = ResolverTraitsFor<ResolverT>;
  using n_t = typename ResolverT::n_t;
  using f_t = typename ResolverT::f_t;

  explicit CallGraphAnalysis(NonNullPtr<const ProjectIRDBBase<DB>> IRDB,
                             NonNullPtr<const CFGBase<CFG>> CF, ResolverT Res,
                             llvm::ArrayRef<f_t> EntryPointFns)
      : IRDB(IRDB), CF(CF), Res(std::move(Res)) {
    initWorkList(EntryPointFns);
  }

  [[nodiscard]] CallGraph<n_t, f_t> solve(Soundness S = Soundness::Soundy) {
    VisitedFunctions.reserve(IRDB->getNumFunctions());

    bool RequiresIndirectCallsFixpoint =
        S != psr::Soundness::Unsound && Res.mutatesHelperAnalysisInformation();

    bool FixpointReached;

    do {
      FixpointReached = true;
      while (!FunctionWL.empty()) {
        f_t Fun = FunctionWL.pop_back_val();
        FixpointReached &= processFunction(Fun);
      }

      if (RequiresIndirectCallsFixpoint) {
        /// XXX This can probably be done more efficiently.
        /// However, we cannot just work on the IndirectCalls-delta as we are
        /// mutating the points-to-info on the fly
        for (auto [CS, _] : IndirectCalls) {
          FixpointReached &= !constructDynamicCall(CS);
        }
      }
    } while (!FixpointReached);
    IF_LOG_LEVEL_ENABLED(WARNING, {
      for (const auto &[IndirectCall, Targets] : IndirectCalls) {
        if (Targets == 0) {
          PHASAR_LOG_LEVEL(WARNING, "No callees found for callsite "
                                        << NToString(IndirectCall));
        }
      }
    });

    PAMM_GET_INSTANCE;
    REG_COUNTER("CG Functions",
                CGBuilder.viewCallGraph().getNumVertexFunctions(), Full);
    REG_COUNTER("CG CallSites",
                CGBuilder.viewCallGraph().getNumVertexCallSites(), Full);
    PHASAR_LOG_LEVEL_CAT(INFO, "CallGraphAnalysis",
                         "Call graph has been constructed");
    return CGBuilder.consumeCallGraph();
  }

private:
  void initWorkList(llvm::ArrayRef<f_t> EntryPointFns) {
    auto NumFuns = IRDB->getNumFunctions();
    FunctionWL.reserve(NumFuns);
    FunctionWL.append(EntryPointFns.begin(), EntryPointFns.end());

    CGBuilder.reserve(NumFuns);
  }

  bool fillPossibleTargets(typename Traits::FunctionSetTy &PossibleTargets,
                           ByConstRef<n_t> CS) {
    if (const auto *StaticCallee = CF->getStaticCalleeOrNull(CS)) {
      PossibleTargets.insert(StaticCallee);

      PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                           "Found static call-site: " << "  " << NToString(CS));
      return true;
    }

    // if (llvm::isa<llvm::InlineAsm>(CS->getCalledOperand())) {
    //   return true;
    // }

    // the function call must be resolved dynamically
    PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                         "Found dynamic call-site: " << "  " << NToString(CS));

    Res.resolve(CS, PossibleTargets);

    IndirectCalls[CS] = PossibleTargets.size();
    return false;
  }

  bool processFunction(ByConstRef<f_t> Fun) {
    PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                         "Walking in function: " << Fun->getName());
    if (Fun->isDeclaration() || !VisitedFunctions.insert(Fun).second) {
      PHASAR_LOG_LEVEL_CAT(
          DEBUG, "CallGraphAnalysis",
          "Function already visited or only declaration: " << Fun->getName());
      return true;
    }

    // add a node for function F to the call graph (if not present already)
    std::ignore = CGBuilder.addFunctionVertex(Fun);

    bool FixpointReached = true;

    // iterate all instructions of the current function
    typename Traits::FunctionSetTy PossibleTargets;

    for (const auto &I : CF->getAllInstructionsOf(Fun)) {
      if (!CF->isCallSite(I)) {
        continue;
      }

      FixpointReached &= fillPossibleTargets(PossibleTargets, I);

      PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                           "Found " << PossibleTargets.size()
                                    << " possible target(s)");

      Res.handlePossibleTargets(I, PossibleTargets);

      auto *CallSiteId = CGBuilder.addInstructionVertex(I);

      // Insert possible target inside the graph and add the link with
      // the current function
      for (const auto *PossibleTarget : PossibleTargets) {
        CGBuilder.addCallEdge(I, CallSiteId, PossibleTarget);
        FunctionWL.push_back(PossibleTarget);
      }
      PossibleTargets.clear();
    }

    return FixpointReached;
  }

  bool constructDynamicCall(ByConstRef<n_t> CS) {
    if (!CF->isCallSite(CS)) {
      llvm::report_fatal_error("[constructDynamicCall]: No call: " +
                               llvm::Twine(NToString(CS)));
    }

    // Find vertex of callsite.
    auto *Callees = CGBuilder.getInstVertexOrNull(CS);
    if (!Callees) {
      llvm::report_fatal_error(
          "[constructDynamicCall]: Did not find vertex of callsite " +
          llvm::Twine(NToString(CS)));
    }

    // the function call must be resolved dynamically
    PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                         "Looking into dynamic call-site: ");
    PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis", "  " << NToString(CS));

    // call the resolve routine

    typename Traits::FunctionSetTy PossibleTargets;
    Res.resolve(CS, PossibleTargets);

    assert(IndirectCalls.count(CS));
    auto &NumIndCalls = IndirectCalls[CS];

    if (NumIndCalls >= PossibleTargets.size()) {
      // No new targets found
      return false;
    }

    PHASAR_LOG_LEVEL_CAT(DEBUG, "CallGraphAnalysis",
                         "Found " << PossibleTargets.size() - NumIndCalls
                                  << " new possible target(s)");
    NumIndCalls = PossibleTargets.size();

    // Throw out already found targets
    for (const auto *Tgt : *Callees) {
      PossibleTargets.erase(Tgt);
    }

    Res.handlePossibleTargets(CS, PossibleTargets);

    // Insert possible target inside the graph and add the link with
    // the current function
    for (const auto *PossibleTarget : PossibleTargets) {
      CGBuilder.addCallEdge(CS, Callees, PossibleTarget);
      FunctionWL.push_back(PossibleTarget);
    }

    return true;
  }

  NonNullPtr<const ProjectIRDBBase<DB>> IRDB;
  NonNullPtr<const CFGBase<CFG>> CF;
  ResolverT Res;

  CallGraphBuilder<n_t, f_t> CGBuilder{};

  llvm::DenseSet<f_t> VisitedFunctions{};

  // The worklist for direct callee resolution.
  llvm::SmallVector<f_t, 0> FunctionWL{};

  // Map indirect calls to the number of possible targets found for it. Fixpoint
  // is not reached when more targets are found.
  llvm::DenseMap<n_t, unsigned> IndirectCalls{};
};

template <typename DB, typename CFG, typename ResolverT>
CallGraphAnalysis(const ProjectIRDBBase<DB> *IRDB, const CFGBase<CFG> *CF,
                  ResolverT Res,
                  llvm::ArrayRef<typename ResolverT::f_t> EntryPointFns)
    -> CallGraphAnalysis<DB, CFG, ResolverT>;
} // namespace psr

#endif // PHASAR_CONTROLFLOW_CALLGRAPHANALYSIS_H
