/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_FLOWEDGEFUNCTIONCACHE_PLL_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_FLOWEDGEFUNCTIONCACHE_PLL_H

#include "phasar/DataFlow/IfdsIde/FlowFunctions.h"
#include "phasar/DataFlow/IfdsIde/IDETabulationProblem.h"
#include "phasar/DataFlow/IfdsIde/Solver/EdgeFunctionKind.h"
#include "phasar/DataFlow/IfdsIde/Solver/MapKeyCompressorPll.h"
#include "phasar/Utils/EquivalenceClassMap.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/Utilities.h"

#include "parallel_hashmap/phmap.h"
#include "parallel_hashmap/phmap_fwd_decl.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>

namespace llvm {
class Value;
} // namespace llvm

namespace psr {

/**
 * This class caches flow and edge functions to avoid their reconstruction.
 * When a flow or edge function must be applied to multiple times, a cached
 * version is used if existend, otherwise a new one is created and inserted
 * into the cache.
 */
template <typename AnalysisDomainTy,
          typename Container =
              phmap::parallel_node_hash_set_m<typename AnalysisDomainTy::d_t>>
class FlowEdgeFunctionCachePll {
  using IDEProblemType = IDETabulationProblem<AnalysisDomainTy, Container>;
  using FlowFunctionPtrType = typename IDEProblemType::FlowFunctionPtrType;

  using n_t = typename AnalysisDomainTy::n_t;
  using d_t = typename AnalysisDomainTy::d_t;
  using f_t = typename AnalysisDomainTy::f_t;
  using t_t = typename AnalysisDomainTy::t_t;
  using l_t = typename AnalysisDomainTy::l_t;

  using FlowFunctionType = FlowFunction<d_t, Container>;
  using EdgeFunctionType = EdgeFunction<l_t>;

public:
  // Ctor allows access to the IDEProblem in order to get access to flow and
  // edge function factory functions.
  FlowEdgeFunctionCachePll(
      IDETabulationProblem<AnalysisDomainTy, Container> &Problem)
      : Problem(Problem),
        AutoAddZero(Problem.getIFDSIDESolverConfig().autoAddZero()),
        ZV(Problem.getZeroValue()) {
    PAMM_GET_INSTANCE;
    REG_COUNTER("Normal-FF Construction", 0, Full);
    REG_COUNTER("Normal-FF Cache Hit", 0, Full);
    // Counters for the call flow functions
    REG_COUNTER("Call-FF Construction", 0, Full);
    REG_COUNTER("Call-FF Cache Hit", 0, Full);
    // Counters for return flow functions
    REG_COUNTER("Return-FF Construction", 0, Full);
    REG_COUNTER("Return-FF Cache Hit", 0, Full);
    // Counters for the call to return flow functions
    REG_COUNTER("CallToRet-FF Construction", 0, Full);
    REG_COUNTER("CallToRet-FF Cache Hit", 0, Full);
    // Counters for the summary flow functions
    REG_COUNTER("Summary-FF Construction", 0, Full);
    REG_COUNTER("Summary-FF Cache Hit", 0, Full);
    // Counters for the normal edge functions
    REG_COUNTER("Normal-EF Construction", 0, Full);
    REG_COUNTER("Normal-EF Cache Hit", 0, Full);
    // Counters for the call edge functions
    REG_COUNTER("Call-EF Construction", 0, Full);
    REG_COUNTER("Call-EF Cache Hit", 0, Full);
    // Counters for the return edge functions
    REG_COUNTER("Return-EF Construction", 0, Full);
    REG_COUNTER("Return-EF Cache Hit", 0, Full);
    // Counters for the call to return edge functions
    REG_COUNTER("CallToRet-EF Construction", 0, Full);
    REG_COUNTER("CallToRet-EF Cache Hit", 0, Full);
    // Counters for the summary edge functions
    REG_COUNTER("Summary-EF Construction", 0, Full);
    REG_COUNTER("Summary-EF Cache Hit", 0, Full);
  }

  ~FlowEdgeFunctionCachePll() = default;

