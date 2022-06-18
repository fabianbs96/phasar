/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/Utils/SummaryStrategy.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

llvm::raw_ostream &psr::operator<<(llvm::raw_ostream &OS,
                                   SummaryGenerationStrategy S) {
  switch (S) {
#define SUMMARY_STRATEGY(NAME)                                                 \
  case SummaryGenerationStrategy::NAME:                                        \
    return OS << #NAME;
#include "phasar/PhasarLLVM/Utils/SummaryStrategy.def"
  }
  llvm_unreachable("Invalid SummaryGenerationStrategy");
}
