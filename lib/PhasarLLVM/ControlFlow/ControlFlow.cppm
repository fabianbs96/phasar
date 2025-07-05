module;
#include "phasar/PhasarLLVM/ControlFlow.h"

export module phasar.llvm.controlflow;

export namespace psr {
using psr::buildLLVMBasedCallGraph;
using psr::CFGTraits;
using psr::CHAResolver;
using psr::createDefaultResolverPipeline;
using psr::DirectCallResolver;
using psr::getEntryFunctions;
using psr::getEntryFunctionsMut;
using psr::getNonPureVirtualVFTEntry;
using psr::getReceiverType;
using psr::getReceiverTypeName;
using psr::getVFTIndex;
using psr::GlobalCtorsDtorsModel;
using psr::ICFGBase;
using psr::isConsistentCall;
using psr::isHeapAllocatingFunction;
using psr::isVirtualCall;
using psr::LLVMBasedBackwardCFG;
using psr::LLVMBasedBackwardICFG;
using psr::LLVMBasedCallGraph;
using psr::LLVMBasedCFG;
using psr::LLVMBasedICFG;
using psr::LLVMGenericResolver;
using psr::LLVMGenericResolverRef;
using psr::LLVMVFTableProvider;
using psr::NOResolver;
using psr::OTFResolver;
using psr::Resolver;
using psr::RTAResolver;
using psr::SoundyFallbackResolver;
using psr::SparseLLVMBasedCFG;
using psr::SparseLLVMBasedCFGProvider;
using psr::SparseLLVMBasedICFG;
using psr::SparseLLVMBasedICFGView;
using psr::valueOf;
} // namespace psr
