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
  FlowEdgeFunctionCache(
      IDETabulationProblem<AnalysisDomainTy, Container> &Problem)
      : FlowEdgeFunctionCacheBase<
            FlowEdgeFunctionCache<AnalysisDomainTy, Container>,
            AnalysisDomainTy, Container>(Problem) {}

  using Base = FlowEdgeFunctionCacheBase<
      FlowEdgeFunctionCache<AnalysisDomainTy, Container>, AnalysisDomainTy,
      Container>;

  ~FlowEdgeFunctionCache() = default;

  FlowEdgeFunctionCache(const FlowEdgeFunctionCache &FEFC) = default;
  FlowEdgeFunctionCache &operator=(const FlowEdgeFunctionCache &FEFC) = default;

  FlowEdgeFunctionCache(FlowEdgeFunctionCache &&FEFC) noexcept = default;
  FlowEdgeFunctionCache &
  operator=(FlowEdgeFunctionCache &&FEFC) noexcept = default;

private:
  template <typename CacheContainer, typename CacheKey,
            typename PAMMConstructionType, typename PAMMHitType>
  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheNonNormalFlowFunction(CacheContainer &CurrContainer,
                             std::invocable<> auto FlowFuncCallback,
                             PAMMConstructionType &PAMMFFConstruction,
                             PAMMHitType &PAMMFFCacheHit, CacheKey Key) {
    auto [It, Inserted] = CurrContainer.try_emplace(std::move(Key));
    if (Inserted) {
      PAMMFFConstruction++;
      auto FF = FlowFuncCallback();
      It->second =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      PAMMFFCacheHit++;
    }

    return getPointerFrom(It->second);
  }

