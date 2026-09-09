#include "cellsearch/file_iq_stream.h"

#include <stdexcept>

namespace cellsearch {

FileIqStream::FileIqStream(const std::string& path, double fs_hz, double fc_hz)
    : info_{fs_hz, fc_hz}, file_(std::fopen(path.c_str(), "rb"), &std::fclose) {
  if (!file_) {
    throw std::runtime_error("FileIqStream: cannot open " + path);
  }
}

FileIqStream::~FileIqStream() = default;

const IqStreamInfo& FileIqStream::info() const { return info_; }

std::size_t FileIqStream::read(std::complex<float>* dst,
                               std::size_t max_samples) {
  return std::fread(dst, sizeof(std::complex<float>), max_samples, file_.get());
}

} // namespace cellsearch
