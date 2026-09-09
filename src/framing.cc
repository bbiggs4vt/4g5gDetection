#include "src/framing.h"

#include <cmath>
#include <complex>
#include <cstring>
#include <stdexcept>

extern "C" {
#include <srsran/phy/resampling/resampler.h>
#include <srsran/phy/utils/ringbuffer.h>
#include <srsran/phy/utils/vector.h>
}

namespace cellsearch {

namespace {

constexpr std::size_t kIoChunkSamples = 32768;

// std::complex<float> is layout-compatible with srsRAN's cf_t
// (_Complex float): both are exactly {float re; float im;}.
cf_t* as_cf(std::complex<float>* p) { return reinterpret_cast<cf_t*>(p); }

} // namespace

struct Framer::SrsranState {
  srsran_ringbuffer_t rb{};
  srsran_resampler_fft_t resampler{};
  bool resampler_active = false;

  ~SrsranState() {
    srsran_ringbuffer_free(&rb);
    if (resampler_active) {
      srsran_resampler_fft_free(&resampler);
    }
  }
};

Framer::Framer(const Config& config)
    : config_(config), nco_cfo_(0.0f), nco_phase_(1.0f, 0.0f),
      srs_(std::make_unique<SrsranState>()) {
  if (config_.frame_len == 0 || config_.in_rate_hz <= 0.0 ||
      config_.out_rate_hz <= 0.0) {
    throw std::invalid_argument("Framer: invalid config");
  }

  const double ratio = config_.in_rate_hz / config_.out_rate_hz;
  ratio_ = static_cast<unsigned>(std::lround(ratio));
  if (ratio_ < 1 || std::abs(ratio - ratio_) > 1e-6 * ratio) {
    throw std::invalid_argument(
        "Framer: in_rate/out_rate must be a positive integer ratio (got " +
        std::to_string(ratio) + ")");
  }

  // Normalized NCO frequency. To move the region at fc + offset down to
  // baseband we rotate by e^{-j2*pi*offset*n/Fs}; srsran_vec_apply_cfo
  // rotates by e^{+j2*pi*cfo*n}, hence the minus sign.
  nco_cfo_ = static_cast<float>(-config_.nco_offset_hz / config_.in_rate_hz);

  const std::size_t raw_len = config_.frame_len * ratio_;
  io_buf_.resize(kIoChunkSamples);
  raw_buf_.resize(raw_len);
  frame_buf_.resize(config_.frame_len);

  const int capacity_bytes =
      static_cast<int>((raw_len + kIoChunkSamples) * sizeof(cf_t));
  if (srsran_ringbuffer_init(&srs_->rb, capacity_bytes) != SRSRAN_SUCCESS) {
    throw std::runtime_error("Framer: ringbuffer init failed");
  }

  if (ratio_ > 1) {
    if (srsran_resampler_fft_init(&srs_->resampler,
                                  SRSRAN_RESAMPLER_MODE_DECIMATE,
                                  ratio_) != SRSRAN_SUCCESS) {
      throw std::runtime_error("Framer: resampler init failed (ratio " +
                               std::to_string(ratio_) + ")");
    }
    srs_->resampler_active = true;
  }
}

Framer::~Framer() = default;

bool Framer::next_frame(IqStream& stream, SyncFrame& out) {
  const std::size_t raw_len = config_.frame_len * ratio_;
  const int raw_bytes = static_cast<int>(raw_len * sizeof(cf_t));

  // Fill the ring buffer until one frame's worth of raw samples is queued.
  while (srsran_ringbuffer_status(&srs_->rb) < raw_bytes) {
    const std::size_t n = stream.read(io_buf_.data(), io_buf_.size());
    if (n == 0) {
      return false; // End of stream mid-frame.
    }
    srsran_ringbuffer_write(&srs_->rb, io_buf_.data(),
                            static_cast<int>(n * sizeof(cf_t)));
  }
  srsran_ringbuffer_read(&srs_->rb, raw_buf_.data(), raw_bytes);

  // Optional NCO: mix the target sync region to baseband before decimating.
  // apply_cfo restarts at phase 0 every call, so carry the phase across
  // frames ourselves to keep successive frames time-continuous.
  if (nco_cfo_ != 0.0f) {
    srsran_vec_apply_cfo(as_cf(raw_buf_.data()), nco_cfo_,
                         as_cf(raw_buf_.data()), static_cast<int>(raw_len));
    cf_t rotated;
    std::memcpy(&rotated, &nco_phase_, sizeof(cf_t));
    srsran_vec_sc_prod_ccc(as_cf(raw_buf_.data()), rotated,
                           as_cf(raw_buf_.data()),
                           static_cast<uint32_t>(raw_len));
    const double cycles =
        std::fmod(static_cast<double>(nco_cfo_) * static_cast<double>(raw_len),
                  1.0);
    nco_phase_ *= std::polar(
        1.0f, 2.0f * static_cast<float>(M_PI) * static_cast<float>(cycles));
    nco_phase_ /= std::abs(nco_phase_); // Keep it on the unit circle.
  }

  // Decimate to the sync rate (srsRAN FFT resampler keeps filter state, so
  // frames stay contiguous). Ratio 1 needs no resampler.
  if (ratio_ > 1) {
    srsran_resampler_fft_run(&srs_->resampler, as_cf(raw_buf_.data()),
                             as_cf(frame_buf_.data()),
                             static_cast<uint32_t>(raw_len));
  } else {
    frame_buf_ = raw_buf_;
  }

  out.samples = frame_buf_.data();
  out.count = config_.frame_len;
  out.fs_hz = config_.out_rate_hz;
  out.fc_hz = config_.fc_hz;
  out.nco_offset_hz = config_.nco_offset_hz;
  return true;
}

} // namespace cellsearch
