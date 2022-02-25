/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

/*
 * LLVMBasedInterproceduralICFG.cpp
 *
 *  Created on: 09.09.2016
 *      Author: pdschbrt
 */

#include <cassert>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iomanip>
#include <iterator>
#include <memory>
#include <memory_resource>
#include <ostream>
#include <string>

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/ADT/SmallVector.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/IR/AbstractCallSite.h"
#include "llvm/IR/BasicBlock.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/InstrTypes.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Module.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

#include "boost/graph/copy.hpp"
#include "boost/graph/depth_first_search.hpp"
#include "boost/graph/graph_utility.hpp"
#include "boost/graph/graphviz.hpp"
#include "boost/range/iterator_range_core.hpp"

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/PhasarLLVM/ControlFlow/LLVMBasedICFG.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/CHAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/DTAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/NOResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/OTFResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/RTAResolver.h"
#include "phasar/PhasarLLVM/ControlFlow/Resolver/Resolver.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToGraph.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToInfo.h"
#include "phasar/PhasarLLVM/Pointer/LLVMPointsToSet.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMTypeHierarchy.h"
#include "phasar/PhasarLLVM/TypeHierarchy/LLVMVFTable.h"
#include "phasar/PhasarPass/Options.h"
#include "phasar/Utils/LLVMIRToSrc.h"
#include "phasar/Utils/LLVMShorthands.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/Utilities.h"

#include "nlohmann/json.hpp"

using namespace psr;
using namespace std;

// Define some handy helper functionalities
namespace {
template <class graphType> class VertexWriter {
public:
  VertexWriter(const graphType &CGraph) : CGraph(CGraph) {}
  template <class VertexOrEdge>
  void operator()(std::ostream &Out, const VertexOrEdge &V) const {
    const auto &Label = CGraph[V].getFunctionName();
    Out << "[label=" << boost::escape_dot_string(Label) << "]";
  }

private:
  const graphType &CGraph;
};

template <class graphType> class EdgeLabelWriter {
public:
  EdgeLabelWriter(const graphType &CGraph) : CGraph(CGraph) {}
  template <class VertexOrEdge>
  void operator()(std::ostream &Out, const VertexOrEdge &V) const {
    const auto &Label = CGraph[V].getCallSiteAsString();
    Out << "[label=" << boost::escape_dot_string(Label) << "]";
  }

private:
  const graphType &CGraph;
};

} // anonymous namespace

namespace psr {

struct LLVMBasedICFG::dependency_visitor : boost::default_dfs_visitor {
  std::vector<vertex_t> &Vertices;
  dependency_visitor(std::vector<vertex_t> &V) : Vertices(V) {}
  template <typename Vertex, typename Graph>
  void finish_vertex(Vertex U, const Graph & /*G*/) {
    Vertices.push_back(U);
  }
};

LLVMBasedICFG::VertexProperties::VertexProperties(const llvm::Function *F)
    : F(F) {}

std::string LLVMBasedICFG::VertexProperties::getFunctionName() const {
  return F->getName().str();
}

LLVMBasedICFG::EdgeProperties::EdgeProperties(const llvm::Instruction *I)
    : CS(I), ID(stoull(getMetaDataID(I))) {}

std::string LLVMBasedICFG::EdgeProperties::getCallSiteAsString() const {
  return llvmIRToString(CS);
}

// Need to provide copy constructor explicitly to avoid multiple frees of TH and
// PT in case any of them is allocated within the constructor. To this end, we
// set UserTHInfos and UserPTInfos to true here.
LLVMBasedICFG::LLVMBasedICFG(const LLVMBasedICFG &ICF)
    : ICFG(ICF), LLVMBasedCFG(ICF), IRDB(ICF.IRDB), TH(ICF.TH), PT(ICF.PT),
      // TODO copy resolver
      Res(nullptr), VisitedFunctions(ICF.VisitedFunctions), S(ICF.S),
      CGType(ICF.CGType), CallGraph(ICF.CallGraph),
      FunctionVertexMap(ICF.FunctionVertexMap) {}

LLVMBasedICFG::LLVMBasedICFG(ProjectIRDB &IRDB, CallGraphAnalysisType CGType,
                             const std::set<std::string> &EntryPoints,
                             LLVMTypeHierarchy *TH, LLVMPointsToInfo *PT,
                             Soundness S, bool IncludeGlobals)
    : IRDB(IRDB), TH(TH), PT(PT), S(S), CGType(CGType) {

  //   std::chrono::high_resolution_clock::time_point StartTime =
  //       std::chrono::high_resolution_clock::now();
  PAMM_GET_INSTANCE;
  // check for faults in the logic
  if (!TH && (CGType != CallGraphAnalysisType::NORESOLVE)) {
    // no type hierarchy information provided by the user,
    // we need to construct a type hierarchy ourselfes
    this->TH = new LLVMTypeHierarchy(IRDB);
    UserTHInfos = false;
  }
  if (!PT && (CGType == CallGraphAnalysisType::OTF)) {
    // no pointer information provided by the user,
    // we need to construct a points-to infos ourselfes
    this->PT = new LLVMPointsToSet(IRDB);
    UserPTInfos = false;
  }
  if (this->PT == nullptr) {
    llvm::report_fatal_error("LLVMPointsToInfo not passed and "
                             "CallGraphAnalysisType::OTF was not specified.");
  }
  if (EntryPoints.count("__ALL__")) {
    // Handle the special case in which a user wishes to treat all functions as
    // entry points.
    auto Funs = IRDB.getAllFunctions();
    for (const auto *Fun : Funs) {
      if (!Fun->isDeclaration() && Fun->hasName()) {
        UserEntryPoints.insert(IRDB.getFunctionDefinition(Fun->getName()));
      }
    }
  } else {
    for (const auto &EntryPoint : EntryPoints) {
      auto *F = IRDB.getFunctionDefinition(EntryPoint);
      if (F == nullptr) {
        llvm::report_fatal_error("Could not retrieve function for entry point");
      }
      UserEntryPoints.insert(F);
    }
  }
  if (IncludeGlobals) {
    assert(IRDB.getNumberOfModules() == 1 &&
           "IncludeGlobals is currently only supported for WPA");
    const auto *GlobCtor =
        buildCRuntimeGlobalCtorsDtorsModel(*IRDB.getWPAModule());
    FunctionWL.push_back(GlobCtor);
  } else {
    FunctionWL.insert(FunctionWL.end(), UserEntryPoints.begin(),
                      UserEntryPoints.end());
  }
  // instantiate the respective resolver type
  Res = makeResolver(IRDB, *this->TH, *this->PT);
  LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), INFO)
                << "Starting CallGraphAnalysisType: " << CGType);
  VisitedFunctions.reserve(IRDB.getAllFunctions().size());
  bool FixpointReached;
  do {
    FixpointReached = true;
    while (!FunctionWL.empty()) {
      const llvm::Function *F = FunctionWL.back();
      FunctionWL.pop_back();
      processFunction(F, *Res, FixpointReached);
    }

    if (S != Soundness::Unsound) {
      for (auto [CS, _] : IndirectCalls) {
        FixpointReached &= !constructDynamicCall(CS, *Res);
      }
    } else {
      for (auto &CS : UnsoundIndirectCalls) {
        FixpointReached &= !constructDynamicCall(CS, *Res);
      }
      UnsoundCallSites.insert(UnsoundIndirectCalls.begin(),
                              UnsoundIndirectCalls.end());
      UnsoundIndirectCalls.clear();
    }

  } while (!FixpointReached);

  //   int CallsitesToInstrument = 0;
  //   for (const auto &[IndirectCall, Targets] : IndirectCalls) {
  //     if (Targets == 0) {
  //       LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), WARNING)
  //                     << "No callees found for callsite "
  //                     << llvmIRToString(IndirectCall));
  //       CallsitesToInstrument++;
  //     }
  //   }
  //   std::cerr << "Callsites to instrument: " << CallsitesToInstrument <<
  //   '\n';

  //   std::chrono::high_resolution_clock::time_point EndTime =
  //       std::chrono::high_resolution_clock::now();

  //   std::cerr << "Callgraph construction took (ms): "
  //             <<
  //             std::chrono::duration_cast<std::chrono::milliseconds>(EndTime -
  //                                                                      StartTime)
  //                    .count()
  //             << '\n';
  //   std::cerr << "Callgraph vertices: " << getNumOfVertices()
  //             << ", edges: " << getNumOfEdges() << '\n';

  REG_COUNTER("CG Vertices", getNumOfVertices(), PAMM_SEVERITY_LEVEL::Full);
  REG_COUNTER("CG Edges", getNumOfEdges(), PAMM_SEVERITY_LEVEL::Full);
  LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), INFO)
                << "Call graph has been constructed");
}

