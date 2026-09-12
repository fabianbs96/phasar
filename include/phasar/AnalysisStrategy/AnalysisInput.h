#pragma once

#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/Macros.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/IntrusiveRefCntPtr.h"

#include <concepts>
#include <cstddef>
#include <tuple>
#include <utility>

namespace llvm {
class Function;
} // namespace llvm

namespace psr {

template <typename T> class AnalysisResultTag {};

namespace detail {
template <typename AInputT, typename T>
concept ProvidesResult = requires(AInputT &AI) {
  { AI.getResult(AnalysisResultTag<T>{}) } -> std::convertible_to<const T &>;
};

template <typename Tag, typename... StageTs>
static constexpr int indexOfResult() noexcept {
  constexpr bool Matches[] = {std::same_as<StageTs, Tag>..., false};
  int Result = -1;
  for (size_t Idx = 0; Idx < sizeof...(StageTs); ++Idx) {
    if (Matches[Idx]) {
      // return last index
      Result = int(Idx);
    }
  }
  return Result;
}

template <typename ResultT, typename... StageTs>
static constexpr int indexOfMatchingStage() noexcept {
  constexpr bool Matches[] = {ProvidesResult<StageTs, ResultT>...};
  int Result = -1;
  for (size_t Idx = 0; Idx < sizeof...(StageTs); ++Idx) {
    if (Matches[Idx]) {
      // return last index
      Result = int(Idx);
    }
  }
  return Result;
}

} // namespace detail

template <typename Prefix> class SharedAnalysisInput;

template <typename... StagesT> class AnalysisInputImpl {
public:
  using TupT = std::tuple<std::unique_ptr<StagesT>...>;

  explicit AnalysisInputImpl(TupT &&Tup) noexcept : Stages(std::move(Tup)) {}

  template <typename T>
    requires(detail::indexOfMatchingStage<T, StagesT...>() >= 0)
  [[nodiscard]] constexpr decltype(auto)
  getResult(AnalysisResultTag<T> Tag = {}) noexcept {
    constexpr int Idx = detail::indexOfMatchingStage<T, StagesT...>();
    return std::get<Idx>(Stages)->getResult(Tag);
  }

  template <typename T>
  [[nodiscard]] constexpr auto
  getResultOrNull(AnalysisResultTag<T> Tag = {}) noexcept {
    constexpr int Idx = detail::indexOfMatchingStage<T, StagesT...>();

    if constexpr (Idx >= 0) {
      return &std::get<Idx>(Stages)->getResult(Tag);
    } else {
      return nullptr;
    }
  }

  template <typename NextStageT>
  [[nodiscard]] auto with(auto &&...NextArgs) && {
    return std::move(*this).withValue(
        std::make_unique<NextStageT>(*this, PSR_FWD(NextArgs)...));
  }

  template <typename GeneratorFn, typename... ArgsT>
    requires std::invocable<GeneratorFn, AnalysisInputImpl, ArgsT...>
  [[nodiscard]] auto with(GeneratorFn Generator, ArgsT &&...Args) && {
    return std::invoke(std::move(Generator), std::move(*this),
                       PSR_FWD(Args)...);
  }

  template <typename NextStageT>
  [[nodiscard]] auto withValue(std::unique_ptr<NextStageT> Next) && {
    return AnalysisInputImpl<StagesT..., NextStageT>(
        std::tuple_cat(std::move(Stages), std::make_tuple(std::move(Next))));
  }

  [[nodiscard]] auto shared() && {
    return SharedAnalysisInput(std::move(*this));
  }

private:
  [[no_unique_address]] TupT Stages;
};

template <typename Prefix> class SharedAnalysisInput {
  struct Root : public llvm::ThreadSafeRefCountedBase<Root> {
    Prefix P;
    explicit Root(Prefix &&P) noexcept : P(std::move(P)) {}
  };

public:
  explicit SharedAnalysisInput(Prefix &&Base)
      : Rc(llvm::makeIntrusiveRefCnt<Root>(std::move(Base))) {}

  template <typename T>
    requires detail::ProvidesResult<Prefix, T>
  [[nodiscard]] constexpr decltype(auto)
  getResult(AnalysisResultTag<T> Tag = {}) noexcept {
    return Rc->P.getResult(Tag);
  }

  template <typename T>
  [[nodiscard]] constexpr auto
  getResultOrNull(AnalysisResultTag<T> Tag = {}) noexcept {
    if constexpr (detail::ProvidesResult<Prefix, T>) {
      return &Rc->P.getResult(Tag);
    } else {
      return nullptr;
    }
  }

  template <typename NextStageT> [[nodiscard]] auto with(auto &&...NextArgs) {
    return this->withValue(NextStageT(*this, PSR_FWD(NextArgs)...));
  }

  template <typename GeneratorFn, typename... ArgsT>
    requires std::invocable<GeneratorFn, SharedAnalysisInput &, ArgsT...>
  [[nodiscard]] auto with(GeneratorFn Generator, ArgsT &&...Args) {
    return std::invoke(std::move(Generator), *this, PSR_FWD(Args)...);
  }

  template <typename NextStageT>
  [[nodiscard]] auto withValue(std::unique_ptr<NextStageT> Next) {
    return AnalysisInputImpl<SharedAnalysisInput, NextStageT>(
        std::make_tuple(*this, std::move(Next)));
  }

  [[nodiscard]] SharedAnalysisInput shared() const noexcept { return *this; }

private:
  llvm::IntrusiveRefCntPtr<Root> Rc{};
};

template <typename... ResultTs> class [[gsl::Pointer]] GenericAnalysisInputRef {
public:
  template <typename AnalysisInputT>
  constexpr GenericAnalysisInputRef(
      AnalysisInputT *Input PSR_LIFETIMEBOUND) noexcept
      : VT(&VTableFor<AnalysisInputT>), Data(&assertNotNull(Input)) {}

  template <typename T>
  [[nodiscard]] constexpr decltype(auto)
  getResult(AnalysisResultTag<T> Tag = {}) noexcept {
    return VT->getResult(Data, Tag);
  }

  template <typename T>
  [[nodiscard]] constexpr auto
  getResultOrNull(AnalysisResultTag<T> Tag = {}) noexcept {
    constexpr auto Idx = detail::indexOfResult<T, ResultTs...>();
    if constexpr (size_t(Idx) < sizeof...(ResultTs)) {
      return &VT->getResult(Tag);
    } else {
      return nullptr;
    }
  }

private:
  template <typename T>
  using GetResultRetTy =
      std::conditional_t<CanEfficientlyPassByValue<T>, T, T &>;

  template <typename T> using GetResultFn = GetResultRetTy<T> (*)(void *);
  struct VTable {
    std::tuple<GetResultFn<ResultTs>...> GetResultFuns;

    template <typename T>
    constexpr GetResultRetTy<T>
    getResult(void *Ctx, AnalysisResultTag<T> /*unused*/) const noexcept {
      constexpr auto Idx = detail::indexOfResult<T, ResultTs...>();
      if constexpr (size_t(Idx) < sizeof...(ResultTs)) {
        auto FnPtr = std::get<Idx>(GetResultFuns);
        return FnPtr(Ctx);
      } else {
        static_assert(size_t(Idx) < sizeof...(ResultTs),
                      "Analysis result for requested type is not provided");
      }
    }
  };

  template <typename AnalysisInputT, typename T>
  static GetResultRetTy<T> getResultThunk(void *Ctx) {
    return static_cast<AnalysisInputT *>(Ctx)->getResult(
        AnalysisResultTag<T>{});
  }

  template <typename AnalysisInputT>
  static constexpr VTable VTableFor = {
      .GetResultFuns = {(&getResultThunk<AnalysisInputT, ResultTs>)...},
  };

  // ---
  const VTable *VT{};
  void *Data{};
};

template <typename AnalysisInputT, typename... Ts>
concept AnalysisInputOf = (detail::ProvidesResult<AnalysisInputT, Ts> && ...);

namespace analysis_input {

struct EntryPoints : std::vector<std::string> {
  using std::vector<std::string>::vector;

  explicit EntryPoints(std::vector<std::string> Entries)
      : std::vector<std::string>(std::move(Entries)) {}
};
struct EntryFunctions : std::vector<const llvm::Function *> {
  using std::vector<const llvm::Function *>::vector;

  explicit EntryFunctions(std::vector<const llvm::Function *> Entries)
      : std::vector<const llvm::Function *>(std::move(Entries)) {}
};

template <typename T, typename AnalysisInputT>
[[nodiscard]] decltype(auto) getResult(AnalysisInputT &Inp,
                                       AnalysisResultTag<T> Tag = {}) {
  return Inp.getResult(Tag);
}

template <typename T, typename AnalysisInputT>
[[nodiscard]] auto getResultOrNull(AnalysisInputT &Inp,
                                   AnalysisResultTag<T> Tag = {}) {
  return Inp.getResultOrNull(Tag);
}

} // namespace analysis_input

} // namespace psr
