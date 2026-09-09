#include "src/lte_branch.h"

#include <chrono>
#include <cstring>
#include <stdexcept>

extern "C" {
#include <srsran/phy/common/phy_common.h>
#include <srsran/phy/ue/ue_cell_search.h>
#include <srsran/phy/ue/ue_mib.h>
}

namespace cellsearch {

namespace {

// Sync attempts are budgeted in 5 ms half-frames by srsran_ue_cellsearch.
constexpr uint32_t kMaxFramesPss = 10;      // Per-N_id_2 scan budget.
constexpr uint32_t kNofValidPssFrames = 8;  // Agreement threshold.
// PBCH decode budget in 10 ms radio frames; the real bound is the SyncFrame
// running out of samples (the recv callback then errors out).
constexpr uint32_t kMaxFramesPbch = 40;

// Serves one SyncFrame's samples to srsRAN's pull-style recv callbacks.
struct FrameCursor {
  const cf_t* data = nullptr;
  std::size_t total = 0;
  std::size_t pos = 0;
};

int cursor_recv(void* handler, void* buf, uint32_t nsamples,
                srsran_timestamp_t* /*ts*/) {
  auto* cursor = static_cast<FrameCursor*>(handler);
  if (cursor->pos + nsamples > cursor->total) {
    return SRSRAN_ERROR; // Frame exhausted — aborts the ongoing attempt.
  }
  std::memcpy(buf, cursor->data + cursor->pos, nsamples * sizeof(cf_t));
  cursor->pos += nsamples;
  return static_cast<int>(nsamples);
}

int cursor_recv_multi(void* handler, cf_t* buf[SRSRAN_MAX_CHANNELS],
                      uint32_t nsamples, srsran_timestamp_t* ts) {
  return cursor_recv(handler, buf[0], nsamples, ts);
}

PhichConfig to_phich(const srsran_cell_t& cell) {
  PhichConfig p{};
  p.length = cell.phich_length == SRSRAN_PHICH_EXT
                 ? PhichConfig::Length::EXTENDED
                 : PhichConfig::Length::NORMAL;
  switch (cell.phich_resources) {
    case SRSRAN_PHICH_R_1_6: p.resources = PhichConfig::Resources::ONE_SIXTH; break;
    case SRSRAN_PHICH_R_1_2: p.resources = PhichConfig::Resources::HALF; break;
    case SRSRAN_PHICH_R_1:   p.resources = PhichConfig::Resources::ONE; break;
    case SRSRAN_PHICH_R_2:
    default:                 p.resources = PhichConfig::Resources::TWO; break;
  }
  return p;
}

} // namespace

struct LteBranch::Impl {
  FrameCursor cursor;
  srsran_ue_cellsearch_t cs{};
  srsran_ue_mib_sync_t mib_sync{};
  bool mib_sync_ready = false;

  Impl() {
    if (srsran_ue_cellsearch_init(&cs, kMaxFramesPss, cursor_recv, &cursor) !=
        SRSRAN_SUCCESS) {
      throw std::runtime_error("LteBranch: ue_cellsearch init failed");
    }
    srsran_ue_cellsearch_set_nof_valid_frames(&cs, kNofValidPssFrames);

    if (srsran_ue_mib_sync_init_multi(&mib_sync, cursor_recv_multi, 1,
                                      &cursor) != SRSRAN_SUCCESS) {
      srsran_ue_cellsearch_free(&cs);
      throw std::runtime_error("LteBranch: ue_mib_sync init failed");
    }
    mib_sync_ready = true;
  }