LLVMBasedICFG::~LLVMBasedICFG() {
  // if we had to compute type hierarchy or points-to information ourselfs,
  // we need to clean up
  if (!UserTHInfos) {
    delete TH;
  }
  if (!UserPTInfos) {
    delete PT;
  }
}

void LLVMBasedICFG::processFunction(const llvm::Function *F, Resolver &Resolver,
                                    bool &FixpointReached) {
  LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                << "Walking in function: " << F->getName().str());
  if (F->isDeclaration() || !VisitedFunctions.insert(F).second) {
    LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                  << "Function already visited or only declaration: "
                  << F->getName().str());
    return;
  }

  // add a node for function F to the call graph (if not present already)
  vertex_t ThisFunctionVertexDescriptor;
  auto FvmItr = FunctionVertexMap.find(F);
  if (FvmItr != FunctionVertexMap.end()) {
    ThisFunctionVertexDescriptor = FvmItr->second;
  } else {
    ThisFunctionVertexDescriptor =
        boost::add_vertex(VertexProperties(F), CallGraph);
    FunctionVertexMap[F] = ThisFunctionVertexDescriptor;
  }

  // iterate all instructions of the current function
  Resolver::FunctionSetTy PossibleTargets;
  for (const auto &I : llvm::instructions(F)) {
    if (const auto *CS = llvm::dyn_cast<llvm::CallBase>(&I)) {
      Resolver.preCall(&I);

      // check if function call can be resolved statically
      if (CS->getCalledFunction() != nullptr) {
        PossibleTargets.insert(CS->getCalledFunction());
        LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                      << "Found static call-site: "
                      << "  " << llvmIRToString(CS));
      } else {
        // still try to resolve the called function statically
        const llvm::Value *SV = CS->getCalledOperand()->stripPointerCasts();
        const llvm::Function *ValueFunction =
            !SV->hasName() ? nullptr : IRDB.getFunction(SV->getName());
        if (ValueFunction) {
          PossibleTargets.insert(ValueFunction);
          LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                        << "Found static call-site: " << llvmIRToString(CS));
        } else {
          if (llvm::isa<llvm::InlineAsm>(SV)) {
            continue;
          }
          // the function call must be resolved dynamically
          LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                        << "Found dynamic call-site: "
                        << "  " << llvmIRToString(CS));
          IndirectCalls[CS] = 0;

          if (S != Soundness::Unsound) {
            FixpointReached = false;
          } else {
            UnsoundIndirectCalls.push_back(CS);
          }

          continue;
        }
      }

      LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                    << "Found " << PossibleTargets.size()
                    << " possible target(s)");

      Resolver.handlePossibleTargets(CS, PossibleTargets);
      // Insert possible target inside the graph and add the link with
      // the current function
      for (const auto &PossibleTarget : PossibleTargets) {
        vertex_t TargetVertex;
        auto TargetFvmItr = FunctionVertexMap.find(PossibleTarget);
        if (TargetFvmItr != FunctionVertexMap.end()) {
          TargetVertex = TargetFvmItr->second;
        } else {
          TargetVertex =
              boost::add_vertex(VertexProperties(PossibleTarget), CallGraph);
          FunctionVertexMap[PossibleTarget] = TargetVertex;
        }
        boost::add_edge(ThisFunctionVertexDescriptor, TargetVertex,
                        EdgeProperties(CS), CallGraph);
      }

      // continue resolving
      FunctionWL.insert(FunctionWL.end(), PossibleTargets.begin(),
                        PossibleTargets.end());

      Resolver.postCall(&I);
    } else {
      Resolver.otherInst(&I);
    }
    PossibleTargets.clear();
  }
}

bool LLVMBasedICFG::constructDynamicCall(const llvm::Instruction *I,
                                         Resolver &Resolver) {

  bool NewTargetsFound = false;
  // Find vertex of calling function.
  vertex_t ThisFunctionVertexDescriptor;
  auto FvmItr = FunctionVertexMap.find(I->getFunction());
  if (FvmItr != FunctionVertexMap.end()) {
    ThisFunctionVertexDescriptor = FvmItr->second;
  } else {
    LOG_IF_ENABLE(
        BOOST_LOG_SEV(lg::get(), ERROR)
        << "constructDynamicCall: Did not find vertex of calling function "
        << I->getFunction()->getName().str() << " at callsite "
        << llvmIRToString(I));
    std::terminate();
  }

  if (const auto *CallSite = llvm::dyn_cast<llvm::CallBase>(I)) {
    Resolver.preCall(I);

    // the function call must be resolved dynamically
    LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                  << "Looking into dynamic call-site: ");
    LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG) << "  " << llvmIRToString(I));
    // call the resolve routine

    auto PossibleTargets = LLVMBasedICFG::isVirtualFunctionCall(CallSite)
                               ? Resolver.resolveVirtualCall(CallSite)
                               : Resolver.resolveFunctionPointer(CallSite);

    assert(IndirectCalls.count(I));

    if (IndirectCalls[I] < PossibleTargets.size()) {
      LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                    << "Found " << PossibleTargets.size() - IndirectCalls[I]
                    << " new possible target(s)");
      IndirectCalls[I] = PossibleTargets.size();
      NewTargetsFound = true;
    }
    if (!NewTargetsFound) {
      return NewTargetsFound;
    }
    // Throw out already found targets
    for (const auto &OE : boost::make_iterator_range(
             boost::out_edges(ThisFunctionVertexDescriptor, CallGraph))) {
      if (CallGraph[OE].CS == I) {
        PossibleTargets.erase(CallGraph[boost::target(OE, CallGraph)].F);
      }
    }
    Resolver.handlePossibleTargets(CallSite, PossibleTargets);
    // Insert possible target inside the graph and add the link with
    // the current function
    for (const auto &PossibleTarget : PossibleTargets) {
      vertex_t TargetVertex;
      auto TargetFvmItr = FunctionVertexMap.find(PossibleTarget);
      if (TargetFvmItr != FunctionVertexMap.end()) {
        TargetVertex = TargetFvmItr->second;
      } else {
        TargetVertex =
            boost::add_vertex(VertexProperties(PossibleTarget), CallGraph);
        FunctionVertexMap[PossibleTarget] = TargetVertex;
      }
      boost::add_edge(ThisFunctionVertexDescriptor, TargetVertex,
                      EdgeProperties(I), CallGraph);
    }

    // continue resolving
    FunctionWL.insert(FunctionWL.end(), PossibleTargets.begin(),
                      PossibleTargets.end());

    Resolver.postCall(I);
  } else {
    Resolver.otherInst(I);
  }

  return NewTargetsFound;
}