public:
  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheNormalFlowFunction(Base::EdgeFuncInstKey Key, Base::n_t Curr,
                          Base::n_t Succ) {
    // operator[] instead of try_emplace: NormalEdgeFlowData holds both the
    // flow function ptr and the edge function map for the same (Curr,Succ)
    // key, so getNormalEdgeFunction shares this entry via the same lookup.
    auto &NormalFE = NormalFunctionCache[std::move(Key)];
    if (!NormalFE.FlowFuncPtr) {
      Base::NormalFF_Construction++;
      auto FF = Base::Problem.getNormalFlowFunction(Curr, Succ);
      NormalFE.FlowFuncPtr =
          Base::AutoAddZero
              ? std::make_unique<typename Base::ZFF>(std::move(FF), Base::ZV)
              : std::move(FF);
      PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
    } else {
      PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
      Base::NormalFF_CacheHit++;
    }

    return getPointerFrom(NormalFE.FlowFuncPtr);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t> Key,
                        Base::n_t CallSite, Base::f_t DestFun) {
    return cacheNonNormalFlowFunction(
        CallFlowFunctionCache,
        [&]() { return Base::Problem.getCallFlowFunction(CallSite, DestFun); },
        Base::CallFF_Construction, Base::CallFF_CacheHit, Key);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheRetFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t,
                                  typename Base::n_t, typename Base::n_t> Key,
                       Base::n_t CallSite, Base::f_t CalleeFun,
                       Base::n_t ExitInst, Base::n_t RetSite) {
    return cacheNonNormalFlowFunction(
        ReturnFlowFunctionCache,
        [&]() {
          return Base::Problem.getRetFlowFunction(CallSite, CalleeFun, ExitInst,
                                                  RetSite);
        },
        Base::ReturnFF_Construction, Base::ReturnFF_CacheHit, Key);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallToRetFlowFunction(
      std::tuple<typename Base::n_t, typename Base::n_t> Key,
      Base::n_t CallSite, Base::n_t RetSite,
      llvm::ArrayRef<typename Base::f_t> Callees) {
    return cacheNonNormalFlowFunction(
        CallToRetFlowFunctionCache,
        [&]() {
          return Base::Problem.getCallToRetFlowFunction(CallSite, RetSite,
                                                        Callees);
        },
        Base::CallToRetFF_Construction, Base::CallToRetFF_CacheHit, Key);
  }

  /// \note Unlike the other get*FlowFunction methods, this returns a nullable
  /// FlowFunctionPtrType rather than NonNullPtr. A null return means no special
  /// summary is available for this call site; the solver falls back to
  /// propagating through the callee body.
  [[nodiscard]] FlowFunctionPtrType getSummaryFlowFunction(n_t CallSite,
                                                           f_t DestFun) {
    assertNotNull(CallSite);
    assertNotNull(DestFun);

    Base::SummaryFF_Construction++;
    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Summary flow function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Stmt : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(F) Dest Mthd : " << FToString(DestFun));
        PHASAR_LOG_LEVEL(DEBUG, ' '));
    auto FF = Base::Problem.getSummaryFlowFunction(CallSite, DestFun);
    return FF;
  }

  [[nodiscard]] EdgeFunctionType getNormalEdgeFunction(n_t Curr, d_t CurrNode,
                                                       n_t Succ, d_t SuccNode) {
    assertNotNull(Curr);
    assertNotNull(Succ);

    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Normal edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Curr Inst : " << NToString(Curr));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Curr Node : " << DToString(CurrNode));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Succ Inst : " << NToString(Succ));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Succ Node : " << DToString(SuccNode)));

    typename Base::EdgeFuncInstKey OuterMapKey =
        this->createEdgeFunctionInstKey(Curr, Succ);
    auto &NormalFE = NormalFunctionCache[std::move(OuterMapKey)];

    auto Ret = NormalFE.EdgeFunctionMap.getOrInsertLazy(
        Base::createEdgeFunctionNodeKey(CurrNode, SuccNode),
        [&] {
          Base::NormalEF_Construction++;
          auto EF = Base::Problem.getNormalEdgeFunction(Curr, CurrNode, Succ,
                                                        SuccNode);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          return EF;
        },
        [&] {
          Base::NormalEF_CacheHit++;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        });
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Ret);

    return Ret;
  }

  [[nodiscard]] EdgeFunctionType getCallEdgeFunction(n_t CallSite, d_t SrcNode,
                                                     f_t DestinationFunction,
                                                     d_t DestNode) {

    assertNotNull(CallSite);
    assertNotNull(DestinationFunction);

    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Call edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Stmt : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Src Node  : " << DToString(SrcNode));

        PHASAR_LOG_LEVEL(DEBUG,
                         "(F) Dest Fun : " << FToString(DestinationFunction));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Dest Node : " << DToString(DestNode)));
    auto Key = std::tie(CallSite, SrcNode, DestinationFunction, DestNode);

    auto [It, Inserted] = CallEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      Base::CallEF_Construction++;
      It->second = Base::Problem.getCallEdgeFunction(
          CallSite, SrcNode, DestinationFunction, DestNode);

      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    } else {
      Base::CallEF_CacheHit++;
      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
  }

  [[nodiscard]] EdgeFunctionType
  getReturnEdgeFunction(n_t CallSite, f_t CalleeFunction, n_t ExitInst,
                        d_t ExitNode, n_t RetSite, d_t RetNode) {
    assertNotNull(CallSite);
    assertNotNull(CalleeFunction);
    assertNotNull(ExitInst);
    assertNotNull(RetSite);

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
    auto [It, Inserted] = ReturnEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      Base::ReturnEF_Construction++;
      It->second = Base::Problem.getReturnEdgeFunction(
          CallSite, CalleeFunction, ExitInst, ExitNode, RetSite, RetNode);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    } else {
      Base::ReturnEF_CacheHit++;
      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
  }

  [[nodiscard]] EdgeFunctionType
  getCallToRetEdgeFunction(n_t CallSite, d_t CallNode, n_t RetSite,
                           d_t RetSiteNode, llvm::ArrayRef<f_t> Callees) {
    assertNotNull(CallSite);
    assertNotNull(RetSite);
    assertAllNotNull(Callees);

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

    typename Base::EdgeFuncInstKey OuterMapKey =
        this->createEdgeFunctionInstKey(CallSite, RetSite);
    auto &Outer = CallToRetEdgeFunctionCache[std::move(OuterMapKey)];

    auto Ret = Outer.getOrInsertLazy(
        std::move(Base::createEdgeFunctionNodeKey(CallNode, RetSiteNode)),
        [&] {
          Base::CallToRetEF_Construction++;
          auto Ret = Base::Problem.getCallToRetEdgeFunction(
              CallSite, CallNode, RetSite, RetSiteNode, Callees);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          return Ret;
        },
        [&] {
          Base::CallToRetEF_CacheHit++;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        });
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Ret);
    return Ret;
  }

  [[nodiscard]] EdgeFunctionType getSummaryEdgeFunction(n_t CallSite,
                                                        d_t CallNode,
                                                        n_t RetSite,
                                                        d_t RetSiteNode) {
    assertNotNull(CallSite);
    assertNotNull(RetSite);

    IF_LOG_ENABLED(
        PHASAR_LOG_LEVEL(DEBUG, "Summary edge function factory call");
        PHASAR_LOG_LEVEL(DEBUG, "(N) Call Site : " << NToString(CallSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Call Node : " << DToString(CallNode));
        PHASAR_LOG_LEVEL(DEBUG, "(N) Ret Site  : " << NToString(RetSite));
        PHASAR_LOG_LEVEL(DEBUG, "(D) Ret Node  : " << DToString(RetSiteNode));
        PHASAR_LOG_LEVEL(DEBUG, ' '));
    auto Key = std::tie(CallSite, CallNode, RetSite, RetSiteNode);
    auto [It, Inserted] = SummaryEdgeFunctionCache.try_emplace(std::move(Key));

    if (Inserted) {
      Base::SummaryEF_Construction++;
      It->second = Base::Problem.getSummaryEdgeFunction(CallSite, CallNode,
                                                        RetSite, RetSiteNode);
      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    } else {
      Base::SummaryEF_CacheHit++;
      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << It->second);

    return It->second;
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
