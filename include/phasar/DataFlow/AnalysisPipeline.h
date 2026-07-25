#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#include "phasar/DB/ProjectIRDB.h"
#include "phasar/Utils/EmptyBaseOptimizationUtils.h"
#include "phasar/Utils/Lazy.h"
#include "phasar/Utils/Macros.h"
#include "phasar/Utils/NonNullPtr.h"

#include <memory>

namespace psr {

class PipelineRoot {
public:
  [[nodiscard]] PipelineRoot getResult(PipelineRoot /*unused*/) { return {}; };
};

template <typename T, typename Tag, typename Base>
class PipelineStage : public Base {

public:
  template <typename ArgsT>
    requires std::is_constructible_v<T, ArgsT>
  explicit PipelineStage(Tag /*unused*/, ArgsT Args, Base &&B)
      : Base(std::move(B)), Result(std::make_unique<T>(PSR_FWD(Args))) {}

  explicit PipelineStage(Tag /*unused*/, std::unique_ptr<T> Result, Base &&B)
      : Base(std::move(B)), Result(std::move(Result)) {}

  using Base::getResult;

  [[nodiscard]] T &getResult(Tag /*unused*/) & noexcept { return *Result; }
  [[nodiscard]] T getResult(Tag /*unused*/) && noexcept {
    return std::move(*Result);
  }

  template <typename OtherTag>
  [[nodiscard]] auto getResultOrNull(OtherTag /*unused*/) noexcept {
    if constexpr (requires { this->getResult(OtherTag{}); }) {
      return &getResult(OtherTag{});
    } else {
      return nullptr;
    }
  }

  template <typename NextTag>
  [[nodiscard]] auto with(NextTag /*unused*/, auto &&...NextArgs) &&
  // requires requires { NextTag::build(*this, PSR_FWD(NextArgs)...); }
  {
    using NextT = decltype(NextTag::build(*this, PSR_FWD(NextArgs)...));
    auto Res =
        std::make_unique<NextT>(NextTag::build(*this, PSR_FWD(NextArgs)...));
    return PipelineStage<NextT, NextTag, PipelineStage>{
        NextTag{}, std::move(Res), std::move(*this)};
  }

private:
  std::unique_ptr<T> Result;
};

} // namespace psr