bool LLVMBasedICFG::addRuntimeEdges(
    llvm::ArrayRef<std::pair<unsigned, unsigned>> CallerCalleeMap) {
  bool ICFGAugumented = false;
  for (const auto &[CallSiteId, FunctionPrefixId] : CallerCalleeMap) {
    const auto *CallSite = IRDB.getInstruction(CallSiteId);
    assert(CallSite && "addRuntimeEdges: callSite not found in IRDB");
    const auto *CallBase = llvm::dyn_cast<llvm::CallBase>(CallSite);
    if (!CallBase) {
      continue;
    }
    const auto *RuntimeCallTarget = IRDB.getFunctionById(FunctionPrefixId);
    if (!RuntimeCallTarget) {
      /// TODO: At least log this incident
      continue;
    }
    vertex_t CallSiteVertexDescriptor;
    if (auto FvmItr = FunctionVertexMap.find(CallSite->getFunction());
        FvmItr != FunctionVertexMap.end()) {
      CallSiteVertexDescriptor = FvmItr->second;
    } else {
      LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), ERROR)
                    << "addRuntimeEdges: Did not find vertex of "
                       "calling function "
                    << CallSite->getFunction()->getName().str()
                    << " at callsite " << llvmIRToString(CallSite));
      continue;
    }

    vertex_t RuntimeTargetVertex;
    auto TargetFvmItr = FunctionVertexMap.find(RuntimeCallTarget);
    if (TargetFvmItr != FunctionVertexMap.end()) {
      RuntimeTargetVertex = TargetFvmItr->second;
    } else {
      RuntimeTargetVertex =
          boost::add_vertex(VertexProperties(RuntimeCallTarget), CallGraph);
      FunctionVertexMap[RuntimeCallTarget] = RuntimeTargetVertex;
    }

    if (!boost::edge(CallSiteVertexDescriptor, RuntimeTargetVertex, CallGraph)
             .second) {
      Res->preCall(CallSite);
      boost::add_edge(CallSiteVertexDescriptor, RuntimeTargetVertex,
                      EdgeProperties(CallSite), CallGraph);
      // Updating the points-to information
      psr::Resolver::FunctionSetTy CallTargetSet{RuntimeCallTarget};
      Res->handlePossibleTargets(CallBase, CallTargetSet);
      TotalRuntimeEdgesAdded++;
      ICFGAugumented = true;

      Res->postCall(CallSite);
    }
  }
  return ICFGAugumented;
}

std::unique_ptr<Resolver> LLVMBasedICFG::makeResolver(ProjectIRDB &IRDB,
                                                      LLVMTypeHierarchy &TH,
                                                      LLVMPointsToInfo &PT) {
  switch (CGType) {
  case (CallGraphAnalysisType::NORESOLVE):
    return make_unique<NOResolver>(IRDB);
    break;
  case (CallGraphAnalysisType::CHA):
    return make_unique<CHAResolver>(IRDB, TH);
    break;
  case (CallGraphAnalysisType::RTA):
    return make_unique<RTAResolver>(IRDB, TH);
    break;
  case (CallGraphAnalysisType::DTA):
    return make_unique<DTAResolver>(IRDB, TH);
    break;
  case (CallGraphAnalysisType::OTF):
    return make_unique<OTFResolver>(IRDB, TH, *this, PT);
    break;
  default:
    llvm::report_fatal_error("Resolver strategy not properly instantiated");
    break;
  }
}

bool LLVMBasedICFG::isIndirectFunctionCall(const llvm::Instruction *N) const {
  const auto *CallSite = llvm::dyn_cast<llvm::CallBase>(N);
  return CallSite && CallSite->isIndirectCall();
}

bool LLVMBasedICFG::isVirtualFunctionCall(const llvm::Instruction *N) const {
  const auto *CallSite = llvm::dyn_cast<llvm::CallBase>(N);
  if (!CallSite) {
    return false;
  }
  // check potential receiver type
  const auto *RecType = getReceiverType(CallSite);
  if (!RecType) {
    return false;
  }
  if (!TH->hasType(RecType)) {
    return false;
  }
  if (!TH->hasVFTable(RecType)) {
    return false;
  }
  return getVFTIndex(CallSite) >= 0;
}

const llvm::Function *LLVMBasedICFG::getFunction(const string &Fun) const {
  return IRDB.getFunction(Fun);
}

const llvm::Function *LLVMBasedICFG::getFirstGlobalCtorOrNull() const {
  auto It = GlobalCtors.begin();
  if (It != GlobalCtors.end()) {
    return It->second;
  }
  return nullptr;
}
const llvm::Function *LLVMBasedICFG::getLastGlobalDtorOrNull() const {
  auto It = GlobalDtors.rbegin();
  if (It != GlobalDtors.rend()) {
    return It->second;
  }
  return nullptr;
}

vector<const llvm::Function *> LLVMBasedICFG::getAllFunctions() const {
  return IRDB.getAllFunctions();
}

boost::container::flat_set<const llvm::Function *>
LLVMBasedICFG::getAllVertexFunctions() const {
  boost::container::flat_set<const llvm::Function *> VertexFuncs;
  VertexFuncs.reserve(FunctionVertexMap.size());
  for (auto V : FunctionVertexMap) {
    VertexFuncs.insert(V.first);
  }
  return VertexFuncs;
}

std::vector<const llvm::Instruction *>
LLVMBasedICFG::getOutEdges(const llvm::Function *F) const {
  auto FunctionMapIt = FunctionVertexMap.find(F);
  if (FunctionMapIt == FunctionVertexMap.end()) {
    return {};
  }

  std::vector<const llvm::Instruction *> Edges;
  for (const auto EdgeIt : boost::make_iterator_range(
           boost::out_edges(FunctionMapIt->second, CallGraph))) {
    auto Edge = CallGraph[EdgeIt];
    Edges.push_back(Edge.CS);
  }

  return Edges;
}

LLVMBasedICFG::OutEdgesAndTargets
LLVMBasedICFG::getOutEdgeAndTarget(const llvm::Function *F) const {
  auto FunctionMapIt = FunctionVertexMap.find(F);
  if (FunctionMapIt == FunctionVertexMap.end()) {
    return {};
  }

  OutEdgesAndTargets Edges;
  for (const auto EdgeIt : boost::make_iterator_range(
           boost::out_edges(FunctionMapIt->second, CallGraph))) {
    auto Edge = CallGraph[EdgeIt];
    auto Target = CallGraph[boost::target(EdgeIt, CallGraph)];
    Edges.insert(std::make_pair(Edge.CS, Target.F));
  }

  return Edges;
}

size_t LLVMBasedICFG::removeEdges(const llvm::Function *F,
                                  const llvm::Instruction *I) {
  auto FunctionMapIt = FunctionVertexMap.find(F);
  if (FunctionMapIt == FunctionVertexMap.end()) {
    return 0;
  }

  size_t EdgesRemoved = 0;
  auto OutEdges = boost::out_edges(FunctionMapIt->second, CallGraph);
  for (auto EdgeIt : boost::make_iterator_range(OutEdges)) {
    auto Edge = CallGraph[EdgeIt];
    if (Edge.CS == I) {
      boost::remove_edge(EdgeIt, CallGraph);
      ++EdgesRemoved;
    }
  }
  return EdgesRemoved;
}

bool LLVMBasedICFG::removeVertex(const llvm::Function *F) {
  auto FunctionMapIt = FunctionVertexMap.find(F);
  if (FunctionMapIt == FunctionVertexMap.end()) {
    return false;
  }

  boost::remove_vertex(FunctionMapIt->second, CallGraph);
  FunctionVertexMap.erase(FunctionMapIt);
  return true;
}

