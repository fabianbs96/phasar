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

  [[nodiscard]] CompressedType getCompressedID(KeyType Key) {
    CompressedType Ret;

    // TODO: for some reason, Map.size() creates a data race. It seems to be not
    // thread safe, no idea why.
    // TODO: remove mutex here and find a cleaner solution.
    std::lock_guard Guard(MapMutex);
    Map.lazy_emplace_l(
        Key, [&](auto &Found) { Ret = Found.second; },
        [&](auto &&Ctor) {
          // return Map.insert(std::make_pair(Key, Map.size() +
          // 1)).first->second;
          Ret = Map.size() + 1;
          Ctor(std::make_pair(Key, Ret));
        });

    return Ret;
  }

private:
  phmap::parallel_node_hash_map_m<KeyType, CompressedType> Map{};
  std::mutex MapMutex;
};

} // namespace psr

#endif
