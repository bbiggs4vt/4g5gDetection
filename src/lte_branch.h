#pragma once

#include "src/rat_branch.h"

namespace cellsearch {

/// LTE decode branch: PSS/SSS sync → PBCH/MIB, all via srsRAN
/// (`srsran_ue_sync` + `srsran_ue_mib`). Emits PCI, system bandwidth, PHICH
/// config, SFN, CP type and antenna-port count as a CellRecord.
class LteBranch final : public RatBranch {
public:
  LteBranch();
  ~LteBranch() override;

  /// LTE cell search is cheap: the central 6 RBs (PSS/SSS + PBCH region) at
  /// 1.92 Msps, regardless of the cell's real bandwidth.
  double sync_rate_hz() const override;

  /// LTE's PSS/SSS sits at carrier center — no NCO shift needed.
  std::vector<double> nco_offsets_hz(double fc_hz) const override;

  std::optional<CellRecord> search(const SyncFrame& frame) override;
};

} // namespace cellsearch
