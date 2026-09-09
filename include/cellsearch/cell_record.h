#pragma once

#include <chrono>
#include <cstdint>
#include <optional>
#include <variant>

namespace cellsearch {

enum class Rat : std::uint8_t { LTE, NR };

/// LTE cyclic prefix type (from PSS/SSS + PBCH).
enum class CpType : std::uint8_t { NORMAL, EXTENDED };

/// LTE PHICH configuration (from MIB).
struct PhichConfig {
  enum class Length : std::uint8_t { NORMAL, EXTENDED };
  enum class Resources : std::uint8_t { ONE_SIXTH, HALF, ONE, TWO };
  Length length;
  Resources resources;
};

/// LTE system bandwidth, expressed as the MIB's PRB count (6/15/25/50/75/100).
struct LteBandwidth {
  std::uint8_t n_prb;
};

/// NR common subcarrier spacing from MIB (15/30/60/120 kHz).
struct NrScs {
  std::uint16_t scs_khz;
};

/// Both branches report one of these: LTE system BW or NR common SCS.
using BandwidthOrScs = std::variant<LteBandwidth, NrScs>;

/// One normalized record per detected cell — the MIB-level fingerprint
/// `(RAT, Fc, PCI, [SSB index], bandwidth/SCS, SFN)` plus RAT-specific extras.
/// Identity stops here by design: the globally unique cell ID lives in SIB1,
/// which is out of scope.
struct CellRecord {
  Rat rat;
  double fc_hz;       ///< Center frequency of the analyzed stream.
  std::uint16_t pci;  ///< 0–503 (LTE) / 0–1007 (NR).
  std::optional<std::uint8_t> ssb_index; ///< NR only (→ beam in multi-beam).
  BandwidthOrScs bandwidth_scs;
  std::uint16_t sfn;

  // LTE-only extras (from PBCH/MIB).
  std::optional<PhichConfig> phich_config;
  std::optional<CpType> cp_type;
  std::optional<std::uint8_t> n_ports;

  // NR-only extras.
  /// pdcch-ConfigSIB1: pointer to CORESET#0 — captured but NOT followed.
  std::optional<std::uint8_t> pdcch_config_sib1;

  // Metadata.
  std::chrono::system_clock::time_point timestamp;
  std::optional<float> snr_or_metric; ///< Lock confidence, if available.
};

} // namespace cellsearch
