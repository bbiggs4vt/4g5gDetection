#include "src/lte_branch.h"

extern "C" {
#include <srsran/phy/common/phy_common.h>
#include <srsran/phy/ue/ue_mib.h>
#include <srsran/phy/ue/ue_sync.h>
}

namespace cellsearch {

namespace {
// PSS/SSS + PBCH live in the central 6 PRBs.
constexpr uint32_t kSyncRegionPrb = 6;
} // namespace

LteBranch::LteBranch() = default;
LteBranch::~LteBranch() = default;

double LteBranch::sync_rate_hz() const {
  // 1.92 Msps — srsRAN's rate for a 6-PRB (1.4 MHz) grid.
  return static_cast<double>(srsran_sampling_freq_hz(kSyncRegionPrb));
}

double LteBranch::frame_duration_s() const { return 0.08; }

std::vector<double> LteBranch::nco_offsets_hz(double /*fc_hz*/) const {
  return {0.0};
}

std::optional<CellRecord> LteBranch::search(const SyncFrame& /*frame*/) {
  // TODO(step 3): wire up srsRAN over the framed samples — no hand-rolled DSP:
  //   1. srsran_ue_cellsearch / srsran_ue_sync (buffer-fed via
  //      srsran_ue_sync_init_multi with a recv callback over SyncFrame) for
  //      PSS/SSS timing + CFO and the PCI / CP-type hypothesis.
  //   2. srsran_ue_mib for PBCH decode: MIB → bandwidth (n_prb), PHICH
  //      config, SFN, antenna ports.
  //   3. Flatten into CellRecord{Rat::LTE, ...} with fc_hz from the frame.
  // Scope stops at MIB — nothing past srsran_ue_mib belongs here.
  return std::nullopt;
}

} // namespace cellsearch
