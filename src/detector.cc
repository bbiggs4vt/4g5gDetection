#include "cellsearch/detector.h"

#include <array>
#include <memory>

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
};

Detector::Detector(ProbeOptions options)
    : impl_(std::make_unique<Impl>(Impl{options, {}, {}})) {}

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
    for (double nco_offset : branch->nco_offsets_hz(info.fc_hz)) {
      Framer framer({/*in_rate_hz=*/info.fs_hz,
                     /*out_rate_hz=*/branch->sync_rate_hz(),
                     /*nco_offset_hz=*/nco_offset,
                     /*frame_len=*/0 /* TODO(step 2): size per branch */});

      for (unsigned attempt = 0;
           attempt < impl_->options.max_attempts_per_branch; ++attempt) {
        SyncFrame frame{};
        if (!framer.next_frame(stream, frame)) {
          break; // Stream ended (or framing not implemented yet).
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
