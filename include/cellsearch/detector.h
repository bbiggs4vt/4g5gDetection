#pragma once

#include <memory>
#include <string>
#include <vector>

#include "cellsearch/cell_record.h"
#include "cellsearch/iq_stream.h"

namespace cellsearch {

/// Knobs for one per-channel probe.
struct ProbeOptions {
  /// Maximum number of sync attempts (analysis frames) per hypothesis before
  /// it is declared a miss.
  unsigned max_attempts_per_branch = 4;

  /// Length of the probe window buffered from the stream. Every RAT and
  /// frequency hypothesis re-analyzes this same window (a stream can't be
  /// rewound), so it must cover at least one branch analysis frame — LTE
  /// needs 200 ms. A shorter stream is analyzed in full.
  double capture_duration_s = 0.5;
};

/// Per-channel probe dispatcher.
///
/// Runs an LTE cell-search attempt and an NR SSB-search attempt over framed
/// samples from the stream; each RAT has a distinct sync-signal structure, so
/// "detect either" falls out of trying both correlators and reporting
/// whichever locks. Results from both branches are flattened into CellRecord.
class Detector {
public:
  explicit Detector(ProbeOptions options = {});
  ~Detector();

  Detector(const Detector&) = delete;
  Detector& operator=(const Detector&) = delete;

  /// Fully analyze one channel: consume samples from `stream` (fixed Fc) and
  /// return one record per detected cell. Empty vector: no lock on either RAT.
  std::vector<CellRecord> probe(IqStream& stream);

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Version string of the srsRAN PHY engine this library was built against.
std::string phy_engine_version();

} // namespace cellsearch
