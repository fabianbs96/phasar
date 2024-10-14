#include "phasar/Utils/DefaultValue.h"

#include "llvm/ADT/StringMap.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
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
  DataFlowFact() noexcept : Fact(Parameter{UINT16_MAX}) {}

  std::variant<Parameter, ReturnValue> Fact;
};

class FunctionDataFlowFacts {
public:
  FunctionDataFlowFacts();

  // insert a set of data flow facts
  void insertSet(llvm::StringRef FuncKey, uint32_t Index,
                 std::vector<DataFlowFact> OutSet) {
    Fdff[FuncKey].try_emplace(Index, std::move(OutSet));
  }

  // insert a single data flow fact
  void addElement(llvm::StringRef FuncKey, uint32_t Index, DataFlowFact Out) {
    Fdff[FuncKey][Index].emplace_back(Out);
  }

  void
  insert(llvm::StringRef FuncKey,
         std::unordered_map<uint32_t, std::vector<DataFlowFact>> &InnerMap) {
    Fdff[FuncKey] = InnerMap;
  }

  // get outset for a function an the parameter index
  [[nodiscard]] const std::vector<DataFlowFact> &
  getDataFlowFacts(llvm::StringRef FuncKey, uint32_t &Index) const {
    auto It = Fdff.find(FuncKey);
    if (It != Fdff.end()) {
      auto Itt = It->second.find(Index);
      return Itt->second;
    }

    return getDefaultValue<std::vector<DataFlowFact>>();
  }

  [[nodiscard]] std::unordered_map<uint32_t, std::vector<DataFlowFact>>
  get(llvm::StringRef FuncKey) {
    auto It = Fdff.find(FuncKey);
    if (It != Fdff.end()) {
      return It->second;
    }

    return It->second;
  }

  [[nodiscard]] auto begin() const noexcept { return Fdff.begin(); }

  [[nodiscard]] auto begin() noexcept { return Fdff.begin(); }

  [[nodiscard]] auto end() const noexcept { return Fdff.end(); }

  [[nodiscard]] auto end() noexcept { return Fdff.end(); }

  [[nodiscard]] size_t size() const { return Fdff.size(); }

  [[nodiscard]] FunctionDataFlowFacts deserialize(const llvm::Twine &FilePath);

  [[nodiscard]] FunctionDataFlowFacts deserialize(llvm::MemoryBufferRef File);

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

  llvm::StringMap<std::unordered_map<uint32_t, std::vector<DataFlowFact>>> Fdff;
};

void serialize(const FunctionDataFlowFacts &Fdff, llvm::raw_ostream &OS);

} // namespace psr
