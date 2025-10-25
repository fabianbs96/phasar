module;

#include "phasar/PhasarLLVM/Pointer.h"

export module phasar.llvm.pointer;

export namespace psr {
using psr::AliasAnalysisView;
using psr::AliasInfoTraits;
using psr::FilteredLLVMAliasSet;
using psr::FunctionAliasView;
using psr::isInterestingPointer;
using psr::LLVMAliasInfo;
using psr::LLVMAliasInfoRef;
using psr::LLVMAliasIteratorRef;
using psr::LLVMAliasSet;
using psr::LLVMAliasSetData;
using psr::LLVMPointsToIterator;
using psr::LLVMPointsToIteratorRef;

#ifdef PHASAR_USE_SVF
using psr::createLLVMSVFPointsToIterator;
using psr::createSVFDDAPointsToInfo;
using psr::createSVFVFSPointsToInfo;
using psr::SVFBasedPointsToInfo;
using psr::SVFBasedPointsToInfoRef;
using psr::SVFPointsToInfoTraits;
#endif

} // namespace psr
