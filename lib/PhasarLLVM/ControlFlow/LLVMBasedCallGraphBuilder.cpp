#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraphBuilder.h"

#include "phasar/ControlFlow/CallGraphAnalysis.h"
#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/ControlFlow/EntryFunctionUtils.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraph.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DefaultResolverPipeline.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverBase.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverUtils.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/Soundness.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/IR/InstrTypes.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"

#include <memory>

using namespace psr;

namespace {
struct ResolverWrapper {
  LLVMGenericResolverRef BaseResolver;

  using n_t = const llvm::Instruction *;
  using f_t = const llvm::Function *;
  using FunctionSetTy = LLVMResolverTraits::FunctionSetTy;

  bool resolve(n_t Call, FunctionSetTy &PossibleTargets) {
    return BaseResolver.resolve(llvm::cast<llvm::CallBase>(Call),
                                PossibleTargets);
  }

  [[nodiscard]] bool mutatesHelperAnalysisInformation() const noexcept {
    return BaseResolver.mutatesHelperAnalysisInformation();
  }

  void handlePossibleTargets(n_t CallSite, FunctionSetTy &CalleeTargets) {
    return BaseResolver.handlePossibleTargets(
        llvm::cast<llvm::CallBase>(CallSite), CalleeTargets);
  }
};

} // namespace

auto psr::buildLLVMBasedCallGraph(
    const LLVMProjectIRDB &IRDB, LLVMGenericResolverRef CGResolver,
    llvm::ArrayRef<const llvm::Function *> EntryPoints, Soundness S)
    -> LLVMBasedCallGraph {

  PHASAR_LOG_LEVEL_CAT(
      INFO, "LLVMBasedICFG",
      "Starting ICFG construction "
          << std::chrono::steady_clock::now().time_since_epoch().count());

  scope_exit FinishTiming = [] {
    PHASAR_LOG_LEVEL_CAT(
        INFO, "LLVMBasedICFG",
        "Finished ICFG construction "
            << std::chrono::steady_clock::now().time_since_epoch().count());
  };

  LLVMBasedCFG CF;
  CallGraphAnalysis CGAnalysis(&IRDB, &CF, ResolverWrapper{CGResolver},
                               EntryPoints);

  return CGAnalysis.solve(S);
}

auto psr::buildLLVMBasedCallGraph(
    LLVMProjectIRDB &IRDB, CallGraphAnalysisType CGType,
    llvm::ArrayRef<const llvm::Function *> EntryPoints,
    DIBasedTypeHierarchy &TH, LLVMVFTableProvider &VTP, LLVMAliasInfoRef PT,
    Soundness S) -> LLVMBasedCallGraph {

  LLVMAliasInfo PTOwn;
  if (!PT && CGType == CallGraphAnalysisType::OTF) {
    PTOwn = std::make_unique<LLVMAliasSet>(&IRDB);
    PT = PTOwn.asRef();
  }

  auto Res = createDefaultResolverPipeline(CGType, &IRDB, &VTP, &TH, PT);
  return buildLLVMBasedCallGraph(IRDB, Res, EntryPoints, S);
}

auto psr::buildLLVMBasedCallGraph(LLVMProjectIRDB &IRDB,
                                  CallGraphAnalysisType CGType,
                                  llvm::ArrayRef<std::string> EntryPoints,
                                  DIBasedTypeHierarchy &TH,
                                  LLVMVFTableProvider &VTP, LLVMAliasInfoRef PT,
                                  Soundness S) -> LLVMBasedCallGraph {
  auto EntryPointFns = getEntryFunctions(IRDB, EntryPoints);
  return buildLLVMBasedCallGraph(IRDB, CGType, EntryPointFns, TH, VTP, PT, S);
}

auto psr::buildLLVMBasedCallGraph(const LLVMProjectIRDB &IRDB,
                                  LLVMGenericResolverRef CGResolver,
                                  llvm::ArrayRef<std::string> EntryPoints,
                                  Soundness S) -> LLVMBasedCallGraph {
  auto EntryPointFns = getEntryFunctions(IRDB, EntryPoints);
  return buildLLVMBasedCallGraph(IRDB, CGResolver, EntryPointFns, S);
}
