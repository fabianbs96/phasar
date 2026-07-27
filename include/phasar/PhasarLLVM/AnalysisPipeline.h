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
#include "phasar/PhasarLLVM/ControlFlow/FunctionCompressor.h"
#include "phasar/PhasarLLVM/ControlFlow/GlobalCtorsDtorsModel.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMVFTableProvider.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/TaintConfig/LLVMTaintConfig.h"
#include "phasar/PhasarLLVM/TaintConfig/TaintConfigData.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/Utils/UsedGlobals.h"
#include "phasar/Pointer/AliasAnalysisType.h"
#include "phasar/Pointer/UnionFindAliasAnalysisType.h"
#include "phasar/Utils/FunctionId.h"
#include "phasar/Utils/Lazy.h"
#include "phasar/Utils/Macros.h"
#include "phasar/Utils/NonNullPtr.h"
#include "phasar/Utils/SCCGeneric.h"
#include "phasar/Utils/UsedGlobalsHolder.h"

#include "llvm/ADT/Twine.h"
#include "llvm/IR/Function.h"

#include <type_traits>

namespace psr {

struct IRDBStage {
  using tag_t = IRDBTag;
  using result_t = LLVMProjectIRDB;
};

struct EntrypointsStage {
  using tag_t = EntrypointsTag;
  using result_t = std::vector<std::string>;

  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return getDefaultEntryPoints(IRDB);
  }

  static result_t build(auto & /*PrevPipeline*/,
                        std::vector<std::string> ExplicitEntryPoints) {
    return ExplicitEntryPoints;
  }
};
struct EntryFunctionsStage {
  using tag_t = EntryFunctionsTag;
  using result_t = std::vector<const llvm::Function *>;

  static result_t
  build(StageRequire<IRDBTag, EntrypointsTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &EntryPoints = PrevPipeline.getResult(EntrypointsTag{});
    return getEntryFunctions(IRDB, EntryPoints);
  }

  static result_t
  build(auto & /*PrevPipeline*/,
        std::vector<const llvm::Function *> ExplicitEntryPoints) {
    return ExplicitEntryPoints;
  }
};

struct GlobalCtorsDtorsStage {
  using tag_t = EntrypointsTag;
  using result_t = std::vector<std::string>;

  static result_t
  build(StageRequire<IRDBTag, EntrypointsTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &Entries = PrevPipeline.getResult(EntrypointsTag{});

    if (Entries.size() != 1 || Entries[0] != "main") {
      // Currently, the GlobalCtorsDtorsModel only works with "main" as single
      // entrypoint. We can relax this condition, once the implementation has
      // caught up
      return Entries;
    }

    auto *Model = GlobalCtorsDtorsModel::buildModel(IRDB, Entries);
    return {Model->getName().str()};
  }
};

struct TypeHierarchyStage {
  using tag_t = TypeHierarchyTag;
  using result_t = DIBasedTypeHierarchy;

  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return DIBasedTypeHierarchy(IRDB);
  }
};

struct VFTableProviderStage {
  using tag_t = VFTableProviderTag;
  using result_t = LLVMVFTableProvider;

  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMVFTableProvider(IRDB);
  }
};

class ICFGStage {
public:
  using tag_t = ICFGTag;
  using result_t = LLVMBasedICFG;

  static auto build(StageRequire<IRDBTag, EntrypointsTag, VFTableProviderTag,
                                 TypeHierarchyTag> auto &PrevPipeline,
                    CallGraphAnalysisType CGTy) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &Entry = PrevPipeline.getResult(EntrypointsTag{});
    auto &VTP = PrevPipeline.getResult(VFTableProviderTag{});
    auto &TH = PrevPipeline.getResult(TypeHierarchyTag{});
    auto PT = PrevPipeline.getResultOrNull(AliasInfoTag{});
    auto BaseCG = PrevPipeline.getResultOrNull(ICFGTag{});
    return buildImpl(IRDB, Entry, VTP, TH, PT, BaseCG, CGTy);
  }

private:
  static LLVMBasedICFG buildImpl(LLVMProjectIRDB &IRDB,
                                 const std::vector<std::string> &Entry,
                                 LLVMVFTableProvider &VTP,
                                 DIBasedTypeHierarchy &TH, LLVMAliasInfo *PT,
                                 LLVMBasedICFG *BaseCG,
                                 CallGraphAnalysisType CGTy);
};

