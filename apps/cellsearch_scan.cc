// Live sweep scanner: SoapySDR radio → sweep layer → per-channel probe core.
// The core never sees the radio; this app wires SoapyIqSource into Sweeper.
//
//   cellsearch_scan -s 11.52e6 -f 806e6,1842.5e6,3499.98e6 [-a driver=uhd]
//                   [-g 40] [-t settle_s] [-l]

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <string>
#include <vector>

#include "cellsearch/sweep.h"
#include "sdr/soapy_source.h"

namespace {

void print_record(const cellsearch::CellRecord& r) {
  std::printf("  %s cell: fc=%.3f MHz pci=%u sfn=%u",
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

int usage(const char* argv0) {
  std::fprintf(stderr,
               "usage: %s -s <fs_hz> -f <fc_hz,fc_hz,...> [options]\n"
               "  -a <args>     SoapySDR device args (default: first device)\n"
               "  -g <gain_db>  fixed RX gain (default: AGC where supported)\n"
               "  -t <settle_s> post-retune settle time (default 0.05)\n"
               "  -l            loop the sweep until interrupted\n",
               argv0);
  return 2;
}

} // namespace

int main(int argc, char** argv) {
  std::string device_args;
  std::optional<double> gain_db;
  double fs_hz = 0.0;
  double settle_s = 0.05;
  bool loop = false;
  std::vector<double> fcs;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
    if (arg == "-a") {
      const char* v = next();
      if (!v) return usage(argv[0]);
      device_args = v;
    } else if (arg == "-g") {
      const char* v = next();
      if (!v) return usage(argv[0]);
      gain_db = std::atof(v);
    } else if (arg == "-s") {
      const char* v = next();
      if (!v) return usage(argv[0]);
      fs_hz = std::atof(v);
    } else if (arg == "-t") {
      const char* v = next();
      if (!v) return usage(argv[0]);
      settle_s = std::atof(v);
    } else if (arg == "-f") {
      const char* v = next();
      if (!v) return usage(argv[0]);
      for (const char* p = v; *p != '\0';) {
        char* end = nullptr;
        fcs.push_back(std::strtod(p, &end));
        p = (end != nullptr && *end == ',') ? end + 1 : end;
        if (p == nullptr) break;
      }
    } else if (arg == "-l") {
      loop = true;
    } else {
      return usage(argv[0]);
    }
  }
  if (fs_hz <= 0.0 || fcs.empty()) {
    return usage(argv[0]);
  }

  std::vector<cellsearch::SweepChannel> plan;
  for (double fc : fcs) {
    plan.push_back({fc, fs_hz});
  }

  try {
    cellsearch::SoapyIqSource source(device_args, gain_db);

    cellsearch::SweepOptions options;
    options.settle_s = settle_s;
    options.on_channel = [](const cellsearch::SweepChannel& ch,
                            const std::vector<cellsearch::CellRecord>& recs) {
      std::printf("%.3f MHz: %zu cell(s)\n", ch.fc_hz / 1e6, recs.size());
      for (const auto& r : recs) {
        print_record(r);
      }
    };
    cellsearch::Sweeper sweeper(source, options);

    do {
      sweeper.sweep_once(plan);
    } while (loop);
    return 0;
  } catch (const std::exception& e) {
    std::fprintf(stderr, "error: %s\n", e.what());
    return 1;
  }
}
