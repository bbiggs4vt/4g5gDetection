// End-to-end NR test: run the Detector over a known SSB capture and check
// the emitted fingerprint (PCI, common SCS, SSB index, pdcch-ConfigSIB1).
//
// Usage: nr_e2e_test <capture.fc32> <fs_hz> <fc_hz> <expected_pci> <expected_scs_khz>
// Exits 77 (ctest SKIP_RETURN_CODE) when the capture is missing — regenerate
// with scripts/gen_nr_captures.sh.

#include <cstdio>
#include <cstdlib>

#include "cellsearch/detector.h"
#include "cellsearch/file_iq_stream.h"

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr,
                 "usage: %s <capture.fc32> <fs_hz> <fc_hz> <pci> <scs_khz>\n",
                 argv[0]);
    return 2;
  }
  const char* path = argv[1];
  const double fs = std::atof(argv[2]);
  const double fc = std::atof(argv[3]);
  const int expected_pci = std::atoi(argv[4]);
  const int expected_scs = std::atoi(argv[5]);

  if (std::FILE* f = std::fopen(path, "rb")) {
    std::fclose(f);
  } else {
    std::printf("SKIP: capture %s not found (scripts/gen_nr_captures.sh)\n",
                path);
    return 77;
  }

  try {
    cellsearch::FileIqStream stream(path, fs, fc);
    cellsearch::Detector detector;
    const auto records = detector.probe(stream);

    for (const auto& r : records) {
      if (r.rat != cellsearch::Rat::NR) {
        continue;
      }
      const auto* scs = std::get_if<cellsearch::NrScs>(&r.bandwidth_scs);
      std::printf("NR lock: pci=%u scs=%u kHz sfn=%u ssb_idx=%u "
                  "pdcch_cfg_sib1=0x%02x snr=%.1f dB\n",
                  r.pci, scs ? scs->scs_khz : 0, r.sfn,
                  r.ssb_index ? *r.ssb_index : 255,
                  r.pdcch_config_sib1 ? *r.pdcch_config_sib1 : 0,
                  r.snr_or_metric ? *r.snr_or_metric : 0.0f);
      if (r.pci != expected_pci) {
        std::printf("FAIL: expected pci %d\n", expected_pci);
        return 1;
      }
      if (!scs || scs->scs_khz != expected_scs) {
        std::printf("FAIL: expected scs %d kHz\n", expected_scs);
        return 1;
      }
      if (!r.ssb_index || *r.ssb_index != 0 || !r.pdcch_config_sib1 ||
          *r.pdcch_config_sib1 != 0x10 || r.sfn >= 1024 || r.fc_hz != fc) {
        std::printf("FAIL: record fields incomplete or wrong\n");
        return 1;
      }
      std::printf("nr_e2e_test: PASS\n");
      return 0;
    }
    std::printf("FAIL: no NR cell detected in %s\n", path);
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 2;
  }
}
