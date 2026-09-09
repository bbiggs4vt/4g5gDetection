#pragma once

#include <complex>
#include <cstddef>
#include <vector>

#include "cellsearch/iq_stream.h"
#include "src/rat_branch.h"

namespace cellsearch {

/// Framing layer (build-order step 2 — interface only for now).
///
/// Owns the path:  ring buffer → [optional NCO shift] → decimate to sync rate.
/// Ring buffer in, fixed-size analysis frames out. Correlators never run at
/// the full channel rate.
class Framer {
public:
  struct Config {
    double in_rate_hz;      ///< Stream rate (Fs).
    double out_rate_hz;     ///< Branch sync rate to decimate to.
    double nco_offset_hz;   ///< Mix target region to baseband before
                            ///< decimating; 0 disables the NCO.
    std::size_t frame_len;  ///< Samples per analysis frame, at out_rate_hz.
  };

  explicit Framer(const Config& config);

  /// Pull samples from `stream` until one full analysis frame is ready.
  /// The frame's storage is owned by the Framer and valid until the next call.
  /// @return false when the stream ended before a full frame could be built.
  ///
  /// TODO(step 2): implement — ring buffer, NCO mixer, rational resampler.
  /// Not implemented in the skeleton; currently fails at link/run time by
  /// design so nothing silently feeds full-rate samples to a correlator.
  bool next_frame(IqStream& stream, SyncFrame& out);

private:
  Config config_;
  std::vector<std::complex<float>> frame_buf_;
};

} // namespace cellsearch
