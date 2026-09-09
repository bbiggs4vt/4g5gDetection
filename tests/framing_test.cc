// Framing-layer test: feed a synthetic complex tone through the
// ring-buffer → NCO → decimator path and verify the glue (rates, lengths,
// NCO sign, phase continuity across frames) — the DSP itself is srsRAN's.

#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>

#include "cellsearch/iq_stream.h"
#include "src/framing.h"

namespace {

using cellsearch::Framer;
using cellsearch::IqStream;
using cellsearch::IqStreamInfo;
using cellsearch::SyncFrame;

class ToneStream final : public IqStream {
public:
  /// `n_samples` of amplitude-0.7 e^{j2*pi*f0*t} at fs, centered at fc.
  ToneStream(double fs_hz, double fc_hz, double f0_hz, std::size_t n_samples)
      : info_{fs_hz, fc_hz}, f0_hz_(f0_hz), total_(n_samples) {}

  const IqStreamInfo& info() const override { return info_; }

  std::size_t read(std::complex<float>* dst, std::size_t max_samples) override {
    std::size_t n = 0;
    for (; n < max_samples && pos_ < total_; ++n, ++pos_) {
      const double phase = 2.0 * M_PI * f0_hz_ * static_cast<double>(pos_) /
                           info_.fs_hz;
      dst[n] = std::complex<float>(0.7f * static_cast<float>(std::cos(phase)),
                                   0.7f * static_cast<float>(std::sin(phase)));
    }
    return n;
  }

private:
  IqStreamInfo info_;
  double f0_hz_;
  std::size_t total_;
  std::size_t pos_ = 0;
};

int failures = 0;

#define CHECK(cond)                                                            \
  do {                                                                         \
    if (!(cond)) {                                                             \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);              \
      ++failures;                                                              \
    }                                                                          \
  } while (0)

/// Mean frequency (Hz) of a block via average sample-to-sample rotation.
double estimate_freq_hz(const std::complex<float>* x, std::size_t n,
                        double fs_hz) {
  std::complex<double> acc{0.0, 0.0};
  for (std::size_t i = 1; i < n; ++i) {
    acc += std::conj(std::complex<double>(x[i - 1])) *
           std::complex<double>(x[i]);
  }
  return std::arg(acc) * fs_hz / (2.0 * M_PI);
}

double avg_power(const std::complex<float>* x, std::size_t n) {
  double p = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    p += std::norm(std::complex<double>(x[i]));
  }
  return p / static_cast<double>(n);
}

// Tone at +200 kHz, NCO offset 200 kHz, decimate 7.68 -> 1.92 Msps:
// the tone must land at DC in every frame, amplitude intact, and frames
// must be phase-continuous.
void test_nco_and_decimation() {
  const double fs_in = 7.68e6;
  const double fs_out = 1.92e6;
  const std::size_t frame_len = 19200; // 10 ms at fs_out.
  const std::size_t raw_per_frame = frame_len * 4;

  ToneStream stream(fs_in, 806e6, 200e3, raw_per_frame * 5 / 2);
  Framer framer({fs_in, 806e6, fs_out, 200e3, frame_len});

  SyncFrame f1{};
  SyncFrame f2{};
  CHECK(framer.next_frame(stream, f1));
  CHECK(f1.count == frame_len);
  CHECK(f1.fs_hz == fs_out);
  CHECK(f1.fc_hz == 806e6);
  CHECK(f1.nco_offset_hz == 200e3);

  // Skip the decimation filter's ramp-in at the start of the very first
  // frame, then expect the shifted tone at DC with its power intact.
  const std::size_t skip = 512;
  CHECK(std::abs(estimate_freq_hz(f1.samples + skip, f1.count - skip,
                                  fs_out)) < 50.0);
  const double p1 = avg_power(f1.samples + skip, f1.count - skip);
  CHECK(p1 > 0.35 && p1 < 0.65); // Tone power is 0.49.

  // Save the last sample of frame 1 (frame storage is reused by next_frame).
  const std::complex<float> f1_last = f1.samples[f1.count - 1];

  CHECK(framer.next_frame(stream, f2));
  CHECK(std::abs(estimate_freq_hz(f2.samples, f2.count, fs_out)) < 50.0);

  // Phase continuity across the frame boundary: a DC tone must not jump.
  const double boundary_step =
      std::abs(std::arg(std::conj(std::complex<double>(f1_last)) *
                        std::complex<double>(f2.samples[0])));
  CHECK(boundary_step < 0.05);

  // Only 2.5 frames of input were supplied.
  SyncFrame f3{};
  CHECK(!framer.next_frame(stream, f3));
}

// Ratio 1 (stream already at sync rate), no NCO: passthrough.
void test_passthrough() {
  const double fs = 1.92e6;
  const std::size_t frame_len = 1920;
  ToneStream stream(fs, 1e9, 100e3, frame_len * 3);
  Framer framer({fs, 1e9, fs, 0.0, frame_len});

  SyncFrame f{};
  CHECK(framer.next_frame(stream, f));
  CHECK(f.count == frame_len);
  CHECK(std::abs(estimate_freq_hz(f.samples, f.count, fs) - 100e3) < 50.0);
}

// Non-integer rate ratio must be rejected at construction.
void test_bad_ratio() {
  bool threw = false;
  try {
    Framer framer({30.72e6, 1e9, 11.52e6, 0.0, 1024});
  } catch (const std::invalid_argument&) {
    threw = true;
  }
  CHECK(threw);
}

// A stream shorter than one frame yields no frames.
void test_short_stream() {
  const double fs = 1.92e6;
  const std::size_t frame_len = 1920;
  ToneStream stream(fs, 1e9, 0.0, frame_len / 2);
  Framer framer({fs, 1e9, fs, 0.0, frame_len});

  SyncFrame f{};
  CHECK(!framer.next_frame(stream, f));
}

} // namespace

int main() {
  test_nco_and_decimation();
  test_passthrough();
  test_bad_ratio();
  test_short_stream();

  if (failures == 0) {
    std::printf("framing_test: all checks passed\n");
    return 0;
  }
  std::printf("framing_test: %d check(s) FAILED\n", failures);
  return 1;
}