size_t LLVMBasedICFG::getCallerCount(const llvm::Function *F) const {
  auto MapEntry = FunctionVertexMap.find(F);
  if (MapEntry == FunctionVertexMap.end()) {
    return 0;
  }

  auto EdgeIterators = boost::in_edges(MapEntry->second, CallGraph);
  return std::distance(EdgeIterators.first, EdgeIterators.second);
}

set<const llvm::Function *>
LLVMBasedICFG::getCalleesOfCallAt(const llvm::Instruction *N) const {
  if (!llvm::isa<llvm::CallBase>(N)) {
    return {};
  }

  auto MapEntry = FunctionVertexMap.find(N->getFunction());
  if (MapEntry == FunctionVertexMap.end()) {
    return {};
  }

  set<const llvm::Function *> Callees;

  out_edge_iterator EI;
  out_edge_iterator EIEnd;
  for (boost::tie(EI, EIEnd) = boost::out_edges(MapEntry->second, CallGraph);
       EI != EIEnd; ++EI) {
    auto Edge = CallGraph[*EI];
    if (N == Edge.CS) {
      auto Target = boost::target(*EI, CallGraph);
      Callees.insert(CallGraph[Target].F);
    }
  }
  return Callees;
}

llvm::SmallPtrSet<const llvm::Function *, 8>
LLVMBasedICFG::internalGetCalleesOfCallAt(const llvm::Instruction *N) const {
  llvm::SmallPtrSet<const llvm::Function *, 8> Callees;
  if (!llvm::isa<llvm::CallBase>(N)) {
    return Callees;
  }

  auto MapEntry = FunctionVertexMap.find(N->getFunction());
  if (MapEntry == FunctionVertexMap.end()) {
    return Callees;
  }

  for (auto EdgeDesc : boost::make_iterator_range(
           boost::out_edges(MapEntry->second, CallGraph))) {
    auto Edge = CallGraph[EdgeDesc];
    if (N != Edge.CS) {
      continue;
    }
    auto Target = boost::target(EdgeDesc, CallGraph);
    const auto *F = CallGraph[Target].F;
    Callees.insert(F);
  }

  return Callees;
}

void LLVMBasedICFG::forEachCalleeOfCallAt(
    const llvm::Instruction *I,
    llvm::function_ref<void(const llvm::Function *)> Callback) const {
  if (!llvm::isa<llvm::CallBase>(I)) {
    return;
  }

  auto MapEntry = FunctionVertexMap.find(I->getFunction());
  if (MapEntry == FunctionVertexMap.end()) {
    return;
  }

  for (auto EdgeDesc : boost::make_iterator_range(
           boost::out_edges(MapEntry->second, CallGraph))) {
    auto Edge = CallGraph[EdgeDesc];
    if (I != Edge.CS) {
      continue;
    }
    auto Target = boost::target(EdgeDesc, CallGraph);
    const auto *F = CallGraph[Target].F;
    Callback(F);
  }
}

set<const llvm::Instruction *>
LLVMBasedICFG::getCallersOf(const llvm::Function *F) const {
  set<const llvm::Instruction *> CallersOf;
  auto MapEntry = FunctionVertexMap.find(F);
  if (MapEntry == FunctionVertexMap.end()) {
    return CallersOf;
  }
  in_edge_iterator EI;

  in_edge_iterator EIEnd;
  for (boost::tie(EI, EIEnd) = boost::in_edges(MapEntry->second, CallGraph);
       EI != EIEnd; ++EI) {
    auto Edge = CallGraph[*EI];
    CallersOf.insert(Edge.CS);
  }
  return CallersOf;
}

set<const llvm::Instruction *>
LLVMBasedICFG::getCallsFromWithin(const llvm::Function *F) const {
  set<const llvm::Instruction *> CallSites;
  for (const auto &I : llvm::instructions(F)) {
    if (llvm::isa<llvm::CallBase>(I)) {
      CallSites.insert(&I);
    }
  }
  return CallSites;
}

/**
 * Returns all statements to which a call could return.
 * In the RHS paper, for every call there is just one return site.
 * We, however, use as return site the successor statements, of which
 * there can be many in case of exceptional flow.
 */
set<const llvm::Instruction *>
LLVMBasedICFG::getReturnSitesOfCallAt(const llvm::Instruction *N) const {
  set<const llvm::Instruction *> ReturnSites;
  if (const auto *Invoke = llvm::dyn_cast<llvm::InvokeInst>(N)) {
    const llvm::Instruction *NormalSucc = &Invoke->getNormalDest()->front();
    auto *UnwindSucc = &Invoke->getUnwindDest()->front();
    if (!IgnoreDbgInstructions &&
        llvm::isa<llvm::DbgInfoIntrinsic>(NormalSucc)) {
      NormalSucc = NormalSucc->getNextNonDebugInstruction(
          false /*Only debug instructions*/);
    }
    if (!IgnoreDbgInstructions &&
        llvm::isa<llvm::DbgInfoIntrinsic>(UnwindSucc)) {
      UnwindSucc = UnwindSucc->getNextNonDebugInstruction(
          false /*Only debug instructions*/);
    }
    if (NormalSucc != nullptr) {
      ReturnSites.insert(NormalSucc);
    }
    if (UnwindSucc != nullptr) {
      ReturnSites.insert(UnwindSucc);
    }
  } else {
    auto Succs = getSuccsOf(N);
    ReturnSites.insert(Succs.begin(), Succs.end());
  }
  return ReturnSites;
}

void LLVMBasedICFG::collectGlobalCtors() {
  for (const auto *Module : IRDB.getAllModules()) {
    insertGlobalCtorsDtorsImpl(GlobalCtors, Module, "llvm.global_ctors");
    // auto Part = getGlobalCtorsDtorsImpl(Module, "llvm.global_ctors");
    // GlobalCtors.insert(GlobalCtors.begin(), Part.begin(), Part.end());
  }

  // for (auto it = GlobalCtors.cbegin(), end = GlobalCtors.cend(); it != end;
  //      ++it) {
  //   GlobalCtorFn.try_emplace(it->second, it);
  // }
}

void LLVMBasedICFG::collectGlobalDtors() {
  for (const auto *Module : IRDB.getAllModules()) {
    insertGlobalCtorsDtorsImpl(GlobalDtors, Module, "llvm.global_dtors");
    // auto Part = getGlobalCtorsDtorsImpl(Module, "llvm.global_dtors");
    // GlobalDtors.insert(GlobalDtors.begin(), Part.begin(), Part.end());
  }

  // for (auto it = GlobalDtors.cbegin(), end = GlobalDtors.cend(); it != end;
  //      ++it) {
  //   GlobalDtorFn.try_emplace(it->second, it);
  // }
}

void LLVMBasedICFG::collectGlobalInitializers() {
  // get all functions used to initialize global variables
  forEachGlobalCtor([this](auto *GlobalCtor) {
    for (const auto &I : llvm::instructions(*GlobalCtor)) {
      if (auto Call = llvm::dyn_cast<llvm::CallInst>(&I)) {
        GlobalInitializers.push_back(Call->getCalledFunction());
      }
    }
  });
}

