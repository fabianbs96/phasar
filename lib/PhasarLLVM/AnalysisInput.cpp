#include "phasar/PhasarLLVM/AnalysisInput.h"

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraphBuilder.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/PrecomputedResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/Pointer/AndersenOTFAA.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointerAssignmentGraph.h"
#include "phasar/PhasarLLVM/Pointer/LLVMUnionFindAliasSet.h"
#include "phasar/Utils/ValueCompressor.h"

#include <memory>

using namespace psr;

LLVMBasedICFG ICFGInput::buildImpl(LLVMProjectIRDB &IRDB,
                                   const std::vector<std::string> &Entries,
                                   const LLVMVFTableProvider &VTP,
                                   const DIBasedTypeHierarchy &TH,
                                   LLVMAliasInfoRef PT,
                                   const LLVMBasedICFG *BaseCG,
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

  auto Res = Resolver::create(CGTy, &IRDB, &VTP, &TH, PT, GetBaseRes);
  return LLVMBasedICFG(
      buildLLVMBasedCallGraphWithExternCallbackModels(IRDB, *Res, Entries),
      &IRDB);
}

LLVMRawAliasSet
SteensgaardAliasInfoInput::buildImpl(LLVMProjectIRDB &IRDB,
                                     const LLVMBasedICFG *BaseCG,
                                     UnionFindAliasAnalysisType UFAATy) {
  return LLVMRawAliasSet(&IRDB, BaseCG->getCallGraph(),
                         LLVMUnionFindAliasSet::Config{.AType = UFAATy});
}

AndersenAliasInfoInput::AndersenAliasInfoInput(
    const LLVMProjectIRDB &IRDB, llvm::ArrayRef<const llvm::Function *> Entries,
    ContextSensitivityOptions AAConfig, Soundness SoundnessFlag)
    : AndersenAliasInfoInput(IRDB, [&] {
        auto VC = std::make_unique<ValueCompressor<PAGVariable>>();
        auto AARes = psr::computeAndersenOTFRaw(
            IRDB, Entries, VC.get(), SoundnessFlag, std::move(AAConfig));
        return std::pair{std::move(AARes), std::move(VC)};
      }()) {}

AndersenAliasInfoInput::AndersenAliasInfoInput(
    const LLVMProjectIRDB &IRDB,
    std::pair<AndersenOTFResult, std::unique_ptr<ValueCompressor<PAGVariable>>>
        AARes)
    : ICF(std::move(AARes.first.CG), &IRDB),
      AI(std::move(AARes.first), std::move(AARes.second)) {}
