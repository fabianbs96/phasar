/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_DB_PROJECTIRDB_H_
#define PHASAR_DB_PROJECTIRDB_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/PassManager.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/raw_os_ostream.h"

#include "phasar/Utils/EnumFlags.h"

namespace llvm {
class Value;
class Instruction;
class Type;
class Function;
class GlobalVariable;
} // namespace llvm

namespace psr {

enum class IRDBOptions : uint32_t { NONE = 0, WPA = (1 << 0), OWNS = (1 << 1) };

/**
 * This class owns the LLVM IR code of the project under analysis and some
 * very important information associated with the IR.
 * When an object of this class is destroyed it will clean up all IR related
 * stuff that is stored in it.
 */
class ProjectIRDB {
private:
  llvm::Module *WPAModule = nullptr;
  IRDBOptions Options;
  llvm::PassBuilder PB;
  llvm::ModuleAnalysisManager MAM;
  llvm::ModulePassManager MPM;
  // Stores all allocation instructions
  llvm::DenseSet<const llvm::Instruction *> AllocaInstructions;
  // Stores all allocated types
  llvm::DenseSet<const llvm::Type *> AllocatedTypes;
  // Return or resum instructions
  llvm::DenseSet<const llvm::Instruction *> RetOrResInstructions;
  // Stores the contexts
  std::unique_ptr<llvm::LLVMContext> Context;
  // Contains all modules that correspond to a project and owns them
  llvm::StringMap<std::unique_ptr<llvm::Module>> Modules;
  /// NOTE: IDInstructionMapping is improved in the
  /// IntelliSecPhasar-ImproveICFGExportPerformance branch. So, to avoid even
  /// more merge conflicts, don't change it here!
  // Maps an id to its corresponding instruction
  std::map<std::size_t, llvm::Instruction *> IDInstructionMapping;

  void buildIDModuleMapping(llvm::Module *M);

  void preprocessModule(llvm::Module *M);
  static bool wasCompiledWithDebugInfo(const llvm::Module *M) {
    return M->getNamedMetadata("llvm.dbg.cu") != nullptr;
  };

  void preprocessAllModules();

  [[nodiscard]] llvm::Function *
  internalGetFunction(llvm::StringRef FunctionName) const;
  [[nodiscard]] llvm::Function *
  internalGetFunctionDefinition(llvm::StringRef FunctionName) const;
  [[nodiscard]] llvm::Module *
  internalGetModuleDefiningFunction(llvm::StringRef FunctionName) const;

public:
  /// Constructs an empty ProjectIRDB
  ProjectIRDB(IRDBOptions Options);
  /// Constructs a ProjectIRDB from a bunch of LLVM IR files
  ProjectIRDB(const std::vector<std::string> &IRFiles,
              IRDBOptions Options = (IRDBOptions::WPA | IRDBOptions::OWNS),
              llvm::Linker::Flags LinkerFlags = llvm::Linker::LinkOnlyNeeded);
  /// Constructs a ProjecIRDB from a bunch of LLVM Modules
  ProjectIRDB(const std::vector<llvm::Module *> &Modules,
              IRDBOptions Options = IRDBOptions::WPA,
              llvm::Linker::Flags LinkerFlags = llvm::Linker::LinkOnlyNeeded);

  ProjectIRDB(ProjectIRDB &&) noexcept = default;
  ProjectIRDB &operator=(ProjectIRDB &&) = default;

  ProjectIRDB(ProjectIRDB &) = delete;
  ProjectIRDB &operator=(const ProjectIRDB &) = delete;

  ~ProjectIRDB();

  void insertModule(llvm::Module *M);

  // add WPA support by providing a fat completely linked module
  void
  linkForWPA(llvm::Linker::Flags LinkerFlags = llvm::Linker::LinkOnlyNeeded);
  // get a completely linked module for the WPA_MODE
  llvm::Module *getWPAModule();

  [[nodiscard]] inline bool containsSourceFile(llvm::StringRef File) const {
    return Modules.count(File);
  };

  [[nodiscard]] inline bool empty() const { return Modules.empty(); };

  [[nodiscard]] bool debugInfoAvailable() const;

  llvm::Module *getModule(llvm::StringRef ModuleName);

  [[nodiscard]] inline auto getAllModules() const noexcept {
    return llvm::map_range(
        Modules, [](const auto &Entry) { return Entry.getValue().get(); });
  }

