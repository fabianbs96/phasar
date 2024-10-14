#include "phasar/PhasarLLVM/DataFlow/IfdsIde/FunctionDataFlowFacts.h"

#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/ADT/Twine.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"
#include "llvm/Support/raw_ostream.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

using namespace psr;
using llvm::yaml::CustomMappingTraits;
using llvm::yaml::Input;
using llvm::yaml::IO;
using llvm::yaml::Output;

namespace llvm::yaml {
template <> struct MissingTrait<psr::DataFlowFact>;
} // namespace llvm::yaml

namespace llvm::yaml {
template <>
struct MissingTrait<std::unordered_map<uint32_t, psr::DataFlowFact>>;
} // namespace llvm::yaml

namespace llvm::yaml {
template <> struct MissingTrait<psr::FunctionDataFlowFacts>;
} // namespace llvm::yaml

namespace llvm::yaml {
template <> struct ScalarTraits<psr::DataFlowFact> {
  static void output(const psr::DataFlowFact &Val, void * /*unused*/,
                     llvm::raw_ostream &Out) {
    // stream out custom formatting
    if (const auto &Param = std::get_if<psr::Parameter>(&Val.Fact)) {
      Out << std::to_string(Param->Index);
    } else {
      Out << "ReturnValue";
    }
  }
  static StringRef input(StringRef Scalar, void * /*unused*/,
                         psr::DataFlowFact &Value) {
    // parse scalar and set `value`
    // return empty string on success, or error string
    uint16_t IntVal = 0;
    if (Scalar.getAsInteger(0, IntVal)) {
      Value = DataFlowFact(ReturnValue{});
      return {};
    }
    Value = DataFlowFact(psr::Parameter{IntVal});

    return {};
  }
  static QuotingType mustQuote(StringRef /*unused*/) {
    return QuotingType::Double;
  }
};
} // namespace llvm::yaml

LLVM_YAML_IS_FLOW_SEQUENCE_VECTOR(psr::DataFlowFact)

template <>
struct CustomMappingTraits<
    std::unordered_map<uint32_t, std::vector<psr::DataFlowFact>>> {
  static void
  inputOne(IO &Io, StringRef Key,
           std::unordered_map<uint32_t, std::vector<psr::DataFlowFact>> &V) {
    Io.mapRequired(Key.str().c_str(), V[std::atoi(Key.str().c_str())]);
  }
  static void
  output(IO &Io,
         std::unordered_map<uint32_t, std::vector<psr::DataFlowFact>> &V) {
    for (auto &Iter : V) {
      Io.mapRequired(utostr(Iter.first).c_str(), Iter.second);
    }
  }
};

template <> struct CustomMappingTraits<psr::FunctionDataFlowFacts> {
  static void inputOne(IO &Io, StringRef Key, psr::FunctionDataFlowFacts &V) {
    std::unordered_map<uint32_t, std::vector<psr::DataFlowFact>> FunctionFacts =
        V.get(Key);
    Io.mapRequired(Key.str().c_str(), FunctionFacts);
  }
  static void output(IO &Io, psr::FunctionDataFlowFacts &Val) {
    for (auto &Iter : Val) {
      Io.mapRequired(Iter.first().str().c_str(), Iter.second);
    }
  }
};

LLVM_YAML_IS_SEQUENCE_VECTOR(FunctionDataFlowFacts)

void psr::serialize(const FunctionDataFlowFacts &Fdff, llvm::raw_ostream &OS) {
  Output YamlOut(OS);
  // YAML IO needs mutable types
  // TODO: Fix once LLVM fixed API
  YamlOut << const_cast<FunctionDataFlowFacts &>(Fdff);
}

[[nodiscard]] FunctionDataFlowFacts
FunctionDataFlowFacts::deserialize(const llvm::Twine &FilePath) {
  llvm::SmallVector<char> BUF(256);
  Input YamlIn(FilePath.toStringRef(BUF));
  std::vector<FunctionDataFlowFacts> YamlResult;
  YamlIn >> YamlResult;
  return YamlResult[0];
}

[[nodiscard]] FunctionDataFlowFacts
FunctionDataFlowFacts::deserialize(llvm::MemoryBufferRef File) {
  Input YamlIn(File);
  std::vector<FunctionDataFlowFacts> YamlResult;
  YamlIn >> YamlResult;
  return YamlResult[0];
}
