#pragma once

/******************************************************************************
 * Copyright (c) 2026 Fabian Schiebel.
 * All rights reserved. This program and the accompanying materials are made
 * available under the terms of LICENSE.txt.
 *
 * Contributors:
 *     Fabian Schiebel and others
 *****************************************************************************/

namespace psr {

/// Static configuration of the DFISolver. Derive from it to change options.
struct DFISolverConfig {
  /// Propagate the zero fact along all flow functions, even if the analysis
  /// problem does not do so explicitly
  static constexpr bool AutoAddZero = true;
  /// Build the reachability index required by DFISolver::getReachability()
  static constexpr bool BuildQueryIndex = true;
};

/// DFISolverConfig without reachability index; use it if only the IFDS
/// results are of interest
struct DFISolverConfigNoIndex : DFISolverConfig {
  static constexpr bool BuildQueryIndex = false;
};

} // namespace psr
