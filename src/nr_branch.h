#pragma once

#include "src/rat_branch.h"

namespace cellsearch {

/// NR decode branch: SSB search → PBCH/MIB, all via srsRAN's NR SSB /
/// `srsran_ue_sync_nr` path. Emits PCI, SSB index, common SCS, SFN and
/// pdcch-ConfigSIB1 (captured, NOT followed) as a CellRecord.
class NrBranch final : public RatBranch {
public:
  NrBranch();
  ~NrBranch() override;

  /// Rate of the narrow SSB region; depends on the SCS hypothesis.
  /// The stub reports the rate for the default hypothesis.
  double sync_rate_hz() const override;

  /// The SSB sits on the GSCN sync raster, which is generally offset from
  /// Fc even on a "centered" carrier — so NR routinely needs the NCO shift.
  /// Returns candidate GSCN raster points near fc_hz, as offsets from it.
  std::vector<double> nco_offsets_hz(double fc_hz) const override;

  std::optional<CellRecord> search(const SyncFrame& frame) override;
};

} // namespace cellsearch