llvm::SmallVector<std::pair<llvm::FunctionCallee, llvm::Value *>, 4>
collectRegisteredDtorsForModule(const llvm::Module *Mod) {
  // NOLINTNEXTLINE
  llvm::SmallVector<std::pair<llvm::FunctionCallee, llvm::Value *>, 4>
      RegisteredDtors, RegisteredLocalStaticDtors;

  auto *CxaAtExitFn = Mod->getFunction("__cxa_atexit");
  if (!CxaAtExitFn) {
    return RegisteredDtors;
  }

  auto getConstantBitcastArgument = // NOLINT
      [](llvm::Value *V) -> llvm::Value * {
    auto *CE = llvm::dyn_cast<llvm::ConstantExpr>(V);
    if (!CE) {
      return V;
    }

    return CE->getOperand(0);
  };

  for (auto *User : CxaAtExitFn->users()) {
    auto *Call = llvm::dyn_cast<llvm::CallBase>(User);
    if (!Call) {
      continue;
    }

    auto *DtorOp = llvm::dyn_cast_or_null<llvm::Function>(
        getConstantBitcastArgument(Call->getArgOperand(0)));
    auto *DtorArgOp = getConstantBitcastArgument(Call->getArgOperand(1));

    if (!DtorOp || !DtorArgOp) {
      continue;
    }

    if (Call->getFunction()->getName().contains("__cxx_global_var_init")) {
      RegisteredDtors.emplace_back(DtorOp, DtorArgOp);
    } else {
      RegisteredLocalStaticDtors.emplace_back(DtorOp, DtorArgOp);
    }
  }

  // Destructors of local static variables are registered last, no matter where
  // they are declared in the source code
  RegisteredDtors.append(RegisteredLocalStaticDtors.begin(),
                         RegisteredLocalStaticDtors.end());

  return RegisteredDtors;
}

std::string getReducedModuleName(const llvm::Module &M) {
  auto Name = M.getName().str();
  if (auto Idx = Name.find_last_of('/'); Idx != std::string::npos) {
    Name.erase(0, Idx + 1);
  }

  return Name;
}

llvm::Function *createDtorCallerForModule(
    llvm::Module *Mod,
    const llvm::SmallVectorImpl<std::pair<llvm::FunctionCallee, llvm::Value *>>
        &RegisteredDtors) {

  auto *PhasarDtorCaller = llvm::cast<llvm::Function>(
      Mod->getOrInsertFunction("__psrGlobalDtorsCaller." +
                                   getReducedModuleName(*Mod),
                               llvm::Type::getVoidTy(Mod->getContext()))
          .getCallee());

  auto *BB =
      llvm::BasicBlock::Create(Mod->getContext(), "entry", PhasarDtorCaller);

  llvm::IRBuilder<> IRB(BB);

  for (auto It = RegisteredDtors.rbegin(), End = RegisteredDtors.rend();
       It != End; ++It) {
    IRB.CreateCall(It->first, {It->second});
  }

  IRB.CreateRetVoid();

  return PhasarDtorCaller;
}

llvm::Function *LLVMBasedICFG::buildCRuntimeGlobalDtorsModel(llvm::Module &M) {

  if (GlobalDtors.size() == 1) {
    return GlobalDtors.begin()->second;
  }

  auto &CTX = M.getContext();
  auto *Cleanup = llvm::cast<llvm::Function>(
      M.getOrInsertFunction("__psrCRuntimeGlobalDtorsModel",
                            llvm::Type::getVoidTy(CTX))
          .getCallee());

  auto *EntryBB = llvm::BasicBlock::Create(CTX, "entry", Cleanup);

  llvm::IRBuilder<> IRB(EntryBB);

  /// Call all statically/dynamically registered dtors

  for (auto [unused, Dtor] : GlobalDtors) {
    assert(Dtor);
    assert(Dtor->arg_empty());
    IRB.CreateCall(Dtor);
  }

  IRB.CreateRetVoid();

  IRDB.addFunctionToDB(Cleanup);

  return Cleanup;
}

const llvm::Function *
LLVMBasedICFG::buildCRuntimeGlobalCtorsDtorsModel(llvm::Module &M) {
  collectGlobalCtors();

  collectGlobalDtors();
  collectRegisteredDtors();

  if (!GlobalCleanupFn) {
    GlobalCleanupFn = buildCRuntimeGlobalDtorsModel(M);
  }

  auto &CTX = M.getContext();
  auto *GlobModel = llvm::cast<llvm::Function>(
      M.getOrInsertFunction(GlobalCRuntimeModelName,
                            /*retTy*/
                            llvm::Type::getVoidTy(CTX),
                            /*argc*/
                            llvm::Type::getInt32Ty(CTX),
                            /*argv*/
                            llvm::Type::getInt8PtrTy(CTX)->getPointerTo())
          .getCallee());

  auto *EntryBB = llvm::BasicBlock::Create(CTX, "entry", GlobModel);

  llvm::IRBuilder<> IRB(EntryBB);

  /// First, call all global ctors

  for (auto [unused, Ctor] : GlobalCtors) {
    assert(Ctor != nullptr);
    assert(Ctor->arg_size() == 0);

    IRB.CreateCall(Ctor);
  }

  /// After all ctors have been called, now go for the user-defined entrypoints

  assert(!UserEntryPoints.empty());

  auto callUEntry = [&](llvm::Function *UEntry) { // NOLINT
    switch (UEntry->arg_size()) {
    case 0:
      IRB.CreateCall(UEntry);
      break;
    case 2:
      if (UEntry->getName() != "main") {
        llvm::errs() << "ERROR: The only entrypoint, where parameters are "
                        "supported, is main. If you need this entrypoint '"
                     << UEntry->getName()
                     << "' please disable global ctor/dtor handling\n";
        break;
      }

      IRB.CreateCall(UEntry, {GlobModel->getArg(0), GlobModel->getArg(1)});
      break;
    default:
      llvm::errs()
          << "ERROR: Entrypoints with parameters are not supported, "
             "except for argc and argv in main. If you need this entrypoint '"
          << UEntry->getName()
          << "' please disable global ctor/dtor handling\n";
      break;
    }

    if (UEntry->getName() == "main") {
      ///  After the main function, we must call all global destructors...
      IRB.CreateCall(GlobalCleanupFn);
    }
  };

  if (UserEntryPoints.size() == 1) {
    auto *MainFn = *UserEntryPoints.begin();
    callUEntry(MainFn);
    IRB.CreateRetVoid();
  } else {

    auto *UEntrySelectorFn = llvm::cast<llvm::Function>(
        M.getOrInsertFunction("__psrCRuntimeUserEntrySelector",
                              llvm::Type::getInt32Ty(CTX))
            .getCallee());

    auto *UEntrySelector = IRB.CreateCall(UEntrySelectorFn, {});

    auto *DefaultBB = llvm::BasicBlock::Create(CTX, "invalid", GlobModel);
    auto *SwitchEnd = llvm::BasicBlock::Create(CTX, "switchEnd", GlobModel);

    auto *UEntrySwitch =
        IRB.CreateSwitch(UEntrySelector, DefaultBB, UserEntryPoints.size());

    IRB.SetInsertPoint(DefaultBB);
    IRB.CreateUnreachable();

    unsigned Idx = 0;

    for (auto *UEntry : UserEntryPoints) {
      auto *BB =
          llvm::BasicBlock::Create(CTX, "call" + UEntry->getName(), GlobModel);
      IRB.SetInsertPoint(BB);
      callUEntry(UEntry);
      IRB.CreateBr(SwitchEnd);

      UEntrySwitch->addCase(IRB.getInt32(Idx), BB);

      ++Idx;
    }

    /// After all user-entries have been called, we are done

    IRB.SetInsertPoint(SwitchEnd);
    IRB.CreateRetVoid();
  }

  ModulesToSlotTracker::updateMSTForModule(&M);
  IRDB.addFunctionToDB(GlobModel);

  return GlobModel;
}

void LLVMBasedICFG::collectRegisteredDtors() {

  for (auto *Mod : IRDB.getAllModules()) {
    LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                  << "Collect Registered Dtors for Module "
                  << Mod->getName().str());

    auto RegisteredDtors = collectRegisteredDtorsForModule(Mod);

    if (RegisteredDtors.empty()) {
      continue;
    }

    LOG_IF_ENABLE(BOOST_LOG_SEV(lg::get(), DEBUG)
                  << "> Found " << RegisteredDtors.size()
                  << " Registered Dtors");

    auto *RegisteredDtorCaller =
        createDtorCallerForModule(Mod, RegisteredDtors);
    // auto It =
    GlobalDtors.emplace(0, RegisteredDtorCaller);
    // GlobalDtorFn.try_emplace(RegisteredDtorCaller, it);
    GlobalRegisteredDtorsCaller.try_emplace(Mod, RegisteredDtorCaller);
  }
}

