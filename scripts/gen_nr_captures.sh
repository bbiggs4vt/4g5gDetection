#!/usr/bin/env bash
# Regenerates the NR test captures under captures/ using this repo's own
# gen_nr_ssb_capture tool (build it first: it's part of the CMake build).
# SSB frequencies are GSCN sync-raster points (3 GHz + N * 1.44 MHz up
# there), deliberately offset from Fc so the NCO path is exercised.
#
# Usage: scripts/gen_nr_captures.sh [build_dir]   (default: ./build)
set -euo pipefail

BUILD_DIR="${1:-$(dirname "$0")/../build}"
GEN="$BUILD_DIR/gen_nr_ssb_capture"
OUT_DIR="$(dirname "$0")/../captures"

[ -x "$GEN" ] || { echo "$GEN not built (cmake --build $BUILD_DIR)" >&2; exit 1; }

# Fc is 3499.98 MHz (not on the raster — the typical case): srsran_ssb's
# encoder needs (ssb_freq - fc) to be an integer multiple of the SCS, which
# this Fc satisfies for both raster points below. The receiver has no such
# constraint — its NCO mixes the candidate exactly to DC.

# 11.52 Msps, SSB at GSCN 3499.68 MHz = fc - 300 kHz: ratio-1 path with a
# small NCO shift (the "centered carrier still needs the NCO" case).
"$GEN" "$OUT_DIR/nr_pci500_fs11.52e6_fc3499.98e6.fc32" 11.52e6 3499.98e6 3499.68e6 500 30 200

# 23.04 Msps, SSB at GSCN 3502.56 MHz = fc + 2.58 MHz: NCO shift plus 2x
# decimation, and only the third-nearest raster candidate — exercises the
# GSCN hypothesis iteration.
"$GEN" "$OUT_DIR/nr_pci77_fs23.04e6_fc3499.98e6.fc32" 23.04e6 3499.98e6 3502.56e6 77 30 200

ls -l "$OUT_DIR"/nr_*.fc32
