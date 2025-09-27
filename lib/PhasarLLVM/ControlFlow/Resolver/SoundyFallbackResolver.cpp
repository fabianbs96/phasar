#include "phasar/PhasarLLVM/ControlFlow/Resolver/SoundyFallbackResolver.h"

#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"

#include "llvm/IR/InstrTypes.h"

using namespace psr;

SoundyFallbackResolver::SoundyFallbackResolver(const LLVMProjectIRDB &IRDB)
    : ATF(IRDB) {}

bool SoundyFallbackResolver::resolve(
    const llvm::CallBase *Call,
    LLVMResolverTraits::FunctionSetTy &PossibleTargets) {
  // TODO: Update with #785

  // we may wish to optimise this function
  // naive implementation that considers every function whose signature
  // matches the call-site's signature as a callee target
  PHASAR_LOG_LEVEL(DEBUG, "Call function pointer: " << llvmIRToString(Call));

  for (const auto *F : ATF) {
    if (isConsistentCall(Call, F)) {
      PossibleTargets.insert(F);
    }
  }

  return !PossibleTargets.empty();
}
