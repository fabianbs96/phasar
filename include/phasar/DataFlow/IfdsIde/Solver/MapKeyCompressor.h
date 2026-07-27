#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_MAPKEYCOMPRESSOR_H

#include "llvm/ADT/DenseMap.h"
#include "llvm/IR/Value.h"

#include <atomic>

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

template <typename Container = llvm::DenseMap<const llvm::Value *, uint32_t>>
class LLVMMapKeyCompressor {
public:
  using KeyType = const llvm::Value *;
  using CompressedType = uint32_t;

  [[nodiscard]] inline CompressedType getCompressedID(KeyType Key) {
    if constexpr (std::is_same_v<Container, llvm::DenseMap<const llvm::Value *,
                                                           uint32_t>>) {
      auto Search = Map.find(Key);
      if (Search == Map.end()) {
        return Map.insert(std::make_pair(Key, Map.size() + 1))
            .first->getSecond();
      }
      return Search->getSecond();
    } else {
      CompressedType Ret;

      Map.lazy_emplace_l(
          Key, [&](auto &Found) { Ret = Found.second; },
          [&](auto &&Ctor) {
            Ret = MapSizeCounter.fetch_add(1, std::memory_order_relaxed);
            Ctor(std::make_pair(Key, Ret));
          });

      return Ret;
    }
  }

private:
  Container Map{};
  std::atomic_uint32_t MapSizeCounter{};
};

} // namespace psr

#endif
