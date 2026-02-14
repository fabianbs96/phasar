#pragma once

#include "phasar/DataFlow/IfdsIde/EdgeFunctionUtils.h"
#include "phasar/DataFlow/IfdsIde/InitialSeeds.h"
#include "phasar/DataFlow/IfdsIde/Solver/IterativeIDESolver.h"
#include "phasar/Utils/ByRef.h"
#include "phasar/Utils/JoinLattice.h"
#include "phasar/Utils/MapUtils.h"
#include "phasar/Utils/Soundness.h"

#include "llvm/Support/ErrorHandling.h"

namespace psr {
template <typename ProblemTy, typename StaticSolverConfigTy>
class DemandIdBasedSolverResults {
  using solver_t = IterativeIDESolver<ProblemTy, StaticSolverConfigTy>;
  using InterPropagationJob = solver_t::InterPropagationJob;
  using EdgeFunPtrTy = solver_t::EdgeFunctionPtrType;
  using l_t = solver_t::l_t;
  using n_t = solver_t::n_t;
  using d_t = solver_t::d_t;
  using f_t = solver_t::f_t;

public:
  DemandIdBasedSolverResults(const solver_t *Solver) : Solver(Solver) {
    assert(Solver != nullptr);
  }

  /// Performs an on-demand lookup of the analysis-results at (Inst, Fact). The
  /// result is (partially) cached.
  [[nodiscard]] l_t resultAt(ByConstRef<n_t> Inst, ByConstRef<d_t> Fact) {
    auto InstId = Solver->NodeCompressor.getOrNull(Inst);
    auto FactId = Solver->FactCompressor.getOrNull(Fact);

    if (!InstId || !FactId) {
      return l_t{};
    }

    auto EF = lookupResultAt(*InstId, *FactId);
    if (!EF) {
      return l_t{};
    }

    return EF.computeTarget(Solver->Problem.bottomElement());
  }

  /// Computes the facts that hold at the given instruction, without triggering
  /// value-computation.
  /// Use this to call resultAt() in an informed way.
  [[nodiscard]] std::set<d_t> ifdsResultsAt(ByConstRef<n_t> Inst) const {
    auto InstId = Solver->NodeCompressor.getOrNull(Inst);
    if (!InstId) {
      return {};
    }

    std::set<d_t> Result;
    for (const auto &[SrcTgtFactId, Unused] :
         Solver->JumpFunctions[*InstId].cells()) {
      auto [SrcFactId, TgtFactId] = solver_t::splitId(SrcTgtFactId);
      Result.insert(Solver->FactCompressor[TgtFactId]);
    }
    return Result;
  }

  /// Checks whether the given Fact holds at the given Inst, without triggering
  /// the value-computation.
  [[nodiscard]] bool containsResultAt(ByConstRef<n_t> Inst,
                                      ByConstRef<d_t> Fact) const {
    auto InstId = Solver->NodeCompressor.getOrNull(Inst);
    auto FactId = Solver->FactCompressor.getOrNull(Fact);
    if (!InstId || !FactId) {
      return false;
    }

    for (const auto &[SrcTgtFactId, EF] :
         Solver->JumpFunctions[*InstId].cells()) {
      auto [SrcFactId, TgtFactId] = solver_t::splitId(SrcTgtFactId);
      if (TgtFactId == *FactId) {
        return true;
      }
    }
    return false;
  }

  /// Performs a light-weight check on whether there is any result computed for
  /// the given Inst.
  [[nodiscard]] bool containsNode(ByConstRef<n_t> Inst) const {
    return Solver->NodeCompressor.getOrNull(Inst) != std::nullopt;
  }

private:
  [[nodiscard]] EdgeFunPtrTy lookupResultAt(uint32_t InstId, uint32_t FactId) {
    const auto &InitialStates = Solver->JumpFunctions[InstId];
    if (InitialStates.empty()) {
      return nullptr;
    }

    auto AtInstruction = Solver->NodeCompressor[InstId];
    auto FunId = Solver->FunCompressor.getOrNull(
        Solver->ICFG.getFunctionOf(AtInstruction));
    if (!FunId) {
      return nullptr;
    }

    EdgeFunPtrTy Result = nullptr;
    for (const auto &[SrcTgtFactId, EF] : InitialStates.cells()) {
      auto [SrcFactId, TgtFactId] = solver_t::splitId(SrcTgtFactId);
      if (TgtFactId != FactId) {
        continue;
      }

      auto ComposedEF = [&]() -> EdgeFunPtrTy {
        if (EF.isConstant()) {
          return EF;
        }
        return lookupIntermediateResultAt(*FunId, SrcFactId, std::move(EF));
      }();
      if (!Result) {
        Result = std::move(ComposedEF);
      } else {
        Result = Solver->Problem.combine(Result, std::move(ComposedEF));
        if (Result.template isa<AllBottom<l_t>>()) {
          break;
        }
      }
    }
    return Result;
  }

