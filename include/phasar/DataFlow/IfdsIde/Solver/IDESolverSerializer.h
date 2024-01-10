/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * IDESolverSerializer.h
 *
 *  Created on: 09.01.2024
 *      Author: mxHuber
 */

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERSERIALIZER_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_IDESOLVERSERIALIZER_H

#include "IDESolver.h"

namespace psr {

template <typename AnalysisDomainTy,
          typename Container = std::set<typename AnalysisDomainTy::d_t>>
class IDESolverSerializer {

public:
  using ProblemTy = IDETabulationProblem<AnalysisDomainTy, Container>;
  using container_type = typename ProblemTy::container_type;
  using FlowFunctionPtrType = typename ProblemTy::FlowFunctionPtrType;

  using l_t = typename AnalysisDomainTy::l_t;
  using n_t = typename AnalysisDomainTy::n_t;
  using i_t = typename AnalysisDomainTy::i_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using f_t = typename AnalysisDomainTy::f_t;
  using t_t = typename AnalysisDomainTy::t_t;
  using v_t = typename AnalysisDomainTy::v_t;

  static void
  saveDataInJSONs(const llvm::Twine &Path,
                  const IDESolver<AnalysisDomainTy, Container> &Solver) {
    saveJumpFunctions(Path, Solver);
    saveWorkList(Path, Solver);
    saveEndsummaryTab(Path, Solver);
    saveIncomingTab(Path, Solver);
  }

  static void
  saveJumpFunctions(const llvm::Twine &Path,
                    const IDESolver<AnalysisDomainTy, Container> &Solver) {
    {

      IDESolverSerializer Serializer;
      nlohmann::json JSON;
      size_t Index = 0;

      Solver.JumpFn->foreachCell([&Serializer, &Index, &JSON](const auto &Row,
                                                              const auto &Col,
                                                              const auto &Val) {
        JSON["Target"].push_back(getMetaDataID(Row));
        JSON["SourceVal"].push_back(Serializer.getMetaDataIDOrZeroValue(Col));

        for (const auto CurrentVal : Val) {
          JSON["TargetVal"][std::to_string(Index)].push_back(
              Serializer.getMetaDataIDOrZeroValue(CurrentVal.first));
          JSON["EdgeFn"][std::to_string(Index)].push_back(
              Serializer.edgeFunctionToString(CurrentVal.second));
        }
        Index++;
      });

      std::error_code EC;
      llvm::raw_fd_ostream FileStream(Path.getSingleStringRef(), EC);

      if (EC) {
        PHASAR_LOG_LEVEL(ERROR, EC.message());
        return;
      }

      FileStream << JSON;
    }
  }

  static void
  saveWorkList(const llvm::Twine &Path,
               const IDESolver<AnalysisDomainTy, Container> &Solver) {
    if (Solver.WorkList.empty()) {
      llvm::report_fatal_error("Worklist empty");
    }

    IDESolverSerializer Serializer;
    nlohmann::json JSON;

    for (const auto &Curr : Solver.WorkList) {
      // PathEdge serialization
      auto [PathEdgeFirst, PathEdgeSecond, PathEdgeThird] = Curr.first.get();

      JSON["PathEdge"]["DSource"].push_back(
          Serializer.getMetaDataIDOrZeroValue(PathEdgeFirst));
      JSON["PathEdge"]["Target"].push_back(
          Serializer.getMetaDataIDOrZeroValue(PathEdgeSecond));
      JSON["PathEdge"]["DTarget"].push_back(
          Serializer.getMetaDataIDOrZeroValue(PathEdgeThird));
      JSON["EdgeFn"].push_back(Serializer.edgeFunctionToString(Curr.second));
    }

    std::error_code EC;
    llvm::raw_fd_ostream FileStream(Path.getSingleStringRef(), EC);

    if (EC) {
      PHASAR_LOG_LEVEL(ERROR, EC.message());
      return;
    }

    FileStream << JSON;
  }

  static void
  saveEndsummaryTab(const llvm::Twine &Path,
                    const IDESolver<AnalysisDomainTy, Container> &Solver) {
    IDESolverSerializer Serializer;
    nlohmann::json JSON;
    size_t Index = 0;

    Solver.EndsummaryTab.foreachCell([&Serializer, &JSON,
                                      &Index](const auto &Row, const auto &Col,
                                              const auto &Val) {
      JSON["n_t"].push_back(getMetaDataID(Row));
      JSON["d_t"].push_back(Serializer.getMetaDataIDOrZeroValue(Col));

      Val.foreachCell([&Serializer, &JSON, &Index](
                          const auto &Row, const auto &Col, const auto &Val) {
        std::string InnerTableName = "InnerTable_" + std::to_string(Index);

        JSON[InnerTableName]["n_t"].push_back(getMetaDataID(Row));
        JSON[InnerTableName]["d_t"].push_back(
            Serializer.getMetaDataIDOrZeroValue(Col));
        JSON[InnerTableName]["EdgeFn"].push_back(
            Serializer.edgeFunctionToString(Val));
      });

      Index++;
    });

    std::error_code EC;
    llvm::raw_fd_ostream FileStream(Path.getSingleStringRef(), EC);
    if (EC) {
      PHASAR_LOG_LEVEL(ERROR, EC.message());
      return;
    }

    FileStream << JSON;
  }