  [[nodiscard]] std::vector<const llvm::Function *> getAllFunctions() const;

  [[nodiscard]] const llvm::Function *getFunctionById(unsigned Id);

  [[nodiscard]] inline const llvm::Function *
  getFunctionDefinition(llvm::StringRef FunctionName) const {
    return internalGetFunctionDefinition(FunctionName);
  }

  [[nodiscard]] inline llvm::Function *
  getFunctionDefinition(llvm::StringRef FunctionName) {
    return internalGetFunctionDefinition(FunctionName);
  }

  [[nodiscard]] inline const llvm::Function *
  getFunction(llvm::StringRef FunctionName) const {
    return internalGetFunction(FunctionName);
  }
  [[nodiscard]] inline llvm::Function *
  getFunction(llvm::StringRef FunctionName) {
    return internalGetFunction(FunctionName);
  }

  [[nodiscard]] const llvm::GlobalVariable *
  getGlobalVariableDefinition(llvm::StringRef GlobalVariableName) const;

  [[nodiscard]] inline llvm::Module *
  getModuleDefiningFunction(llvm::StringRef FunctionName) {
    return internalGetModuleDefiningFunction(FunctionName);
  }

  [[nodiscard]] inline const llvm::Module *
  getModuleDefiningFunction(llvm::StringRef FunctionName) const {
    return internalGetModuleDefiningFunction(FunctionName);
  }

  [[nodiscard]] inline const llvm::DenseSet<const llvm::Instruction *> &
  getAllocaInstructions() const {
    return AllocaInstructions;
  };

  /**
   * LLVM's intrinsic global variables are excluded.
   *
   * @brief Returns all stack and heap allocations, including global variables.
   */
  [[nodiscard]] std::set<const llvm::Value *> getAllMemoryLocations() const;

  [[nodiscard]] inline auto getAllSourceFiles() const {
    return llvm::map_range(Modules,
                           [](const auto &Entry) { return Entry.getKey(); });
  }

  [[nodiscard]] llvm::DenseSet<const llvm::Type *> getAllocatedTypes() const {
    return AllocatedTypes;
  };

  [[nodiscard]] llvm::DenseSet<const llvm::StructType *>
  getAllocatedStructTypes() const;

  [[nodiscard]] inline llvm::DenseSet<const llvm::Instruction *>
  getRetOrResInstructions() const {
    return RetOrResInstructions;
  };

  [[nodiscard]] inline std::size_t getNumberOfModules() const {
    return Modules.size();
  };

  [[nodiscard]] inline std::size_t getNumInstructions() const {
    return IDInstructionMapping.size();
  }

  [[nodiscard]] std::size_t getNumGlobals() const;

  [[nodiscard]] llvm::Instruction *getInstruction(std::size_t Id);

  [[nodiscard]] static std::size_t getInstructionID(const llvm::Instruction *I);

  void print() const;

  inline void emitPreprocessedIR(std::ostream &OS,
                                 bool ShortenIR = false) const {
    llvm::raw_os_ostream ROS(OS);
    emitPreprocessedIR(ROS, ShortenIR);
  }
  void emitPreprocessedIR(llvm::raw_ostream &OS = llvm::outs(),
                          bool ShortenIR = false) const;
  /**
   * Allows the (de-)serialization of Instructions, Arguments, GlobalValues and
   * Operands into unique Hexastore string representation.
   *
   * What values can be serialized and what scheme is used?
   *
   * 	1. Instructions
   *
   * 		<function name>.<id>
   *
   * 	2. Formal parameters
   *
   *		<function name>.f<arg-no>
   *
   *	3. Global variables
   *
   *		<global variable name>
   *
   *	4. ZeroValue
   *
   *		<ZeroValueInternalName>
   *
   *	5. Operand of an instruction
   *
   *		<function name>.<id>.o.<operand no>
   *
   * @brief Creates a unique string representation for any given
   * llvm::Value.
   */
  [[nodiscard]] static std::string valueToPersistedString(const llvm::Value *V);
  /**
   * @brief Convertes the given string back into the llvm::Value it represents.
   * @return Pointer to the converted llvm::Value.
   */
  [[nodiscard]] const llvm::Value *
  persistedStringToValue(const std::string &StringRep) const;
};

} // namespace psr

#endif
