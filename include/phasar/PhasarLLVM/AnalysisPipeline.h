#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/DataFlow/AnalysisPipeline.h"
#include "phasar/PhasarLLVM/ControlFlow/EntryFunctionUtils.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraphBuilder.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMVFTableProvider.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/PrecomputedResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/PhasarLLVM/TaintConfig/TaintConfigData.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/Pointer/AliasAnalysisType.h"
#include "phasar/Pointer/UnionFindAliasAnalysisType.h"
#include "phasar/Utils/Lazy.h"
#include "phasar/Utils/Macros.h"
#include "phasar/Utils/NonNullPtr.h"

#include "llvm/ADT/Twine.h"

#include <memory>
#include <type_traits>

namespace psr {

struct IRDBStage {
  using tag_t = IRDBTag;
  using result_t = LLVMProjectIRDB;
};

struct EntrypointsStage {
  using tag_t = EntrypointsTag;
  using result_t = std::vector<std::string>;

  static auto build(auto &PrevPipeline) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return getDefaultEntryPoints(IRDB);
  }
};
struct EntryFunctionsStage {
  using tag_t = EntryFunctionsTag;
  using result_t = std::vector<const llvm::Function *>;

  static auto build(auto &PrevPipeline) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    PSR_REQUIRE_STAGE(PrevPipeline, EntrypointsTag, "missing EntrypointsStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &EntryPoints = PrevPipeline.getResult(EntrypointsTag{});
    return getEntryFunctions(IRDB, EntryPoints);
  }
};
struct TypeHierarchyStage {
  using tag_t = TypeHierarchyTag;
  using result_t = DIBasedTypeHierarchy;

  static auto build(auto &PrevPipeline) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return DIBasedTypeHierarchy(IRDB);
  }
};

struct VFTableProviderStage {
  using tag_t = VFTableProviderTag;
  using result_t = LLVMVFTableProvider;

  static auto build(auto &PrevPipeline) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMVFTableProvider(IRDB);
  }
};

struct ICFGStage {
  using tag_t = ICFGTag;
  using result_t = LLVMBasedICFG;

  static auto build(auto &PrevPipeline, CallGraphAnalysisType CGTy);
};

struct AliasInfoStage {
  using tag_t = AliasInfoTag;
  using result_t = LLVMAliasInfo;

  static LLVMAliasInfo buildImpl(LLVMProjectIRDB &IRDB,
                                 const LLVMBasedICFG *BaseCG,
                                 AliasAnalysisType AATy,
                                 UnionFindAliasAnalysisType UFAATy);

  static auto build(auto &PrevPipeline, AliasAnalysisType AATy,
                    UnionFindAliasAnalysisType UFAATy =
                        UnionFindAliasAnalysisType::CtxIndSens) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto *BaseCG = PrevPipeline.getResultOrNull(ICFGTag{});
    return buildImpl(IRDB, BaseCG, AATy, UFAATy);
  }

  static auto build(auto &PrevPipeline, UnionFindAliasAnalysisType UFAATy) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    PSR_REQUIRE_STAGE(PrevPipeline, ICFGTag, "missing ICFGStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &BaseCG = PrevPipeline.getResult(ICFGTag{});
    return buildImpl(IRDB, &BaseCG, AliasAnalysisType::UnionFind, UFAATy);
  }
};

inline auto ICFGStage::build(auto &PrevPipeline, CallGraphAnalysisType CGTy) {
  PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
  PSR_REQUIRE_STAGE(PrevPipeline, EntrypointsTag, "missing EntrypointsStage");
  PSR_REQUIRE_STAGE(PrevPipeline, VFTableProviderTag,
                    "missing VFTableProviderStage");
  PSR_REQUIRE_STAGE(PrevPipeline, TypeHierarchyTag,
                    "missing TypeHierarchyStage");
  auto &IRDB = PrevPipeline.getResult(IRDBTag{});
  auto &Entry = PrevPipeline.getResult(EntrypointsTag{});
  auto &VTP = PrevPipeline.getResult(VFTableProviderTag{});
  auto &TH = PrevPipeline.getResult(TypeHierarchyTag{});
  auto PT = PrevPipeline.getResultOrNull(AliasInfoTag{});
  auto BaseCG = PrevPipeline.getResultOrNull(ICFGTag{});

  auto BaseRes = [BaseCG]() {
    // Note: double-wrapping in callback to avoid stack-use-after-scope,
    //       since Resolver::BaseResolverProvider is a function_ref.
    //       We cannot just use a ternary below, because BaseCG may be nullptr_t
    if constexpr (std::is_null_pointer_v<decltype(BaseCG)>) {
      (void)BaseCG;
      return []() -> Resolver::BaseResolverProvider { return nullptr; };
    } else {
      return [BaseCG] {
        return [BaseCG](const LLVMProjectIRDB *IRDB,
                        const LLVMVFTableProvider *VTP,
                        const DIBasedTypeHierarchy * /*TH*/,
                        LLVMAliasInfoRef /*PT*/) {
          auto &CG = BaseCG->getCallGraph();
          return std::make_unique<PrecomputedResolver>(IRDB, VTP, &CG);
        };
      };
    }
  }();

  auto Res = Resolver::create(CGTy, &IRDB, &VTP, &TH,
                              LLVMAliasInfo::asRefOrNull(PT), BaseRes());
  return LLVMBasedICFG(
      buildLLVMBasedCallGraphWithExternCallbackModels(IRDB, *Res, Entry),
      &IRDB);
}

