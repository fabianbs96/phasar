#include "phasar/PhasarLLVM/AnalysisPipeline.h"

#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasSet.h"
#include "phasar/PhasarLLVM/Pointer/LLVMUnionFindAliasSet.h"

#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/WithColor.h"

#include <memory>

using namespace psr;

LLVMAliasInfo AliasInfoStage::buildImpl(LLVMProjectIRDB &IRDB,
                                        const LLVMBasedICFG *BaseCG,
                                        AliasAnalysisType AATy,
                                        UnionFindAliasAnalysisType UFAATy) {
  switch (AATy) {
  case AliasAnalysisType::Basic:
  case AliasAnalysisType::CFLSteens:
  case AliasAnalysisType::CFLAnders:
    return std::make_unique<LLVMAliasSet>(&IRDB, true, AATy);
  case AliasAnalysisType::PointsTo:
    llvm::WithColor::error()
        << "AliasAnalysisType::PointsTo not implemented yet\n";
  case AliasAnalysisType::UnionFind:
    if (!BaseCG) {
      llvm::WithColor::error()
          << "UnionFind alias analysis requires a base-call-graph!\n";
    }
    return std::make_unique<LLVMUnionFindAliasSet>(
        &IRDB, BaseCG->getCallGraph(),
        LLVMUnionFindAliasSet::Config{.AType = UFAATy});
  case AliasAnalysisType::Invalid:
    llvm::report_fatal_error("Invalid AliasAnalysisType");
    break;
  }

  // TODO #ifdef PHASAR_USE_SVF
  llvm::report_fatal_error("unimplemented");
}
