#pragma once

#include <complex>
#include <cstddef>

namespace cellsearch {

/// Metadata that must accompany every IQ stream. A raw sample buffer carries
/// no center frequency; results are meaningless without `fc_hz`, and the NR
/// branch needs it to place GSCN raster hypotheses.
struct IqStreamInfo {
  double fs_hz; ///< Sample rate of the stream, in Hz.
  double fc_hz; ///< Center frequency the stream was captured at, in Hz.
};

/// Generic IQ sample source. Files, sockets, and live radios are all "just a
/// stream" behind this interface — the core never touches SDR/driver APIs.
class IqStream {
public:
  virtual ~IqStream() = default;

  /// Stream metadata. Fixed for the lifetime of the stream (per-channel probe
  /// model: one fixed-Fc channel is analyzed fully).
  virtual const IqStreamInfo& info() const = 0;

  /// Read up to `max_samples` complex samples into `dst`.
  /// Blocks until at least one sample is available or the stream ends.
  /// @return number of samples written; 0 means end of stream.
  virtual std::size_t read(std::complex<float>* dst, std::size_t max_samples) = 0;
};

} // namespace cellsearch
