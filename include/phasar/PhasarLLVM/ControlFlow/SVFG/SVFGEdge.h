#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGNode.h"
#include "phasar/Utils/StrongTypeDef.h"

#include "llvm/ADT/StringRef.h"

#include <cstdint>

namespace psr {

enum class SVFGEdgeKind : uint8_t {
  Direct,   // SSA def-use within a function
  Indirect, // alias-based store-to-load flow through memory
  Call,     // ActualParam to FormalParam across a call edge
  Return,   // FormalRet to ActualRet across a return edge
};

[[nodiscard]] llvm::StringRef svfgEdgeKindName(SVFGEdgeKind K) noexcept;

struct SVFGEdge {
  SVFGNodeId Target;
  SVFGEdgeKind Kind;

  [[nodiscard]] bool operator==(const SVFGEdge &) const noexcept = default;
  [[nodiscard]] bool operator<(const SVFGEdge &O) const noexcept {
    if (Target != O.Target) {
      return to_underlying(Target) < to_underlying(O.Target);
    }
    return Kind < O.Kind;
  }
};

} // namespace psr
