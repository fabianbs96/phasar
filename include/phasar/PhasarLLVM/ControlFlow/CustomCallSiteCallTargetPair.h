/******************************************************************************
 * Copyright (c) 2017 Philipp Schubert.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Sriteja Kummita and others
 *****************************************************************************/

#ifndef PHASAR_PHASARLLVM_CONTROLFLOW_CUSTOM_CALLSITE_CALLTARGET_PAIR_H_
#define PHASAR_PHASARLLVM_CONTROLFLOW_CUSTOM_CALLSITE_CALLTARGET_PAIR_H_

#include <string_view>

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/Utils/LLVMShorthands.h"

namespace psr {
struct CustomCallSite {
  CustomCallSite(unsigned Id, std::string_view FuncName) noexcept
      : Index(Id), FunctionName(FuncName) {}
  unsigned Index;
  std::string_view FunctionName;
};

template <typename CallSiteTy = CustomCallSite,
          typename CallTargetTy = std::string_view>
class CustomCallSiteCallTargetPair {
public:
  CustomCallSiteCallTargetPair(CallSiteTy CallSite,
                               CallTargetTy CallTarget) noexcept
      : CallSite(CallSite), CallTarget(CallTarget){};
  ~CustomCallSiteCallTargetPair() = default;

  CallSiteTy getCallSite() const { return CallSite; }
  CallTargetTy getCallTarget() const { return CallTarget; }

private:
  CallSiteTy CallSite;
  CallTargetTy CallTarget;
};
} // namespace psr

#endif
