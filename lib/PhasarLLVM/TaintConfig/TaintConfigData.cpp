#include "phasar/PhasarLLVM/TaintConfig/TaintConfigData.h"

#include "phasar/PhasarLLVM/TaintConfig/TaintConfigBase.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/IO.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/NlohmannLogging.h"

#include "llvm/IR/DebugInfo.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/Support/raw_ostream.h"

#include "nlohmann/json-schema.hpp"
#include "nlohmann/json.hpp"

#include <system_error>

#include <nlohmann/json_fwd.hpp>

namespace psr {

TaintConfigData::TaintConfigData(const std::string &Filepath) {
  std::optional<nlohmann::json> TaintConfig = readJsonFile(Filepath);
  nlohmann::json_schema::json_validator Validator;
  try {
    static const nlohmann::json TaintConfigSchema =
#include "../config/TaintConfigSchema.json"
        ;

    Validator.set_root_schema(TaintConfigSchema); // insert root-schema
  } catch (const std::exception &E) {
    PHASAR_LOG_LEVEL(ERROR,
                     "Validation of schema failed, here is why: " << E.what());
    return;
  }

  // a custom error handler
  class CustomJsonErrorHandler
      : public nlohmann::json_schema::basic_error_handler {
    void error(const nlohmann::json::json_pointer &Pointer,
               const nlohmann::json &Instance,
               const std::string &Message) override {
      nlohmann::json_schema::basic_error_handler::error(Pointer, Instance,
                                                        Message);
      PHASAR_LOG_LEVEL(ERROR, Pointer.to_string()
                                  << "' - '" << Instance << "': " << Message);
    }
  };

  CustomJsonErrorHandler Err;
  Validator.validate(*TaintConfig, Err);
  if (Err) {
    TaintConfig.reset();
    return;
  }

  if (!TaintConfig) {
    llvm::outs()
        << "[TaintConfigData::TaintConfigData()]: TaintConfigData is null!";
    llvm::outs().flush();
    return;
  };

  nlohmann::json Config = *TaintConfig;

  // handle functions
  if (Config.contains("functions")) {
    for (const auto &Func : Config["functions"]) {
      FunctionData Data = FunctionData();
      bool FuncPushBackFlag = false;

      if (Func.contains("name")) {
        Data.Name = Func["name"].get<std::string>();
        FuncPushBackFlag = true;
      }

      if (Func.contains("ret")) {
        Data.ReturnType = Func["ret"];
        FuncPushBackFlag = true;
      }

      if (Func.contains("params") && Func["params"].contains("source")) {
        for (const auto &Curr : Func["params"]["source"]) {
          Data.SourceValues.push_back(Curr.get<int>());
        }
        FuncPushBackFlag = true;
      }

      /*if (Params.contains("sink")) {
        for (const auto &Idx : Params["sink"]) {
          if (Idx.is_number()) {
            if (Idx >= Fun->arg_size()) {
              llvm::errs()
                  << "ERROR: The source-function parameter index is out of "
                     "bounds: "
                  << Idx << "\n";
              continue;
            }
            addTaintCategory(Fun->getArg(Idx), TaintCategory::Sink);
          } else if (Idx.is_string()) {
            const auto Sinks = Idx.get<std::string>();
            if (Sinks == "all") {
              for (const auto &Arg : Fun->args()) {
                addTaintCategory(&Arg, TaintCategory::Sink);
              }
            }
          }
        }
      }*/

      if (Func.contains("params") && Func["params"].contains("sink")) {
        for (const auto &Curr : Func["params"]["sink"]) {
          if (Curr.is_string()) {
            Data.SinkStringValues.push_back(Curr.get<std::string>());
          } else {
            Data.SinkValues.push_back(Curr.get<int>());
          }
        }
        FuncPushBackFlag = true;
      }

      if (Func.contains("params") && Func["params"].contains("sanitizer")) {
        for (const auto &Curr : Func["params"]["sanitizer"]) {

          Data.SanitizerValues.push_back(Curr.get<int>());
        }
        FuncPushBackFlag = true;
      }
      if (FuncPushBackFlag) {
        Functions.push_back(std::move(Data));
      }
    }
  }

  // handle variables
  if (Config.contains("variables")) {
    for (const auto &Var : Config["variables"]) {
      VariableData Data = VariableData();
      bool VarPushBackFlag = false;
      if (Var.contains("line")) {
        Data.Line = Var["line"].get<int>();
        VarPushBackFlag = true;
      }

      if (Var.contains("name")) {
        Data.Name = Var["name"].get<std::string>();
        VarPushBackFlag = true;
      }

      if (Var.contains("scope")) {
        Data.Scope = Var["scope"].get<std::string>();
        VarPushBackFlag = true;
      }

      if (Var.contains("cat")) {
        Data.Cat = Var["cat"].get<std::string>();
        VarPushBackFlag = true;
      }
      if (VarPushBackFlag) {
        Variables.push_back(std::move(Data));
      }
    }
  }
}

std::vector<std::string> TaintConfigData::getAllFunctionNames() const {
  std::vector<std::string> FunctionNames;
  FunctionNames.reserve(Functions.size());

  for (const auto &Func : Functions) {
    FunctionNames.push_back(Func.Name);
  }

  return FunctionNames;
}

std::vector<std::string> TaintConfigData::getAllVariableLines() const {
  std::vector<std::string> VariableLines;
  VariableLines.reserve(Variables.size());

  for (const auto &Var : Variables) {
    VariableLines.push_back(Var.Name);
  }

  return VariableLines;
}
std::vector<std::string> TaintConfigData::getAllVariableCats() const {
  std::vector<std::string> VariableCats;
  VariableCats.reserve(Variables.size());

  for (const auto &Var : Variables) {
    VariableCats.push_back(Var.Name);
  }

  return VariableCats;
}

} // namespace psr