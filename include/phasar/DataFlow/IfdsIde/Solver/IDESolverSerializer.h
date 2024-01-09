#pragma once
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
