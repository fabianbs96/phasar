#include "phasar/PhasarLLVM/ControlFlow/Resolver/ResolverBase.h"

#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/CHAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/ComposedResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DirectCallResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/NOResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/OTFResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/RTAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/SoundyFallbackResolver.h"
#include "phasar/Utils/Macros.h"

using namespace psr;

template <typename ResT> GenericResolver wrap(ResT &&Res) {
  return std::make_unique<std::decay_t<ResT>>(PSR_FWD(Res));
}

GenericResolver psr::createDefaultResolverPipeline(
    CallGraphAnalysisType Ty, const LLVMProjectIRDB *IRDB,
    const LLVMVFTableProvider *VTP, const DIBasedTypeHierarchy *TH,
    LLVMAliasInfoRef PT) {
  // TODO: combine DirectCallResolver + [concrete resolver based on Ty] +
  // SoundyFallbackResolver

  assert(IRDB != nullptr);
  assert(VTP != nullptr);

  switch (Ty) {
  case CallGraphAnalysisType::NORESOLVE:
    return std::make_unique<NOResolver>(IRDB, VTP);
  case CallGraphAnalysisType::CHA:
    assert(TH != nullptr);
    return wrap(DirectCallResolver{} | CHAResolver(IRDB, VTP, TH) |
                SoundyFallbackResolver{IRDB});
  case CallGraphAnalysisType::RTA:
    assert(TH != nullptr);
    return wrap(DirectCallResolver{} | RTAResolver(IRDB, VTP, TH) |
                SoundyFallbackResolver{IRDB});
  case CallGraphAnalysisType::VTA:
    llvm::report_fatal_error(
        "The VTA callgraph algorithm is not implemented yet");
  case CallGraphAnalysisType::OTF:
    assert(PT);
    return wrap(DirectCallResolver{} | OTFResolver(IRDB, VTP, PT) |
                SoundyFallbackResolver{IRDB});
  case CallGraphAnalysisType::Invalid:
    llvm::report_fatal_error("Invalid callgraph algorithm specified");
  }
}