class AliasInfoStage {
public:
  using tag_t = AliasInfoTag;
  using result_t = LLVMAliasInfo;

  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline,
                        AliasAnalysisType AATy,
                        UnionFindAliasAnalysisType UFAATy =
                            UnionFindAliasAnalysisType::CtxIndSens) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto *BaseCG = PrevPipeline.getResultOrNull(ICFGTag{});
    return buildImpl(IRDB, BaseCG, AATy, UFAATy);
  }

  static result_t build(StageRequire<IRDBTag, ICFGTag> auto &PrevPipeline,
                        UnionFindAliasAnalysisType UFAATy) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &BaseCG = PrevPipeline.getResult(ICFGTag{});
    return buildImpl(IRDB, &BaseCG, AliasAnalysisType::UnionFind, UFAATy);
  }

private:
  static LLVMAliasInfo buildImpl(LLVMProjectIRDB &IRDB,
                                 const LLVMBasedICFG *BaseCG,
                                 AliasAnalysisType AATy,
                                 UnionFindAliasAnalysisType UFAATy);
};

struct TaintConfigStage {
  using tag_t = TaintConfigTag;
  using result_t = LLVMTaintConfig;

  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return LLVMTaintConfig(IRDB);
  }
  static result_t build(StageRequire<IRDBTag> auto &PrevPipeline,
                        const TaintConfigData &TC) {
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
  static auto build(StageRequire<IRDBTag, EntrypointsTag> auto &PrevPipeline,
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

struct FunctionCompressorStage {
  using tag_t = FunctionCompressorTag;
  using result_t = FunctionCompressor<const llvm::Function *>;

  static result_t
  build(StageRequire<ICFGTag, EntryFunctionsTag> auto &PrevPipeline) {
    auto &ICF = PrevPipeline.getResult(ICFGTag{});
    auto &Entries = PrevPipeline.getResult(EntryFunctionsTag{});
    return psr::compressFunctions(ICF.getCallGraph(), Entries);
  }

  template <StageRequire<IRDBTag> PrevPipelineT>
  static result_t build(PrevPipelineT &PrevPipeline)
    requires(!StageRequire<PrevPipelineT, ICFGTag>)
  {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    return psr::compressFunctions(IRDB);
  }
};

class CGSCCsStage {
public:
  using tag_t = CGSCCsTag;
  using result_t = SCCHolder<FunctionId>;

  static result_t
  build(StageRequire<ICFGTag, FunctionCompressorTag> auto &PrevPipeline) {
    auto &ICF = PrevPipeline.getResult(ICFGTag{});
    auto &Functions = PrevPipeline.getResult(FunctionCompressorTag{});

    return buildImpl(ICF, Functions);
  }

private:
  static result_t
  buildImpl(const LLVMBasedICFG &ICF,
            const FunctionCompressor<const llvm::Function *> &Functions);
};

class CGSCCCallersStage {
public:
  using tag_t = CGSCCCallersTag;
  using result_t = SCCDependencyGraph<FunctionId>;

  static result_t build(StageRequire<ICFGTag, FunctionCompressorTag,
                                     CGSCCsTag> auto &PrevPipeline) {
    auto &ICF = PrevPipeline.getResult(ICFGTag{});
    auto &Functions = PrevPipeline.getResult(FunctionCompressorTag{});
    auto &SCCs = PrevPipeline.getResult(CGSCCsTag{});

    return buildImpl(ICF, Functions, SCCs);
  }

private:
  static result_t
  buildImpl(const LLVMBasedICFG &ICF,
            const FunctionCompressor<const llvm::Function *> &Functions,
            const SCCHolder<FunctionId> &SCCs);
};

struct UsedGlobalsStage {
  using tag_t = UsedGlobalsTag;
  using result_t = UsedGlobalsHolder<const llvm::GlobalVariable *>;

  static result_t build(StageRequire<IRDBTag, FunctionCompressorTag, CGSCCsTag,
                                     CGSCCCallersTag> auto &PrevPipeline) {
    auto &IRDB = PrevPipeline.getResult(IRDBTag{});
    auto &Functions = PrevPipeline.getResult(FunctionCompressorTag{});
    auto &SCCs = PrevPipeline.getResult(CGSCCsTag{});
    auto &SCCCallers = PrevPipeline.getResult(CGSCCCallersTag{});

    return psr::computeUsedGlobals(IRDB, Functions, SCCs, SCCCallers);
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
      .with(GlobalCtorsDtorsStage{})
      .with(ICFGStage{}, CallGraphAnalysisType::RTA)
      .with(AliasInfoStage{}, UnionFindAliasAnalysisType::CtxIndSens)
      .with(ICFGStage{}, CallGraphAnalysisType::VTA);
}

} // namespace psr
