#pragma once

#include <complex>
#include <cstddef>
#include <optional>
#include <vector>

#include "cellsearch/cell_record.h"

namespace cellsearch {

/// One fixed-size analysis frame handed to a RAT branch: samples already
/// mixed (optional NCO) and decimated to the branch's sync rate by the
/// framing layer. Big enough for one sync attempt — PSS/SSS correlation plus
/// a few PBCH periods for LTE (tens of ms), ~one SSB burst period (~20 ms)
/// for NR.
struct SyncFrame {
  const std::complex<float>* samples;
  std::size_t count;
  double fs_hz;         ///< Rate of these samples (the branch's sync rate).
  double fc_hz;         ///< Original stream center frequency.
  double nco_offset_hz; ///< Shift already applied (0 if none): samples are
                        ///< baseband-centered on fc_hz + nco_offset_hz.
};

/// Common shape of the LTE and NR decode branches. Implementations wrap
/// srsRAN's sync + PBCH/MIB blocks — they do not reimplement DSP.
class RatBranch {
public:
  virtual ~RatBranch() = default;

  /// Sample rate the framing layer must decimate to before calling search().
  virtual double sync_rate_hz() const = 0;

  /// NCO frequency-shift hypotheses (offsets from stream Fc, in Hz) the
  /// framing layer should try for this branch. LTE's PSS/SSS is at carrier
  /// center, so LTE returns {0}. NR returns GSCN raster points near Fc,
  /// because the SSB sits at a raster offset even on a "centered" carrier.
  virtual std::vector<double> nco_offsets_hz(double fc_hz) const = 0;

  /// Run one sync + PBCH/MIB attempt over the frame.
  /// @return a normalized record if the branch locked, std::nullopt otherwise.
  virtual std::optional<CellRecord> search(const SyncFrame& frame) = 0;
};

} // namespace cellsearch