  FlowEdgeFunctionCachePll(const FlowEdgeFunctionCachePll &FEFC) = default;
  FlowEdgeFunctionCachePll &
  operator=(const FlowEdgeFunctionCachePll &FEFC) = default;

  FlowEdgeFunctionCachePll(FlowEdgeFunctionCachePll &&FEFC) noexcept = default;
  FlowEdgeFunctionCachePll &
  operator=(FlowEdgeFunctionCachePll &&FEFC) noexcept = default;

  FlowFunctionPtrType getNormalFlowFunction(n_t Curr, n_t Succ) {
    assertNotNull(Curr);
    assertNotNull(Succ);
    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Normal flow function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Curr Inst : " << NToString(Curr));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Succ Inst : " << NToString(Succ)));
    auto Key = createEdgeFunctionInstKey(Curr, Succ);

    // operator[] instead of try_emplace: NormalEdgeFlowData holds both the
    // flow function ptr and the edge function map for the same (Curr,Succ)
    // key, so getNormalEdgeFunction shares this entry via the same lookup.
    auto &NormalFE = NormalFunctionCache[std::move(Key)];
    if (!NormalFE.FlowFuncPtr) {
      INC_COUNTER("Normal-FF Construction", 1, Full);
      auto FF = Problem.getNormalFlowFunction(Curr, Succ);
      NormalFE.FlowFuncPtr = AutoAddZero
                                 ? std::make_unique<ZFF>(std::move(FF), ZV)
                                 : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Normal-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(NormalFE.FlowFuncPtr);
  }

