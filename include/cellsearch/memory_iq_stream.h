#pragma once

#include "cellsearch/iq_stream.h"

namespace cellsearch {

/// Non-owning IqStream view over a sample buffer. Used by the dispatcher to
/// replay one captured probe window under multiple RAT/frequency hypotheses,
/// and handy for tests. The buffer must outlive the stream.
class MemoryIqStream final : public IqStream {
public:
  MemoryIqStream(const std::complex<float>* data, std::size_t count,
                 IqStreamInfo info)
      : info_(info), data_(data), count_(count) {}

  const IqStreamInfo& info() const override { return info_; }

  std::size_t read(std::complex<float>* dst, std::size_t max_samples) override {
    const std::size_t n = std::min(max_samples, count_ - pos_);
    for (std::size_t i = 0; i < n; ++i) {
      dst[i] = data_[pos_ + i];
    }
    pos_ += n;
    return n;
  }

private:
  IqStreamInfo info_;
  const std::complex<float>* data_;
  std::size_t count_;
  std::size_t pos_ = 0;
};

} // namespace cellsearch
