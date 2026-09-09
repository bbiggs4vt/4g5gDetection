#include "src/nr_branch.h"

#include <chrono>
#include <cmath>
#include <stdexcept>

#include "src/gscn.h"

extern "C" {
#include <srsran/phy/common/phy_common_nr.h>
#include <srsran/phy/sync/ssb.h>
}

namespace cellsearch {

namespace {

constexpr double kSyncRateHz = 11.52e6;

// Widest FR1 SSB: 20 PRB at 30 kHz SCS. A GSCN candidate must fit entirely
// inside the captured band before the NCO centers it for decimation.
constexpr double kSsbHalfBwHz = 3.6e6;

// srsRAN's SSB object needs the SSB pattern (TS 38.213 4.1) to place burst
// candidates. FR1: case A is 15 kHz, case C is 30 kHz (case B is a 30 kHz
// variant used in a few bands — TODO if a capture from one ever shows up).
srsran_ssb_pattern_t pattern_for(srsran_subcarrier_spacing_t scs) {
  return scs == srsran_subcarrier_spacing_15kHz ? SRSRAN_SSB_PATTERN_A
                                                : SRSRAN_SSB_PATTERN_C;
}

} // namespace

struct NrBranch::Impl {
  srsran_ssb_t ssb{};

  Impl() {
    srsran_ssb_args_t args = {};
    args.max_srate_hz = kSyncRateHz;
    args.min_scs = srsran_subcarrier_spacing_15kHz;
    args.enable_search = true;
    args.enable_decode = true;
    if (srsran_ssb_init(&ssb, &args) != SRSRAN_SUCCESS) {
      throw std::runtime_error("NrBranch: ssb init failed");
    }
  }

  ~Impl() { srsran_ssb_free(&ssb); }
};

NrBranch::NrBranch() : impl_(std::make_unique<Impl>()) {}
NrBranch::~NrBranch() = default;

double NrBranch::sync_rate_hz() const { return kSyncRateHz; }

double NrBranch::frame_duration_s() const { return 0.02; }

std::vector<double> NrBranch::nco_offsets_hz(double fc_hz, double fs_hz) const {
  const double half_span = fs_hz / 2.0 - kSsbHalfBwHz;
  std::vector<double> offsets;
  for (double ss_ref : gscn_ss_ref_hz(fc_hz, half_span)) {
    offsets.push_back(ss_ref - fc_hz);
  }
  return offsets;
}

std::optional<CellRecord> NrBranch::search(const SyncFrame& frame) {
  // The framing layer has already mixed the GSCN candidate to baseband, so
  // the expected SSB center and the frame center coincide.
  const double ssb_center_hz = frame.fc_hz + frame.nco_offset_hz;

  // srsRAN's NR search iterates SCS hypotheses — both FR1 SSB spacings are
  // tried over the same frame (30 kHz first: the common FR1 TDD case).
  for (srsran_subcarrier_spacing_t scs : {srsran_subcarrier_spacing_30kHz,
                                          srsran_subcarrier_spacing_15kHz}) {
    srsran_ssb_cfg_t cfg = {};
    cfg.srate_hz = frame.fs_hz;
    cfg.center_freq_hz = ssb_center_hz;
    cfg.ssb_freq_hz = ssb_center_hz;
    cfg.scs = scs;
    cfg.pattern = pattern_for(scs);
    cfg.duplex_mode = SRSRAN_DUPLEX_MODE_FDD;
    cfg.periodicity_ms = 20;
    if (srsran_ssb_set_cfg(&impl_->ssb, &cfg) != SRSRAN_SUCCESS) {
      continue;
    }

    srsran_ssb_search_res_t res = {};
    if (srsran_ssb_search(&impl_->ssb,
                          reinterpret_cast<const cf_t*>(frame.samples),
                          static_cast<uint32_t>(frame.count),
                          &res) != SRSRAN_SUCCESS) {
      continue;
    }
    if (!res.pbch_msg.crc) {
      continue; // No SSB with a valid PBCH under this hypothesis.
    }

    srsran_mib_nr_t mib = {};
    if (srsran_pbch_msg_nr_mib_unpack(&res.pbch_msg, &mib) != SRSRAN_SUCCESS) {
      continue;
    }

    CellRecord record{};
    record.rat = Rat::NR;
    record.fc_hz = frame.fc_hz;
    record.pci = static_cast<std::uint16_t>(res.N_id);
    record.ssb_index = static_cast<std::uint8_t>(mib.ssb_idx);
    record.bandwidth_scs =
        NrScs{static_cast<std::uint16_t>(SRSRAN_SUBC_SPACING_NR(mib.scs_common) / 1000)};
    record.sfn = static_cast<std::uint16_t>(mib.sfn);
    // pdcch-ConfigSIB1 (8 bits: CORESET#0 index | SearchSpace#0 index) —
    // captured for the fingerprint, NOT followed: scope stops at MIB.
    record.pdcch_config_sib1 = static_cast<std::uint8_t>(
        ((mib.coreset0_idx & 0xF) << 4) | (mib.ss0_idx & 0xF));
    record.timestamp = std::chrono::system_clock::now();
    record.snr_or_metric = res.measurements.snr_dB;
    return record;
  }
  return std::nullopt;
}

} // namespace cellsearch
