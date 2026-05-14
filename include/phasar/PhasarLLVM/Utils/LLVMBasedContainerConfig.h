/******************************************************************************
 * Copyright (c) 2022 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_UTILS_LLVMBASEDCONTAINERCONFIG_H
#define PHASAR_PHASARLLVM_UTILS_LLVMBASEDCONTAINERCONFIG_H

#include "phasar/Utils/Macros.h"

#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/IR/Module.h"

#include <memory>

namespace psr {

template <typename T> struct Ref2PointerConverter {
  PSR_CXX23_STATIC constexpr T *operator()(T &Ref) PSR_PRECXX23_CONST noexcept {
    return std::addressof(Ref);
  }
  PSR_CXX23_STATIC constexpr const T *
  operator()(const T &Ref) PSR_PRECXX23_CONST noexcept {
    return std::addressof(Ref);
  }
};

using FunctionRange = llvm::iterator_range<llvm::mapped_iterator<
    llvm::Module::const_iterator, Ref2PointerConverter<llvm::Function>>>;
} // namespace psr

#endif // PHASAR_PHASARLLVM_UTILS_LLVMBASEDCONTAINERCONFIG_H
