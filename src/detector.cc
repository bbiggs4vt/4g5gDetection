#include "cellsearch/detector.h"

#include <array>
#include <cmath>
#include <memory>
#include <stdexcept>

#include "src/framing.h"
#include "src/lte_branch.h"
#include "src/nr_branch.h"
#include "src/rat_branch.h"

extern "C" {
#include <srsran/version.h>
}

namespace cellsearch {

struct Detector::Impl {
  ProbeOptions options;
  LteBranch lte;
  NrBranch nr;

  explicit Impl(ProbeOptions o) : options(o) {}
};

Detector::Detector(ProbeOptions options)
    : impl_(std::make_unique<Impl>(options)) {}

Detector::~Detector() = default;

std::vector<CellRecord> Detector::probe(IqStream& stream) {
  std::vector<CellRecord> records;
  const IqStreamInfo& info = stream.info();

  // Try both RAT correlators over framed samples and report whichever locks.
  // NOTE: with the per-channel probe model both branches should see the same
  // captured samples; once the framing layer lands (step 2) the ring buffer
  // will be shared and each branch will get its own shifted/decimated view.
  const std::array<RatBranch*, 2> branches = {&impl_->lte, &impl_->nr};

  for (RatBranch* branch : branches) {
    const double sync_rate = branch->sync_rate_hz();
    const auto frame_len = static_cast<std::size_t>(
        std::lround(branch->frame_duration_s() * sync_rate));

    for (double nco_offset : branch->nco_offsets_hz(info.fc_hz)) {
      std::unique_ptr<Framer> framer;
      try {
        framer = std::make_unique<Framer>(Framer::Config{
            /*in_rate_hz=*/info.fs_hz, /*fc_hz=*/info.fc_hz,
            /*out_rate_hz=*/sync_rate, /*nco_offset_hz=*/nco_offset,
            /*frame_len=*/frame_len});
      } catch (const std::invalid_argument&) {
        // Fs is not an integer multiple of this branch's sync rate — skip
        // the branch rather than abort the probe. TODO: arbitrary rational
        // resampling (srsran_resample_arb) + a diagnostics callback so the
        // caller can see why a branch was skipped.
        break;
      }

      for (unsigned attempt = 0;
           attempt < impl_->options.max_attempts_per_branch; ++attempt) {
        SyncFrame frame{};
        if (!framer->next_frame(stream, frame)) {
          break; // Stream ended.
        }
        if (auto record = branch->search(frame)) {
          records.push_back(*record);
          break;
        }
      }
    }
  }

  return records;
}

std::string phy_engine_version() { return "srsRAN " SRSRAN_VERSION_STRING; }

} // namespace cellsearch
