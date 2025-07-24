#include "phasar/PhasarLLVM/ControlFlow/Resolver/LibraryFunctionResolver.h"

#include "phasar/PhasarLLVM/ControlFlow/GlobalCtorsDtorsModel.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"

#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Module.h"

using namespace psr;

static void handleExit(llvm::SmallVectorImpl<const llvm::Function *> &ToInsert,
                       const llvm::Module *Mod) {
  if (const auto *Dtors =
          Mod->getFunction(GlobalCtorsDtorsModel::DtorModelName)) {
    ToInsert.push_back(Dtors);
  }
}

bool LibraryFunctionResolver::resolve(
    const llvm::CallBase *Call,
    LLVMResolverTraits::FunctionSetTy &PossibleTargets) {

  llvm::SmallVector<const llvm::Function *> ToInsert;
  for (const auto *Target : PossibleTargets) {
    auto TgtName = Target->getName();
    if (TgtName == "exit") {
      handleExit(ToInsert, Target->getParent());
      continue;
    }

    // Would really like to be able to handle pthread_create and other extern
    // functions with callbacks here, but then the subsequent analyses don't
    // know anymore, how to interpret the argument list.
    // We should probably do some IR rewriting, similar as we already do for
    // global ctors/dtors.
    // Tracking issue: #786
  }

  PossibleTargets.insert(ToInsert.begin(), ToInsert.end());

  return !PossibleTargets.empty();
}
