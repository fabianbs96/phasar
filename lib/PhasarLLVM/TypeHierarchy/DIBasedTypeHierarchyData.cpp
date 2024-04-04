/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Maximilian Leo Huber and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchyData.h"

#include "phasar/Utils/IO.h"
#include "phasar/Utils/NlohmannLogging.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>
#include <string>
#include <utility>

namespace psr {

static DIBasedTypeHierarchyData getDataFromJson(const nlohmann::json &Json) {
  DIBasedTypeHierarchyData Data;

  for (const auto &[Key, Value] :
       Json["NameToType"].get<nlohmann::json::object_t>()) {
    Data.NameToType.try_emplace(Key, Value.get<std::string>());
  }

  for (const auto &[Key, Value] :
       Json["TypeToVertex"].get<nlohmann::json::object_t>()) {
    Data.TypeToVertex.try_emplace(Key, Value.get<size_t>());
  }

  for (const auto &Value : Json["VertexTypes"]) {
    Data.VertexTypes.push_back(Value.get<std::string>());
  }

  for (const auto &CurrPair : Json["TransitiveDerivedIndex"]) {
    Data.TransitiveDerivedIndex.emplace_back(CurrPair[0].get<uint32_t>(),
                                             CurrPair[1].get<uint32_t>());
  }

  for (const auto &Value : Json["Hierarchy"]) {
    Data.Hierarchy.push_back(Value.get<std::string>());
  }

  for (const auto &CurrVTable : Json["VTables"]) {
    auto &DataPos = Data.VTables.emplace_back();

    for (const auto &CurrVFunc : CurrVTable) {
      DataPos.push_back(CurrVFunc.get<std::string>());
    }
  }

  return Data;
}

void DIBasedTypeHierarchyData::printAsJson(llvm::raw_ostream &OS) {
  nlohmann::json JSON;

  for (const auto &Curr : NameToType) {
    JSON["NameToType"][Curr.getKey()] = Curr.getValue();
  }

  for (const auto &Curr : TypeToVertex) {
    JSON["TypeToVertex"][Curr.getKey()] = Curr.getValue();
  }

  for (const auto &Curr : VertexTypes) {
    JSON["VertexTypes"].push_back(Curr);
  }

  /// TODO: geschachteltes Array verwenden, statt Counter
  int Number = 0;
  for (const auto &Curr : TransitiveDerivedIndex) {
    JSON["TransitiveDerivedIndex"][Number].push_back(Curr.first);
    JSON["TransitiveDerivedIndex"][Number++].push_back(Curr.second);
  }

  for (const auto &Curr : Hierarchy) {
    JSON["Hierarchy"].push_back(Curr);
  }

  for (const auto &CurrVTable : VTables) {
    auto &DataPos = JSON["VTables"].emplace_back();

    for (const auto &CurrVFunc : CurrVTable) {
      DataPos.push_back(CurrVFunc);
    }
  }

  OS << JSON;
}

DIBasedTypeHierarchyData
DIBasedTypeHierarchyData::deserializeJson(const llvm::Twine &Path) {
  return getDataFromJson(readJsonFile(Path));
}

DIBasedTypeHierarchyData
DIBasedTypeHierarchyData::loadJsonString(llvm::StringRef JsonAsString) {
  nlohmann::json Data =
      nlohmann::json::parse(JsonAsString.begin(), JsonAsString.end());
  return getDataFromJson(Data);
}

} // namespace psr