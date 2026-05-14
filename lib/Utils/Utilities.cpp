/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Demangle/Demangle.h"

#include <algorithm>
#include <chrono>

using namespace psr;

std::string psr::createTimeStamp() {
  auto Now = std::chrono::system_clock::now();
  auto NowTime = std::chrono::system_clock::to_time_t(Now);
  std::string TimeStr(std::ctime(&NowTime));
  std::ranges::replace(TimeStr, ' ', '-');
  TimeStr.erase(std::ranges::remove(TimeStr, '\n').begin(), TimeStr.end());
  return TimeStr;
}

bool psr::isConstructor(llvm::StringRef MangledName) {
  // WARNING: Doesn't work for templated classes, should
  // the best way to do it I can think of is to use a lexer
  // on the name to detect the constructor point explained
  // in the Itanium C++ ABI:
  // see https://itanium-cxx-abi.github.io/cxx-abi/abi.html#mangling

  // This version will not work in some edge cases

  auto Constructor = MangledName.rfind("C2E");
  if (Constructor != llvm::StringRef::npos) {
    return true;
  }

  Constructor = MangledName.rfind("C1E");
  if (Constructor != llvm::StringRef::npos) {
    return true;
  }

  Constructor = MangledName.rfind("C3E");
  if (Constructor != llvm::StringRef::npos) {
    return true;
  }

  return false;
}

bool psr::isMangled(llvm::StringRef Name) {
  // See llvm/Demangle/Demangle.cpp
  if (Name.starts_with("_Z") || Name.starts_with("___Z")) {
    // Itanium ABI
    return true;
  }
  if (Name.starts_with("_R")) {
    // Rust ABI
    return true;
  }
  if (Name.starts_with("_D")) {
    // D ABI
    return true;
  }
  // Microsoft ABI is a bit more complicated...
  return Name != llvm::demangle(Name.str());
}

bool psr::StringIDLess::operator()(const std::string &Lhs,
                                   const std::string &Rhs) PSR_PRECXX23_CONST {
  char *Endptr1;

  char *Endptr2;
  long LhsVal = strtol(Lhs.c_str(), &Endptr1, 10);
  long RhsVal = strtol(Rhs.c_str(), &Endptr2, 10);
  if (Lhs.c_str() == Endptr1 && Lhs.c_str() == Endptr2) {
    return Lhs < Rhs;
  }
  if (Lhs.c_str() == Endptr1 && Rhs.c_str() != Endptr2) {
    return false;
  }
  if (Lhs.c_str() != Endptr1 && Rhs.c_str() == Endptr2) {
    return true;
  }
  return LhsVal < RhsVal;
}
