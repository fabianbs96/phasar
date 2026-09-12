#include "phasar/PhasarLLVM/AnalysisPipeline.h"

#include "phasar/ControlFlow/CGSCCs.h"
#include "phasar/PhasarLLVM/ControlFlow/FunctionCompressor.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraphBuilder.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/PrecomputedResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/Pointer/LLVMUnionFindAliasSet.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/WithColor.h"

#include <memory>

using namespace psr;

LLVMBasedICFG ICFGStage::buildImpl(LLVMProjectIRDB &IRDB,
                                   const std::vector<std::string> &Entry,
                                   LLVMVFTableProvider &VTP,
                                   DIBasedTypeHierarchy &TH, LLVMAliasInfo *PT,
                                   LLVMBasedICFG *BaseCG,
                                   CallGraphAnalysisType CGTy) {
  Resolver::BaseResolverProvider GetBaseRes = nullptr;
  auto Precomputed =
      [BaseCG](const LLVMProjectIRDB *IRDB, const LLVMVFTableProvider *VTP,
               const DIBasedTypeHierarchy * /*TH*/, LLVMAliasInfoRef /*PT*/) {
        const auto &CG = BaseCG->getCallGraph();
        return std::make_unique<PrecomputedResolver>(IRDB, VTP, &CG);
      };
  if (BaseCG) {
    GetBaseRes = Precomputed;
  }

  auto Res = Resolver::create(CGTy, &IRDB, &VTP, &TH,
                              LLVMAliasInfo::asRefOrNull(PT), GetBaseRes);
  return LLVMBasedICFG(
      buildLLVMBasedCallGraphWithExternCallbackModels(IRDB, *Res, Entry),
      &IRDB);
}

LLVMAliasInfo AliasInfoStage::buildImpl(LLVMProjectIRDB &IRDB,
                                        const LLVMBasedICFG *BaseCG,
                                        AliasAnalysisType AATy,
                                        UnionFindAliasAnalysisType UFAATy) {
  switch (AATy) {
  case AliasAnalysisType::Basic:
  case AliasAnalysisType::CFLSteens:
  case AliasAnalysisType::CFLAnders:
    return std::make_unique<LLVMAliasSet>(&IRDB, true, AATy);
  case AliasAnalysisType::UnionFind:
    if (!BaseCG) {
      llvm::WithColor::error()
          << "UnionFind alias analysis requires a base-call-graph!\n";
      llvm::report_fatal_error(
          "UnionFind alias analysis requires a base-call-graph");
    }
    return std::make_unique<LLVMUnionFindAliasSet>(
        &IRDB, BaseCG->getCallGraph(),
        LLVMUnionFindAliasSet::Config{.AType = UFAATy});
  case AliasAnalysisType::PointsTo:
  case AliasAnalysisType::AndersenOTF:
  case AliasAnalysisType::AndersenOTFCtx:
  case AliasAnalysisType::AndersenOTFDynCtx:
    llvm::report_fatal_error("TODO: implement");
  case AliasAnalysisType::Invalid:
    llvm::report_fatal_error("Invalid AliasAnalysisType");
    break;
  }

  // TODO #ifdef PHASAR_USE_SVF
  llvm::report_fatal_error("unimplemented");
}

auto CGSCCsStage::buildImpl(
    const LLVMBasedICFG &ICF,
    const FunctionCompressor<const llvm::Function *> &Functions) -> result_t {
  return computeCGSCCs(ICF, Functions);
}

auto CGSCCCallersStage::buildImpl(
    const LLVMBasedICFG &ICF,
    const FunctionCompressor<const llvm::Function *> &Functions,
    const SCCHolder<FunctionId> &SCCs) -> result_t {
  return computeCGSCCCallers(ICF, Functions, SCCs);
}
