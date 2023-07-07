/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/Config/Configuration.h"
#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/LLVMZeroValue.h"
#include "phasar/PhasarLLVM/Passes/FunctionAnnotationPass.h"
#include "phasar/PhasarLLVM/Passes/GeneralStatisticsAnalysis.h"
#include "phasar/PhasarLLVM/Passes/ValueAnnotationPass.h"
#include "phasar/PhasarLLVM/Utils/LLVMShorthands.h"
#include "phasar/Utils/EnumFlags.h"
#include "phasar/Utils/IO.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/Bitcode/BitcodeWriter.h"
#include "llvm/Demangle/Demangle.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Metadata.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Transforms/Utils.h"

#include <algorithm>
#include <cassert>
#include <charconv>
#include <filesystem>
#include <ostream>
#include <string>

using namespace std;

namespace psr {

llvm::PassManager<llvm::Module> ProjectIRDB::createDefaultPassManager() {
  llvm::PassManager<llvm::Module> PassManager;
  PassManager.addPass(ValueAnnotationPass());
  PassManager.addPass(FunctionAnnotationPass());
  return PassManager;
}

/// Constructs an empty ProjectIRDB
ProjectIRDB::ProjectIRDB(IRDBOptions Options,
                         llvm::PassManager<llvm::Module> ModulePasses)
    : Options(Options) {
  // register the GeneralStaticsPass analysis pass to the ModuleAnalysisManager
  // such that we can query its results later on
  MAM.registerPass([&]() { return GeneralStatisticsAnalysis(); });
  PB.registerModuleAnalyses(MAM);

  // add the transformation pass ValueAnnotationPass
  MPM.addPass(std::move(ModulePasses));

  // just to be sure that none of the passes messed up the module!
  MPM.addPass(llvm::VerifierPass());
  ModulesToSlotTracker::updateMSTForModule(LLVMZeroValueMod.get());
}

/// Constructs a ProjectIRDB from a bunch of LLVM IR files
ProjectIRDB::ProjectIRDB(const std::vector<std::string> &IRFiles,
                         llvm::PassManager<llvm::Module> Passes)
    : ProjectIRDB(IRFiles, DefaultIRDBOptionsIRFiles, std::move(Passes)){};

ProjectIRDB::ProjectIRDB(const std::vector<std::string> &IRFiles,
                         IRDBOptions Options,
                         llvm::PassManager<llvm::Module> Passes)
    : ProjectIRDB(IRFiles, Options, DefaultLinkerFlags, std::move(Passes)) {}

ProjectIRDB::ProjectIRDB(const std::vector<std::string> &IRFiles,
                         IRDBOptions Options, llvm::Linker::Flags LinkerFlags,
                         llvm::PassManager<llvm::Module> Passes)
    : ProjectIRDB(Options, std::move(Passes)) {

  Context = std::make_unique<llvm::LLVMContext>();

  for (const auto &File : IRFiles) {
    // if we have a file that is already compiled to llvm ir

    if ((File.find(".ll") != std::string::npos ||
         File.find(".bc") != std::string::npos) &&
        std::filesystem::exists(File)) {
      llvm::SMDiagnostic Diag;
      std::unique_ptr<llvm::Module> M = llvm::parseIRFile(File, Diag, *Context);
      bool BrokenDebugInfo = false;
      if (M == nullptr) {
        Diag.print(File.c_str(), llvm::errs());
      }
      /* Crash in presence of llvm-3.9.1 module (segfault) */
      if (M == nullptr ||
          llvm::verifyModule(*M, &llvm::errs(), &BrokenDebugInfo)) {
        throw std::runtime_error(File + " could not be parsed correctly");
      }
      if (BrokenDebugInfo) {
        llvm::errs() << "caution: debug info is broken\n";
      }
      Modules.insert(std::make_pair(File, std::move(M)));
    } else {
      throw std::invalid_argument(File + " is not a valid llvm module");
    }
  }
  if (Options & IRDBOptions::WPA) {
    linkForWPA(LinkerFlags);
  }
  preprocessAllModules();
}

ProjectIRDB::ProjectIRDB(const std::vector<llvm::Module *> &Modules,
                         IRDBOptions Options, llvm::Linker::Flags LinkerFlags)
    : ProjectIRDB(Modules, Options, createDefaultPassManager(), LinkerFlags) {}

ProjectIRDB::ProjectIRDB(const std::vector<llvm::Module *> &Modules,
                         IRDBOptions Options,
                         llvm::PassManager<llvm::Module> Passes,
                         llvm::Linker::Flags LinkerFlags)
    : ProjectIRDB(Options, std::move(Passes)) {

  for (auto *M : Modules) {
    this->Modules.try_emplace(M->getModuleIdentifier(), M);
  }

  if (Options & IRDBOptions::WPA) {
    linkForWPA(LinkerFlags);
  }

  preprocessAllModules();
}

ProjectIRDB::~ProjectIRDB() {
  for (auto *Mod : getAllModules()) {
    ModulesToSlotTracker::deleteMSTForModule(Mod);
  }
  // release resources if IRDB does not own
  if (!(Options & IRDBOptions::OWNS)) {
    // for (auto &Context : Contexts) {
    Context.release(); // NOLINT Just prevent the Context to be deleted
    //}
    for (auto &Entry : Modules) {
      // NOLINTNEXTLINE Just prevent the Module to be deleted
      Entry.getValue().release();
    }
  }
  MAM.clear();
}

void ProjectIRDB::preprocessModule(llvm::Module *M) {
  PAMM_GET_INSTANCE;
  // add moduleID to timer name if performing MWA!
  START_TIMER("LLVM Passes", PAMM_SEVERITY_LEVEL::Full);
  PHASAR_LOG_LEVEL(INFO, "Preprocess module: " << M->getModuleIdentifier());
  MPM.run(*M, MAM);
  // retrieve data from the GeneralStatisticsAnalysis registered earlier
  auto GSPResult = MAM.getResult<GeneralStatisticsAnalysis>(*M);
  StatsJson = GSPResult.getAsJson();
  NumberCallsites = GSPResult.getFunctioncalls();
  auto Allocas = GSPResult.getAllocaInstructions();
  AllocaInstructions.insert(Allocas.begin(), Allocas.end());
  auto ATypes = GSPResult.getAllocatedTypes();
  AllocatedTypes.insert(ATypes.begin(), ATypes.end());
  auto RRInsts = GSPResult.getRetResInstructions();
  RetOrResInstructions.insert(RRInsts.begin(), RRInsts.end());
  STOP_TIMER("LLVM Passes", PAMM_SEVERITY_LEVEL::Full);
  buildIDModuleMapping(M);
  ModulesToSlotTracker::updateMSTForModule(M);
}

void ProjectIRDB::linkForWPA(llvm::Linker::Flags LinkerFlags) {
  // Linking llvm modules:
  // Unfortunately linking between different contexts is currently not possible.
  // Therefore we must load all modules into one single context and then perform
  // the linkage. This is still very fast compared to compiling and
  // pre-processing
  // all modules.
  if (Modules.size() > 1) {
    llvm::Module *MainMod = getModuleDefiningFunction("main");

    if (!MainMod) {
      MainMod = Modules.begin()->second.get();
    }
    llvm::Linker Link(*MainMod);

    for (auto &Entry : Modules) {
      auto &Module = Entry.getValue();

      // we do not want to link a module with itself!
      if (Module.get() == MainMod) {
        continue;
      }

      if (&Module->getContext() == &MainMod->getContext()) {
        if (Link.linkInModule(std::move(Module), LinkerFlags)) {
          llvm::report_fatal_error(
              "Error: trying to link modules into single WPA module failed!");
        }
        continue;
      }

      // reload the modules into the module containing the main function
      std::string IRBuffer;
      llvm::raw_string_ostream RSO(IRBuffer);
      llvm::WriteBitcodeToFile(*Module, RSO);
      RSO.flush();
      llvm::SMDiagnostic ErrorDiagnostics;
      auto MemBuffer = llvm::MemoryBuffer::getMemBuffer(IRBuffer);
      auto TmpMod =
          llvm::parseIR(*MemBuffer, ErrorDiagnostics, MainMod->getContext());
      bool BrokenDebugInfo = false;
      if (!TmpMod ||
          llvm::verifyModule(*TmpMod, &llvm::errs(), &BrokenDebugInfo)) {
        llvm::report_fatal_error("Error: module is broken!");
      }
      if (BrokenDebugInfo) {
        // at least log this incident
        PHASAR_LOG_LEVEL(WARNING, "Reloading the module '"
                                      << Module->getName()
                                      << "' lead to broken debug info!")
      }
      // now we can safely perform the linking
      if (Link.linkInModule(std::move(TmpMod), LinkerFlags)) {
        llvm::report_fatal_error(
            "Error: trying to link modules into single WPA module failed!");
      }
    }

    // Update the IRDB reflecting that we now only need 'MainMod' and its
    // corresponding context!
    // delete every other module

    for (auto It = Modules.begin(); It != Modules.end();) {
      auto Old = It++;
      if (Old->second.get() != MainMod) {
        Modules.erase(Old);
      }
    }
    WPAModule = MainMod;
    ModulesToSlotTracker::updateMSTForModule(WPAModule);
  } else if (Modules.size() == 1) {
    // In this case we only have one module anyway, so we do not have
    // to link at all. But we have to update the WPAMOD pointer!
    WPAModule = Modules.begin()->second.get();
  }
  ModulesToSlotTracker::updateMSTForModule(WPAModule);
}

void ProjectIRDB::preprocessAllModules() {
  for (auto *Mod : getAllModules()) {
    preprocessModule(Mod);
  }
}

llvm::Module *ProjectIRDB::getWPAModule() {
  if (!WPAModule) {
    linkForWPA();
  }
  return WPAModule;
}

void ProjectIRDB::buildIDModuleMapping(llvm::Module *M) {
  for (auto &F : *M) {
    for (auto &I : llvm::instructions(F)) {
      AllInstructions.push_back(&I);
      if (AllInstructions.size() > 1) {
        auto PrevId =
            stol(getMetaDataID(AllInstructions[AllInstructions.size() - 2]));
        auto Id = stol(getMetaDataID(&I));
        if (PrevId + 1 != Id) {
          llvm::report_fatal_error("Invalid Id->Instruction mapping!");
        }
      } else {
        auto Id = stol(getMetaDataID(&I));
        FirstInstId = Id;
        assert(Id >= 0);
      }
    }
  }
}

llvm::Module *ProjectIRDB::getModule(llvm::StringRef ModuleName) {
  if (auto It = Modules.find(ModuleName); It != Modules.end()) {
    return It->getValue().get();
  }
  return nullptr;
}

std::size_t ProjectIRDB::getNumGlobals() const {
  std::size_t Ret = 0;
  for (const auto *Mod : getAllModules()) {
    Ret += Mod->global_size();
  }
  return Ret;
}

const llvm::Instruction *
ProjectIRDB::getInstruction(std::size_t Id) const noexcept {
  if (Id < FirstInstId || Id - FirstInstId >= AllInstructions.size()) {
    return nullptr;
  }
  return AllInstructions[Id - FirstInstId];
}

std::size_t ProjectIRDB::getInstructionID(const llvm::Instruction *I) {
  std::size_t Id = 0;
  if (auto *MD = llvm::cast<llvm::MDString>(
          I->getMetadata(PhasarConfig::MetaDataKind())->getOperand(0))) {
    Id = stol(MD->getString().str());
  }
  return Id;
}

void ProjectIRDB::print() const {
  for (const auto &Entry : Modules) {
    llvm::outs() << "Module: " << Entry.getKey() << '\n';
    llvm::outs() << *Entry.getValue();
    llvm::outs().flush();
  }
}

void ProjectIRDB::printAsJson(llvm::raw_ostream &OS) const {
  OS << StatsJson.dump(4) << '\n';
}

void ProjectIRDB::emitPreprocessedIR(llvm::raw_ostream &OS,
                                     bool ShortenIR) const {
  for (const auto &Entry : Modules) {
    auto File = Entry.getKey();
    auto *Module = Entry.getValue().get();
    OS << "IR module: " << File << '\n';
    // print globals
    for (auto &Glob : Module->globals()) {
      if (ShortenIR) {
        OS << llvmIRToShortString(&Glob);
      } else {
        OS << llvmIRToString(&Glob);
      }
      OS << '\n';
    }
    OS << '\n';
    for (const auto &Fun : *Module) {
      const auto *F = &Fun;
      if (!F->isDeclaration() && Module->getFunction(F->getName())) {
        OS << F->getName();
        if (auto FId = getFunctionId(F)) {
          OS << " | FunID: " << *FId << " {\n";
        } else {
          OS << " {\n";
        }
        for (const auto &BB : *F) {
          // do not print the label of the first BB
          if (BB.getPrevNode()) {
            OS << "\n<label ";
            BB.printAsOperand(OS, false);
            OS << ">\n";
          }
          // print all instructions
          for (const auto &I : BB) {
            OS << "  ";
            if (ShortenIR) {
              OS << llvmIRToShortString(&I);
            } else {
              OS << llvmIRToString(&I);
            }
            OS << '\n';
          }
        }
        OS << "}\n\n";
      }
    }
    OS << '\n';
  }
}

void ProjectIRDB::addFunctionToDB(const llvm::Function *F) {
  for (const auto &I : llvm::instructions(F)) {
    AllInstructions.push_back(&I);
    /// TODO: Should we add metadata IDs to them?
    /// TODO: Should we add them to AllocaInstructions and RetOrResInstructions?
  }
}

llvm::Function *
ProjectIRDB::internalGetFunctionDefinition(llvm::StringRef FunctionName) const {
  for (const auto *Mod : getAllModules()) {
    auto *F = Mod->getFunction(FunctionName);
    if (F && !F->isDeclaration()) {
      return F;
    }
  }
  return nullptr;
}

llvm::Function *
ProjectIRDB::internalGetFunction(llvm::StringRef FunctionName) const {
  for (const auto *Mod : getAllModules()) {
    auto *F = Mod->getFunction(FunctionName);
    if (F) {
      return F;
    }
  }
  return nullptr;
}

const llvm::GlobalVariable *ProjectIRDB::getGlobalVariableDefinition(
    llvm::StringRef GlobalVariableName) const {
  for (const auto *Mod : getAllModules()) {
    auto *G = Mod->getGlobalVariable(GlobalVariableName);
    if (G && !G->isDeclaration()) {
      return G;
    }
  }
  return nullptr;
}

llvm::Module *ProjectIRDB::internalGetModuleDefiningFunction(
    llvm::StringRef FunctionName) const {
  for (auto *Mod : getAllModules()) {
    auto *F = Mod->getFunction(FunctionName);
    if (F && !F->isDeclaration()) {
      return Mod;
    }
  }
  return nullptr;
}

std::string ProjectIRDB::valueToPersistedString(const llvm::Value *V) {
  if (LLVMZeroValue::isLLVMZeroValue(V)) {
    return LLVMZeroValue::getInstance()->getName().str();
  }
  if (const auto *I = llvm::dyn_cast<llvm::Instruction>(V)) {
    return I->getFunction()->getName().str() + "." + getMetaDataID(I);
  }
  if (const auto *A = llvm::dyn_cast<llvm::Argument>(V)) {
    return A->getParent()->getName().str() + ".f" +
           std::to_string(A->getArgNo());
  }
  if (const auto *G = llvm::dyn_cast<llvm::GlobalValue>(V)) {
    llvm::outs() << "special case: WE ARE AN GLOBAL VARIABLE\n";
    llvm::outs() << "all user:\n";
    for (const auto *User : V->users()) {
      if (const auto *I = llvm::dyn_cast<llvm::Instruction>(User)) {
        llvm::outs() << I->getFunction()->getName().str() << "\n";
      }
    }
    return G->getName().str();
  }
  if (llvm::isa<llvm::Value>(V)) {
    // In this case we should have an operand of an instruction which can be
    // identified by the instruction id and the operand index.
    llvm::outs() << "special case: WE ARE AN OPERAND\n";
    // We should only have one user in this special case
    for (const auto *User : V->users()) {
      if (const auto *I = llvm::dyn_cast<llvm::Instruction>(User)) {
        for (unsigned Idx = 0; Idx < I->getNumOperands(); ++Idx) {
          if (I->getOperand(Idx) == V) {
            return I->getFunction()->getName().str() + "." + getMetaDataID(I) +
                   ".o." + std::to_string(Idx);
          }
        }
      }
    }
    llvm::report_fatal_error("Error: llvm::Value is of unexpected type.");
    return "";
  }
  llvm::report_fatal_error("Error: llvm::Value is of unexpected type.");
  return "";
}

const llvm::Value *
ProjectIRDB::persistedStringToValue(const std::string &S) const {
  if (S.find(LLVMZeroValue::getInstance()->getName()) != std::string::npos) {
    return LLVMZeroValue::getInstance();
  }
  if (S.find('.') == std::string::npos) {
    return getGlobalVariableDefinition(S);
  }
  if (S.find(".f") != std::string::npos) {
    unsigned Argno = stoi(S.substr(S.find(".f") + 2, S.size()));
    return getNthFunctionArgument(
        getFunctionDefinition(S.substr(0, S.find(".f"))), Argno);
  }
  if (S.find(".o.") != std::string::npos) {
    unsigned I = S.find('.');
    unsigned J = S.find(".o.");
    unsigned InstID = stoi(S.substr(I + 1, J));
    // llvm::outs() << "FOUND instID: " << instID << "\n";
    unsigned OpIdx = stoi(S.substr(J + 3, S.size()));
    // llvm::outs() << "FOUND opIdx: " << to_string(opIdx) << "\n";
    const llvm::Function *F = getFunctionDefinition(S.substr(0, S.find('.')));
    for (const auto &BB : *F) {
      for (const auto &Inst : BB) {
        if (getMetaDataID(&Inst) == std::to_string(InstID)) {
          return Inst.getOperand(OpIdx);
        }
      }
    }
    llvm::report_fatal_error("Error: operand not found.");
  } else if (S.find('.') != std::string::npos) {
    const llvm::Function *F = getFunctionDefinition(S.substr(0, S.find('.')));
    for (const auto &BB : *F) {
      for (const auto &Inst : BB) {
        if (getMetaDataID(&Inst) == S.substr(S.find('.') + 1, S.size())) {
          return &Inst;
        }
      }
    }
    llvm::report_fatal_error("Error: llvm::Instruction not found.");
  } else {
    llvm::report_fatal_error(
        "Error: string cannot be translated into llvm::Value.");
  }
  return nullptr;
}

std::vector<const llvm::Function *> ProjectIRDB::getAllFunctions() const {
  std::vector<const llvm::Function *> Functions;
  for (const auto *Mod : getAllModules()) {
    for (const auto &F : *Mod) {
      Functions.push_back(&F);
    }
  }
  return Functions;
}

const llvm::Function *ProjectIRDB::getFunctionById(unsigned Id) const {
  /// Maybe cache this mapping later on
  for (const auto *Mod : getAllModules()) {
    for (const auto &F : *Mod) {
      auto FId = getFunctionId(&F);
      if (FId && *FId == Id) {
        return &F;
      }
    }
  }
  return nullptr;
}

void ProjectIRDB::insertFunction(llvm::Function *F) {
  assert(WPAModule && "insertFunction is only suported in WPA mode!");
  /// TODO: Can we safely ignore the metadata ids for generated functions?

  // auto Id = IDInstructionMapping.size() + NumGlobals;
  // auto &Context = F->getContext();
  // for (auto &Inst : llvm::instructions(F)) {
  //   llvm::MDNode *Node = llvm::MDNode::get(
  //       Context, llvm::MDString::get(Context, std::to_string(Id)));
  //   Inst.setMetadata(PhasarConfig::MetaDataKind(), Node);

  //   IDInstructionMapping[Id] = &Inst;
  //   ++Id;
  // }
}

llvm::DenseSet<const llvm::StructType *>
ProjectIRDB::getAllocatedStructTypes() const {
  llvm::DenseSet<const llvm::StructType *> StructTypes;
  for (const auto *Ty : AllocatedTypes) {
    if (const auto *StructTy = llvm::dyn_cast<llvm::StructType>(Ty)) {
      StructTypes.insert(StructTy);
    }
  }
  return StructTypes;
}

set<const llvm::Value *> ProjectIRDB::getAllMemoryLocations() const {
  // get all stack and heap alloca instructions
  auto AllocaInsts = getAllocaInstructions();
  set<const llvm::Value *> AllMemoryLoc;
  for (const auto *AllocaInst : AllocaInsts) {
    AllMemoryLoc.insert(static_cast<const llvm::Value *>(AllocaInst));
  }
  set<string> IgnoredGlobalNames = {"llvm.used",
                                    "llvm.compiler.used",
                                    "llvm.global_ctors",
                                    "llvm.global_dtors",
                                    "vtable",
                                    "typeinfo"};
  // add global varibales to the memory location set, except the llvm
  // intrinsic global variables
  for (const auto *Mod : getAllModules()) {
    for (const auto &GV : Mod->globals()) {
      if (GV.hasName()) {
        string GVName = llvm::demangle(GV.getName().str());
        if (!IgnoredGlobalNames.count(GVName.substr(0, GVName.find(' ')))) {
          AllMemoryLoc.insert(&GV);
        }
      }
    }
  }
  return AllMemoryLoc;
}

bool ProjectIRDB::debugInfoAvailable() const {
  if (WPAModule) {
    return wasCompiledWithDebugInfo(WPAModule);
  }
  // During unittests WPAMOD might not be set
  if (!Modules.empty()) {
    for (const auto *Mod : getAllModules()) {
      if (!wasCompiledWithDebugInfo(Mod)) {
        return false;
      }
    }
    return true;
  }
  return false;
}

} // namespace psr

const llvm::Value *psr::fromMetaDataId(const ProjectIRDB &IRDB,
                                       llvm::StringRef Id) {
  if (Id.empty() || Id[0] == '-') {
    return nullptr;
  }

  auto ParseInt = [](llvm::StringRef Str) -> std::optional<unsigned> {
    unsigned Num;
    auto [Ptr, EC] = std::from_chars(Str.data(), Str.data() + Str.size(), Num);

    if (EC == std::errc{}) {
      return Num;
    }

    PHASAR_LOG_LEVEL(WARNING, "Invalid metadata id '"
                                  << Str.str() << "': "
                                  << std::make_error_code(EC).message());
    return std::nullopt;
  };

  if (auto Dot = Id.find('.'); Dot != llvm::StringRef::npos) {
    auto FName = Id.slice(0, Dot);

    auto ArgNr = ParseInt(Id.drop_front(Dot + 1));

    if (!ArgNr) {
      return nullptr;
    }

    const auto *F = IRDB.getFunction(FName);
    if (F) {
      return getNthFunctionArgument(F, *ArgNr);
    }

    return nullptr;
  }

  auto IdNr = ParseInt(Id);
  return IdNr ? IRDB.getInstruction(*IdNr) : nullptr;
}