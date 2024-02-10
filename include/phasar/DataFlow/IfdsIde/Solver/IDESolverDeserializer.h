/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * IDESolverDeserializer.h
 *
 *  Created on: 11.01.2024
 *      Author: mxHuber
 */

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERDESERIALIZER_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERDESERIALIZER_H

#include "phasar/DataFlow/IfdsIde/Solver/IDESolver.h"

namespace psr {

class IDESolverDeserializer {

public:
  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void
  loadDataFromJSONs(const LLVMProjectIRDB &IRDB, const llvm::Twine &PathToJSONs,
                    IDESolver<AnalysisDomainTy, Container> &Solver) {
    loadJumpFunctions(IRDB, PathToJSONs, Solver);
    loadWorkList(IRDB, PathToJSONs, Solver);
    loadEndsummaryTab(IRDB, PathToJSONs, Solver);
    loadIncomingTab(IRDB, PathToJSONs, Solver);
  }

  // std::shared_ptr<JumpFunctions<AnalysisDomainTy, Container>> JumpFn;
  // Table<n_t, d_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>>
  //    NonEmptyReverseLookup;

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void
  loadJumpFunctions(const LLVMProjectIRDB &IRDB, const llvm::Twine &PathToFile,
                    IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile + "JumpFunctions.json");

    std::vector<std::string> SourceValStrs;
    std::vector<std::string> TargetStrs;
    std::vector<std::vector<std::string>> TargetValStrs(
        JSON["TargetVal"].size());
    std::vector<std::vector<std::string>> EdgeFnStrs(JSON["EdgeFn"].size());
    IDESolverDeserializer Deserializer;

    // SourceVals
    for (const auto &CurrSourceVal : JSON["SourceVal"]) {
      SourceValStrs.push_back(CurrSourceVal);
    }

    // Targets
    for (const auto &CurrTarget : JSON["Target"]) {
      TargetStrs.push_back(CurrTarget);
    }

    // TargetVals
    size_t TargetValsIndex = 0;
    for (const auto &TargetValElement : JSON["TargetVal"]) {
      for (const auto &CurrVal : TargetValElement) {
        TargetValStrs[TargetValsIndex].push_back(CurrVal);
      }
      TargetValsIndex++;
    }

    // EdgeFns
    size_t EdgeFnsIndex = 0;
    for (const auto &EdgeFnElement : JSON["EdgeFn"]) {
      for (const auto &CurrVal : EdgeFnElement) {
        EdgeFnStrs[EdgeFnsIndex].push_back(CurrVal);
      }
      EdgeFnsIndex++;
    }

    using l_t = typename AnalysisDomainTy::l_t;
    using n_t = typename AnalysisDomainTy::n_t;
    using d_t = typename AnalysisDomainTy::d_t;

    for (size_t Index = 0; Index < TargetStrs.size(); Index++) {
      d_t SourceVal =
          Deserializer.getDTFromMetaDataId(IRDB, SourceValStrs[Index], Solver);
      n_t Target = Deserializer.getNTFromMetaDataId(IRDB, TargetStrs[Index]);

      for (size_t InnerIndex = 0; InnerIndex < EdgeFnStrs[Index].size();
           InnerIndex++) {
        d_t TargetVal = Deserializer.getDTFromMetaDataId(
            IRDB, TargetValStrs[Index][InnerIndex], Solver);
        EdgeFunction<l_t> EdgeFunc =
            Deserializer.stringToEdgeFunction<EdgeFunction<l_t>>(
                EdgeFnStrs[Index][InnerIndex]);

        Solver.JumpFn->addFunction(SourceVal, Target, TargetVal, EdgeFunc);
      }
    }
  }

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void loadWorkList(const LLVMProjectIRDB &IRDB,
                           const llvm::Twine &PathToFile,
                           IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile + "WorkList.json");
    IDESolverDeserializer Deserializer;

