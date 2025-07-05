module;

#include "phasar/ControlFlow.h"

export module phasar.controlflow;

export namespace psr {
using psr::CallGraph;
using psr::CallGraphAnalysisType;
using psr::CallGraphBuilder;
using psr::CGTraits;
using psr::toCallGraphAnalysisType;
using psr::toString;
using psr::operator<<;
using psr::CallGraphBase;
using psr::CallGraphData;
using psr::CFGBase;
using psr::CFGTraits;
using psr::ComposedResolver;
using psr::GenericResolver;
using psr::GenericResolverRef;
using psr::has_getSparseCFG;
using psr::has_getSparseCFG_v;
using psr::ICFGBase;
using psr::IntersectResolver;
using psr::is_cfg_v;
using psr::is_icfg_v;
using psr::is_sparse_cfg_v;
using psr::SparseCFGBase;
using psr::SparseCFGProvider;
using psr::SpecialMemberFunctionType;
using psr::toSpecialMemberFunctionType;
using psr::UnionResolver;
using psr::valueOf;
} // namespace psr