  FlowFunctionPtrType getCallFlowFunction(n_t CallSite, f_t DestFun) {
    assertNotNull(CallSite);
    assertNotNull(DestFun);
    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Call flow function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Stmt : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Dest Fun : " << FToString(DestFun)));
    auto Key = std::tie(CallSite, DestFun);

    // TODO: make thread safe
    // I feel like I can't get around a lock guard here.
    auto [It, Inserted] = CallFlowFunctionCache.try_emplace(std::move(Key));
    if (Inserted) {
      INC_COUNTER("Call-FF Construction", 1, Full);
      auto FF = Problem.getCallFlowFunction(CallSite, DestFun);
      It->second = AutoAddZero ? std::make_unique<ZFF>(std::move(FF), ZV)
                               : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Call-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(It->second);
  }

  FlowFunctionPtrType getRetFlowFunction(n_t CallSite, f_t CalleeFun,
                                         n_t ExitInst, n_t RetSite) {
    assertNotNull(CallSite);
    assertNotNull(CalleeFun);
    assertNotNull(ExitInst);
    assertNotNull(RetSite);
    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Return flow function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Callee    : " << FToString(CalleeFun));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Exit Stmt : " << NToString(ExitInst));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite)));
    auto Key = std::tie(CallSite, CalleeFun, ExitInst, RetSite);

    auto [It, Inserted] = ReturnFlowFunctionCache.try_emplace(std::move(Key));
    if (Inserted) {
      INC_COUNTER("Return-FF Construction", 1, Full);
      auto FF =
          Problem.getRetFlowFunction(CallSite, CalleeFun, ExitInst, RetSite);
      It->second = AutoAddZero ? std::make_unique<ZFF>(std::move(FF), ZV)
                               : std::move(FF);

      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Return-FF Cache Hit", 1, Full);
    }
    return getPointerFrom(It->second);
  }

  FlowFunctionPtrType getCallToRetFlowFunction(n_t CallSite, n_t RetSite,
                                               llvm::ArrayRef<f_t> Callees) {
    assertNotNull(CallSite);
    assertNotNull(RetSite);
    assertAllNotNull(Callees);
    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Call-to-Return flow function factory call");

        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Callee's  : ");
        for (auto Callee : Callees) {
          PHASAR_LOG_LEVEL(DEBUG, "  " << FToString(Callee));
        };);
    auto Key = std::tie(CallSite, RetSite);

    auto [It, Inserted] =
        CallToRetFlowFunctionCache.try_emplace(std::move(Key));
    if (Inserted) {
      INC_COUNTER("CallToRet-FF Construction", 1, Full);
      auto FF = Problem.getCallToRetFlowFunction(CallSite, RetSite, Callees);
      It->second = AutoAddZero ? std::make_unique<ZFF>(std::move(FF), ZV)
                               : std::move(FF);

      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("CallToRet-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(It->second);
  }

  FlowFunctionPtrType getSummaryFlowFunction(n_t CallSite, f_t DestFun) {
    assertNotNull(CallSite);
    assertNotNull(DestFun);
    // PAMM_GET_INSTANCE;
    // INC_COUNTER("Summary-FF Construction", 1, Full);
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Summary flow function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Stmt : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Dest Mthd : " << FToString(DestFun));
        PHASAR_LOG_LEVEL(DEBUG, ' '));
    auto FF = Problem.getSummaryFlowFunction(CallSite, DestFun);
    return FF;
  }

  EdgeFunction<l_t> getNormalEdgeFunction(n_t Curr, d_t CurrNode, n_t Succ,
                                          d_t SuccNode) {
    assertNotNull(Curr);
    assertNotNull(Succ);

    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Normal edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Curr Inst : " << NToString(Curr));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Curr Node : " << DToString(CurrNode));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Succ Inst : " << NToString(Succ));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Succ Node : " << DToString(SuccNode)));

    EdgeFuncInstKey OuterMapKey = createEdgeFunctionInstKey(Curr, Succ);
    auto &NormalFE = NormalFunctionCache[std::move(OuterMapKey)];
    auto Ret = NormalFE.EdgeFunctionMap.getOrInsertLazy(
        createEdgeFunctionNodeKey(CurrNode, SuccNode),
        [&] {
          INC_COUNTER("Normal-EF Construction", 1, Full);
          auto EF =
              Problem.getNormalEdgeFunction(Curr, CurrNode, Succ, SuccNode);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          return EF;
        },
        [&] {
          INC_COUNTER("Normal-EF Cache Hit", 1, Full);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        });
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Ret);
    return Ret;
  }

  EdgeFunction<l_t> getCallEdgeFunction(n_t CallSite, d_t SrcNode,
                                        f_t DestinationFunction, d_t DestNode) {

    assertNotNull(CallSite);
    assertNotNull(DestinationFunction);

    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Call edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Stmt : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Src Node  : " << DToString(SrcNode));

        PHASAR_LOG_LEVEL(DEBUG,
                         "(F) Dest Fun : " << FToString(DestinationFunction));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Dest Node : " << DToString(DestNode)));
    auto Key = std::tie(CallSite, SrcNode, DestinationFunction, DestNode);

    EdgeFunction<l_t> ReturnEF{};
    CallEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          INC_COUNTER("Call-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Call-EF Construction", 1, Full);
          auto EF = Problem.getCallEdgeFunction(CallSite, SrcNode,
                                                DestinationFunction, DestNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
        });

    return ReturnEF;
  }

  EdgeFunction<l_t> getReturnEdgeFunction(n_t CallSite, f_t CalleeFunction,
                                          n_t ExitInst, d_t ExitNode,
                                          n_t RetSite, d_t RetNode) {
    assertNotNull(CallSite);
    assertNotNull(CalleeFunction);
    assertNotNull(ExitInst);
    assertNotNull(RetSite);

    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Return edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG,
                         "(F) Callee    : " << FToString(CalleeFunction));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Exit Stmt : " << NToString(ExitInst));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Exit Node : " << DToString(ExitNode));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Ret Node  : " << DToString(RetNode)));
    auto Key = std::tie(CallSite, CalleeFunction, ExitInst, ExitNode, RetSite,
                        RetNode);

    EdgeFunction<l_t> ReturnEF{};
    ReturnEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          INC_COUNTER("Return-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Return-EF Construction", 1, Full);
          auto EF = Problem.getReturnEdgeFunction(
              CallSite, CalleeFunction, ExitInst, ExitNode, RetSite, RetNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
        });

    return ReturnEF;
  }

  EdgeFunction<l_t> getCallToRetEdgeFunction(n_t CallSite, d_t CallNode,
                                             n_t RetSite, d_t RetSiteNode,
                                             llvm::ArrayRef<f_t> Callees) {
    assertNotNull(CallSite);
    assertNotNull(RetSite);
    assertAllNotNull(Callees);

    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Call-to-Return edge function factory call");

        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Call Node : " << DToString(CallNode));

        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Ret Node  : " << DToString(RetSiteNode));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Callee's  : ");
        for (auto Callee : Callees) {
          PHASAR_LOG_LEVEL(DEBUG, "  " << FToString(Callee));
        });

    EdgeFuncInstKey OuterMapKey = createEdgeFunctionInstKey(CallSite, RetSite);
    std::pair<EdgeFuncInstKey, InnerEdgeFunctionMapType>
        CallToRetEdgeFunctionCacheEntry;
    if (CallToRetEdgeFunctionCache.if_contains(
            OuterMapKey, [&CallToRetEdgeFunctionCacheEntry](auto &Entry) {
              CallToRetEdgeFunctionCacheEntry = Entry;
            })) {
      auto SearchEdgeFunc = CallToRetEdgeFunctionCacheEntry.second.find(
          createEdgeFunctionNodeKey(CallNode, RetSiteNode));
      if (SearchEdgeFunc != CallToRetEdgeFunctionCacheEntry.second.end()) {
        INC_COUNTER("CallToRet-EF Cache Hit", 1, Full);
        PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        PHASAR_LOG_LEVEL(DEBUG,
                         "Provide Edge Function: " << SearchEdgeFunc->second);
        return SearchEdgeFunc->second;
      }
      INC_COUNTER("CallToRet-EF Construction", 1, Full);
      auto EF = Problem.getCallToRetEdgeFunction(CallSite, CallNode, RetSite,
                                                 RetSiteNode, Callees);

      CallToRetEdgeFunctionCacheEntry.second.insert(
          createEdgeFunctionNodeKey(CallNode, RetSiteNode), EF);

      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
      PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
      return EF;
    }

    INC_COUNTER("CallToRet-EF Construction", 1, Full);
    auto EF = Problem.getCallToRetEdgeFunction(CallSite, CallNode, RetSite,
                                               RetSiteNode, Callees);

    CallToRetEdgeFunctionCache.emplace(
        OuterMapKey,
        InnerEdgeFunctionMapType{std::make_pair(
            createEdgeFunctionNodeKey(CallNode, RetSiteNode), EF)});
    PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
    return EF;
  }

  EdgeFunction<l_t> getSummaryEdgeFunction(n_t CallSite, d_t CallNode,
                                           n_t RetSite, d_t RetSiteNode) {
    assertNotNull(CallSite);
    assertNotNull(RetSite);

    PAMM_GET_INSTANCE;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Summary edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Call Node : " << DToString(CallNode));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Ret Node  : " << DToString(RetSiteNode));
        PHASAR_LOG_LEVEL(DEBUG, ' '));
    auto Key = std::tie(CallSite, CallNode, RetSite, RetSiteNode);

    EdgeFunction<l_t> ReturnEF{};
    SummaryEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          INC_COUNTER("Summary-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Summary-EF Construction", 1, Full);
          auto EF = Problem.getSummaryEdgeFunction(CallSite, CallNode, RetSite,
                                                   RetSiteNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
        });

    return ReturnEF;
  }

  void print() {
    if constexpr (PAMM_CURR_SEV_LEVEL >= PAMM_SEVERITY_LEVEL::Full) {
      PAMM_GET_INSTANCE;
      PHASAR_LOG_LEVEL(INFO, "=== Flow-Edge-Function Cache Statistics ===");
      PHASAR_LOG_LEVEL(INFO, "Normal-flow function cache hits: "
                                 << GET_COUNTER("Normal-FF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Normal-flow function constructions: "
                                 << GET_COUNTER("Normal-FF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Call-flow function cache hits: "
                                 << GET_COUNTER("Call-FF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Call-flow function constructions: "
                                 << GET_COUNTER("Call-FF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Return-flow function cache hits: "
                                 << GET_COUNTER("Return-FF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Return-flow function constructions: "
                                 << GET_COUNTER("Return-FF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Call-to-Return-flow function cache hits: "
                                 << GET_COUNTER("CallToRet-FF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Call-to-Return-flow function constructions: "
                                 << GET_COUNTER("CallToRet-FF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Summary-flow function cache hits: "
                                 << GET_COUNTER("Summary-FF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Summary-flow function constructions: "
                                 << GET_COUNTER("Summary-FF Construction"));
      PHASAR_LOG_LEVEL(INFO,
                       "Total flow function cache hits: " << GET_SUM_COUNT(
                           {"Normal-FF Cache Hit", "Call-FF Cache Hit",
                            "Return-FF Cache Hit", "CallToRet-FF Cache Hit"}));
      //"Summary-FF Cache Hit"});
      PHASAR_LOG_LEVEL(INFO, "Total flow function constructions: "
          << GET_SUM_COUNT({"Normal-FF Construction", "Call-FF Construction",
                            "Return-FF Construction",
                            "CallToRet-FF Construction" /*,
                "Summary-FF Construction"*/}));
      PHASAR_LOG_LEVEL(INFO, ' ');
      PHASAR_LOG_LEVEL(INFO, "Normal edge function cache hits: "
                                 << GET_COUNTER("Normal-EF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Normal edge function constructions: "
                                 << GET_COUNTER("Normal-EF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Call edge function cache hits: "
                                 << GET_COUNTER("Call-EF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Call edge function constructions: "
                                 << GET_COUNTER("Call-EF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Return edge function cache hits: "
                                 << GET_COUNTER("Return-EF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Return edge function constructions: "
                                 << GET_COUNTER("Return-EF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Call-to-Return edge function cache hits: "
                                 << GET_COUNTER("CallToRet-EF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Call-to-Return edge function constructions: "
                                 << GET_COUNTER("CallToRet-EF Construction"));
      PHASAR_LOG_LEVEL(INFO, "Summary edge function cache hits: "
                                 << GET_COUNTER("Summary-EF Cache Hit"));
      PHASAR_LOG_LEVEL(INFO, "Summary edge function constructions: "
                                 << GET_COUNTER("Summary-EF Construction"));
      PHASAR_LOG_LEVEL(INFO,
                       "Total edge function cache hits: " << GET_SUM_COUNT(
                           {"Normal-EF Cache Hit", "Call-EF Cache Hit",
                            "Return-EF Cache Hit", "CallToRet-EF Cache Hit",
                            "Summary-EF Cache Hit"}));
      PHASAR_LOG_LEVEL(
          INFO, "Total edge function constructions: " << GET_SUM_COUNT(
                    {"Normal-EF Construction", "Call-EF Construction",
                     "Return-EF Construction", "CallToRet-EF Construction",
                     "Summary-EF Construction"}));
      PHASAR_LOG_LEVEL(INFO, "----------------------------------------------");
    } else {
      PHASAR_LOG_LEVEL(
          INFO, "Cache statistics only recorded on PAMM severity level: Full.");
    }
  }

  template <typename Handler> void foreachCachedEdgeFunction(Handler Fn) const {
    NormalFunctionCache.for_each_m([&Fn](const auto &OuterEntry) {
      OuterEntry.second.for_each_m([&Fn](const auto &InnerEntry) {
        std::invoke(Fn, InnerEntry.second, EdgeFunctionKind::Normal);
      });
    });

    CallEdgeFunctionCache.for_each_m([&Fn](const auto &OuterEntry) {
      OuterEntry.second.for_each_m([&Fn](const auto &InnerEntry) {
        std::invoke(Fn, InnerEntry.second, EdgeFunctionKind::Call);
      });
    });

    ReturnEdgeFunctionCache.for_each_m([&Fn](const auto &OuterEntry) {
      OuterEntry.second.for_each_m([&Fn](const auto &InnerEntry) {
        std::invoke(Fn, InnerEntry.second, EdgeFunctionKind::Return);
      });
    });

    CallToRetEdgeFunctionCache.for_each_m([&Fn](const auto &OuterEntry) {
      OuterEntry.second.for_each_m([&Fn](const auto &InnerEntry) {
        std::invoke(Fn, InnerEntry.second, EdgeFunctionKind::CallToReturn);
      });
    });

    SummaryEdgeFunctionCache.for_each_m([&Fn](const auto &Entry) {
      std::invoke(Fn, Entry.second, EdgeFunctionKind::Summary);
    });
  }

private:
  using DTKeyCompressorType = std::conditional_t<
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<d_t>>,
      LLVMMapKeyCompressorPll, DefaultMapKeyCompressorPll<d_t>>;
  using NTKeyCompressorType = std::conditional_t<
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<n_t>>,
      LLVMMapKeyCompressorPll, DefaultMapKeyCompressorPll<n_t>>;

  using MapKeyCompressorType = std::conditional_t<
      std::is_same_v<NTKeyCompressorType, DTKeyCompressorType>,
      NTKeyCompressorType,
      MapKeyCompressorCombinatorPll<NTKeyCompressorType, DTKeyCompressorType>>;

  using EdgeFuncInstKey = uint64_t;
  using EdgeFuncNodeKey = std::conditional_t<
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<d_t>>, uint64_t,
      std::pair<d_t, d_t>>;
  using InnerEdgeFunctionMapType =
      EquivalenceClassMap<EdgeFuncNodeKey, EdgeFunctionType>;

  using ZFF = ZeroedFlowFunction<d_t, Container>;

  struct NormalEdgeFlowData {
    NormalEdgeFlowData() noexcept = default;
    NormalEdgeFlowData(FlowFunctionPtrType Val) : FlowFuncPtr(std::move(Val)) {}
    NormalEdgeFlowData(InnerEdgeFunctionMapType Map)
        : EdgeFunctionMap{std::move(Map)} {}

    FlowFunctionPtrType FlowFuncPtr{};
    InnerEdgeFunctionMapType EdgeFunctionMap{};
  };

  constexpr EdgeFuncInstKey createEdgeFunctionInstKey(n_t Lhs, n_t Rhs) {
    uint64_t Val = 0;
    Val |= KeyCompressor.getCompressedID(Lhs);
    Val <<= 32;
    Val |= KeyCompressor.getCompressedID(Rhs);
    return Val;
  }

  constexpr EdgeFuncNodeKey createEdgeFunctionNodeKey(d_t Lhs, d_t Rhs) {
    if constexpr (std::is_base_of_v<llvm::Value, std::remove_pointer_t<d_t>>) {
      uint64_t Val = 0;
      Val |= KeyCompressor.getCompressedID(Lhs);
      Val <<= 32;
      Val |= KeyCompressor.getCompressedID(Rhs);
      return Val;
    } else {
      return std::make_pair(Lhs, Rhs);
    }
  }

  MapKeyCompressorType KeyCompressor;

  IDETabulationProblem<AnalysisDomainTy, Container> &Problem;
  // Auto add zero
  bool AutoAddZero;
  d_t ZV;

  // Caches for the flow/edge functions
  phmap::parallel_node_hash_map_m<EdgeFuncInstKey, NormalEdgeFlowData>
      NormalFunctionCache;

  // Caches for the flow functions
  phmap::parallel_node_hash_map_m<std::tuple<n_t, f_t>, FlowFunctionPtrType>
      CallFlowFunctionCache;
  phmap::parallel_node_hash_map_m<std::tuple<n_t, f_t, n_t, n_t>,
                                  FlowFunctionPtrType>
      ReturnFlowFunctionCache;
  phmap::parallel_node_hash_map_m<std::tuple<n_t, n_t>, FlowFunctionPtrType>
      CallToRetFlowFunctionCache;
  // Caches for the edge functions
  phmap::parallel_node_hash_map_m<std::tuple<n_t, d_t, f_t, d_t>,
                                  EdgeFunctionType>
      CallEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<std::tuple<n_t, f_t, n_t, d_t, n_t, d_t>,
                                  EdgeFunctionType>
      ReturnEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<EdgeFuncInstKey, InnerEdgeFunctionMapType>
      CallToRetEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<std::tuple<n_t, d_t, n_t, d_t>,
                                  EdgeFunctionType>
      SummaryEdgeFunctionCache;
};

} // namespace psr

#endif