struct TaintConfigStage {
  using tag_t = TaintConfigTag;
  using result_t = LLVMTaintConfig;

  static auto build(auto &PrevPipeline) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMTaintConfig(IRDB);
  }
  static auto build(auto &PrevPipeline, const TaintConfigData &TC) {
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMTaintConfig(IRDB, TC);
  }
  // TODO: callbacks
};

template <typename ProblemTy> struct DataflowAnalysisStage {
  using tag_t = DataflowAnalysisTag;
  using result_t = ProblemTy;

  [[no_unique_address]] std::type_identity<ProblemTy> ProblemType;

  template <typename... ArgTys>
  static auto build(auto &PrevPipeline, ArgTys &&...Args) {
    // mirror createAnalysisProblem():
    PSR_REQUIRE_STAGE(PrevPipeline, IRDBTag, "missing IRDBStage");
    PSR_REQUIRE_STAGE(PrevPipeline, EntrypointsTag, "missing EntrypointsStage");

    if constexpr (std::is_constructible_v<ProblemTy, const LLVMProjectIRDB *,
                                          std::vector<std::string>,
                                          ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             const LLVMBasedICFG *, std::vector<std::string>,
                             ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       &PrevPipeline.getResult(ICFGTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             LLVMAliasInfoRef, std::vector<std::string>,
                             ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       PrevPipeline.getResult(AliasInfoTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             const LLVMBasedICFG *, LLVMAliasInfoRef,
                             std::vector<std::string>, ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       &PrevPipeline.getResult(ICFGTag{}),
                       PrevPipeline.getResult(AliasInfoTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             LLVMAliasInfoRef, const LLVMTaintConfig *,
                             std::vector<std::string>, ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       PrevPipeline.getResult(AliasInfoTag{}),
                       &PrevPipeline.getResult(TaintConfigTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             const DIBasedTypeHierarchy *, const LLVMBasedCFG *,
                             LLVMAliasInfoRef, std::vector<std::string>,
                             ArgTys...>) {
      // TODO: have an own CFG tag
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       &PrevPipeline.getResult(TypeHierarchyTag{}),
                       &PrevPipeline.getResult(ICFGTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             const DIBasedTypeHierarchy *,
                             const LLVMBasedICFG *, LLVMAliasInfoRef,
                             std::vector<std::string>, ArgTys...>) {
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       &PrevPipeline.getResult(TypeHierarchyTag{}),
                       &PrevPipeline.getResult(ICFGTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else if constexpr (std::is_constructible_v<
                             ProblemTy, const LLVMProjectIRDB *,
                             const LLVMBasedCFG *, LLVMAliasInfoRef,
                             std::vector<std::string>, ArgTys...>) {
      // TODO: have an own CFG tag
      return ProblemTy(&PrevPipeline.getResult(IRDBTag{}),
                       &PrevPipeline.getResult(ICFGTag{}),
                       PrevPipeline.getResult(AliasInfoTag{}),
                       PrevPipeline.getResult(EntrypointsTag{}),
                       PSR_FWD(Args)...);
    } else {
      static_assert(std::is_constructible_v<ProblemTy, ArgTys...>,
                    "Cannot construct analysis problem from pipeline");
    }
  }
};

// --- pipeline constructors:

[[nodiscard]] inline auto pipeline(const llvm::Twine &IRFile) {
  return Pipeline<>{}.withValue(IRDBStage{},
                                PSR_LAZY(LLVMProjectIRDB::loadOrExit(IRFile)));
}

[[nodiscard]] inline auto pipeline(NonNullPtr<llvm::Module> Mod) {
  return Pipeline<>{}.withValue(IRDBStage{},
                                PSR_LAZY(LLVMProjectIRDB(Mod.get())));
}

[[nodiscard]] inline auto defaultPipelineStart(const llvm::Twine &IRFile) {
  return pipeline(IRFile)
      .with(EntrypointsStage{})
      .with(TypeHierarchyStage{})
      .with(VFTableProviderStage{});
}

[[nodiscard]] inline auto defaultPipeline(const llvm::Twine &IRFile) {
  return defaultPipelineStart(IRFile)
      .with(ICFGStage{}, CallGraphAnalysisType::RTA)
      .with(AliasInfoStage{}, UnionFindAliasAnalysisType::CtxIndSens)
      .with(ICFGStage{}, CallGraphAnalysisType::VTA);
}

} // namespace psr
