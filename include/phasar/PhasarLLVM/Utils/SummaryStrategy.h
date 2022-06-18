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

namespace llvm {
class raw_ostream;
} // namespace llvm

namespace psr {

enum class SummaryGenerationStrategy {
#define SUMMARY_STRATEGY(NAME) NAME,
#include "phasar/PhasarLLVM/Utils/SummaryStrategy.def"
};

llvm::raw_ostream &operator<<(llvm::raw_ostream &Os,
                              SummaryGenerationStrategy S);

} // namespace psr

#endif
