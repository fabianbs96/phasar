module;

#include "phasar/PhasarLLVM/TaintConfig.h"

export module phasar.llvm.taintconfig;

export namespace psr {
using psr::collectGeneratedFacts;
using psr::collectLeakedFacts;
using psr::collectSanitizedFacts;
using psr::FunctionData;
using psr::LLVMTaintConfig;
using psr::parseTaintConfig;
using psr::parseTaintConfigOrNull;
using psr::TaintCategory;
using psr::TaintConfigBase;
using psr::TaintConfigData;
using psr::TaintConfigTraits;
using psr::VariableData;
} // namespace psr
