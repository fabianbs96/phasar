#include <iostream>
#include <string>

#include "llvm/ADT/APInt.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_os_ostream.h"

#include "phasar/Config/Configuration.h"
#include "phasar/PhasarLLVM/Passes/FunctionAnnotationPass.h"
#include "phasar/Utils/Logger.h"

using namespace psr;

namespace psr {
llvm::AnalysisKey FunctionAnnotationPass::Key;

size_t FunctionAnnotationPass::UniqueFunctionId = 0;

FunctionAnnotationPass::FunctionAnnotationPass() = default;

llvm::PreservedAnalyses
FunctionAnnotationPass::run(llvm::Module &M,
                            [[maybe_unused]] llvm::ModuleAnalysisManager &MAM) {
  LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), INFO)
                << "Running FunctionAnnotationPass");
  auto &Context = M.getContext();
  for (auto &F : M) {
    if (!F.isDeclaration() && !F.isIntrinsic()) {
      F.addMetadata(
          FunctionMetadataId,
          *llvm::MDNode::get(M.getContext(),
                             llvm::ValueAsMetadata::get(llvm::ConstantInt::get(
                                 llvm::IntegerType::get(M.getContext(), 32),
                                 llvm::APInt(32, UniqueFunctionId)))));
      LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                    << "Function, " << F.getName().str()
                    << ", has the id: " << UniqueFunctionId);
      UniqueFunctionId++;
    }
  }
  return llvm::PreservedAnalyses::all();
}

void FunctionAnnotationPass::resetFunctionID() {
  LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), INFO) << "Reset FunctionId");
  UniqueFunctionId = 0;
}
} // namespace psr
