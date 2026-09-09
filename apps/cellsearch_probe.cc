// Per-channel probe over a capture file: the thin CLI around the core
// library used for the "does it lock on a known capture?" bar. Files are just
// streams, so Fs/Fc must be given on the command line.

#include <cstdio>
#include <cstdlib>
#include <exception>

#include "cellsearch/detector.h"
#include "cellsearch/file_iq_stream.h"

namespace {

void print_record(const cellsearch::CellRecord& r) {
  std::printf("%s cell: fc=%.3f MHz pci=%u sfn=%u",
              r.rat == cellsearch::Rat::LTE ? "LTE" : "NR", r.fc_hz / 1e6,
              r.pci, r.sfn);
  if (const auto* bw = std::get_if<cellsearch::LteBandwidth>(&r.bandwidth_scs)) {
    std::printf(" bw=%u PRB", bw->n_prb);
  } else if (const auto* scs = std::get_if<cellsearch::NrScs>(&r.bandwidth_scs)) {
    std::printf(" scs=%u kHz", scs->scs_khz);
  }
  if (r.ssb_index) {
    std::printf(" ssb_idx=%u", *r.ssb_index);
  }
  std::printf("\n");
}

} // namespace

int main(int argc, char** argv) {
  if (argc != 4) {
    std::fprintf(stderr,
                 "usage: %s <capture.fc32> <fs_hz> <fc_hz>\n"
                 "  raw interleaved complex float32 capture; Fs/Fc are\n"
                 "  required sidecar metadata (files don't carry them)\n",
                 argv[0]);
    return 2;
  }

  try {
    cellsearch::FileIqStream stream(argv[1], std::atof(argv[2]),
                                    std::atof(argv[3]));
    cellsearch::Detector detector;

    std::printf("PHY engine: %s\n", cellsearch::phy_engine_version().c_str());

    auto records = detector.probe(stream);
    for (const auto& r : records) {
      print_record(r);
    }
    if (records.empty()) {
      std::printf("no cell detected\n");
    }
    return records.empty() ? 1 : 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 2;
  }
}
