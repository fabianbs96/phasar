#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/DataFlow/IfdsIde/IDETabulationProblem.h"
#include "phasar/DataFlow/IfdsIde/Solver/IterativeIDESolver.h"
#include "phasar/DataFlow/Mono/InterMonoProblem.h"
#include "phasar/DataFlow/Mono/Solver/InterMonoSolver.h"
#include "phasar/DataFlow/MonoIfds/MonoIFDSProblem.h"
#include "phasar/DataFlow/MonoIfds/MonoIFDSSolver.h"
#include "phasar/Utils/Macros.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace psr {

struct IRDBTag {};
struct EntrypointsTag {};
struct EntryFunctionsTag {};
struct TypeHierarchyTag {};
struct VFTableProviderTag {};
struct ICFGTag {};
struct AliasInfoTag {};
struct TaintConfigTag {};
struct DataflowAnalysisTag {};
struct FunctionCompressorTag {};
struct CGSCCsTag {};

template <typename ProblemT, typename I>
auto solveDataFlowAnalysisProblem(auto &Pipeline, ProblemT &Problem, I &ICF);

class PipelineRoot {
public:
  [[nodiscard]] PipelineRoot getResult(PipelineRoot /*unused*/) { return {}; };
};

template <typename Base> class SharedPipelineStage;

template <typename StageT, typename Base> class PipelineStage : public Base {
public:
  using result_t = typename StageT::result_t;
  using tag_t = typename StageT::tag_t;

  template <typename ArgsT>
    requires std::is_constructible_v<result_t, ArgsT>
  explicit PipelineStage(StageT /*unused*/, ArgsT Args, Base &&B)
      : Base(std::move(B)), Result(std::make_unique<result_t>(PSR_FWD(Args))) {}

  explicit PipelineStage(StageT /*unused*/, std::unique_ptr<result_t> Result,
                         Base &&B)
      : Base(std::move(B)), Result(std::move(Result)) {}

  using Base::getResult;

  [[nodiscard]] result_t &
  getResult(typename StageT::tag_t /*unused*/) & noexcept {
    return *Result;
  }
  [[nodiscard]] result_t
  getResult(typename StageT::tag_t /*unused*/) && noexcept {
    return std::move(*Result);
  }

  template <typename OtherTag>
  [[nodiscard]] auto getResultOrNull(OtherTag /*unused*/) noexcept {
    if constexpr (requires { this->getResult(OtherTag{}); }) {
      return &getResult(OtherTag{});
    } else {
      return nullptr;
    }
  }

  template <typename NextStageT>
  [[nodiscard]] auto with(NextStageT /*unused*/, auto &&...NextArgs) && {
    using NextT = typename NextStageT::result_t;
    auto Res =
        std::make_unique<NextT>(NextStageT::build(*this, PSR_FWD(NextArgs)...));
    return PipelineStage<NextStageT, PipelineStage>{
        NextStageT{}, std::move(Res), std::move(*this)};
  }

  [[nodiscard]] auto shared() &&;

  auto solve() && {
    auto &Problem = this->getResult(DataflowAnalysisTag{});
    auto &ICF = this->getResult(ICFGTag{});
    return solveDataFlowAnalysisProblem(*this, Problem, ICF);
  }

private:
  std::unique_ptr<result_t> Result;
};

template <typename Base> class SharedPipelineStage {
  struct SharedPipelineRoot
      : public llvm::ThreadSafeRefCountedBase<SharedPipelineRoot>,
        public Base {
    SharedPipelineRoot(Base &&B) : Base(std::move(B)) {}
  };

public:
  SharedPipelineStage(Base &&B)
      : Rc(llvm::makeIntrusiveRefCnt<SharedPipelineRoot>(std::move(B))) {}

  template <typename Tag>
  [[nodiscard]] auto getResult(Tag /*unused*/) noexcept
      -> decltype(std::declval<Base &>().getResult(Tag{})) {
    return Rc->getResult(Tag{});
  }

  template <typename OtherTag>
  [[nodiscard]] auto getResultOrNull(OtherTag /*unused*/) noexcept {
    if constexpr (requires { this->getResult(OtherTag{}); }) {
      return &getResult(OtherTag{});
    } else {
      return nullptr;
    }
  }

  template <typename NextStageT>
  [[nodiscard]] auto with(NextStageT /*unused*/, auto &&...NextArgs) {
    using NextT = typename NextStageT::result_t;
    auto Res =
        std::make_unique<NextT>(NextStageT::build(*Rc, PSR_FWD(NextArgs)...));
    auto Cpy = *this;
    return PipelineStage<NextStageT, SharedPipelineStage>{
        NextStageT{}, std::move(Res), std::move(Cpy)};
  }

  [[nodiscard]] auto shared() { return *this; }

  auto solve() {
    auto &Problem = this->getResult(DataflowAnalysisTag{});
    auto &ICF = this->getResult(ICFGTag{});
    return solveDataFlowAnalysisProblem(*Rc, Problem, ICF);
  }

private:
  llvm::IntrusiveRefCntPtr<SharedPipelineRoot> Rc{};
};

template <typename Tag, typename Base>
inline auto PipelineStage<Tag, Base>::shared() && {
  return SharedPipelineStage{std::move(*this)};
}

template <typename ProblemT, typename I>
auto solveDataFlowAnalysisProblem(auto &Pipeline, ProblemT &Problem, I &ICF) {
  if constexpr (std::derived_from<
                    ProblemT, IDETabulationProblem<
                                  typename ProblemT::ProblemAnalysisDomain>>) {
    return IterativeIDESolver(&Problem, &ICF).solve();
  } else if (monoifds::MonoIFDSProblem<ProblemT>) {
    auto Solver = monoifds::MonoIFDSSolver(&Problem, &ICF);
    auto Functions = Pipeline.getResultOrNull(FunctionCompressorTag{});
    auto SCCs = Pipeline.getResultOrNull(CGSCCsTag{});

    if constexpr (!std::is_null_pointer_v<decltype(Functions)>) {
      Solver.setFunctionCompressor(Functions);
      if constexpr (!std::is_null_pointer_v<decltype(SCCs)>) {
        Solver.setCGSCCs(SCCs);
      }
    }

    return std::move(Solver).solve();
  } else {
    static_assert(
        InterMonoAnalysisDomain<typename ProblemT::ProblemAnalysisDomain>);
    InterMonoSolver Solver(Problem);
    Solver.solve();
    return std::move(Solver).getAnalysis();
  }
}

} // namespace psr
