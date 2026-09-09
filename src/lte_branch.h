#pragma once

#include <memory>

#include "src/rat_branch.h"

namespace cellsearch {

/// LTE decode branch: PSS/SSS sync → PBCH/MIB, all via srsRAN
/// (`srsran_ue_cellsearch` + `srsran_ue_mib_sync`, fed from SyncFrames
/// through a memory recv callback). Emits PCI, system bandwidth, PHICH
/// config, SFN, CP type and antenna-port count as a CellRecord.
class LteBranch final : public RatBranch {
public:
  LteBranch();
  ~LteBranch() override;

  LteBranch(const LteBranch&) = delete;
  LteBranch& operator=(const LteBranch&) = delete;

  /// LTE cell search is cheap: the central 6 RBs (PSS/SSS + PBCH region) at
  /// 1.92 Msps, regardless of the cell's real bandwidth.
  double sync_rate_hz() const override;

  /// Room for a full PSS/SSS scan (3 N_id_2 hypotheses at 5 ms per try)
  /// plus a PBCH decode over several 40 ms TTIs; the frame is rewound
  /// between the two phases, so they share the same samples.
  double frame_duration_s() const override;

  /// LTE's PSS/SSS sits at carrier center — no NCO shift needed.
  std::vector<double> nco_offsets_hz(double fc_hz) const override;

  std::optional<CellRecord> search(const SyncFrame& frame) override;

private:
  struct Impl; // srsRAN objects (ue_cellsearch, ue_mib_sync) + frame cursor.
  std::unique_ptr<Impl> impl_;
};

} // namespace cellsearch
