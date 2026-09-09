#include "cellsearch/sweep.h"

#include <cmath>
#include <complex>
#include <vector>

namespace cellsearch {

Sweeper::Sweeper(TunableIqSource& source, SweepOptions options)
    : source_(source), options_(std::move(options)),
      detector_(options_.probe) {}

std::vector<CellRecord> Sweeper::sweep_once(
    const std::vector<SweepChannel>& plan) {
  std::vector<CellRecord> all_records;
  std::vector<std::complex<float>> discard_buf(32768);

  for (const SweepChannel& channel : plan) {
    if (!source_.tune(channel.fc_hz, channel.fs_hz)) {
      continue; // Device rejected the channel.
    }

    // Settle: read and drop samples so the probe never sees the LO mid-slew.
    auto to_discard = static_cast<std::size_t>(
        std::lround(options_.settle_s * channel.fs_hz));
    while (to_discard > 0) {
      const std::size_t n = source_.stream().read(
          discard_buf.data(), std::min(to_discard, discard_buf.size()));
      if (n == 0) {
        break; // Source ended mid-settle; probe will just find nothing.
      }
      to_discard -= n;
    }

    std::vector<CellRecord> channel_records =
        detector_.probe(source_.stream());
    if (options_.on_channel) {
      options_.on_channel(channel, channel_records);
    }
    for (auto& record : channel_records) {
      all_records.push_back(std::move(record));
    }
  }
  return all_records;
}

} // namespace cellsearch
