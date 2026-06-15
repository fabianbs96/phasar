#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_PLL_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_PLL_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

#include "parallel_hashmap/phmap.h"

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

  [[nodiscard]] inline CompressedType getCompressedID(KeyType Key) {
    std::lock_guard Guard(MapMutex);
    auto Search = Map.find(Key);
    if (Search == Map.end()) {
      return Map.insert(std::make_pair(Key, Map.size() + 1)).first->second;
    }
    return Search->second;
  }

private:
  phmap::parallel_flat_hash_map_m<KeyType, CompressedType> Map{};
  std::mutex MapMutex;
};

} // namespace psr

#endif