  static void
  saveIncomingTab(const llvm::Twine &Path,
                  const IDESolver<AnalysisDomainTy, Container> &Solver) {
    IDESolverSerializer Serializer;
    nlohmann::json JSON;

    Solver.IncomingTab.foreachCell([&Serializer, &JSON](const auto &Row,
                                                        const auto &Col,
                                                        const auto &Val) {
      JSON["n_t"].push_back(getMetaDataID(Row));
      JSON["d_t"].push_back(Serializer.getMetaDataIDOrZeroValue(Col));

      size_t MapIndex = 0;
      for (const auto &Curr : Val) {
        std::string MapEntryName = "map" + std::to_string(MapIndex++);
        JSON[MapEntryName]["n_t"] = getMetaDataID(Curr.first);

        for (const auto &ContainerElement : Curr.second) {
          JSON[MapEntryName]["d_t"].push_back(
              Serializer.getMetaDataIDOrZeroValue(ContainerElement));
        }
      }
    });

    std::error_code EC;
    llvm::raw_fd_ostream FileStream(Path.getSingleStringRef(), EC);

    if (EC) {
      PHASAR_LOG_LEVEL(ERROR, EC.message());
    }

    FileStream << JSON;
  }

  void loadDataFromJSONs(const LLVMProjectIRDB &IRDB,
                         const llvm::Twine &PathToJSONs,
                         const IDESolver<AnalysisDomainTy, Container> &Solver) {
    std::string PathToJumpFunctionJSON =
        PathToJSONs.str() + "JumpFunctions.json";
    loadJumpFunctions(IRDB, PathToJumpFunctionJSON);

    std::string PathToWorkListJSON = PathToJSONs.str() + "WorkList.json";
    loadWorkList(IRDB, PathToWorkListJSON);

    std::string PathToEndsummaryTabJSON =
        PathToJSONs.str() + "EndsummaryTab.json";
    loadEndsummaryTab(IRDB, PathToEndsummaryTabJSON);

    std::string PathToIncomingTabJSON = PathToJSONs.str() + "IncomingTab.json";
    loadIncomingTab(IRDB, PathToIncomingTabJSON);
  }

  // std::shared_ptr<JumpFunctions<AnalysisDomainTy, Container>> JumpFn;
  // Table<n_t, d_t, llvm::SmallVector<std::pair<d_t, EdgeFunction<l_t>>, 1>>
  //    NonEmptyReverseLookup;
  void loadJumpFunctions(const LLVMProjectIRDB &IRDB,
                         const llvm::Twine &PathToFile,
                         const IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile);

    std::vector<std::string> SourceValStrs;
    std::vector<std::string> TargetStrs;
    std::vector<std::vector<std::string>> TargetValStrs(
        JSON["TargetVal"].size());
    std::vector<std::vector<std::string>> EdgeFnStrs(JSON["EdgeFn"].size());
    IDESolverSerializer Deserializer;

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

    for (size_t Index = 0; Index < TargetStrs.size(); Index++) {
      d_t SourceVal =
          Deserializer.getDTFromMetaDataId(IRDB, SourceValStrs[Index]);
      n_t Target = Deserializer.getNTFromMetaDataId(IRDB, TargetStrs[Index]);

      for (size_t InnerIndex = 0; InnerIndex < EdgeFnStrs[Index].size();
           InnerIndex++) {
        d_t TargetVal = Deserializer.getDTFromMetaDataId(
            IRDB, TargetValStrs[Index][InnerIndex]);
        EdgeFunction<l_t> EdgeFunc =
            stringToEdgeFunction(EdgeFnStrs[Index][InnerIndex]);

        Solver.JumpFn->addFunction(SourceVal, Target, TargetVal, EdgeFunc);
      }
    }
  }

  void loadWorkList(const LLVMProjectIRDB &IRDB, const llvm::Twine &PathToFile,
                    const IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile);
    IDESolverSerializer Deserializer;

    std::vector<PathEdge<n_t, d_t>> WorkListPathEdges;
    std::vector<EdgeFunction<l_t>> WorkListEdgeFunctions;

    size_t Index = 0;
    for (const auto &PathEdgeJSON : JSON["PathEdge"]["DSource"]) {
      std::string DSourceTest = JSON["PathEdge"]["DSource"][Index];
      std::string TargetTest = JSON["PathEdge"]["Target"][Index];
      std::string DTargetTest = JSON["PathEdge"]["DTarget"][Index];

      d_t DSource = Deserializer.getDTFromMetaDataId(IRDB, DSourceTest);
      n_t Target = Deserializer.getNTFromMetaDataId(IRDB, TargetTest);
      d_t DTarget = Deserializer.getDTFromMetaDataId(IRDB, DTargetTest);

      PathEdge PathEdgeToEmplace(DSource, Target, DTarget);

      WorkListPathEdges.push_back(PathEdgeToEmplace);
      Index++;
    }