    using l_t = typename AnalysisDomainTy::l_t;
    using n_t = typename AnalysisDomainTy::n_t;
    using d_t = typename AnalysisDomainTy::d_t;

    std::vector<PathEdge<n_t, d_t>> WorkListPathEdges;
    std::vector<EdgeFunction<l_t>> WorkListEdgeFunctions;

    size_t Index = 0;
    for (const auto &PathEdgeJSON : JSON["PathEdge"]["DSource"]) {
      std::string DSourceTest = JSON["PathEdge"]["DSource"][Index];
      std::string TargetTest = JSON["PathEdge"]["Target"][Index];
      std::string DTargetTest = JSON["PathEdge"]["DTarget"][Index];

      d_t DSource = Deserializer.getDTFromMetaDataId(IRDB, DSourceTest, Solver);
      n_t Target = Deserializer.getNTFromMetaDataId(IRDB, TargetTest);
      d_t DTarget = Deserializer.getDTFromMetaDataId(IRDB, DTargetTest, Solver);

      PathEdge PathEdgeToEmplace(DSource, Target, DTarget);

      WorkListPathEdges.push_back(PathEdgeToEmplace);
      Index++;
    }

    for (const auto &EdgeFnJSON : JSON["EdgeFn"]) {
      EdgeFunction EdgeFunctionToEmplace =
          (Deserializer.stringToEdgeFunction<EdgeFunction<l_t>>(EdgeFnJSON));
      WorkListEdgeFunctions.push_back(std::move(EdgeFunctionToEmplace));
    }

    assert(WorkListPathEdges.size() == WorkListEdgeFunctions.size());

