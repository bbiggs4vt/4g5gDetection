#!/usr/bin/env bash
# Regenerates the LTE test captures under captures/ using srsRAN's
# pdsch_enodeb example (transmits PSS/SSS/PBCH for a chosen PCI to a file —
# no radio involved). Captures are git-ignored; run this after building
# srsRAN_4G with its examples.
#
# Usage: PDSCH_ENODEB=/path/to/srsRAN_4G/build/lib/examples/pdsch_enodeb \
#        scripts/gen_lte_captures.sh
set -euo pipefail

ENODEB="${PDSCH_ENODEB:-pdsch_enodeb}"
OUT_DIR="$(dirname "$0")/../captures"

command -v "$ENODEB" >/dev/null || {
  echo "pdsch_enodeb not found; set PDSCH_ENODEB to the built binary" >&2
  exit 1
}

# 6 PRB -> 1.92 Msps: exercises the ratio-1 (no decimation) path. 1 s long.
"$ENODEB" -o "$OUT_DIR/lte_pci123_fs1.92e6_fc806e6.fc32" -c 123 -p 6 -n 100 \
  </dev/null

# 50 PRB -> 11.52 Msps (srsRAN default symbol size): exercises the NCO-less
# 6x decimation path. 0.6 s long.
"$ENODEB" -o "$OUT_DIR/lte_pci321_fs11.52e6_fc1842.5e6.fc32" -c 321 -p 50 -n 60 \
  </dev/null

ls -l "$OUT_DIR"/*.fc32