/**
 * Returns the set of all nodes that are neither call nor start nodes.
 */
set<const llvm::Instruction *> LLVMBasedICFG::allNonCallStartNodes() const {
  set<const llvm::Instruction *> NonCallStartNodes;

  for (const auto *Inst : IRDB.instructions()) {
    if (!llvm::isa<llvm::CallBase>(Inst) && !isStartPoint(Inst)) {
      NonCallStartNodes.insert(Inst);
    }
  }

  return NonCallStartNodes;
}

void LLVMBasedICFG::mergeWith(const LLVMBasedICFG &Other) {
  using vertex_t = bidigraph_t::vertex_descriptor;
  using vertex_map_t = std::map<vertex_t, vertex_t>;
  vertex_map_t OldToNewVertexMapping;
  boost::associative_property_map<vertex_map_t> VertexMapWrapper(
      OldToNewVertexMapping);
  boost::copy_graph(Other.CallGraph, CallGraph,
                    boost::orig_to_copy(VertexMapWrapper));

  // This vector holds the call-sites that are used to merge the whole-module
  // points-to graphs
  vector<pair<const llvm::CallBase *, const llvm::Function *>> Calls;
  vertex_iterator VIv;

  vertex_iterator VIvEnd;

  vertex_iterator VIu;

  vertex_iterator VIuEnd;
  // Iterate the vertices of this graph 'v'
  for (boost::tie(VIv, VIvEnd) = boost::vertices(CallGraph); VIv != VIvEnd;
       ++VIv) {
    // Iterate the vertices of the other graph 'u'
    for (boost::tie(VIu, VIuEnd) = boost::vertices(CallGraph); VIu != VIuEnd;
         ++VIu) {
      // Check if we have a virtual node that can be replaced with the actual
      // node
      if (CallGraph[*VIv].F == CallGraph[*VIu].F &&
          CallGraph[*VIv].F->isDeclaration() &&
          !CallGraph[*VIu].F->isDeclaration()) {
        in_edge_iterator EI;

        in_edge_iterator EIEnd;
        for (boost::tie(EI, EIEnd) = boost::in_edges(*VIv, CallGraph);
             EI != EIEnd; ++EI) {
          auto Source = boost::source(*EI, CallGraph);
          auto Edge = CallGraph[*EI];
          // This becomes the new edge for this graph to the other graph
          boost::add_edge(Source, *VIu, Edge.CS, CallGraph);
          Calls.emplace_back(llvm::cast<llvm::CallBase>(Edge.CS),
                             CallGraph[*VIu].F);
          // Remove the old edge flowing into the virtual node
          boost::remove_edge(Source, *VIv, CallGraph);
        }
        // Remove the virtual node
        boost::remove_vertex(*VIv, CallGraph);
      }
    }
  }

  // Update the FunctionVertexMap:
  for (const auto &OtherValues : Other.FunctionVertexMap) {
    auto MappingIter = OldToNewVertexMapping.find(OtherValues.second);
    if (MappingIter != OldToNewVertexMapping.end()) {
      FunctionVertexMap.insert(
          make_pair(OtherValues.first, MappingIter->second));
    }
  }

  // Merge the already visited functions
  VisitedFunctions.insert(Other.VisitedFunctions.begin(),
                          Other.VisitedFunctions.end());
  // Merge the points-to graphs
  // WholeModulePTG.mergeWith(Other.WholeModulePTG, Calls);
}

CallGraphAnalysisType LLVMBasedICFG::getCallGraphAnalysisType() const {
  return CGType;
}

void LLVMBasedICFG::print(ostream &OS) const {
  OS << "Call Graph:\n";
  vertex_iterator UI;

  vertex_iterator UIEnd;
  for (boost::tie(UI, UIEnd) = boost::vertices(CallGraph); UI != UIEnd; ++UI) {
    OS << CallGraph[*UI].getFunctionName() << " --> ";
    out_edge_iterator EI;

    out_edge_iterator EIEnd;
    for (boost::tie(EI, EIEnd) = boost::out_edges(*UI, CallGraph); EI != EIEnd;
         ++EI) {
      OS << CallGraph[target(*EI, CallGraph)].getFunctionName() << " ";
    }
    OS << '\n';
  }
}

void LLVMBasedICFG::printAsDot(std::ostream &OS, bool PrintEdgeLabels) const {
  if (PrintEdgeLabels) {
    boost::write_graphviz(OS, CallGraph, VertexWriter<bidigraph_t>(CallGraph),
                          EdgeLabelWriter<bidigraph_t>(CallGraph));
  } else {
    boost::write_graphviz(OS, CallGraph, VertexWriter<bidigraph_t>(CallGraph));
  }
}

nlohmann::json LLVMBasedICFG::getAsJson() const {
  nlohmann::json J;
  vertex_iterator VIv;

  vertex_iterator VIvEnd;
  out_edge_iterator EI;

  out_edge_iterator EIEnd;
  // iterate all graph vertices
  for (boost::tie(VIv, VIvEnd) = boost::vertices(CallGraph); VIv != VIvEnd;
       ++VIv) {
    J[PhasarConfig::JsonCallGraphID()][CallGraph[*VIv].getFunctionName()];
    // iterate all out edges of vertex vi_v
    for (boost::tie(EI, EIEnd) = boost::out_edges(*VIv, CallGraph); EI != EIEnd;
         ++EI) {
      J[PhasarConfig::JsonCallGraphID()][CallGraph[*VIv].getFunctionName()] +=
          CallGraph[boost::target(*EI, CallGraph)].getFunctionName();
    }
  }
  return J;
}

void LLVMBasedICFG::printAsJson(std::ostream &OS) const { OS << getAsJson(); }

nlohmann::json LLVMBasedICFG::exportICFGAsJson() const {
  nlohmann::json J;

  llvm::DenseSet<const llvm::Instruction *> HandledCallSites;

  for (const auto *F : getAllFunctions()) {
    for (auto &[From, To] : getAllControlFlowEdges(F)) {
      if (llvm::isa<llvm::UnreachableInst>(From)) {
        continue;
      }

      if (const auto *Call = llvm::dyn_cast<llvm::CallBase>(From)) {
        auto [unused, inserted] = HandledCallSites.insert(Call);

        for (const auto *Callee : getCalleesOfCallAt(Call)) {
          if (Callee->isDeclaration()) {
            continue;
          }
          if (inserted) {
            J.push_back(
                {{"from", llvmIRToStableString(From)},
                 {"to", llvmIRToStableString(&Callee->front().front())}});
          }

          for (const auto *ExitInst : getAllExitPoints(Callee)) {
            J.push_back({{"from", llvmIRToStableString(ExitInst)},
                         {"to", llvmIRToStableString(To)}});
          }
        }

      } else {
        J.push_back({{"from", llvmIRToStableString(From)},
                     {"to", llvmIRToStableString(To)}});
      }
    }
  }

  return J;
}