  [[nodiscard]] EdgeFunPtrTy lookupIntermediateResultAt(uint32_t FunId,
                                                        uint32_t SrcFactId,
                                                        EdgeFunPtrTy Suffix) {
    // llvm::errs() << "[lookupIntermediateResultAt]: " << FunId << ", "
    //              << SrcFactId << " + " << to_string(Suffix) << '\n';
    // if (SrcFactId == 0) {
    //   return Suffix;
    // }

    const auto SrcFactAndFunc = solver_t::combineIds(SrcFactId, FunId);
    auto [It, Inserted] = Cache.try_emplace(SrcFactAndFunc);
    auto &CacheEntry = It->second;
    if (!Inserted) {
      // if (!Suffix.template isa<EdgeIdentity<l_t>>()) {
      //   if constexpr (HasJoinLatticeTraits<l_t>) {
      //     CacheEntry = AllBottom<l_t>{};
      //   } else {
      //     CacheEntry = AllBottom<l_t>{Solver->Problem.bottomElement()};
      //   }
      // }
      // llvm::errs() << "> Found in cache\n";
      return CacheEntry ? Solver->Problem.extend(CacheEntry, Suffix) : Suffix;
    }

    const auto *RevLookup =
        Solver->SourceFactAndFuncToInterJob.getOr(SrcFactAndFunc, nullptr);
    while (RevLookup) {
      const auto &EF = RevLookup->SourceEF;
      auto ComposedEF = [&]() -> EdgeFunPtrTy {
        if (EF.isConstant()) {
          return EF;
        }

        auto AtInstruction = Solver->NodeCompressor[RevLookup->CallSite];
        auto CSFunId = Solver->FunCompressor.getOrNull(
            Solver->ICFG.getFunctionOf(AtInstruction));
        if (!CSFunId) {
          llvm::report_fatal_error(
              "Function of call-site not in FunCompressor!");
        }
        // TODO: Get rid of the recursion!
        // llvm::errs() << "-> rec\n";
        return lookupIntermediateResultAt(*CSFunId, RevLookup->SourceFact, EF);
      }();

      if (!CacheEntry) {
        CacheEntry = std::move(ComposedEF);
      } else {
        CacheEntry = Solver->Problem.combine(CacheEntry, ComposedEF);
      }
      if (CacheEntry.template isa<AllBottom<l_t>>()) {
        break;
      }

      RevLookup = RevLookup->NextWithSameSourceFactAndCallee;
    }

    // llvm::errs() << "> return\n";

    if (!CacheEntry) {
      // Found source!

      if (SrcFactId == 0) {
        // Generated from zero: no special seed
        CacheEntry = EdgeIdentity<l_t>{};
        return Suffix;
      }
      const auto &Fun = Solver->FunCompressor[FunId];
      const auto &SrcFact = Solver->FactCompressor[SrcFactId];
      auto SeedVal = lookupSeed(Fun, SrcFact);
      if (SeedVal == Solver->Problem.bottomElement()) {
        // We pass bottom to the resulting EF anyway, so nothing to do here
        CacheEntry = Suffix;
      } else {
        CacheEntry = ConstantEdgeFunction<l_t>{
            NonTopBotValue<l_t>::unwrap(std::move(SeedVal))};
      }
    }

    return Solver->Problem.extend(CacheEntry, Suffix);
  }

  [[nodiscard]] l_t lookupSeed(ByConstRef<f_t> Fun, ByConstRef<d_t> Fact) {
    if (!Seeds) {
      Seeds = Solver->Problem.initialSeeds();
    }
    const auto &Seeds = this->Seeds->getSeeds();
    for (const auto &SP : Solver->ICFG.getStartPointsOf(Fun)) {
      const auto &AtSP = getOrDefault(Seeds, SP);
      const auto *AtFact = getOrNull(AtSP, Fact);
      if (AtFact) {
        // TODO: Can we early return here?
        return *AtFact;
      }
    }

    for (const auto &[Inst, FactsAndValues] : Seeds) {
      if (Solver->ICFG.getFunctionOf(Inst) != Fun) {
        continue;
      }
      const auto *AtFact = getOrNull(FactsAndValues, Fact);
      if (AtFact) {
        // TODO: Can we early return here?
        return *AtFact;
      }
    }

    return Solver->Problem.bottomElement();
  }

  // --- data members:

  const solver_t *Solver{};

  llvm::DenseMap<uint64_t, EdgeFunPtrTy> Cache{};
  std::optional<InitialSeeds<n_t, d_t, l_t>> Seeds{};
};

template <typename ProblemTy, typename StaticSolverConfigTy>
DemandIdBasedSolverResults(
    const IterativeIDESolver<ProblemTy, StaticSolverConfigTy> *)
    -> DemandIdBasedSolverResults<ProblemTy, StaticSolverConfigTy>;
} // namespace psr
