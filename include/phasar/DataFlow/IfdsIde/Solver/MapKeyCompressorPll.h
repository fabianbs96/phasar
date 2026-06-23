#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_PLL_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_PLL_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

#include "parallel_hashmap/phmap.h"

#include <atomic>

namespace psr {

template <typename KeyT> class DefaultMapKeyCompressorPll {
public:
  using KeyType = KeyT;
  using CompressedType = KeyT;

  [[nodiscard]] inline CompressedType getCompressedID(KeyT Key) { return Key; }
};

template <typename... Ts> class MapKeyCompressorCombinatorPll : public Ts... {
public:
  using Ts::getCompressedID...;
};

class LLVMMapKeyCompressorPll {
public:
  using KeyType = const llvm::Value *;
  using CompressedType = uint32_t;

  [[nodiscard]] CompressedType getCompressedID(KeyType Key) {
    CompressedType Ret;

    Map.lazy_emplace_l(
        Key, [&](auto &Found) { Ret = Found.second; },
        [&](auto &&Ctor) {
          Ret = MapSizeCounter.fetch_add(1, std::memory_order_relaxed);
          Ctor(std::make_pair(Key, Ret));
        });

    return Ret;
  }

private:
  phmap::parallel_node_hash_map_m<KeyType, CompressedType> Map{};
  std::atomic_uint32_t MapSizeCounter{};
};

} // namespace psr

#endif
