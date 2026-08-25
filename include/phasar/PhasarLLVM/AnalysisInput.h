#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/AnalysisStrategy/AnalysisInput.h"
#include "phasar/ControlFlow/CallGraphAnalysisType.h"
#include "phasar/PhasarLLVM/ControlFlow/EntryFunctionUtils.h"
#include "phasar/PhasarLLVM/ControlFlow/GlobalCtorsDtorsModel.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedCallGraph.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMVFTableProvider.h"
#include "phasar/PhasarLLVM/DB/LLVMProjectIRDB.h"
#include "phasar/PhasarLLVM/Pointer/AndersenOTFAA.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointerAssignmentGraph.h"
#include "phasar/PhasarLLVM/Pointer/LLVMRawAliasSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/DIBasedTypeHierarchy.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMVFTable.h"
#include "phasar/Pointer/UnionFindAliasAnalysisType.h"
#include "phasar/Utils/Soundness.h"
#include "phasar/Utils/ValueCompressor.h"

#include "llvm/ADT/ArrayRef.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/WithColor.h"

#include <memory>

namespace psr {
class IRDBInput {
public:
  IRDBInput(const llvm::Twine &IRFileName)
      : IRDB(LLVMProjectIRDB::loadOrExit(IRFileName)) {}

  IRDBInput(llvm::Module *IRModule) : IRDB(IRModule) {}

  [[nodiscard]] LLVMProjectIRDB &
  getResult(AnalysisResultTag<LLVMProjectIRDB> /*unused*/) noexcept {
    return IRDB;
  }

private:
  LLVMProjectIRDB IRDB;
};

class EntryPointsInput {
public:
  EntryPointsInput(std::vector<std::string> EntryPoints) noexcept
      : EntryPoints(std::move(EntryPoints)) {}

  explicit EntryPointsInput(AnalysisInputOf<LLVMProjectIRDB> auto &Inp)
      : EntryPoints(psr::getDefaultEntryPoints(
            analysis_input::getResult<LLVMProjectIRDB>(Inp))) {}

  [[nodiscard]] auto &getResult(
      AnalysisResultTag<analysis_input::EntryPoints> /*unused*/) noexcept {
    return EntryPoints;
  }

  // Gets invalidated by changing EntryPoints
  [[nodiscard]] analysis_input::EntryFunctions &getResult(
      AnalysisResultTag<analysis_input::EntryFunctions> /*unused*/) noexcept =
      delete;

private:
  analysis_input::EntryPoints EntryPoints;
};

class EntryFunctionsInput {
public:
  EntryFunctionsInput(std::vector<const llvm::Function *> EntryPoints) noexcept
      : EntryPoints(std::move(EntryPoints)) {}

  explicit EntryFunctionsInput(
      AnalysisInputOf<LLVMProjectIRDB, analysis_input::EntryPoints> auto &Inp)
      : EntryPoints(psr::getEntryFunctions(
            analysis_input::getResult<LLVMProjectIRDB>(Inp),
            analysis_input::getResult<analysis_input::EntryPoints>(Inp))) {}

  [[nodiscard]] auto &getResult(
      AnalysisResultTag<analysis_input::EntryFunctions> /*unused*/) noexcept {
    return EntryPoints;
  }

private:
  analysis_input::EntryFunctions EntryPoints;
};

class GlobalCtorsDtorsInput {
public:
  GlobalCtorsDtorsInput(
      AnalysisInputOf<LLVMProjectIRDB, analysis_input::EntryPoints> auto &Inp)
      : EntryPoints({GlobalCtorsDtorsModel::ModelName.str()}) {
    auto &IRDB = analysis_input::getResult<LLVMProjectIRDB>(Inp);
    auto &Entries = analysis_input::getResult<analysis_input::EntryPoints>(Inp);

    if (Entries.size() != 1 || Entries[0] != "main") {
      // Currently, the GlobalCtorsDtorsModel only works with "main" as single
      // entrypoint. We can relax this condition, once the implementation has
      // caught up
      llvm::WithColor::warning() << "Cannot build GlobalCtorsDtorsModel "
                                    "because EntryPoints are not { 'main' }.\n";
      EntryPoints = Entries;
    }

    GlobalCtorsDtorsModel::buildModel(IRDB, Entries);
  }

  [[nodiscard]] std::vector<std::string> &getResult(
      AnalysisResultTag<analysis_input::EntryPoints> /*unused*/) noexcept {
    return EntryPoints;
  }

  // Gets invalidated by changing EntryPoints
  [[nodiscard]] std::vector<const llvm::Function *> &getResult(
      AnalysisResultTag<analysis_input::EntryFunctions> /*unused*/) noexcept =
      delete;

private:
  std::vector<std::string> EntryPoints;
};

class TypeHierarchyInput {
public:
  TypeHierarchyInput(AnalysisInputOf<LLVMProjectIRDB> auto &Inp)
      : TH(analysis_input::getResult<LLVMProjectIRDB>(Inp)) {}

  [[nodiscard]] auto &
  getResult(AnalysisResultTag<DIBasedTypeHierarchy> /*unused*/) noexcept {
    return TH;
  }

private:
  DIBasedTypeHierarchy TH;
};

class VFTableInput {
public:
  VFTableInput(AnalysisInputOf<LLVMProjectIRDB> auto &Inp)
      : VTP(analysis_input::getResult<LLVMProjectIRDB>(Inp)) {}