    for (size_t Index = 0; Index < WorkListPathEdges.size(); Index++) {
      Solver.WorkList.push_back(
          {WorkListPathEdges[Index], WorkListEdgeFunctions[Index]});
    }
  }

  // Table<n_t, d_t, Table<n_t, d_t, EdgeFunction<l_t>>>

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void
  loadEndsummaryTab(const LLVMProjectIRDB &IRDB, const llvm::Twine &PathToFile,
                    IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile + "EndsummaryTab.json");
    IDESolverDeserializer Deserializer;

    using l_t = typename AnalysisDomainTy::l_t;
    using n_t = typename AnalysisDomainTy::n_t;
    using d_t = typename AnalysisDomainTy::d_t;

    std::vector<n_t> NTValues;
    std::vector<d_t> DTValues;
    std::vector<Table<n_t, d_t, EdgeFunction<l_t>>> InnerTables{};

    for (const auto &CurrentNTVal : JSON["n_t"]) {
      NTValues.push_back(
          Deserializer.getNTFromMetaDataId(IRDB, std::string(CurrentNTVal)));
    }

    for (const auto &CurrentDTVal : JSON["d_t"]) {
      DTValues.push_back(Deserializer.getDTFromMetaDataId(
          IRDB, std::string(CurrentDTVal), Solver));
    }

    for (size_t Index = 0; Index < JSON["n_t"].size(); Index++) {
      Table<n_t, d_t, EdgeFunction<l_t>> CurrentInnerTable;

      for (const auto &CurrenInnerTable : JSON[std::to_string(Index)]) {
        n_t InnerNTVal = Deserializer.getNTFromMetaDataId(
            IRDB, std::string(CurrenInnerTable["n_t"]));
        d_t InnerDTVal = Deserializer.getDTFromMetaDataId(
            IRDB, std::string(CurrenInnerTable["d_t"]), Solver);
        EdgeFunction<l_t> InnerEdgeFn =
            Deserializer.stringToEdgeFunction<EdgeFunction<l_t>>(
                std::string(CurrenInnerTable["EdgeFn"]));

        CurrentInnerTable.insert(InnerNTVal, InnerDTVal, InnerEdgeFn);
      }

      Solver.EndsummaryTab.insert(NTValues[Index], DTValues[Index],
                                  std::move(CurrentInnerTable));
    }
  }

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void loadIncomingTab(const LLVMProjectIRDB &IRDB,
                              const llvm::Twine &PathToFile,
                              IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile + "IncomingTab.json");
    IDESolverDeserializer Deserializer;

    using n_t = typename AnalysisDomainTy::n_t;
    using d_t = typename AnalysisDomainTy::d_t;

    for (size_t Index = 0; JSON.contains("Entry" + std::to_string(Index));
         Index++) {
      std::string CurrentName = "Entry" + std::to_string(Index);

      std::string NTString = JSON[CurrentName]["n_t"];
      std::string DTString = JSON[CurrentName]["d_t"];

      n_t NTVal = Deserializer.getNTFromMetaDataId(IRDB, NTString);
      d_t DTVal = Deserializer.getDTFromMetaDataId(IRDB, DTString, Solver);

      std::map<n_t, Container> MapData;

      for (size_t InnerIndex = 0;
           JSON[CurrentName].contains("map" + std::to_string(InnerIndex));
           InnerIndex++) {
        std::string InnerName = "map" + std::to_string(InnerIndex);

        std::string InnerNTStr = JSON[CurrentName][InnerName]["n_t"];
        n_t InnerNTVal = Deserializer.getNTFromMetaDataId(IRDB, InnerNTStr);

        std::set<const llvm::Value *> ContainerVals;
        for (size_t Index = 0; JSON[CurrentName][InnerName].contains(
                 "Container" + std::to_string(Index));
             Index++) {
          std::string ContainerName = "Container" + std::to_string(InnerIndex);

          std::string InnerDTStr = JSON[CurrentName][InnerName][ContainerName];
          d_t InnerDTVal =
              Deserializer.getDTFromMetaDataId(IRDB, InnerDTStr, Solver);

          ContainerVals.insert(InnerDTVal);
        }

        MapData.try_emplace(InnerNTVal, ContainerVals);
      }

      Solver.IncomingTab.insert(NTVal, DTVal, std::move(MapData));
    }
  }

  // private:
  IDESolverDeserializer() = default;

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  const llvm::Value *
  getDTFromMetaDataId(const LLVMProjectIRDB &IRDB, llvm::StringRef Id,
                      const IDESolver<AnalysisDomainTy, Container> &Solver) {
    const auto *Value = fromMetaDataIdOrZeroValue(IRDB, Id, Solver);

    if (!Value) {
      llvm::report_fatal_error(
          llvm::Twine("[getDTFromMetaDataId()]: Value is null"));
    }

    return Value;
  }

  const llvm::Instruction *getNTFromMetaDataId(const LLVMProjectIRDB &IRDB,
                                               llvm::StringRef Id) {
    const auto *Value = fromMetaDataId(IRDB, Id);

    if (!Value) {
      llvm::report_fatal_error(
          llvm::Twine("[getNTFromMetaDataId()]: Value is null"));
    }

    if (const auto *Instr = llvm::dyn_cast_or_null<llvm::Instruction>(Value)) {
      return Instr;
    }

    llvm::report_fatal_error("Value could not be casted to llvm::Instruction");
  }

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  const llvm::Value *fromMetaDataIdOrZeroValue(
      const LLVMProjectIRDB &IRDB, llvm::StringRef Id,
      const IDESolver<AnalysisDomainTy, Container> &Solver) {
    if (Id == "zero_value") {
      return Solver.ZeroValue;
    }

    return fromMetaDataId(IRDB, Id);
  }

  template <typename AnalysisDomainTy>
  EdgeFunction<typename AnalysisDomainTy::l_t>
  stringToEdgeFunction(const std::string &EdgeFnAsStr) {
    if (EdgeFnAsStr == "AllBottom") {
      return AllBottom<typename AnalysisDomainTy::l_t>();
    }

    if (EdgeFnAsStr == "EdgeIdentity") {
      return EdgeIdentity<typename AnalysisDomainTy::l_t>();
    }

    llvm::report_fatal_error(llvm::Twine("unknown case"));
  }
};

} // namespace psr

#endif
