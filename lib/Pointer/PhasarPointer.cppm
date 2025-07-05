module;

#include "phasar/Pointer.h"

export module phasar.pointer;

export namespace psr {
using psr::AliasAnalysisType;
using psr::toAliasAnalysisType;
using psr::toString;
using psr::operator<<;
using psr::AliasAnalysisType;
using psr::AliasInfo;
using psr::AliasInfoBaseUtils;
using psr::AliasInfoRef;
using psr::AliasInfoTraits;
using psr::AliasResult;
using psr::AnalysisProperties;
using psr::DefaultAATraits;
using psr::IsAliasInfo;
using psr::toAliasResult;
using psr::toString;
using psr::operator<<;
using psr::AliasSetOwner;
using psr::is_equivalent_PointsToTraits_v;
using psr::is_PointsToTraits;
using psr::is_PointsToTraits_v;
using psr::PointsToInfo;
using psr::PointsToInfoBase;
using psr::PointsToInfoRef;
using psr::PointsToTraits;
} // namespace psr
