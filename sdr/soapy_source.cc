#include "sdr/soapy_source.h"

#include <stdexcept>

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Formats.hpp>

namespace cellsearch {

struct SoapyIqSource::Impl {
  class Stream final : public IqStream {
  public:
    const IqStreamInfo& info() const override { return info_; }

    std::size_t read(std::complex<float>* dst,
                     std::size_t max_samples) override {
      if (device == nullptr || rx == nullptr) {
        return 0;
      }
      void* buffs[] = {dst};
      // A couple of retries so a transient timeout/overflow doesn't read as
      // end-of-stream; a persistent error does.
      for (int attempt = 0; attempt < 3; ++attempt) {
        int flags = 0;
        long long time_ns = 0;
        const int ret = device->readStream(rx, buffs,
                                           static_cast<size_t>(max_samples),
                                           flags, time_ns, /*timeoutUs=*/500000);
        if (ret > 0) {
          return static_cast<std::size_t>(ret);
        }
        // ret == 0, SOAPY_SDR_TIMEOUT or SOAPY_SDR_OVERFLOW: try again.
      }
      return 0;
    }

    IqStreamInfo info_{0.0, 0.0};
    SoapySDR::Device* device = nullptr;
    SoapySDR::Stream* rx = nullptr;
  };

  SoapySDR::Device* device = nullptr;
  Stream stream;

  ~Impl() {
    if (device != nullptr) {
      stop_stream();
      SoapySDR::Device::unmake(device);
    }
  }

  void stop_stream() {
    if (stream.rx != nullptr) {
      device->deactivateStream(stream.rx);
      device->closeStream(stream.rx);
      stream.rx = nullptr;
    }
  }
};

SoapyIqSource::SoapyIqSource(const std::string& device_args,
                             std::optional<double> gain_db)
    : impl_(std::make_unique<Impl>()) {
  impl_->device = SoapySDR::Device::make(device_args);
  if (impl_->device == nullptr) {
    throw std::runtime_error("SoapyIqSource: no device for args '" +
                             device_args + "'");
  }
  impl_->stream.device = impl_->device;

  constexpr int dir = SOAPY_SDR_RX;
  constexpr size_t ch = 0;
  if (gain_db.has_value()) {
    if (impl_->device->hasGainMode(dir, ch)) {
      impl_->device->setGainMode(dir, ch, false);
    }
    impl_->device->setGain(dir, ch, *gain_db);
  } else if (impl_->device->hasGainMode(dir, ch)) {
    impl_->device->setGainMode(dir, ch, true); // AGC.
  }
}

SoapyIqSource::~SoapyIqSource() = default;

bool SoapyIqSource::tune(double fc_hz, double fs_hz) {
  constexpr int dir = SOAPY_SDR_RX;
  constexpr size_t ch = 0;
  try {
    // Restarting the stream around the retune drops queued stale samples.
    impl_->stop_stream();
    impl_->device->setSampleRate(dir, ch, fs_hz);
    impl_->device->setFrequency(dir, ch, fc_hz);
    impl_->stream.rx =
        impl_->device->setupStream(dir, SOAPY_SDR_CF32, {ch});
    if (impl_->stream.rx == nullptr) {
      return false;
    }
    if (impl_->device->activateStream(impl_->stream.rx) != 0) {
      impl_->stop_stream();
      return false;
    }
  } catch (const std::exception&) {
    impl_->stop_stream();
    return false; // Device rejected the channel; the sweeper skips it.
  }
  impl_->stream.info_ = IqStreamInfo{fs_hz, fc_hz};
  return true;
}

IqStream& SoapyIqSource::stream() { return impl_->stream; }

} // namespace cellsearch
