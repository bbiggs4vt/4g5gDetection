#pragma once

#include <memory>

#include "src/rat_branch.h"

namespace cellsearch {

/// NR decode branch: SSB search → PBCH/MIB, all via srsRAN's `srsran_ssb`
/// (blind PSS/SSS search + PBCH decode over a SyncFrame whose GSCN candidate
/// has already been mixed to baseband by the framing layer). Emits PCI, SSB
/// index, common SCS, SFN and pdcch-ConfigSIB1 (captured, NOT followed) as a
/// CellRecord.
class NrBranch final : public RatBranch {
public:
  NrBranch();
  ~NrBranch() override;

  NrBranch(const NrBranch&) = delete;
  NrBranch& operator=(const NrBranch&) = delete;

  /// Rate for the narrow SSB region: 11.52 Msps fits the widest FR1 SSB
  /// (20 PRB at 30 kHz SCS = 7.2 MHz) with margin, for either SCS hypothesis.
  double sync_rate_hz() const override;

  /// One SSB burst period (20 ms, the default and worst common case) —
  /// guarantees at least one full burst per frame.
  double frame_duration_s() const override;

  /// GSCN sync-raster points inside the captured bandwidth, as offsets from
  /// Fc, nearest first — the SSB can only sit on these. This is the one
  /// place NR asks more of the core than LTE.
  std::vector<double> nco_offsets_hz(double fc_hz, double fs_hz) const override;

  std::optional<CellRecord> search(const SyncFrame& frame) override;

private:
  struct Impl; // srsran_ssb_t.
  std::unique_ptr<Impl> impl_;
};

} // namespace cellsearch
