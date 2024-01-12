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

#include "phasar/DataFlow/IfdsIde/EdgeFunction.h"

#include "IDESolver.h"

namespace psr {

class IDESolverSerializer {

public:
  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void
  saveDataInJSONs(const llvm::Twine &Path,
                  const IDESolver<AnalysisDomainTy, Container> &Solver) {
    saveJumpFunctions(Path, Solver);
    saveWorkList(Path, Solver);
    saveEndsummaryTab(Path, Solver);
    saveIncomingTab(Path, Solver);
  }

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
  static void
  saveJumpFunctions(const llvm::Twine &Path,
                    const IDESolver<AnalysisDomainTy, Container> &Solver) {

    IDESolverSerializer Serializer;
    nlohmann::json JSON;
    size_t Index = 0;
    const auto &JumpFnTable = Solver.JumpFn->getNonEmptyReverseLookup();
    JumpFnTable->foreachCell([&Serializer, &Index, &JSON](const auto &Row,
                                                          const auto &Col,
                                                          const auto &Val) {
      JSON["Target"].push_back(getMetaDataID(Row));
      JSON["SourceVal"].push_back(Serializer.getMetaDataIDOrZeroValue(Col));

      for (const auto CurrentVal : Val) {
        JSON["TargetVal"][std::to_string(Index)].push_back(
            Serializer.getMetaDataIDOrZeroValue(CurrentVal.first));
        JSON["EdgeFn"][std::to_string(Index)].push_back(
            Serializer.edgeFunctionToString<
                EdgeFunction<typename AnalysisDomainTy::l_t>>(
                CurrentVal.second));
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

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
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
      JSON["EdgeFn"].push_back(
          Serializer.edgeFunctionToString<
              EdgeFunction<typename AnalysisDomainTy::l_t>>(Curr.second));
    }

    std::error_code EC;
    llvm::raw_fd_ostream FileStream(Path.getSingleStringRef(), EC);

    if (EC) {
      PHASAR_LOG_LEVEL(ERROR, EC.message());
      return;
    }

    FileStream << JSON;
  }

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
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
            Serializer.edgeFunctionToString<
                EdgeFunction<typename AnalysisDomainTy::l_t>>((Val)));
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

  template <typename AnalysisDomainTy,
            typename Container = std::set<typename AnalysisDomainTy::d_t>>
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

  // private:
  IDESolverSerializer() = default;

  std::string getMetaDataIDOrZeroValue(const llvm::Value *V) {
    if (LLVMZeroValue::isLLVMZeroValue(V)) {
      return "zero_value";
    }

    return getMetaDataID(V);
  }

  template <typename AnalysisDomainTy>
  std::string
  edgeFunctionToString(EdgeFunction<typename AnalysisDomainTy::l_t> EdgeFnVal) {
    using l_t = typename AnalysisDomainTy::l_t;

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
