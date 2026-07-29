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
          AnalysisDomainTy, Container> {
public:
  // Ctor allows access to the IDEProblem in order to get access to flow and
  // edge function factory functions.
  FlowEdgeFunctionCachePll(
      IDETabulationProblem<AnalysisDomainTy, Container> &Problem)
      : FlowEdgeFunctionCacheBase<
            FlowEdgeFunctionCachePll<AnalysisDomainTy, Container>,
            AnalysisDomainTy, Container>(Problem) {}
  using Base = FlowEdgeFunctionCacheBase<
      FlowEdgeFunctionCachePll<AnalysisDomainTy, Container>, AnalysisDomainTy,
      Container>;

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheNormalFlowFunction(Base::EdgeFuncInstKey Key, Base::n_t Curr,
                          Base::n_t Succ) {
    typename Base::FlowFunctionType *Ret;

    NormalFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Found) {
          Ret = getPointerFrom(Found.second.FlowFuncPtr);
          PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
          INC_COUNTER("Normal-FF Cache Hit", 1, Full);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Normal-FF Construction", 1, Full);
          auto FF = Base::Problem.getNormalFlowFunction(Curr, Succ);
          auto Ptr = Base::AutoAddZero
                         ? std::make_unique<ZFF>(std::move(FF), Base::ZV)
                         : std::move(FF);
          Ret = getPointerFrom(Ptr);
          Ctor(std::move(Key), std::move(Ptr));
          PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
        });

    return getPointerFrom(Ret);
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t> Key,
                        Base::n_t CallSite, Base::f_t DestFun) {
    typename Base::FlowFunctionType *Ret;

    CallFlowFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Found) {
          Ret = getPointerFrom(Found.second);
          PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
          INC_COUNTER("Call-FF Cache Hit", 1, Full);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Call-FF Construction", 1, Full);
          auto FF = Base::Problem.getCallFlowFunction(CallSite, DestFun);
          auto Ptr = Base::AutoAddZero
                         ? std::make_unique<ZFF>(std::move(FF), Base::ZV)
                         : std::move(FF);
          Ret = getPointerFrom(Ptr);
          Ctor(std::move(Key), std::move(Ptr));
          PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
        });

    return Ret;
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheRetFlowFunction(std::tuple<typename Base::n_t, typename Base::f_t,
                                  typename Base::n_t, typename Base::n_t> Key,
                       Base::n_t CallSite, Base::f_t CalleeFun,
                       Base::n_t ExitInst, Base::n_t RetSite) {
    typename Base::FlowFunctionType *Ret;

    ReturnFlowFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Found) {
          Ret = getPointerFrom(Found.second);
          PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
          INC_COUNTER("Return-FF Cache Hit", 1, Full);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Return-FF Construction", 1, Full);
          auto FF = Base::Problem.getRetFlowFunction(CallSite, CalleeFun,
                                                     ExitInst, RetSite);
          auto Ptr = Base::AutoAddZero
                         ? std::make_unique<ZFF>(std::move(FF), Base::ZV)
                         : std::move(FF);
          Ret = getPointerFrom(Ptr);
          Ctor(std::move(Key), std::move(Ptr));

          PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
        });

    return Ret;
  }

  [[nodiscard]] NonNullPtr<typename Base::FlowFunctionType>
  cacheCallToRetFlowFunction(
      std::tuple<typename Base::n_t, typename Base::n_t> Key,
      Base::n_t CallSite, Base::n_t RetSite,
      llvm::ArrayRef<typename Base::f_t> Callees) {
    typename Base::FlowFunctionPtrType Ret;

    CallToRetFlowFunctionCache.lazy_emplace_l(
        Key,
        [&](auto &Found) {
          PHASAR_LOG_LEVEL(DEBUG, "Flow function fetched from cache");
          INC_COUNTER("CallToRet-FF Cache Hit", 1, Full);
          Ret = getPointerFrom(Found.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("CallToRet-FF Construction", 1, Full);
          auto FF = Base::Problem.getCallToRetFlowFunction(CallSite, RetSite,
                                                           Callees);
          auto Ptr = Base::AutoAddZero
                         ? std::make_unique<ZFF>(std::move(FF), Base::ZV)
                         : std::move(FF);
          Ret = getPointerFrom(Ptr);
          Ctor(std::move(Key), std::move(Ptr));

          PHASAR_LOG_LEVEL(DEBUG, "Flow function constructed");
        });

    return getPointerFrom(Ret.get());
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
          INC_COUNTER("Call-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Call-EF Construction", 1, Full);
          auto EF = Base::Problem.getCallEdgeFunction(
              CallSite, SrcNode, DestinationFunction, DestNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(Key, std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
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
          INC_COUNTER("Return-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Return-EF Construction", 1, Full);
          auto EF = Base::Problem.getReturnEdgeFunction(
              CallSite, CalleeFunction, ExitInst, ExitNode, RetSite, RetNode);
          ReturnEF = EF;
          PSR_FWD(Ctor)(std::move(Key), std::move(EF));
          PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
        });

    return ReturnEF;
  }

  [[nodiscard]] Base::EdgeFunctionType
  cacheCallToRetEdgeFunction(typename Base::EdgeFuncInstKey OuterMapKey,
                             Base::n_t CallSite, Base::d_t CallNode,
                             Base::n_t RetSite, Base::d_t RetSiteNode,
                             llvm::ArrayRef<typename Base::f_t> Callees) {
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
      auto EF = Base::Problem.getCallToRetEdgeFunction(
          CallSite, CallNode, RetSite, RetSiteNode, Callees);

      CallToRetEdgeFunctionCacheEntry.second.insert(
          createEdgeFunctionNodeKey(CallNode, RetSiteNode), EF);

      PHASAR_LOG_LEVEL(DEBUG, "Edge function constructed");
      PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << EF);
      return EF;
    }

    INC_COUNTER("CallToRet-EF Construction", 1, Full);
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
          INC_COUNTER("Summary-EF Cache Hit", 1, Full);
          ReturnEF = Val.second;
          PHASAR_LOG_LEVEL(DEBUG, "Edge function fetched from cache");
          PHASAR_LOG_LEVEL(DEBUG, "Provide Edge Function: " << Val.second);
        },
        [&](auto &&Ctor) {
          INC_COUNTER("Summary-EF Construction", 1, Full);
          auto EF = Base::Problem.getSummaryEdgeFunction(CallSite, CallNode,
                                                         RetSite, RetSiteNode);
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

  constexpr EdgeFuncInstKey createEdgeFunctionInstKey(typename Base::n_t Lhs,
                                                      typename Base::n_t Rhs) {
    uint64_t Val = 0;
    Val |= Base::KeyCompressor.getCompressedID(Lhs);
    Val <<= 32;
    Val |= Base::KeyCompressor.getCompressedID(Rhs);
    return Val;
  }

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
  std::array<std::mutex, 64> EdgeFunctionMapMutexes;

  // Caches for the flow/edge functions
  phmap::parallel_node_hash_map_m<EdgeFuncInstKey, NormalEdgeFlowData>
      NormalFunctionCache;

  // Caches for the flow functions
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::f_t>,
      typename Base::FlowFunctionPtrType>
      CallFlowFunctionCache;
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                 typename Base::n_t>,
      typename Base::FlowFunctionPtrType>
      ReturnFlowFunctionCache;
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::n_t>,
      typename Base::FlowFunctionPtrType>
      CallToRetFlowFunctionCache;
  // Caches for the edge functions
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::d_t, typename Base::f_t,
                 typename Base::d_t>,
      typename Base::EdgeFunctionType>
      CallEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::f_t, typename Base::n_t,
                 typename Base::d_t, typename Base::n_t, typename Base::d_t>,
      typename Base::EdgeFunctionType>
      ReturnEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<EdgeFuncInstKey, InnerEdgeFunctionMapType>
      CallToRetEdgeFunctionCache;
  phmap::parallel_node_hash_map_m<
      std::tuple<typename Base::n_t, typename Base::d_t, typename Base::n_t,
                 typename Base::d_t>,
      typename Base::EdgeFunctionType>
      SummaryEdgeFunctionCache;
};

} // namespace psr

#endif
