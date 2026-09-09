#pragma once

#include <functional>
#include <vector>

#include "cellsearch/detector.h"
#include "cellsearch/tunable_source.h"

namespace cellsearch {

/// One channel of a sweep plan.
struct SweepChannel {
  double fc_hz;
  double fs_hz;
};

struct SweepOptions {
  /// Per-channel probe knobs (window length, attempts).
  ProbeOptions probe;

  /// Samples worth of this much time are read and discarded after every
  /// retune, before probing — lets the LO settle so a half-tuned channel
  /// doesn't read as empty (the classic live-scanner bug).
  double settle_s = 0.05;

  /// Optional progress hook, called after each probed channel with that
  /// channel's records (empty = nothing found there). Channels the source
  /// rejects at tune() are skipped and do not invoke the hook.
  std::function<void(const SweepChannel&, const std::vector<CellRecord>&)>
      on_channel;
};

/// Sweep orchestration — the optional outer layer around the per-channel
/// probe core: retune, settle, probe, next. It owns no DSP and no SDR
/// specifics; the core stays a per-channel probe.
class Sweeper {
public:
  explicit Sweeper(TunableIqSource& source, SweepOptions options = {});

  /// One pass over the plan. Returns every record found, in plan order
  /// (each record's fc_hz identifies its channel).
  std::vector<CellRecord> sweep_once(const std::vector<SweepChannel>& plan);

private:
  TunableIqSource& source_;
  SweepOptions options_;
  Detector detector_;
};

} // namespace cellsearch