  ~Impl() {
    if (mib_sync_ready) {
      srsran_ue_mib_sync_free(&mib_sync);
    }
    srsran_ue_cellsearch_free(&cs);
  }
};

LteBranch::LteBranch() : impl_(std::make_unique<Impl>()) {}
LteBranch::~LteBranch() = default;

double LteBranch::sync_rate_hz() const {
  // 1.92 Msps — srsRAN's rate for the 6-PRB sync region (SRSRAN_CS_SAMP_FREQ).
  return static_cast<double>(srsran_sampling_freq_hz(SRSRAN_CS_NOF_PRB));
}

double LteBranch::frame_duration_s() const { return 0.2; }

std::vector<double> LteBranch::nco_offsets_hz(double /*fc_hz*/,
                                              double /*fs_hz*/) const {
  return {0.0};
}

std::optional<CellRecord> LteBranch::search(const SyncFrame& frame) {
  FrameCursor& cursor = impl_->cursor;
  cursor.data = reinterpret_cast<const cf_t*>(frame.samples);
  cursor.total = frame.count;
  cursor.pos = 0;

  // Phase 1: PSS/SSS scan over the three N_id_2 hypotheses → PCI, CP, CFO.
  srsran_ue_cellsearch_result_t found_cells[3] = {};
  const int n_found = srsran_ue_cellsearch_scan(&impl_->cs, found_cells,
                                                nullptr);
  if (n_found <= 0) {
    return std::nullopt;
  }
  const srsran_ue_cellsearch_result_t* best = &found_cells[0];
  for (const auto& c : found_cells) {
    if (c.peak > best->peak) {
      best = &c;
    }
  }
  if (best->cell_id >= SRSRAN_NUM_PCI || best->peak <= 0.0f) {
    return std::nullopt;
  }

  // Phase 2: PBCH/MIB on the same samples — rewind and re-sync with the
  // detected cell (blind antenna-port detection, CFO seeded from the scan,
  // exactly as srsRAN's rf_mib_decoder does).
  cursor.pos = 0;

  srsran_cell_t cell = {};
  cell.id = best->cell_id;
  cell.cp = best->cp;
  cell.frame_type = best->frame_type;
  cell.nof_prb = SRSRAN_UE_MIB_NOF_PRB;
  cell.nof_ports = 0; // Blind detection.

  srsran_ue_mib_sync_reset(&impl_->mib_sync);
  if (srsran_ue_mib_sync_set_cell(&impl_->mib_sync, cell) != SRSRAN_SUCCESS) {
    return std::nullopt;
  }
  impl_->mib_sync.ue_sync.cfo_current_value = best->cfo / 15000.0f;
  impl_->mib_sync.ue_sync.cfo_is_copied = true;
  impl_->mib_sync.ue_sync.cfo_correct_enable_find = true;

  uint8_t bch_payload[SRSRAN_BCH_PAYLOAD_LEN] = {};
  uint32_t nof_tx_ports = 0;
  int sfn_offset = 0;
  const int ret = srsran_ue_mib_sync_decode(&impl_->mib_sync, kMaxFramesPbch,
                                            bch_payload, &nof_tx_ports,
                                            &sfn_offset);
  if (ret != SRSRAN_UE_MIB_FOUND) {
    return std::nullopt;
  }

  uint32_t sfn = 0;
  srsran_pbch_mib_unpack(bch_payload, &cell, &sfn);
  sfn = (sfn + static_cast<uint32_t>(sfn_offset)) % 1024;

  CellRecord record{};
  record.rat = Rat::LTE;
  record.fc_hz = frame.fc_hz;
  record.pci = static_cast<std::uint16_t>(cell.id);
  record.bandwidth_scs = LteBandwidth{static_cast<std::uint8_t>(cell.nof_prb)};
  record.sfn = static_cast<std::uint16_t>(sfn);
  record.phich_config = to_phich(cell);
  record.cp_type = cell.cp == SRSRAN_CP_EXT ? CpType::EXTENDED : CpType::NORMAL;
  record.n_ports = static_cast<std::uint8_t>(nof_tx_ports);
  record.timestamp = std::chrono::system_clock::now();
  record.snr_or_metric = best->peak;
  return record;
}

} // namespace cellsearch
