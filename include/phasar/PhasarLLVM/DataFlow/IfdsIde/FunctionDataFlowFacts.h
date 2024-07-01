#include "phasar/Utils/DefaultValue.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace psr {

struct Parameter {
  uint16_t Index{};
};

struct ReturnValue {};

struct DataFlowFact {
  DataFlowFact(Parameter Param) noexcept : Fact(Param) {}
  DataFlowFact(ReturnValue Ret) noexcept : Fact(Ret) {}

  std::variant<Parameter, ReturnValue> Fact;
};

class FunctionDataFlowFacts {
public:
  FunctionDataFlowFacts() = default;

  // insert a set of data flow facts
  void insertSet(llvm::StringRef FuncKey, uint32_t Index,
                 std::vector<DataFlowFact> OutSet) {
    Fdff[FuncKey].try_emplace(Index, std::move(OutSet));
  }

  // insert a single data flow fact
  void addElement(llvm::StringRef FuncKey, uint32_t Index, DataFlowFact Out) {
    Fdff[FuncKey][Index].emplace_back(Out);
  }

  // get out set for a function an the parameter index
  [[nodiscard]] const std::vector<DataFlowFact> &
  getDataFlowFacts(llvm::StringRef FuncKey, uint32_t &Index) const {
    auto It = Fdff.find(FuncKey);
    if (It != Fdff.end()) {
      auto Itt = It->second.find(Index);
      return Itt->second;
    }

    return getDefaultValue<std::vector<DataFlowFact>>();
  }

  // prints the data structure of given keys to terminal

private:
  [[nodiscard]] const auto &
  getDataFlowFactsOrEmpty(llvm::StringRef FuncKey) const {
    auto It = Fdff.find(FuncKey);
    if (It != Fdff.end()) {
      return It->second;
    }

    return getDefaultValue<
        std::unordered_map<uint32_t, std::vector<DataFlowFact>>>();
  }

  // TODO: Remove public and use stable API instead!
public:
  llvm::StringMap<std::unordered_map<uint32_t, std::vector<DataFlowFact>>> Fdff;
};
} // namespace psr