template <typename EdgeCallBack>
void LLVMBasedICFG::exportICFGAsSourceCodeImpl(
    EdgeCallBack &&CreateEdge) const {
  struct GetSCIFn {
    std::vector<SourceCodeInfoWithIR> SCI;
    llvm::DenseMap<const llvm::Instruction *, SourceCodeInfoWithIR *> Inst2SCI;
    [[maybe_unused]] size_t Capacity;

    GetSCIFn(size_t Capacity) {
      SCI.reserve(Capacity);
      this->Capacity = SCI.capacity();
      Inst2SCI.reserve(Capacity);
    }

    const SourceCodeInfoWithIR &operator()(const llvm::Instruction *Inst) {
      auto &Ret = Inst2SCI[Inst];
      if (Ret) {
        return *Ret;
      }

      auto &Last = SCI.emplace_back(SourceCodeInfoWithIR{
          getSrcCodeInfoFromIR(Inst), llvmIRToStableString(Inst)});
      assert(SCI.capacity() == Capacity &&
             "We must not resize the vector, as otherwise the references are "
             "unstable");

      Ret = &Last;
      return Last;
    }
  }
  // NOLINTNEXTLINE(readability-identifier-naming)
  getSCI(IRDB.getNumInstructions());

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto isRetVoid = [](const llvm::Instruction *Inst) noexcept {
    const auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(Inst);
    return Ret && !Ret->getReturnValue();
  };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto getLastNonEmpty =
      [isRetVoid,
       &getSCI](const llvm::Instruction *Inst) -> const SourceCodeInfoWithIR & {
    if (!isRetVoid(Inst) || !Inst->getPrevNode()) {
      return getSCI(Inst);
    }
    if (const auto *Prev = Inst->getPrevNode()) {
      return getSCI(Prev);
    }

    return getSCI(Inst);
  };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto createInterEdges = [this, &CreateEdge,
                           &getLastNonEmpty](const llvm::Instruction *CS,
                                             const SourceCodeInfoWithIR &From,
                                             const SourceCodeInfoWithIR &To) {
    bool NeedCTREdge = false;
    auto Callees = internalGetCalleesOfCallAt(CS);
    for (const auto *Callee : Callees) {
      if (Callee->isDeclaration()) {
        NeedCTREdge = true;
        continue;
      }
      // Call Edge
      auto InterTo = getFirstNonEmpty(&Callee->front());
      std::invoke(CreateEdge, From, InterTo);

      // Return Edges
      for (const auto *ExitInst : getAllExitPoints(Callee)) {
        std::invoke(CreateEdge, getLastNonEmpty(ExitInst), To);
      }
    }

    if (NeedCTREdge || Callees.empty()) {
      std::invoke(CreateEdge, From, To);
    }
  };

  llvm::SmallVector<const llvm::Instruction *, 2> Successors;

  for (const auto *Inst : IRDB.instructions()) {
    const auto &I = *Inst;
    if (llvm::isa<llvm::UnreachableInst>(&I)) {
      continue;
    }
    if (IgnoreDbgInstructions && llvm::isa<llvm::DbgInfoIntrinsic>(&I)) {
      continue;
    }

    Successors.clear();
    getSuccsOf(&I, Successors);
    const auto &FromSCI = getSCI(&I);

    for (const auto *Successor : Successors) {
      const auto &ToSCI = getSCI(Successor);

      if (llvm::isa<llvm::CallBase>(&I)) {
        createInterEdges(&I, FromSCI, ToSCI);
      } else {
        std::invoke(CreateEdge, FromSCI, ToSCI);
      }
    }
  }
}
nlohmann::json LLVMBasedICFG::exportICFGAsSourceCodeJson() const {
  nlohmann::json J;
  exportICFGAsSourceCodeImpl(
      [&J](const SourceCodeInfoWithIR &From, const SourceCodeInfoWithIR &To) {
        J.push_back({{"from", From}, {"to", To}});
      });
  return J;
}

void LLVMBasedICFG::exportICFGAsSourceCodeJson(llvm::raw_ostream &OS) const {
  OS << "[";
  scope_exit CloseBrace = [&OS]() { OS << "]"; };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto writeSCI = [](llvm::raw_ostream &OS, const SourceCodeInfoWithIR &SCI) {
    OS << "{\"sourceCodeLine\":" << psr::quoted(SCI.SourceCodeLine)
       << ",\"sourceCodeFileName\":" << psr::quoted(SCI.SourceCodeFilename)
       << ",\"sourceCodeFunctionName\":"
       << psr::quoted(SCI.SourceCodeFunctionName) << ",\"line\":" << SCI.Line
       << ",\"column\":" << SCI.Column << ",\"IR\":" << psr::quoted(SCI.IR)
       << "}";
  };

  bool IsFirstEdge = true;

  auto CreateEdge = [&OS, &IsFirstEdge](llvm::StringRef From,
                                        llvm::StringRef To) {
    if (IsFirstEdge) {
      IsFirstEdge = false;
    } else {
      OS << ",";
    }

    OS << "{\"from\":" << From << ",\"to\":" << To << "}";
  };

  auto SerializeSCI = [writeSCI](const SourceCodeInfoWithIR &SCI,
                                 std::pmr::memory_resource *MRes) {
    llvm::SmallString<512> Buffer;
    llvm::raw_svector_ostream OS(Buffer);

    writeSCI(OS, SCI);
    auto *RetData = (char *)MRes->allocate(Buffer.size(), 1);
    memcpy(RetData, Buffer.data(), Buffer.size());

    return llvm::StringRef(RetData, Buffer.size());
  };

  llvm::DenseMap<const llvm::Instruction *, llvm::StringRef> Inst2SCI;
  Inst2SCI.reserve(IRDB.getNumInstructions());
  // NOLINTNEXTLINE(readability-identifier-naming)
  auto getSCI = [Inst2SCI = std::move(Inst2SCI),
                 MRes = std::pmr::monotonic_buffer_resource(),
                 SerializeSCI](const llvm::Instruction *Inst) mutable {
    auto &Ret = Inst2SCI[Inst];
    if (Ret.empty()) {
      Ret = SerializeSCI(
          {getSrcCodeInfoFromIR(Inst), llvmIRToStableString(Inst)}, &MRes);
    }
    return Ret;
  };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto isRetVoid = [](const llvm::Instruction *Inst) noexcept {
    const auto *Ret = llvm::dyn_cast<llvm::ReturnInst>(Inst);
    return Ret && !Ret->getReturnValue();
  };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto getLastNonEmpty = [isRetVoid, &getSCI](const llvm::Instruction *Inst) {
    if (!isRetVoid(Inst) || !Inst->getPrevNode()) {
      return getSCI(Inst);
    }
    if (const auto *Prev = Inst->getPrevNode()) {
      return getSCI(Prev);
    }

    return getSCI(Inst);
  };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto createInterEdges = [this, &CreateEdge, &getLastNonEmpty,
                           &getSCI](const llvm::Instruction *CS,
                                    llvm::StringRef From, llvm::StringRef To) {
    bool NeedCTREdge = false;
    auto Callees = internalGetCalleesOfCallAt(CS);
    for (const auto *Callee : Callees) {
      if (Callee->isDeclaration()) {
        NeedCTREdge = true;
        continue;
      }
      // Call Edge
      const auto *InterTo = &Callee->front().front();
      if (IgnoreDbgInstructions && llvm::isa<llvm::DbgInfoIntrinsic>(InterTo)) {
        InterTo = InterTo->getNextNonDebugInstruction(false);
      }
      std::invoke(CreateEdge, From, getSCI(InterTo));

      // Return Edges
      for (const auto *ExitInst : getAllExitPoints(Callee)) {
        std::invoke(CreateEdge, getLastNonEmpty(ExitInst), To);
      }
    }

    if (NeedCTREdge || Callees.empty()) {
      std::invoke(CreateEdge, From, To);
    }
  };

  llvm::SmallVector<const llvm::Instruction *, 2> Successors;

  for (const auto *Inst : IRDB.instructions()) {
    const auto &I = *Inst;
    if (llvm::isa<llvm::UnreachableInst>(&I)) {
      continue;
    }
    if (IgnoreDbgInstructions && llvm::isa<llvm::DbgInfoIntrinsic>(&I)) {
      continue;
    }

    Successors.clear();
    getSuccsOf(&I, Successors);
    auto FromSCI = getSCI(&I);

    for (const auto *Successor : Successors) {
      auto ToSCI = getSCI(Successor);

      if (llvm::isa<llvm::CallBase>(&I)) {
        createInterEdges(&I, FromSCI, ToSCI);
      } else {
        std::invoke(CreateEdge, FromSCI, ToSCI);
      }
    }
  }
}

