/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_UTILS_SUMMARYSTRATEGY_H_
#define PHASAR_PHASARLLVM_UTILS_SUMMARYSTRATEGY_H_

#include <map>
#include <string>

namespace llvm {
class raw_ostream;
}

namespace psr {

enum class SummaryGenerationStrategy {
#define SUMMARY_STRATEGY(NAME) NAME,
#include "phasar/PhasarLLVM/Utils/SummaryStrategy.def"
};

extern const std::map<SummaryGenerationStrategy, std::string>
    SummaryGenerationStrategyToString;

extern const std::map<std::string, SummaryGenerationStrategy>
    StringToSummaryGenerationStrategy;

llvm::raw_ostream &operator<<(llvm::raw_ostream &Os,
                              const SummaryGenerationStrategy &S);

} // namespace psr

#endif
