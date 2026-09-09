#pragma once

#include "cellsearch/iq_stream.h"

namespace cellsearch {

/// A retunable IQ source — everything the sweep layer needs from a radio,
/// with no SDR/driver types in sight. Live-radio glue (e.g. SoapySDR)
/// implements this in its own optional layer; tests implement it over
/// capture buffers.
class TunableIqSource {
public:
  virtual ~TunableIqSource() = default;

  /// Retune to a new channel. After this returns, stream().info() reflects
  /// the new (Fs, Fc) and samples read afterwards belong to the new channel:
  /// implementations must drop stale in-flight samples from before the
  /// retune. (The sweeper additionally discards a settle window — LO
  /// settling after a tune is the classic live-scanner phantom-miss bug.)
  /// @return false if the device rejects the channel; the sweeper skips it.
  virtual bool tune(double fc_hz, double fs_hz) = 0;

  /// The sample stream at the currently tuned channel.
  virtual IqStream& stream() = 0;
};

} // namespace cellsearch
