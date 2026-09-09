#include "cellsearch/detector.h"

#include <array>
#include <cmath>
#include <complex>
#include <memory>
#include <stdexcept>
#include <vector>

#include "cellsearch/memory_iq_stream.h"

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

  // Buffer one probe window up front: every RAT and GSCN/NCO hypothesis
  // analyzes the same samples (per-channel probe model — the stream itself
  // can't be rewound between hypotheses).
  const auto want = static_cast<std::size_t>(
      std::lround(impl_->options.capture_duration_s * info.fs_hz));
  std::vector<std::complex<float>> window(want);
  std::size_t got = 0;
  while (got < want) {
    const std::size_t n = stream.read(window.data() + got, want - got);
    if (n == 0) {
      break;
    }
    got += n;
  }
  window.resize(got);
  if (got == 0) {
    return records;
  }

  // Try both RAT correlators and report whichever locks.
  const std::array<RatBranch*, 2> branches = {&impl_->lte, &impl_->nr};

  for (RatBranch* branch : branches) {
    const double sync_rate = branch->sync_rate_hz();
    const auto frame_len = static_cast<std::size_t>(
        std::lround(branch->frame_duration_s() * sync_rate));
    bool branch_locked = false;

    for (double nco_offset : branch->nco_offsets_hz(info.fc_hz, info.fs_hz)) {
      if (branch_locked) {
        break;
      }
      MemoryIqStream view(window.data(), window.size(), info);
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
        if (!framer->next_frame(view, frame)) {
          break; // Probe window exhausted.
        }
        if (auto record = branch->search(frame)) {
          records.push_back(*record);
          branch_locked = true;
          break;
        }
      }
    }
  }

  return records;
}

std::string phy_engine_version() { return "srsRAN " SRSRAN_VERSION_STRING; }

} // namespace cellsearch
