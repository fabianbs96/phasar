/******************************************************************************
 * Copyright (c) 2024 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

#ifndef PHASAR_ANALYSISCONFIG_H
#define PHASAR_ANALYSISCONFIG_H

#include "nlohmann/json.hpp"

#include <optional>
#include <string>

namespace t2 {
struct AnalysisConfig {
  std::string OutputFile;
  bool TreatWarningsAsError = false;

  std::optional<nlohmann::json> PrecomputedCG;
  std::optional<nlohmann::json> PrecomputedAA;
  /// TODO: More config options
};
} // namespace t2

#endif
