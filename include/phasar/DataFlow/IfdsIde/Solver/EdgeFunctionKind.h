#ifndef PHASAR_DATAFLOW_IFDSIDE_SOLVER_EDGEFUNCTIONKIND_H
#define PHASAR_DATAFLOW_IFDSIDE_SOLVER_EDGEFUNCTIONKIND_H

#include <cstddef>

namespace psr {
enum class EdgeFunctionKind { Normal, Call, Return, CallToReturn, Summary };
static constexpr size_t EdgeFunctionKindCount = 5;
} // namespace psr

#endif
