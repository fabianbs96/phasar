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

struct IRDBTag {};

struct EntrypointsTag {
  static auto build(auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return getDefaultEntryPoints(IRDB);
  }
};
struct EntryFunctionsTag {
  static auto build(auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &EntryPoints = PrevPipeline.getResult(EntrypointsTag{});
    return getEntryFunctions(IRDB, EntryPoints);
  }
};
struct TypeHierarchyTag {
  static auto build(auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return DIBasedTypeHierarchy(IRDB);
  }
};

struct VFTableProviderTag {
  static auto build(auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMVFTableProvider(IRDB);
  }
};

struct ICFGTag {
  static auto build(auto &PrevPipeline, CallGraphAnalysisType CGTy);
};

struct AliasInfoTag {
  static LLVMAliasInfo buildImpl(LLVMProjectIRDB &IRDB,
                                 const LLVMBasedICFG *BaseCG,
                                 AliasAnalysisType AATy,
                                 UnionFindAliasAnalysisType UFAATy);

  static auto build(auto &PrevPipeline, AliasAnalysisType AATy,
                    UnionFindAliasAnalysisType UFAATy =
                        UnionFindAliasAnalysisType::CtxIndSens) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto *BaseCG = PrevPipeline.getResultOrNull(ICFGTag{});
    return buildImpl(IRDB, BaseCG, AATy, UFAATy);
  }

  static auto build(auto &PrevPipeline, UnionFindAliasAnalysisType UFAATy) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &BaseCG = PrevPipeline.getResult(ICFGTag{});
    return buildImpl(IRDB, &BaseCG, AliasAnalysisType::UnionFind, UFAATy);
  }
};

inline auto ICFGTag::build(auto &PrevPipeline, CallGraphAnalysisType CGTy) {
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

struct TaintConfigTag {
  static auto build(auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMTaintConfig(IRDB);
  }
  static auto build(auto &PrevPipeline, const TaintConfigData &TC) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMTaintConfig(IRDB, TC);
  }
  // TODO: callbacks
};

struct DataflowAnalysisTag {
  template <typename ProblemTy, typename... ArgTys>
  static auto build(auto &PrevPipeline,
                    std::type_identity<ProblemTy> /*unused*/,
                    ArgTys &&...Args) {
    // mirror createAnalysisProblem():

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
  return PipelineStage<LLVMProjectIRDB, IRDBTag, PipelineRoot>{
      IRDBTag{}, PSR_LAZY(LLVMProjectIRDB::loadOrExit(IRFile)), {}};
}

[[nodiscard]] inline auto pipeline(NonNullPtr<llvm::Module> Mod) {
  return PipelineStage<LLVMProjectIRDB, IRDBTag, PipelineRoot>{
      IRDBTag{}, PSR_LAZY(LLVMProjectIRDB(Mod.get())), {}};
}

[[nodiscard]] inline auto defaultPipelineStart(const llvm::Twine &IRFile) {
  return pipeline(IRFile)
      .with(EntrypointsTag{})
      .with(TypeHierarchyTag{})
      .with(VFTableProviderTag{});
}

[[nodiscard]] inline auto defaultPipeline(const llvm::Twine &IRFile) {
  return defaultPipelineStart(IRFile)
      .with(ICFGTag{}, CallGraphAnalysisType::RTA)
      .with(AliasInfoTag{}, UnionFindAliasAnalysisType::CtxIndSens)
      .with(ICFGTag{}, CallGraphAnalysisType::VTA);
}

} // namespace psr
