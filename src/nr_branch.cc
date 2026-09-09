#include "src/nr_branch.h"

extern "C" {
#include <srsran/phy/sync/ssb.h>
}

namespace cellsearch {

NrBranch::NrBranch() = default;
NrBranch::~NrBranch() = default;

double NrBranch::sync_rate_hz() const {
  // TODO(step 4): derive from the active SCS hypothesis (srsRAN's NR search
  // iterates SCS candidates); the SSB spans 20 PRBs, so the region rate is a
  // function of SCS, not of the channel bandwidth. Placeholder: the rate
  // srsran_ssb_t is typically run at for FR1 15/30 kHz SSBs.
  return 11.52e6;
}

double NrBranch::frame_duration_s() const { return 0.02; }

std::vector<double> NrBranch::nco_offsets_hz(double /*fc_hz*/) const {
  // TODO(step 4): enumerate GSCN sync-raster points that fall inside the
  // captured bandwidth around fc_hz and return each as an offset — this is
  // the one place NR asks more of the core than LTE. Placeholder: carrier
  // center only.
  return {0.0};
}

std::optional<CellRecord> NrBranch::search(const SyncFrame& /*frame*/) {
  // TODO(step 4): wire up srsRAN over the framed samples — no hand-rolled DSP:
  //   1. srsran_ssb_t (srsran_ssb_init/srsran_ssb_set_cfg with the frame's
  //      srate, fc and the GSCN-derived ssb_freq, then srsran_ssb_search)
  //      for PSS/SSS → PCI + SSB candidate.
  //   2. srsran_ssb_decode_pbch → srsran_pbch_msg_nr_mib_unpack: MIB → SFN,
  //      common SCS, ssb_idx, pdcch-ConfigSIB1 (captured, NOT followed).
  //   3. Flatten into CellRecord{Rat::NR, ...} with fc_hz from the frame.
  // Scope stops at MIB — no CORESET#0 / SIB1 chase.
  return std::nullopt;
}

} // namespace cellsearch
