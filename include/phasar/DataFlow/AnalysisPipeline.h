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
#include <tuple>
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
struct CGSCCCallersTag {};
struct UsedGlobalsTag {};

template <typename T, typename TagT>
concept StageRequireSingle =
    requires(T &PrevPipeline, TagT Tag) { PrevPipeline.getResult(Tag); };

template <typename T, typename... TagsT>
concept StageRequire = (StageRequireSingle<T, TagsT> && ...);

template <typename ProblemT, typename I>
auto solveDataFlowAnalysisProblem(auto &Pipeline, ProblemT &Problem, I &ICF);

template <typename Impl> class SharedPipeline;

namespace detail {

// Marker for "no shared prefix": all stage results are owned locally.
struct EmptyPrefix {};

// Index of the stage in StageTs... tagged Tag, or -1 if none. The LAST match
// wins (e.g. defaultPipeline() tags two ICFGStage runs, RTA then VTA, with
// the same ICFGTag; the later one must shadow the earlier one).
template <typename Tag, typename... StageTs>
constexpr int indexOfTag() noexcept {
  constexpr bool Matches[] = {std::same_as<typename StageTs::tag_t, Tag>...,
                              false};
  int Result = -1;
  for (size_t Idx = 0; Idx < sizeof...(StageTs); ++Idx) {
    if (Matches[Idx]) {
      Result = static_cast<int>(Idx);
    }
  }
  return Result;
}

// Flat pipeline storage: all stage results live in one std::tuple, and the
// type only grows by appending to StageTs..., so an N-stage pipeline has
// type PipelineImpl<Prefix, S1, ..., SN> (one level) instead of an N-deep
// chain of nested PipelineStage<SN, PipelineStage<SN-1, ...>>.
//
// Prefix is EmptyPrefix (own everything) or a SharedPipeline<...> (stages
// before the branch point live there and are only looked up through Prefix).
template <typename Prefix, typename... StageTs> class PipelineImpl {
public:
  using ResultsTuple =
      std::tuple<std::unique_ptr<typename StageTs::result_t>...>;

  PipelineImpl() = default;
  explicit PipelineImpl(Prefix Prev, ResultsTuple Results)
      : Prev(std::move(Prev)), Results(std::move(Results)) {}

  template <typename Tag>
    requires(indexOfTag<Tag, StageTs...>() >= 0 ||
             StageRequireSingle<Prefix, Tag>)
  [[nodiscard]] auto &getResult(Tag /*unused*/) & noexcept {
    if constexpr (indexOfTag<Tag, StageTs...>() >= 0) {
      return *std::get<indexOfTag<Tag, StageTs...>()>(Results);
    } else {
      return Prev.getResult(Tag{});
    }
  }

  template <typename Tag>
    requires(indexOfTag<Tag, StageTs...>() >= 0 ||
             requires(Prefix &P) { P.getResult(Tag{}); })
  [[nodiscard]] auto getResult(Tag /*unused*/) && noexcept {
    if constexpr (indexOfTag<Tag, StageTs...>() >= 0) {
      return std::move(*std::get<indexOfTag<Tag, StageTs...>()>(Results));
    } else {
      return Prev.getResult(Tag{});
    }
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
    return PipelineImpl<Prefix, StageTs..., NextStageT>(
        std::move(Prev),
        std::tuple_cat(std::move(Results), std::make_tuple(std::move(Res))));
  }

  // Like with(), but builds the result directly from Args instead of via
  // NextStageT::build(); used to seed a pipeline's first stage.
  template <typename NextStageT, typename ArgsT>
    requires std::is_constructible_v<typename NextStageT::result_t, ArgsT>
  [[nodiscard]] auto withValue(NextStageT /*unused*/, ArgsT Args) && {
    using NextT = typename NextStageT::result_t;
    auto Res = std::make_unique<NextT>(PSR_FWD(Args));
    return PipelineImpl<Prefix, StageTs..., NextStageT>(
        std::move(Prev),
        std::tuple_cat(std::move(Results), std::make_tuple(std::move(Res))));
  }

  [[nodiscard]] auto shared() && {
    return SharedPipeline<PipelineImpl>(std::move(Results));
  }

  auto solve() && {
    auto &Problem = this->getResult(DataflowAnalysisTag{});
    auto &ICF = this->getResult(ICFGTag{});
    return solveDataFlowAnalysisProblem(*this, Problem, ICF);
  }

private:
  [[no_unique_address]] Prefix Prev;
  ResultsTuple Results;
};

} // namespace detail

// Owns all of its stage results directly. Move-only: each .with() call
// consumes the pipeline and returns a new, longer one.
template <typename... StageTs>
using Pipeline = detail::PipelineImpl<detail::EmptyPrefix, StageTs...>;

// Reference-counted, copyable handle to a pipeline prefix, created via
// Pipeline::shared(). .with() can be called repeatedly on it to branch off
// several continuations that share the same already-computed prefix.
template <typename Impl> class SharedPipeline {
  struct Root : public llvm::ThreadSafeRefCountedBase<Root> {
    Impl P;
    explicit Root(Impl P) : P(std::move(P)) {}
  };

public:
  explicit SharedPipeline(typename Impl::ResultsTuple Results)
      : Rc(llvm::makeIntrusiveRefCnt<Root>(
            Impl(detail::EmptyPrefix{}, std::move(Results)))) {}

  template <typename Tag>
    requires StageRequireSingle<Impl, Tag>
  [[nodiscard]] decltype(auto) getResult(Tag /*unused*/) noexcept {
    return Rc->P.getResult(Tag{});
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
        std::make_unique<NextT>(NextStageT::build(Rc->P, PSR_FWD(NextArgs)...));
    return detail::PipelineImpl<SharedPipeline, NextStageT>(
        *this, std::make_tuple(std::move(Res)));
  }

  [[nodiscard]] auto shared() { return *this; }

  auto solve() {
    auto &Problem = Rc->P.getResult(DataflowAnalysisTag{});
    auto &ICF = Rc->P.getResult(ICFGTag{});
    return solveDataFlowAnalysisProblem(Rc->P, Problem, ICF);
  }

private:
  llvm::IntrusiveRefCntPtr<Root> Rc{};
};

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
