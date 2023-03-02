/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_DATAFLOWSOLVER_IFDSIDE_PROBLEMS_IFDSNULLPOINTERDEREFERENCE_H
#define PHASAR_PHASARLLVM_DATAFLOWSOLVER_IFDSIDE_PROBLEMS_IFDSNULLPOINTERDEREFERENCE_H

#include <map>
#include <memory>
#include <set>
#include <string>

#include "phasar/PhasarLLVM/DataFlowSolver/IfdsIde/IFDSTabulationProblem.h"
#include "phasar/PhasarLLVM/Domain/LLVMAnalysisDomain.h"
#include "phasar/PhasarLLVM/Pointer/LLVMAliasInfo.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
class Instruction;
class Function;
class StructType;
class Value;
} // namespace llvm

namespace psr {	
class LLVMBasedICFG;
class LLVMTypeHierarchy;

class IFDSNullpointerDereference : public IFDSTabulationProblem<LLVMIFDSAnalysisDomainDefault> {
private:
	struct NullptrDerefResult {
		NullptrDerefResult() = default;
		unsigned int Line = 0;
		std::string FuncName;
		std::string FilePath;
		std::string SrcCode;
		std::vector<std::string> VarNames;
		std::map<IFDSNullpointerDereference::n_t,
             std::set<IFDSNullpointerDereference::d_t>>
        IRTrace;
    [[nodiscard]] bool empty() const;
    void print(llvm::raw_ostream &OS);
	};
	std::map<n_t, std::set<d_t>> NullptrReferencesFound;
public:
	IFDSNullpointerDereference(const LLVMProjectIRDB *IRDB, std::vector<std::string> EntryPoints = {"main"});

	~IFDSNullpointerDereference() override = default;

	FlowFunctionPtrType getNormalFlowFunction(n_t Curr, n_t Succ) override;

	FlowFunctionPtrType getCallFlowFunction(n_t CallSite, f_t DestFun) override;

	FlowFunctionPtrType getRetFlowFunction(n_t CallSite, f_t CalleeFun,
                                         n_t ExitStmt, n_t RetSite) override;

	FlowFunctionPtrType getCallToRetFlowFunction(n_t CallSite, n_t RetSite,
                                               llvm::ArrayRef<f_t> Callees) override;

	InitialSeeds<n_t, d_t, l_t> initialSeeds() override;

	[[nodiscard]] bool isZeroValue(IFDSNullpointerDereference::d_t Fact) const override;

	void printNode(llvm::raw_ostream &OS, n_t Stmt) const override;

  	void printDataFlowFact(llvm::raw_ostream &OS, d_t Fact) const override;

  	void printFunction(llvm::raw_ostream &OS, f_t Func) const override;

	[[nodiscard]] const std::map<n_t, std::set<d_t>> &getAllNullptrDerefsFound() const;

	std::vector<NullptrDerefResult> aggregateResults();
};
} // namespace psr

#endif
