// NR test-capture generator: writes raw fc32 baseband containing periodic
// SS/PBCH blocks (PSS/SSS/PBCH+MIB) for a chosen PCI, using srsRAN's
// srsran_ssb encoder — the TX side of the same object the NR branch decodes
// with. srsRAN_4G ships no NR equivalent of pdsch_enodeb, so this fills the
// gap for the "locks onto a known capture" tests. Test tooling only — not
// part of the core library.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <srsran/phy/common/phy_common_nr.h>
#include <srsran/phy/phch/pbch_msg_nr.h>
#include <srsran/phy/sync/ssb.h>
}

int main(int argc, char** argv) {
  if (argc != 8) {
    std::fprintf(stderr,
                 "usage: %s <out.fc32> <srate_hz> <fc_hz> <ssb_freq_hz> <pci> "
                 "<scs_khz:15|30> <n_ms>\n"
                 "  ssb_freq_hz must be a GSCN sync-raster point within the\n"
                 "  band around fc_hz; SSB periodicity is 20 ms\n",
                 argv[0]);
    return 2;
  }
  const char* out_path = argv[1];
  const double srate_hz = std::atof(argv[2]);
  const double fc_hz = std::atof(argv[3]);
  const double ssb_freq_hz = std::atof(argv[4]);
  const auto pci = static_cast<uint32_t>(std::atoi(argv[5]));
  const int scs_khz = std::atoi(argv[6]);
  const int n_ms = std::atoi(argv[7]);

  if ((scs_khz != 15 && scs_khz != 30) || pci >= SRSRAN_NOF_NID_NR ||
      srate_hz <= 0 || n_ms <= 0) {
    std::fprintf(stderr, "invalid arguments\n");
    return 2;
  }
  const srsran_subcarrier_spacing_t scs = scs_khz == 15
                                              ? srsran_subcarrier_spacing_15kHz
                                              : srsran_subcarrier_spacing_30kHz;

  srsran_ssb_t ssb = {};
  srsran_ssb_args_t args = {};
  args.max_srate_hz = srate_hz;
  args.min_scs = srsran_subcarrier_spacing_15kHz;
  args.enable_encode = true;
  if (srsran_ssb_init(&ssb, &args) != SRSRAN_SUCCESS) {
    std::fprintf(stderr, "ssb init failed\n");
    return 1;
  }

  srsran_ssb_cfg_t cfg = {};
  cfg.srate_hz = srate_hz;
  cfg.center_freq_hz = fc_hz;
  cfg.ssb_freq_hz = ssb_freq_hz;
  cfg.scs = scs;
  // FR1 patterns: case A for 15 kHz, case C for 30 kHz (must match the
  // receiver's hypothesis mapping in src/nr_branch.cc).
  cfg.pattern = scs == srsran_subcarrier_spacing_15kHz ? SRSRAN_SSB_PATTERN_A
                                                       : SRSRAN_SSB_PATTERN_C;
  cfg.duplex_mode = SRSRAN_DUPLEX_MODE_FDD;
  cfg.periodicity_ms = 20;
  if (srsran_ssb_set_cfg(&ssb, &cfg) != SRSRAN_SUCCESS) {
    std::fprintf(stderr, "ssb set_cfg failed\n");
    srsran_ssb_free(&ssb);
    return 1;
  }

  std::FILE* out = std::fopen(out_path, "wb");
  if (out == nullptr) {
    std::fprintf(stderr, "cannot open %s\n", out_path);
    srsran_ssb_free(&ssb);
    return 1;
  }

  // Emit half radio frames (5 ms); srsran_ssb_add places the burst at the
  // right candidate position within a half frame.
  const auto hf_len = static_cast<size_t>(std::lround(srate_hz * 5e-3));
  std::vector<cf_t> buffer(hf_len);
  const int n_hf = (n_ms + 4) / 5;

  int rc = 0;
  for (int hf = 0; hf < n_hf; ++hf) {
    const auto sfn = static_cast<uint32_t>(hf / 2) % 1024;
    const bool hrf = (hf % 2) != 0;
    std::memset(buffer.data(), 0, hf_len * sizeof(cf_t));

    if ((sfn * 10 + (hrf ? 5 : 0)) % cfg.periodicity_ms == 0) {
      srsran_mib_nr_t mib = {};
      mib.sfn = sfn;
      mib.ssb_idx = 0;
      mib.hrf = hrf;
      mib.scs_common = scs;
      mib.coreset0_idx = 1; // pdcch-ConfigSIB1 = 0x10; captured, not followed.
      mib.ss0_idx = 0;
      mib.intra_freq_reselection = true;

      srsran_pbch_msg_nr_t pbch_msg = {};
      if (srsran_pbch_msg_nr_mib_pack(&mib, &pbch_msg) != SRSRAN_SUCCESS ||
          srsran_ssb_add(&ssb, pci, &pbch_msg, buffer.data(), buffer.data()) !=
              SRSRAN_SUCCESS) {
        std::fprintf(stderr, "ssb encode failed at hf %d\n", hf);
        rc = 1;
        break;
      }
    }
    if (std::fwrite(buffer.data(), sizeof(cf_t), hf_len, out) != hf_len) {
      std::fprintf(stderr, "short write\n");
      rc = 1;
      break;
    }
  }

  std::fclose(out);
  srsran_ssb_free(&ssb);
  if (rc == 0) {
    std::printf("wrote %d ms: pci=%u scs=%d kHz ssb at %+.0f Hz from fc\n",
                n_hf * 5, pci, scs_khz, ssb_freq_hz - fc_hz);
  }
  return rc;
}
