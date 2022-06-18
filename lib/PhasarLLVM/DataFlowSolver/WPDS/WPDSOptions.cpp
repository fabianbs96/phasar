/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include <string>

#include "llvm/ADT/StringSwitch.h"
#include "llvm/Support/raw_ostream.h"

#include "phasar/PhasarLLVM/DataFlowSolver/WPDS/WPDSOptions.h"

namespace psr {

WPDSType toWPDSType(const std::string &S) {
  WPDSType Type = llvm::StringSwitch<WPDSType>(S)
#define WPDS_TYPES(NAME, TYPE) .Case(NAME, WPDSType::TYPE)
#include "phasar/PhasarLLVM/DataFlowSolver/WPDS/WPDSType.def"
                      .Default(WPDSType::None);
  return Type;
}

std::string toString(const WPDSType &T) {
  switch (T) {
  default:
#define WPDS_TYPES(NAME, TYPE)                                                 \
  case WPDSType::TYPE:                                                         \
    return NAME;                                                               \
    break;
#include "phasar/PhasarLLVM/DataFlowSolver/WPDS/WPDSType.def"
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const WPDSType &T) {
  return OS << toString(T);
}

WPDSSearchDirection toWPDSSearchDirection(const std::string &S) {
  if (S == "FORWARD") {
    return WPDSSearchDirection::FORWARD;
  }
  return WPDSSearchDirection::BACKWARD;
}

std::string toString(const WPDSSearchDirection &S) {
  switch (S) {
  default:
  case WPDSSearchDirection::FORWARD:
    return "FORWARD";
    break;
  case WPDSSearchDirection::BACKWARD:
    return "BACKWARD";
    break;
  }
}

llvm::raw_ostream &operator<<(llvm::raw_ostream &OS,
                              const WPDSSearchDirection &S) {
  return OS << toString(S);
}

} // namespace psr
