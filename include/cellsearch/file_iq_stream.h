#pragma once

#include <cstdio>
#include <memory>
#include <string>

#include "cellsearch/iq_stream.h"

namespace cellsearch {

/// IqStream over a raw capture file of interleaved complex float32 samples
/// (the common .fc32/.cf32/.iq layout; also what srsRAN file-based tools
/// read/write). Fs and Fc are not stored in such files, so the caller must
/// supply them — files are just streams here.
class FileIqStream final : public IqStream {
public:
  /// @throws std::runtime_error if the file cannot be opened.
  FileIqStream(const std::string& path, double fs_hz, double fc_hz);
  ~FileIqStream() override;

  const IqStreamInfo& info() const override;
  std::size_t read(std::complex<float>* dst, std::size_t max_samples) override;

private:
  IqStreamInfo info_;
  std::unique_ptr<std::FILE, int (*)(std::FILE*)> file_;
};

} // namespace cellsearch
