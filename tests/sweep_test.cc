// Sweep-layer test with a fake tunable source: three channels (an LTE
// capture, an empty one, one the "device" rejects), each retune prepending a
// settle window of garbage samples the sweeper must discard — the classic
// live-scanner bug, simulated. No radio involved.
//
// Exits 77 (ctest SKIP_RETURN_CODE) when the LTE capture is missing.

#include <complex>
#include <cstdio>
#include <random>
#include <vector>

#include "cellsearch/sweep.h"

namespace {

using cellsearch::IqStream;
using cellsearch::IqStreamInfo;

constexpr double kFs = 1.92e6;
constexpr double kLteFc = 806e6;
constexpr double kEmptyFc = 811e6;
constexpr double kRejectedFc = 999e6;
constexpr double kSettleS = 0.05;

class FakeRadio final : public cellsearch::TunableIqSource {
public:
  explicit FakeRadio(std::vector<std::complex<float>> lte_samples)
      : lte_samples_(std::move(lte_samples)) {}

  bool tune(double fc_hz, double fs_hz) override {
    if (fc_hz == kRejectedFc) {
      return false; // "Out of tuning range."
    }
    stream_.info_ = IqStreamInfo{fs_hz, fc_hz};
    stream_.pos = 0;
    stream_.data.clear();

    // Stale-LO garbage: exactly the settle window of full-scale noise that
    // would corrupt the probe window if the sweeper failed to discard it.
    const auto garbage_len =
        static_cast<std::size_t>(kSettleS * fs_hz);
    std::mt19937 rng(1234);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    for (std::size_t i = 0; i < garbage_len; ++i) {
      stream_.data.emplace_back(dist(rng), dist(rng));
    }

    if (fc_hz == kLteFc) {
      stream_.data.insert(stream_.data.end(), lte_samples_.begin(),
                          lte_samples_.end());
    } else {
      stream_.data.resize(stream_.data.size() +
                          static_cast<std::size_t>(0.6 * fs_hz));
    }
    return true;
  }

  IqStream& stream() override { return stream_; }

private:
  class Stream final : public IqStream {
  public:
    const IqStreamInfo& info() const override { return info_; }
    std::size_t read(std::complex<float>* dst,
                     std::size_t max_samples) override {
      const std::size_t n = std::min(max_samples, data.size() - pos);
      for (std::size_t i = 0; i < n; ++i) {
        dst[i] = data[pos + i];
      }
      pos += n;
      return n;
    }
    IqStreamInfo info_{0.0, 0.0};
    std::vector<std::complex<float>> data;
    std::size_t pos = 0;
  };

  std::vector<std::complex<float>> lte_samples_;
  Stream stream_;
};

} // namespace

int main(int argc, char** argv) {
  const char* capture =
      argc > 1 ? argv[1] : "captures/lte_pci123_fs1.92e6_fc806e6.fc32";

  std::FILE* f = std::fopen(capture, "rb");
  if (f == nullptr) {
    std::printf("SKIP: capture %s not found (scripts/gen_lte_captures.sh)\n",
                capture);
    return 77;
  }
  std::vector<std::complex<float>> lte_samples(
      static_cast<std::size_t>(0.6 * kFs));
  lte_samples.resize(std::fread(lte_samples.data(),
                                sizeof(std::complex<float>),
                                lte_samples.size(), f));
  std::fclose(f);

  FakeRadio radio(std::move(lte_samples));

  cellsearch::SweepOptions options;
  options.settle_s = kSettleS;
  int channels_probed = 0;
  options.on_channel = [&](const cellsearch::SweepChannel& ch,
                           const std::vector<cellsearch::CellRecord>& recs) {
    ++channels_probed;
    std::printf("%.1f MHz: %zu record(s)\n", ch.fc_hz / 1e6, recs.size());
  };

  cellsearch::Sweeper sweeper(radio, options);
  const auto records = sweeper.sweep_once(
      {{kLteFc, kFs}, {kRejectedFc, kFs}, {kEmptyFc, kFs}});

  int failures = 0;
  if (channels_probed != 2) {
    std::printf("FAIL: expected 2 probed channels (rejected one skipped), "
                "got %d\n", channels_probed);
    ++failures;
  }
  if (records.size() != 1) {
    std::printf("FAIL: expected exactly 1 record, got %zu\n", records.size());
    ++failures;
  } else {
    const auto& r = records[0];
    if (r.rat != cellsearch::Rat::LTE || r.pci != 123 || r.fc_hz != kLteFc) {
      std::printf("FAIL: wrong record (pci=%u fc=%.1f MHz)\n", r.pci,
                  r.fc_hz / 1e6);
      ++failures;
    }
  }

  if (failures == 0) {
    std::printf("sweep_test: PASS\n");
    return 0;
  }
  return 1;
}
