/**
 * @author Sebastian Roland <seroland86@gmail.com>
 */

#ifndef PHASAR_PHASARLLVM_DATAFLOWSOLVER_IFDSIDE_IFDSFIELDSENSTAINTANALYSIS_STATS_LCOVRETVALWRITER_H
#define PHASAR_PHASARLLVM_DATAFLOWSOLVER_IFDSIDE_IFDSFIELDSENSTAINTANALYSIS_STATS_LCOVRETVALWRITER_H

#include "TraceStatsWriter.h"

namespace psr {

class LcovRetValWriter : public TraceStatsWriter {
public:
  LcovRetValWriter(const TraceStats &_traceStats, const std::string _outFile)
      : TraceStatsWriter(_traceStats, _outFile) {}
  ~LcovRetValWriter() override = default;

  void write() const override;
};

} // namespace psr

#endif
