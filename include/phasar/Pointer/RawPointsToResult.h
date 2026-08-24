#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/Pointer/RawAliasSet.h"
#include "phasar/Utils/ValueCompressor.h"

namespace psr {

/// Base interface for results of a ValueId-based points-to analysis.
template <typename T>
concept RawPointsToResult = requires(const T &Result, ValueId Var) {
  { T::isCached() } noexcept -> std::convertible_to<bool>;
  {
    Result.getRawPointsToSet(Var)
  } -> std::convertible_to<RawAliasSet<ValueId>>;
  { Result.mayPointsTo(Var, Var) } -> std::convertible_to<bool>;
  { Result.size() } noexcept -> std::convertible_to<size_t>;
};

} // namespace psr
