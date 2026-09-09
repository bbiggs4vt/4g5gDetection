#pragma once

#include <memory>
#include <optional>
#include <string>

#include "cellsearch/tunable_source.h"

namespace cellsearch {

/// SoapySDR-backed TunableIqSource — the live-radio glue, deliberately
/// outside the core decode library (which never sees a radio handle; it
/// gets an IqStream like any file or socket). Device-agnostic: anything
/// with a Soapy module (UHD, RTL-SDR, HackRF, LimeSDR, ...) works.
class SoapyIqSource final : public TunableIqSource {
public:
  /// @param device_args SoapySDR device args, e.g. "driver=rtlsdr" or ""
  ///        for the first device found.
  /// @param gain_db Overall RX gain; unset enables AGC where supported.
  /// @throws std::runtime_error if no device matches or setup fails.
  explicit SoapyIqSource(const std::string& device_args,
                         std::optional<double> gain_db = std::nullopt);
  ~SoapyIqSource() override;

  SoapyIqSource(const SoapyIqSource&) = delete;
  SoapyIqSource& operator=(const SoapyIqSource&) = delete;

  /// Sets sample rate + center frequency and restarts the RX stream, so
  /// stale pre-retune samples are dropped by the device layer. The sweeper's
  /// settle window covers LO slew on top of that.
  bool tune(double fc_hz, double fs_hz) override;

  IqStream& stream() override;

private:
  struct Impl; // SoapySDR types stay out of this header.
  std::unique_ptr<Impl> impl_;
};

} // namespace cellsearch