    for (const auto &EdgeFnJSON : JSON["EdgeFn"]) {
      EdgeFunction EdgeFunctionToEmplace = (stringToEdgeFunction(EdgeFnJSON));
      WorkListEdgeFunctions.push_back(std::move(EdgeFunctionToEmplace));
    }

    assert(WorkListPathEdges.size() == WorkListEdgeFunctions.size());

    for (size_t Index = 0; Index < WorkListPathEdges.size(); Index++) {
      Solver.WorkList.push_back(
          {WorkListPathEdges[Index], WorkListEdgeFunctions[Index]});
    }
  }

  // Table<n_t, d_t, Table<n_t, d_t, EdgeFunction<l_t>>>
  void loadEndsummaryTab(const LLVMProjectIRDB &IRDB,
                         const llvm::Twine &PathToFile,
                         const IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile);
    IDESolverSerializer Deserializer;

    std::vector<n_t> NTValues;
    std::vector<d_t> DTValues;
    std::vector<Table<n_t, d_t, EdgeFunction<l_t>>> InnerTables{};

    for (const auto &CurrentNTVal : JSON["n_t"]) {
      NTValues.push_back(
          Deserializer.getNTFromMetaDataId(IRDB, std::string(CurrentNTVal)));
    }

    for (const auto &CurrentDTVal : JSON["d_t"]) {
      DTValues.push_back(
          Deserializer.getDTFromMetaDataId(IRDB, std::string(CurrentDTVal)));
    }

    for (size_t Index = 0; Index < JSON["n_t"].size(); Index++) {
      Table<n_t, d_t, EdgeFunction<l_t>> CurrentInnerTable;

      for (const auto &CurrenInnerTable : JSON[std::to_string(Index)]) {
        n_t InnerNTVal = Deserializer.getNTFromMetaDataId(
            IRDB, std::string(CurrenInnerTable["n_t"]));
        d_t InnerDTVal = Deserializer.getDTFromMetaDataId(
            IRDB, std::string(CurrenInnerTable["d_t"]));
        EdgeFunction<l_t> InnerEdgeFn = Deserializer.stringToEdgeFunction(
            std::string(CurrenInnerTable["EdgeFn"]));

        CurrentInnerTable.insert(InnerNTVal, InnerDTVal, InnerEdgeFn);
      }

      Solver.EndsummaryTab.insert(NTValues[Index], DTValues[Index],
                                  std::move(CurrentInnerTable));
    }
  }

  void loadIncomingTab(const LLVMProjectIRDB &IRDB,
                       const llvm::Twine &PathToFile,
                       const IDESolver<AnalysisDomainTy, Container> &Solver) {
    nlohmann::json JSON = readJsonFile(PathToFile);
    IDESolverSerializer Deserializer;

    for (size_t Index = 0; JSON.contains("Entry" + std::to_string(Index));
         Index++) {
      std::string CurrentName = "Entry" + std::to_string(Index);

      std::string NTString = JSON[CurrentName]["n_t"];
      std::string DTString = JSON[CurrentName]["d_t"];

      n_t NTVal = Deserializer.getNTFromMetaDataId(IRDB, NTString);
      d_t DTVal = Deserializer.getDTFromMetaDataId(IRDB, DTString);

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
          d_t InnerDTVal = Deserializer.getDTFromMetaDataId(IRDB, InnerDTStr);

          ContainerVals.insert(InnerDTVal);
        }

        MapData.try_emplace(InnerNTVal, ContainerVals);
      }

      Solver.IncomingTab.insert(NTVal, DTVal, std::move(MapData));
    }
  }

private:
  friend class IDESolver<AnalysisDomainTy, Container>;
  IDESolverSerializer() = default;

  std::string getMetaDataIDOrZeroValue(const llvm::Value *V) {
    if (LLVMZeroValue::isLLVMZeroValue(V)) {
      return "zero_value";
    }

    return getMetaDataID(V);
  }

  EdgeFunction<l_t> stringToEdgeFunction(const std::string &EdgeFnAsStr) {
    if (EdgeFnAsStr == "AllBottom") {
      return AllBottom<l_t>();
    }

    if (EdgeFnAsStr == "EdgeIdentity") {
      return EdgeIdentity<l_t>();
    }

    llvm::report_fatal_error(llvm::Twine("unknown case"));
  }

  std::string edgeFunctionToString(EdgeFunction<l_t> EdgeFnVal) {
    if (llvm::isa<AllBottom<l_t>>(EdgeFnVal)) {
      return "AllBottom";
    }

    if (llvm::isa<EdgeIdentity<l_t>>(EdgeFnVal)) {
      return "EdgeIdentity";
    }

    llvm::report_fatal_error("ERROR: unknown case");
  }
};

} // namespace psr

#endif
