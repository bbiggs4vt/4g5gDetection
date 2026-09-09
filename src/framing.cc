#include "src/framing.h"

namespace cellsearch {

Framer::Framer(const Config& config) : config_(config) {
  frame_buf_.reserve(config_.frame_len);
}

bool Framer::next_frame(IqStream& /*stream*/, SyncFrame& /*out*/) {
  // TODO(step 2): ring buffer → [NCO mix by config_.nco_offset_hz] →
  // rational resample in_rate_hz → out_rate_hz → emit config_.frame_len
  // samples. Until then, produce no frames rather than mislabeled full-rate
  // samples.
  return false;
}

} // namespace cellsearch
