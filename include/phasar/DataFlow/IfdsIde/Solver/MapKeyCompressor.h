#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

namespace psr {

template <typename KeyT> class DefaultMapKeyCompressor {
public:
  using KeyType = KeyT;
  using CompressedType = KeyT;

  [[nodiscard]] inline CompressedType getCompressedID(KeyT Key) { return Key; }
};

template <typename... Ts> class MapKeyCompressorCombinator : public Ts... {
public:
  using Ts::getCompressedID...;
};

class LLVMMapKeyCompressor {
public:
  using KeyType = const llvm::Value *;
  using CompressedType = uint32_t;

  [[nodiscard]] inline CompressedType getCompressedID(KeyType Key) {
    auto Search = Map.find(Key);
    if (Search == Map.end()) {
      return Map.insert(std::make_pair(Key, Map.size() + 1)).first->getSecond();
    }
    return Search->getSecond();
  }

private:
  llvm::DenseMap<KeyType, CompressedType> Map{};
};

} // namespace psr

#endif
