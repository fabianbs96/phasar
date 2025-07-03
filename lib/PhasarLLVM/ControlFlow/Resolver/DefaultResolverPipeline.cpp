#include "phasar/PhasarLLVM/ControlFlow/Resolver/DefaultResolverPipeline.h"

#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/ControlFlow/Resolver/ComposedResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/CHAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DirectCallResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/NOResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/OTFResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/RTAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/SoundyFallbackResolver.h"
#include "phasar/Utils/Macros.h"

using namespace psr;

template <typename ResT> LLVMGenericResolver wrap(ResT &&Res) {
  return std::make_unique<std::decay_t<ResT>>(PSR_FWD(Res));
}

LLVMGenericResolver psr::createDefaultResolverPipeline(
    CallGraphAnalysisType Ty, NonNullPtr<const LLVMProjectIRDB> IRDB,
    NonNullPtr<const LLVMVFTableProvider> VTP, const DIBasedTypeHierarchy *TH,
    LLVMAliasInfoRef PT) {
  switch (Ty) {
  case CallGraphAnalysisType::NORESOLVE:
    return std::make_unique<NOResolver>(IRDB.get(), VTP.get());
  case CallGraphAnalysisType::CHA:
    return wrap(DirectCallResolver{} | CHAResolver(IRDB, VTP, TH) |
                SoundyFallbackResolver{IRDB});
  case CallGraphAnalysisType::RTA:
    return wrap(DirectCallResolver{} | RTAResolver(IRDB, VTP, TH) |
                SoundyFallbackResolver{IRDB});
  case CallGraphAnalysisType::VTA:
    llvm::report_fatal_error(
        "The VTA callgraph algorithm is not implemented yet");
  case CallGraphAnalysisType::OTF:
    assert(PT);
    // Not adding the SoundyFallbackResolver, because OTFResolver incrementally
    // adds alias-information to make itself more sound
    return wrap(DirectCallResolver{} | OTFResolver(IRDB, VTP, PT));
  case CallGraphAnalysisType::Invalid:
    llvm::report_fatal_error("Invalid callgraph algorithm specified");
  }
}
