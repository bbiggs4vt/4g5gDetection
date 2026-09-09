// End-to-end LTE test: run the Detector over a known capture and check the
// emitted fingerprint. This is the bar from the README — "locks onto the
// cell in the capture", not "it compiles".
//
// Usage: lte_e2e_test <capture.fc32> <fs_hz> <fc_hz> <expected_pci> <expected_prb>
// Exits 77 (ctest SKIP_RETURN_CODE) when the capture is missing — captures
// are not in git; regenerate with scripts/gen_lte_captures.sh.

#include <cstdio>
#include <cstdlib>

#include "cellsearch/detector.h"
#include "cellsearch/file_iq_stream.h"

int main(int argc, char** argv) {
  if (argc != 6) {
    std::fprintf(stderr,
                 "usage: %s <capture.fc32> <fs_hz> <fc_hz> <pci> <prb>\n",
                 argv[0]);
    return 2;
  }
  const char* path = argv[1];
  const double fs = std::atof(argv[2]);
  const double fc = std::atof(argv[3]);
  const int expected_pci = std::atoi(argv[4]);
  const int expected_prb = std::atoi(argv[5]);

  if (std::FILE* f = std::fopen(path, "rb")) {
    std::fclose(f);
  } else {
    std::printf("SKIP: capture %s not found (scripts/gen_lte_captures.sh)\n",
                path);
    return 77;
  }

  try {
    cellsearch::FileIqStream stream(path, fs, fc);
    cellsearch::Detector detector;
    const auto records = detector.probe(stream);

    for (const auto& r : records) {
      if (r.rat != cellsearch::Rat::LTE) {
        continue;
      }
      const auto* bw = std::get_if<cellsearch::LteBandwidth>(&r.bandwidth_scs);
      std::printf("LTE lock: pci=%u prb=%u sfn=%u ports=%u peak=%.2f\n", r.pci,
                  bw ? bw->n_prb : 0, r.sfn, r.n_ports ? *r.n_ports : 0,
                  r.snr_or_metric ? *r.snr_or_metric : 0.0f);
      if (r.pci != expected_pci) {
        std::printf("FAIL: expected pci %d\n", expected_pci);
        return 1;
      }
      if (!bw || bw->n_prb != expected_prb) {
        std::printf("FAIL: expected prb %d\n", expected_prb);
        return 1;
      }
      if (r.fc_hz != fc || r.sfn >= 1024 || !r.cp_type || !r.phich_config ||
          !r.n_ports || *r.n_ports < 1) {
        std::printf("FAIL: record fields incomplete\n");
        return 1;
      }
      std::printf("lte_e2e_test: PASS\n");
      return 0;
    }
    std::printf("FAIL: no LTE cell detected in %s\n", path);
    return 1;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 2;
  }
}
