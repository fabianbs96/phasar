/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_EXPONENT_FOR_SHARDS_H
#define PHASAR_UTILS_EXPONENT_FOR_SHARDS_H

#include <cstddef>

static constexpr size_t ExponentForShards = 7;

// std::pow isn't constexpr, so we need a special function for this.
constexpr size_t getNumOfShards(size_t Exponent) {
  return Exponent == 0 ? 1 : 2 * getNumOfShards(Exponent - 1);
}

#endif
