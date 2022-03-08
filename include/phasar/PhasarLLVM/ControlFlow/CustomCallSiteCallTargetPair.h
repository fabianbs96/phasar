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

namespace psr {
struct CustomCallSite {
  CustomCallSite(unsigned Id, std::string_view FuncName) noexcept
      : Index(Id), FunctionName(FuncName) {}
  unsigned Index;
  std::string_view FunctionName;
};

class CustomCallSiteCallTargetPair {
public:
  CustomCallSiteCallTargetPair(CustomCallSite CallSite,
                               std::string_view CallTarget) noexcept
      : CallSite(CallSite), CallTarget(CallTarget){};
  ~CustomCallSiteCallTargetPair() = default;

  CustomCallSite getCallSite() const { return CallSite; }
  std::string_view getCallTarget() const { return CallTarget; }

private:
  CustomCallSite CallSite;
  std::string_view CallTarget;
};
} // namespace psr

#endif
