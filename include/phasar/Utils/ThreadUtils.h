/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_UTILS_THREAD_UTILS_H
#define PHASAR_UTILS_THREAD_UTILS_H

#include "parallel_hashmap/phmap_fwd_decl.h"

#include <cstddef>

namespace psr {

static constexpr size_t ExponentForShards = 7;

static constexpr size_t NumOfShards = size_t(1) << ExponentForShards;

// N=7 (128 shards) instead of the default N=4 (16 shards): with the thread
// pool defaulting to hardware_concurrency() threads, 16 shards causes heavy
// per-shard mutex contention; more shards trades a small constant memory
// overhead per map for far fewer collisions.
template <typename Key, typename Val>
using PllMap = phmap::parallel_node_hash_map_m<
    Key, Val, phmap::Hash<Key>, phmap::EqualTo<Key>,
    phmap::Allocator<std::pair<const Key, Val>>, ExponentForShards>;

} // namespace psr

#endif