std::string LLVMBasedICFG::exportICFGAsSourceCodeJsonString() const {
  /// Use a heuristic to estimate the number of chars required to store the
  /// json. Note: We probably underestimate the size, but that doesn't matter as
  /// long as the number of reallocations is still very small, e.g. 1 - 3

  // Max is 80 typically
  constexpr size_t ExpNumCharsPerSrcCodeLine = 40;
  // Filenames are typically very long
  constexpr size_t ExpNumCharsPerFileName = 100;
  // Just a made-up number
  constexpr size_t ExpNumCharsPerFunctionName = 20;
  // We may have hundrets of LOC in a file
  constexpr size_t ExpNumCharsPerLine = 3;
  // we typically have up to 80 cols in a line
  constexpr size_t ExpNumCharsPerCol = 2;
  // IR instructions tend to be very long
  constexpr size_t ExpNumCharsPerIRInst = 60;
  // code, file, func, line, col, ir
  // Delims{"":"","":"","":"","":,"":,"":""}
  // Note, sizeof includes the null-terminator; however, we don't need to be
  // very precise here (and it somehow adds up with the quotes needed in some
  // places)
  constexpr size_t ExpNumCharsPerSrcInfo =
      33 + sizeof("sourceCodeLine") + ExpNumCharsPerSrcCodeLine +
      sizeof("sourceCodeFileName") + ExpNumCharsPerFileName +
      sizeof("sourceCodeFunctionName") + ExpNumCharsPerFunctionName +
      sizeof("line") + ExpNumCharsPerLine + sizeof("column") +
      ExpNumCharsPerCol + sizeof("IR");
  // Delims{:,:}, "from", "to", From, To
  constexpr size_t ExpNumCharsPerEdge = 5 + 6 + 4 + 2 * ExpNumCharsPerSrcInfo;

  std::string Ret;
  Ret.reserve(IRDB.getNumInstructions() * ExpNumCharsPerEdge);
  llvm::raw_string_ostream OS(Ret);

  exportICFGAsSourceCodeJson(OS);

  OS.flush();
  return Ret;
}

void LLVMBasedICFG::exportICFGAsSourceCodeDot(llvm::raw_ostream &OS) const {
  OS << "digraph ICFG{\n";
  scope_exit CloseBrace = [&OS]() { OS << "}\n"; };

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto writeSCI = [](llvm::raw_ostream &OS, const llvm::Instruction *Inst) {
    auto SCI = getSrcCodeInfoFromIR(Inst);

    OS << "File: ";
    OS.write_escaped(SCI.SourceCodeFilename);
    OS << "\nFunction: ";
    OS.write_escaped(SCI.SourceCodeFunctionName);
    OS << "\nIR: ";
    OS.write_escaped(llvmIRToStableString(Inst));

    if (SCI.Line) {
      OS << "\nLine: " << SCI.Line << "\nColumn: " << SCI.Column;
    }
  };

  auto IgnoreDbgInstructions = this->IgnoreDbgInstructions;

  // NOLINTNEXTLINE(readability-identifier-naming)
  auto createInterEdges = [&OS, this, IgnoreDbgInstructions](
                              const llvm::Instruction *CS, intptr_t To,
                              llvm::StringRef Label) {
    bool HasDecl = false;
    auto Callees = internalGetCalleesOfCallAt(CS);

    for (const auto *Callee : Callees) {
      if (Callee->isDeclaration()) {
        HasDecl = true;
        continue;
      }

      // Call Edge
      const auto *BB = &Callee->front();
      assert(BB && !BB->empty());
      const auto *InterTo = &BB->front();

      if (IgnoreDbgInstructions && llvm::isa<llvm::DbgInfoIntrinsic>(InterTo)) {
        InterTo = InterTo->getNextNonDebugInstruction(false);
      }
      // createEdge(From, InterTo);
      OS << intptr_t(CS) << "->" << intptr_t(InterTo) << ";\n";

      // Return Edges
      for (const auto *ExitInst : getAllExitPoints(Callee)) {
        /// TODO: Be return/resume aware!
        OS << intptr_t(ExitInst) << "->" << To << Label << ";\n";
      }
    }

    if (HasDecl || Callees.empty()) {
      OS << intptr_t(CS) << "->" << To << Label << ";\n";
    }
  };

  llvm::SmallVector<const llvm::Instruction *, 4> Successors;

  for (const auto *Inst : IRDB.instructions()) {
    if (IgnoreDbgInstructions && llvm::isa<llvm::DbgInfoIntrinsic>(Inst)) {
      continue;
    }

    OS << intptr_t(Inst) << "[label=\"";
    writeSCI(OS, Inst);
    OS << "\"];\n";

    if (llvm::isa<llvm::UnreachableInst>(Inst)) {
      continue;
    }

    Successors.clear();
    getSuccsOf(Inst, Successors);

    if (Successors.size() == 2) {
      if (llvm::isa<llvm::InvokeInst>(Inst)) {
        createInterEdges(Inst, intptr_t(Successors[0]), "[label=\"normal\"]");
        createInterEdges(Inst, intptr_t(Successors[1]), "[label=\"unwind\"]");
      } else {
        OS << intptr_t(Inst) << "->" << intptr_t(Successors[0])
           << "[label=\"true\"];\n";
        OS << intptr_t(Inst) << "->" << intptr_t(Successors[1])
           << "[label=\"false\"];\n";
      }
      continue;
    }

    for (const auto *Successor : Successors) {
      if (llvm::isa<llvm::CallBase>(Inst)) {
        createInterEdges(Inst, intptr_t(Successor), "");
      } else {
        OS << intptr_t(Inst) << "->" << intptr_t(Successor) << ";\n";
      }
    }
  }
}

std::string LLVMBasedICFG::exportICFGAsSourceCodeDotString() const {
  std::string Ret;
  /// Just a heuristic reserve number...
  Ret.reserve(150 * IRDB.getNumInstructions());
  llvm::raw_string_ostream OS(Ret);
  exportICFGAsSourceCodeDot(OS);
  OS.flush();
  return Ret;
}

vector<const llvm::Function *> LLVMBasedICFG::getDependencyOrderedFunctions() {
  vector<vertex_t> Vertices;
  vector<const llvm::Function *> Functions;
  dependency_visitor Deps(Vertices);
  boost::depth_first_search(CallGraph, visitor(Deps));
  for (auto V : Vertices) {
    if (!CallGraph[V].F->isDeclaration()) {
      Functions.push_back(CallGraph[V].F);
    }
  }
  return Functions;
}

unsigned LLVMBasedICFG::getNumOfVertices() const {
  return boost::num_vertices(CallGraph);
}

unsigned LLVMBasedICFG::getNumOfEdges() const {
  return boost::num_edges(CallGraph);
}

unsigned LLVMBasedICFG::getTotalRuntimeEdgesAdded() const {
  return TotalRuntimeEdgesAdded;
}

const llvm::Function *
LLVMBasedICFG::getRegisteredDtorsCallerOrNull(const llvm::Module *Mod) {
  auto It = GlobalRegisteredDtorsCaller.find(Mod);
  if (It != GlobalRegisteredDtorsCaller.end()) {
    return It->second;
  }

  return nullptr;
}

} // namespace psr
