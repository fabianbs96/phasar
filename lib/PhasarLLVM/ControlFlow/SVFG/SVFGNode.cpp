/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGNode.h"

#include "phasar/PhasarLLVM/ControlFlow/SVFG/SVFGEdge.h"

#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorHandling.h"

using namespace psr;

llvm::StringRef psr::svfgNodeKindName(SVFGNodeKind K) noexcept {
  switch (K) {
  case SVFGNodeKind::Addr:
    return "Addr";
  case SVFGNodeKind::Copy:
    return "Copy";
  case SVFGNodeKind::Gep:
    return "Gep";
  case SVFGNodeKind::Store:
    return "Store";
  case SVFGNodeKind::Load:
    return "Load";
  case SVFGNodeKind::ActualParam:
    return "ActualParam";
  case SVFGNodeKind::FormalParam:
    return "FormalParam";
  case SVFGNodeKind::ActualRet:
    return "ActualRet";
  case SVFGNodeKind::FormalRet:
    return "FormalRet";
  }
  llvm_unreachable("unknown SVFGNodeKind");
}

llvm::StringRef psr::svfgEdgeKindName(SVFGEdgeKind K) noexcept {
  switch (K) {
  case SVFGEdgeKind::Direct:
    return "Direct";
  case SVFGEdgeKind::Indirect:
    return "Indirect";
  case SVFGEdgeKind::Call:
    return "Call";
  case SVFGEdgeKind::Return:
    return "Return";
  }
  llvm_unreachable("unknown SVFGEdgeKind");
}