  [[nodiscard]] auto &
  getResult(AnalysisResultTag<LLVMVFTableProvider> /*unused*/) noexcept {
    return VTP;
  }

private:
  LLVMVFTableProvider VTP;
};

class ICFGInput {
public:
  ICFGInput(AnalysisInputOf<LLVMProjectIRDB> auto &Inp, LLVMBasedCallGraph &&CG)
      : ICF(std::make_unique<LLVMBasedICFG>(
            std::move(CG), &analysis_input::getResult<LLVMProjectIRDB>(Inp))) {}

  ICFGInput(
      AnalysisInputOf<LLVMProjectIRDB, analysis_input::EntryPoints,
                      LLVMVFTableProvider, DIBasedTypeHierarchy> auto &Inp,
      CallGraphAnalysisType CGTy)
      : ICF([CGTy](auto &Inp) {
          auto &IRDB = analysis_input::getResult<LLVMProjectIRDB>(Inp);
          auto &Entries =
              analysis_input::getResult<analysis_input::EntryPoints>(Inp);
          auto &VTP = analysis_input::getResult<LLVMVFTableProvider>(Inp);
          auto &TH = analysis_input::getResult<DIBasedTypeHierarchy>(Inp);
          auto PT = analysis_input::getResultOrNull<LLVMAliasInfo>(Inp);
          auto BaseCG = analysis_input::getResultOrNull<LLVMBasedICFG>(Inp);
          return buildImpl(IRDB, Entries, VTP, TH, PT, BaseCG, CGTy);
        }(Inp)) {}

  [[nodiscard]] auto &
  getResult(AnalysisResultTag<LLVMBasedICFG> /*unused*/) noexcept {
    return ICF;
  }

private:
  static LLVMBasedICFG
  buildImpl(LLVMProjectIRDB &IRDB, const std::vector<std::string> &Entries,
            const LLVMVFTableProvider &VTP, const DIBasedTypeHierarchy &TH,
            LLVMAliasInfoRef PT, const LLVMBasedICFG *BaseCG,
            CallGraphAnalysisType CGTy);

  LLVMBasedICFG ICF;
};

class SteensgaardAliasInfoInput {
public:
  explicit SteensgaardAliasInfoInput(
      AnalysisInputOf<LLVMProjectIRDB, LLVMBasedICFG> auto &Inp,
      UnionFindAliasAnalysisType UFAATy)
      : AI([UFAATy](auto &Inp) {
          auto &IRDB = analysis_input::getResult<LLVMProjectIRDB>(Inp);
          auto &BaseCG = analysis_input::getResult<LLVMBasedICFG>(Inp);
          return buildImpl(IRDB, &BaseCG, UFAATy);
        }(Inp)) {}

  [[nodiscard]] LLVMAliasInfoRef
  getResult(AnalysisResultTag<LLVMAliasInfoRef> /*unused*/) noexcept {
    return &AI;
  }

private:
  static LLVMRawAliasSet buildImpl(LLVMProjectIRDB &IRDB,
                                   const LLVMBasedICFG *BaseCG,
                                   UnionFindAliasAnalysisType UFAATy);

  LLVMRawAliasSet AI;
};

class AndersenAliasInfoInput {
public:
  explicit AndersenAliasInfoInput(
      const LLVMProjectIRDB &IRDB,
      llvm::ArrayRef<const llvm::Function *> Entries,
      ContextSensitivityOptions AAConfig,
      Soundness SoundnessFlag = Soundness::Soundy);

  explicit AndersenAliasInfoInput(
      AnalysisInputOf<LLVMProjectIRDB, analysis_input::EntryFunctions> auto
          &Inp,
      ContextSensitivityOptions AAConfig = {},
      Soundness SoundnessFlag = Soundness::Soundy)
      : AndersenAliasInfoInput(
            analysis_input::getResult<LLVMProjectIRDB>(Inp),
            analysis_input::getResult<analysis_input::EntryFunctions>(Inp),
            std::move(AAConfig), SoundnessFlag) {}

  [[nodiscard]] LLVMAliasInfoRef
  getResult(AnalysisResultTag<LLVMAliasInfoRef> /*unused*/) noexcept {
    return &AI;
  }

  [[nodiscard]] auto &
  getResult(AnalysisResultTag<LLVMBasedICFG> /*unused*/) noexcept {
    return ICF;
  }

private:
  explicit AndersenAliasInfoInput(
      const LLVMProjectIRDB &IRDB,
      std::pair<AndersenOTFResult,
                std::unique_ptr<ValueCompressor<PAGVariable>>>
          AARes);

  // Don't reorder: Initialization order is important!
  LLVMBasedICFG ICF;
  LLVMRawAliasSet AI;
};

// TODO: More

namespace detail {
[[nodiscard]] inline auto defaultPhasarInput() {
  return [](AnalysisInputOf<LLVMProjectIRDB> auto &&Inp) {
    return PSR_FWD(Inp)
        .template with<EntryPointsInput>()
        .template with<TypeHierarchyInput>()
        .template with<VFTableInput>();
  };
};
} // namespace detail

[[nodiscard]] inline auto phasarInputRoot(const llvm::Twine &IRFile) {
  return AnalysisInputImpl<AnalysisInputRoot, IRDBInput>(
      {}, std::make_unique<IRDBInput>(IRFile));
}
[[nodiscard]] inline auto phasarInputRoot(llvm::Module *IRModule) {
  return AnalysisInputImpl<AnalysisInputRoot, IRDBInput>(
      {}, std::make_unique<IRDBInput>(IRModule));
}

[[nodiscard]] inline auto phasarInput(const llvm::Twine &IRFile) {
  return phasarInputRoot(IRFile).with(detail::defaultPhasarInput());
}

[[nodiscard]] inline auto phasarInput(llvm::Module *IRModule) {
  return phasarInputRoot(IRModule).with(detail::defaultPhasarInput());
}

} // namespace psr
