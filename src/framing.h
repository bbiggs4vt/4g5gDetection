#pragma once

#include <complex>
#include <cstddef>
#include <memory>
#include <vector>

#include "cellsearch/iq_stream.h"
#include "src/rat_branch.h"

namespace cellsearch {

/// Framing layer (build-order step 2).
///
/// Owns the path:  ring buffer → [optional NCO shift] → decimate to sync rate.
/// Ring buffer in, fixed-size analysis frames out — correlators never see the
/// full channel rate. The DSP inside is srsRAN's, not ours:
/// `srsran_ringbuffer` for buffering, `srsran_vec_apply_cfo` as the NCO, and
/// the FFT resampler (`srsran_resampler_fft`) for anti-aliased decimation.
class Framer {
public:
  struct Config {
    double in_rate_hz;      ///< Stream rate (Fs).
    double fc_hz;           ///< Stream center frequency (passed through to
                            ///< SyncFrame — results need it).
    double out_rate_hz;     ///< Branch sync rate to decimate to.
    double nco_offset_hz;   ///< Mix fc_hz + offset to baseband before
                            ///< decimating; 0 disables the NCO.
    std::size_t frame_len;  ///< Samples per analysis frame, at out_rate_hz.
  };

  /// @throws std::invalid_argument if in_rate/out_rate is not a positive
  /// integer ratio (TODO: arbitrary rational resampling via
  /// srsran_resample_arb if a real capture ever needs it) or frame_len is 0.
  explicit Framer(const Config& config);
  ~Framer();

  Framer(const Framer&) = delete;
  Framer& operator=(const Framer&) = delete;

  /// Pull samples from `stream` until one full analysis frame is ready.
  /// Successive frames are contiguous in time (NCO phase and resampler filter
  /// state carry across calls), so a branch can hand them to a tracking loop.
  /// The frame's storage is owned by the Framer and valid until the next call.
  /// @return false when the stream ended before a full frame could be built.
  bool next_frame(IqStream& stream, SyncFrame& out);

private:
  struct SrsranState; // srsran_ringbuffer_t + srsran_resampler_fft_t.

  Config config_;
  unsigned ratio_;                  ///< in_rate / out_rate.
  float nco_cfo_;                   ///< Normalized NCO freq, cycles/sample.
  std::complex<float> nco_phase_;   ///< Phase carried across frames.
  std::unique_ptr<SrsranState> srs_;
  std::vector<std::complex<float>> io_buf_;    ///< Stream read chunks.
  std::vector<std::complex<float>> raw_buf_;   ///< One frame at in_rate.
  std::vector<std::complex<float>> frame_buf_; ///< One frame at out_rate.
};

} // namespace cellsearch
