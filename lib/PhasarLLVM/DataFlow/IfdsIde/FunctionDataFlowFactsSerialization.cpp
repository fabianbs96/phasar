#include "phasar/PhasarLLVM/DataFlow/IfdsIde/FunctionDataFlowFacts.h"
// #include nlohman.json oder YAML
#include "llvm/Support/YAMLParser.h"
#include "llvm/Support/YAMLTraits.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <llvm/ADT/StringMap.h>
#include <llvm/ADT/StringRef.h>
#include <llvm/Support/Path.h>
#include <llvm/Support/raw_ostream.h>

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
template <> struct MissingTrait<FunctionDataFlowFacts>;
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
    return QuotingType::Single;
  }
};
} // namespace llvm::yaml

namespace llvm::yaml {
template <> struct SequenceElementTraits<psr::DataFlowFact> {
  static const bool flow = true; // NOLINT
};
} // namespace llvm::yaml

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
    for (const auto &Iter : Val) {
      Io.mapRequired(Iter.first().str().c_str(), Iter.second);
    }
  }
};

void serialize(const FunctionDataFlowFacts &Fdff, llvm::raw_ostream &OS) {
  /*std::string Yaml;
  for (const auto &ItOuter : Fdff) {
    OS << "- "
       << "ItOuter.first"
       << ":"
       << "\n";
    for (const auto &ItInner : ItOuter.second) {
      OS << "    "
         << "- " << std::to_string(ItInner.first) << ":"
         << "\n";
      for (DataFlowFact DFF : ItInner.second) {
        if (const Parameter *Param = std::get_if<Parameter>(&DFF.Fact)) {
          OS << "        "
             << "- " << std::to_string(Param->Index) << "\n";
        } else {
          OS << "        "
             << "- "
             << "ReturnValue"
             << "\n";
        }
        // Output yout(OS);
      }
    }
  }*/
  Output YamlOut(OS);
  YamlOut << Fdff;
}

/*[[nodiscard]] FunctionDataFlowFacts deserialize(llvm::raw_ostream &OS) {
  //Input YamlIn(OS);
}*/
