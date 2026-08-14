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
#include "phasar/DataFlow/IfdsIde/Solver/FlowEdgeFunctionCacheBase.h"
#include "phasar/DataFlow/IfdsIde/Solver/MapKeyCompressor.h"
#include "phasar/Utils/EquivalenceClassMap.h"
#include "phasar/Utils/Logger.h"
#include "phasar/Utils/PAMMMacros.h"
#include "phasar/Utils/PointerUtils.h"
#include "phasar/Utils/ThreadUtils.h"
#include "phasar/Utils/Utilities.h"

#include "parallel_hashmap/phmap.h"
#include "parallel_hashmap/phmap_fwd_decl.h"

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
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
class FlowEdgeFunctionCachePll
    : public FlowEdgeFunctionCacheBase<
          FlowEdgeFunctionCachePll<AnalysisDomainTy, Container>,
          AnalysisDomainTy, Container,
          phmap::parallel_flat_hash_map_m<const llvm::Value *, uint32_t>> {
public:
  // Ctor allows access to the IDEProblem in order to get access to flow and
  // edge function factory functions.
  FlowEdgeFunctionCachePll(
      IDETabulationProblem<AnalysisDomainTy, Container> &Problem)
      : FlowEdgeFunctionCacheBase<
            FlowEdgeFunctionCachePll<AnalysisDomainTy, Container>,
            AnalysisDomainTy, Container,
            typename FlowEdgeFunctionCachePll::FlowEdgeFunctionCacheBase::
                CompressorContainerType>(Problem) {}
  using Base = FlowEdgeFunctionCacheBase<
      FlowEdgeFunctionCachePll<AnalysisDomainTy, Container>, AnalysisDomainTy,
      Container,
      typename FlowEdgeFunctionCachePll::FlowEdgeFunctionCacheBase::
          CompressorContainerType>;

private:
  template <typename CacheContainer, typename CacheKey,
            typename PAMMConstructionType, typename PAMMHitType>
  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheFlowFunction(CacheContainer &CurrContainer,
                    std::invocable<> auto FlowFuncCallback,
                    PAMMConstructionType &PAMMFFConstruction,
                    PAMMHitType &PAMMFFCacheHit, CacheKey Key) {
    typename Base::FlowFunctionType *Ret;

    CurrContainer.lazy_emplace_l(
        Key,
        [&](auto &Found) {
          if constexpr (std::is_same_v<
                            CacheContainer,
                            PllMap<EdgeFuncInstKey, NormalEdgeFlowData>>) {
            Ret = getPointerFrom(Found.second.FlowFuncPtr);
          } else {
            Ret = getPointerFrom(Found.second);
          }
          PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
          PAMMFFCacheHit++;
        },
        [&](auto &&Ctor) {
          PAMMFFConstruction++;
          auto FF = FlowFuncCallback();
          auto Ptr = Base::AutoAddZero
                         ? std::make_unique<ZFF>(std::move(FF), Base::ZV)
                         : std::move(FF);
          Ret = getPointerFrom(Ptr);
          Ctor(std::move(Key), std::move(Ptr));
          PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
        });

    return getPointerFrom(Ret);
  }

public:
  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheNormalFlowFunction(Base::EdgeFuncInstKey Key, Base::n_t Curr,
                          Base::n_t Succ) {
    return cacheFlowFunction(
        NormalFunctionCache,
        [&]() { return Base::Problem.getNormalFlowFunction(Curr, Succ); },
        Base::NormalFF_Construction, Base::NormalFF_CacheHit, Key);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t> Key,
                        Base::n_t CallSite, Base::f_t DestFun) {
    return cacheFlowFunction(
        CallFlowFunctionCache,
        [&]() { return Base::Problem.getCallFlowFunction(CallSite, DestFun); },
        Base::CallFF_Construction, Base::CallFF_CacheHit, Key);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheRetFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t,
                                  typename Base::n_t, typename Base::n_t> Key,
                       Base::n_t CallSite, Base::f_t CalleeFun,
                       Base::n_t ExitInst, Base::n_t RetSite) {
    return cacheFlowFunction(
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
    return cacheFlowFunction(
        CallToRetFlowFunctionCache,
        [&]() {
          return Base::Problem.getCallToRetFlowFunction(CallSite, RetSite,
                                                        Callees);
        },
        Base::CallToRetFF_Construction, Base::CallToRetFF_CacheHit, Key);
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheNormalEdgeFunction(typename Base::EdgeFuncInstKey OuterMapKey,
                          Base::n_t Curr, Base::d_t CurrNode, Base::n_t Succ,
                          Base::d_t SuccNode) {
    auto [NormalFE, Inserted] =
        NormalFunctionCache.try_emplace_p(std::move(OuterMapKey));

    typename Base::EdgeFunctionType Ret;
    {
      // EdgeFunctionMap (EquivalenceClassMap) is a plain, unsynchronized
      // linear-scan container, so concurrent access to the *same* Curr/Succ
      // entry must be serialized. Striping the lock by key (instead of one
      // mutex for the whole cache) still guarantees that, while letting
      // unrelated Curr/Succ keys proceed in parallel.
      std::lock_guard Guard(
          EdgeFunctionMapMutexes[std::hash<EdgeFuncInstKey>{}(OuterMapKey) %
                                 EdgeFunctionMapMutexes.size()]);
      Ret = NormalFE->second.EdgeFunctionMap.getOrInsertLazy(
          createEdgeFunctionNodeKey(CurrNode, SuccNode),
          [&] {
            Base::NormalEF_CacheHit++;
            auto EF = Base::Problem.getNormalEdgeFunction(Curr, CurrNode, Succ,
                                                          SuccNode);
            PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
            return EF;
          },
          [&] {
            Base::NormalEF_Construction++;
            PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          });
    }

    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Ret);
    return Ret;
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheCallEdgeFunction(std::tuple<typename Base::n_t, typename Base::d_t,
                                   typename Base::f_t, typename Base::d_t> Key,
                        Base::n_t CallSite, Base::d_t SrcNode,
                        Base::f_t DestinationFunction, Base::d_t DestNode) {
    EdgeFunction<typename Base::l_t> ReturnEF{};

    CallEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          Base::CallEF_CacheHit++;
          ReturnEF = Base::Problem.getCallEdgeFunction(
              CallSite, SrcNode, DestinationFunction, DestNode);
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        },
        [&](auto &&Ctor) {
          Base::CallEF_Construction++;
          auto EF = Base::Problem.getCallEdgeFunction(
              CallSite, SrcNode, DestinationFunction, DestNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
        });

    return ReturnEF;
  }

  [[nodiscard]] Base::EdgeFunctionType cacheReturnEdgeFunction(
      std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                 typename Base::d_t, typename Base::n_t, typename Base::d_t>
          Key,
      Base::n_t CallSite, Base::f_t CalleeFunction, Base::n_t ExitInst,
      Base::d_t ExitNode, Base::n_t RetSite, Base::d_t RetNode) {
    EdgeFunction<typename Base::l_t> ReturnEF{};

    ReturnEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          Base::ReturnEF_CacheHit++;
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        },
        [&](auto &&Ctor) {
          Base::ReturnEF_Construction++;
          auto EF = Base::Problem.getReturnEdgeFunction(
              CallSite, CalleeFunction, ExitInst, ExitNode, RetSite, RetNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(std::move(Key), std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
        });

    return ReturnEF;
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheCallToRetEdgeFunction(typename Base::EdgeFuncInstKey OuterMapKey,
                             Base::n_t CallSite, Base::d_t CallNode,
                             Base::n_t RetSite, Base::d_t RetSiteNode,
                             llvm::ArrayRef<typename Base::f_t> Callees) {
    // TODO: This function might have been badly merged by me (Max).
    //       Go over this again and check if everything is correct.
    //       Both logic and logging.
    std::pair<EdgeFuncInstKey, InnerEdgeFunctionMapType>
        CallToRetEdgeFunctionCacheEntry;
    if (CallToRetEdgeFunctionCache.if_contains(
            OuterMapKey, [&CallToRetEdgeFunctionCacheEntry](auto &Entry) {
              CallToRetEdgeFunctionCacheEntry = Entry;
            })) {
      auto SearchEdgeFunc = CallToRetEdgeFunctionCacheEntry.second.find(
          createEdgeFunctionNodeKey(CallNode, RetSiteNode));
      if (SearchEdgeFunc != CallToRetEdgeFunctionCacheEntry.second.end()) {
        Base::CallToRetEF_Construction++;
        PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
        return SearchEdgeFunc->second;
      }
      Base::CallToRetEF_CacheHit++;
      auto EF = Base::Problem.getCallToRetEdgeFunction(
          CallSite, CallNode, RetSite, RetSiteNode, Callees);

      CallToRetEdgeFunctionCacheEntry.second.insert(
          createEdgeFunctionNodeKey(CallNode, RetSiteNode), EF);

      PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
      return EF;
    }

    auto EF = Base::Problem.getCallToRetEdgeFunction(
        CallSite, CallNode, RetSite, RetSiteNode, Callees);

    CallToRetEdgeFunctionCache.emplace(
        OuterMapKey,
        InnerEdgeFunctionMapType{std::make_pair(
            createEdgeFunctionNodeKey(CallNode, RetSiteNode), EF)});
    PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
    PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
    return EF;
  }

  [[nodiscard]] Base::EdgeFunctionType cacheSummaryEdgeFunction(
      std::tuple<typename Base::n_t, typename Base::d_t, typename Base::n_t,
                 typename Base::d_t> Key,
      Base::n_t CallSite, Base::d_t CallNode, Base::n_t RetSite,
      Base::d_t RetSiteNode) {
    EdgeFunction<typename Base::l_t> ReturnEF{};
    SummaryEdgeFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Val) {
          Base::SummaryEF_CacheHit++;
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
        },
        [&](auto &&Ctor) {
          Base::SummaryEF_Construction++;
          auto EF = Base::Problem.getSummaryEdgeFunction(CallSite, CallNode,
                                                         RetSite, RetSiteNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
        });

    return ReturnEF;
  }

  template <typename Handler>
  void foreachCachedEdgeFunctionImpl(Handler Fn) const {
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
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<typename Base::d_t>>,
      LLVMMapKeyCompressor<>, DefaultMapKeyCompressor<typename Base::d_t>>;
  using NTKeyCompressorType = std::conditional_t<
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<typename Base::n_t>>,
      LLVMMapKeyCompressor<>, DefaultMapKeyCompressor<typename Base::n_t>>;

  using MapKeyCompressorType = std::conditional_t<
      std::is_same_v<NTKeyCompressorType, DTKeyCompressorType>,
      NTKeyCompressorType,
      MapKeyCompressorCombinator<NTKeyCompressorType, DTKeyCompressorType>>;

  using EdgeFuncInstKey = uint64_t;
  using EdgeFuncNodeKey = std::conditional_t<
      std::is_base_of_v<llvm::Value, std::remove_pointer_t<typename Base::d_t>>,
      uint64_t, std::pair<typename Base::d_t, typename Base::d_t>>;
  using InnerEdgeFunctionMapType =
      EquivalenceClassMap<EdgeFuncNodeKey, typename Base::EdgeFunctionType,
                          phmap::parallel_node_hash_set_m<EdgeFuncNodeKey>>;

  using ZFF = ZeroedFlowFunction<typename Base::d_t, Container>;

  struct NormalEdgeFlowData {
    NormalEdgeFlowData() noexcept = default;
    NormalEdgeFlowData(typename Base::FlowFunctionPtrType Val)
        : FlowFuncPtr(std::move(Val)) {}
    NormalEdgeFlowData(InnerEdgeFunctionMapType Map)
        : EdgeFunctionMap{std::move(Map)} {}

    typename Base::FlowFunctionPtrType FlowFuncPtr{};
    InnerEdgeFunctionMapType EdgeFunctionMap{};
  };

  constexpr EdgeFuncNodeKey createEdgeFunctionNodeKey(typename Base::d_t Lhs,
                                                      typename Base::d_t Rhs) {
    if constexpr (std::is_base_of_v<
                      llvm::Value, std::remove_pointer_t<typename Base::d_t>>) {
      uint64_t Val = 0;
      Val |= Base::KeyCompressor.getCompressedID(Lhs);
      Val <<= 32;
      Val |= Base::KeyCompressor.getCompressedID(Rhs);
      return Val;
    } else {
      return std::make_pair(Lhs, Rhs);
    }
  }

  // Striped locks guarding EdgeFunctionMap access in NormalEdgeFlowData (see
  // getNormalEdgeFunction): one mutex for the whole cache would serialize all
  // Curr/Succ keys against each other, not just concurrent accesses to the
  // same key.
  std::array<std::mutex, NumOfShards> EdgeFunctionMapMutexes;

  // Caches for the flow/edge functions
  PllMap<EdgeFuncInstKey, NormalEdgeFlowData> NormalFunctionCache;

  // Caches for the flow functions
  PllMap<std::tuple<typename Base::n_t, typename Base::f_t>,
         typename Base::FlowFunctionPtrType>
      CallFlowFunctionCache;
  PllMap<std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                    typename Base::n_t>,
         typename Base::FlowFunctionPtrType>
      ReturnFlowFunctionCache;
  PllMap<std::tuple<typename Base::n_t, typename Base::n_t>,
         typename Base::FlowFunctionPtrType>
      CallToRetFlowFunctionCache;
  // Caches for the edge functions
  PllMap<std::tuple<typename Base::n_t, typename Base::d_t, typename Base::f_t,
                    typename Base::d_t>,
         typename Base::EdgeFunctionType>
      CallEdgeFunctionCache;
  PllMap<std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                    typename Base::d_t, typename Base::n_t, typename Base::d_t>,
         typename Base::EdgeFunctionType>
      ReturnEdgeFunctionCache;
  PllMap<EdgeFuncInstKey, InnerEdgeFunctionMapType> CallToRetEdgeFunctionCache;
  PllMap<std::tuple<typename Base::n_t, typename Base::d_t, typename Base::n_t,
                    typename Base::d_t>,
         typename Base::EdgeFunctionType>
      SummaryEdgeFunctionCache;
};

} // namespace psr

#endif
