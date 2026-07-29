/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Philipp Schubert and others
 *****************************************************************************/

#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_FLOWEDGEFUNCTIONCACHE_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_FLOWEDGEFUNCTIONCACHE_H

#include "phasar/DataFlow/IfdsIde/EdgeFunctions.h"
#include "phasar/DataFlow/IfdsIde/FlowFunctions.h"
#include "phasar/DataFlow/IfdsIde/IDETabulationProblem.h"
#include "phasar/DataFlow/IfdsIde/Solver/EdgeFunctionKind.h"
#include "phasar/DataFlow/IfdsIde/Solver/FlowEdgeFunctionCacheBase.h"
#include "phasar/DataFlow/IfdsIde/Solver/MapKeyCompressor.h"
#include "phasar/Utils/EquivalenceClassMap.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/NonNullPtr.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/PointerUtils.h"
#include "phasar/Utils/Utilities.h"

#include "llvm/ADT/DenseMap.h"

#include <algorithm>
#include <map>
#include <memory>
#include <set>
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
          typename Container = std::set<typename AnalysisDomainTy::d_t>>
class FlowEdgeFunctionCache
    : public FlowEdgeFunctionCacheBase<
          FlowEdgeFunctionCache<AnalysisDomainTy, Container>, AnalysisDomainTy,
          Container> {
public:
  // Ctor allows access to the IDEProblem in order to get access to flow and
  // edge function factory functions.
  FlowEdgeFunctionCache(
      IDETabulationProblem<AnalysisDomainTy, Container> &Problem)
      : FlowEdgeFunctionCacheBase<
            FlowEdgeFunctionCache<AnalysisDomainTy, Container>,
            AnalysisDomainTy, Container>(Problem) {}
  using Base = FlowEdgeFunctionCacheBase<
      FlowEdgeFunctionCache<AnalysisDomainTy, Container>, AnalysisDomainTy,
      Container>;

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheNormalFlowFunction(Base::EdgeFuncInstKey Key, Base::n_t Curr,
                          Base::n_t Succ) {
    // operator[] instead of try_emplace: NormalEdgeFlowData holds both the
    // flow function ptr and the edge function map for the same (Curr,Succ)
    // key, so getNormalEdgeFunction shares this entry via the same lookup.
    auto &NormalFE = NormalFunctionCache[std::move(Key)];
    if (!NormalFE.FlowFuncPtr) {
      INC_COUNTER("Normal-FF Construction", 1, Full);
      auto FF = Base::Problem.getNormalFlowFunction(Curr, Succ);
      NormalFE.FlowFuncPtr =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Normal-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(NormalFE.FlowFuncPtr);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t> Key,
                        Base::n_t CallSite, Base::f_t DestFun) {
    auto [It, Inserted] = CallFlowFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("Call-FF Construction", 1, Full);
      auto FF = Base::Problem.getCallFlowFunction(CallSite, DestFun);
      It->second =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Call-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(It->second);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheRetFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t,
                                  typename Base::n_t, typename Base::n_t> Key,
                       Base::n_t CallSite, Base::f_t CalleeFun,
                       Base::n_t ExitInst, Base::n_t RetSite) {
    auto [It, Inserted] = ReturnFlowFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("Return-FF Construction", 1, Full);
      auto FF = Base::Problem.getRetFlowFunction(CallSite, CalleeFun, ExitInst,
                                                 RetSite);
      It->second =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);

      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("Return-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(It->second);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallToRetFlowFunction(
      std::tuple<typename Base::n_t, typename Base::n_t> Key,
      Base::n_t CallSite, Base::n_t RetSite,
      llvm::ArrayRef<typename Base::f_t> Callees) {
    auto [It, Inserted] =
        CallToRetFlowFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("CallToRet-FF Construction", 1, Full);
      auto FF =
          Base::Problem.getCallToRetFlowFunction(CallSite, RetSite, Callees);
      It->second =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);

      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      INC_COUNTER("CallToRet-FF Cache Hit", 1, Full);
    }

    return getPointerFrom(It->second);
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheNormalEdgeFunction(typename Base::EdgeFuncInstKey OuterMapKey,
                          Base::n_t Curr, Base::d_t CurrNode, Base::n_t Succ,
                          Base::d_t SuccNode) {
    auto &NormalFE = NormalFunctionCache[std::move(OuterMapKey)];

    auto Ret = NormalFE.EdgeFunctionMap.getOrInsertLazy(
        Base::createEdgeFunctionNodeKey(CurrNode, SuccNode),
        [&] {
          INC_COUNTER("Normal-EF Construction", 1, Full);
          auto EF = Base::Problem.getNormalEdgeFunction(Curr, CurrNode, Succ,
                                                        SuccNode);
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

  [[nodiscard]] Base::EdgeFunctionType
  cacheCallEdgeFunction(std::tuple<typename Base::n_t, typename Base::d_t,
                                   typename Base::f_t, typename Base::d_t> Key,
                        Base::n_t CallSite, Base::d_t SrcNode,
                        Base::f_t DestinationFunction, Base::d_t DestNode) {
    auto [It, Inserted] = CallEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("Call-EF Construction", 1, Full);
      It->second = Base::Problem.getCallEdgeFunction(
          CallSite, SrcNode, DestinationFunction, DestNode);

      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    } else {
      INC_COUNTER("Call-EF Cache Hit", 1, Full);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
  }

  [[nodiscard]] Base::EdgeFunctionType cacheReturnEdgeFunction(
      std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                 typename Base::d_t, typename Base::n_t, typename Base::d_t>
          Key,
      Base::n_t CallSite, Base::f_t CalleeFunction, Base::n_t ExitInst,
      Base::d_t ExitNode, Base::n_t RetSite, Base::d_t RetNode) {
    auto [It, Inserted] = ReturnEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("Return-EF Construction", 1, Full);
      It->second = Base::Problem.getReturnEdgeFunction(
          CallSite, CalleeFunction, ExitInst, ExitNode, RetSite, RetNode);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheCallToRetEdgeFunction(typename Base::EdgeFuncInstKey OuterMapKey,
                             Base::n_t CallSite, Base::d_t CallNode,
                             Base::n_t RetSite, Base::d_t RetSiteNode,
                             llvm::ArrayRef<typename Base::f_t> Callees) {
    auto &Outer = CallToRetEdgeFunctionCache[std::move(OuterMapKey)];

    auto Ret = Outer.getOrInsertLazy(
        std::move(Base::createEdgeFunctionNodeKey(CallNode, RetSiteNode)),
        [&] {
          INC_COUNTER("CallToRet-EF Construction", 1, Full);
          auto Ret = Base::Problem.getCallToRetEdgeFunction(
              CallSite, CallNode, RetSite, RetSiteNode, Callees);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          return Ret;
        },
        [&] {
          INC_COUNTER("CallToRet-EF Cache Hit", 1, Full);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        });
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Ret);
    return Ret;
  }

  [[nodiscard]] Base::EdgeFunctionType cacheSummaryEdgeFunction(
      std::tuple<typename Base::n_t, typename Base::d_t, typename Base::n_t,
                 typename Base::d_t> Key,
      Base::n_t CallSite, Base::d_t CallNode, Base::n_t RetSite,
      Base::d_t RetSiteNode) {
    auto [It, Inserted] = SummaryEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      INC_COUNTER("Summary-EF Construction", 1, Full);
      It->second = Base::Problem.getSummaryEdgeFunction(CallSite, CallNode,
                                                        RetSite, RetSiteNode);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    } else {
      INC_COUNTER("Summary-EF Cache Hit", 1, Full);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
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

  template <typename Handler>
  void foreachCachedEdgeFunctionImpl(Handler Fn) const {
    for (const auto &[Key, NormalFns] : NormalFunctionCache) {
      for (const auto &[Set, EF] : NormalFns.EdgeFunctionMap) {
        std::invoke(Fn, EF, EdgeFunctionKind::Normal);
      }
    }

    for (const auto &[Key, EF] : CallEdgeFunctionCache) {
      std::invoke(Fn, EF, EdgeFunctionKind::Call);
    }

    for (const auto &[Key, EF] : ReturnEdgeFunctionCache) {
      std::invoke(Fn, EF, EdgeFunctionKind::Return);
    }

    for (const auto &[Key, CTRFns] : CallToRetEdgeFunctionCache) {
      for (const auto &[Set, EF] : CTRFns) {
        std::invoke(Fn, EF, EdgeFunctionKind::CallToReturn);
      }
    }

    for (const auto &[Key, EF] : SummaryEdgeFunctionCache) {
      std::invoke(Fn, EF, EdgeFunctionKind::Summary);
    }
  }

private:
  // Caches for the flow/edge functions
  std::map<typename Base::EdgeFuncInstKey, typename Base::NormalEdgeFlowData>
      NormalFunctionCache;

  // Caches for the flow functions
  std::map<std::tuple<typename Base::n_t, typename Base::f_t>,
           typename Base::FlowFunctionPtrType>
      CallFlowFunctionCache;
  std::map<std::tuple<typename Base::n_t, typename Base::f_t,
                      typename Base::n_t, typename Base::n_t>,
           typename Base::FlowFunctionPtrType>
      ReturnFlowFunctionCache;
  std::map<std::tuple<typename Base::n_t, typename Base::n_t>,
           typename Base::FlowFunctionPtrType>
      CallToRetFlowFunctionCache;
  // Caches for the edge functions
  std::map<std::tuple<typename Base::n_t, typename Base::d_t,
                      typename Base::f_t, typename Base::d_t>,
           typename Base::EdgeFunctionType>
      CallEdgeFunctionCache;
  std::map<
      std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                 typename Base::d_t, typename Base::n_t, typename Base::d_t>,
      typename Base::EdgeFunctionType>
      ReturnEdgeFunctionCache;
  std::map<typename Base::EdgeFuncInstKey,
           typename Base::InnerEdgeFunctionMapType>
      CallToRetEdgeFunctionCache;
  std::map<std::tuple<typename Base::n_t, typename Base::d_t,
                      typename Base::n_t, typename Base::d_t>,
           typename Base::EdgeFunctionType>
      SummaryEdgeFunctionCache;
};

} // namespace psr

#endif
