# captures/

Known-good IQ captures used to verify the decode branches. Files are just
streams — the bar for a branch is "locks onto the cell in a capture here and
emits the expected PCI/SFN", not "it compiles".

Files are git-ignored (large binaries); regenerate them or record your own.
The e2e tests skip (exit 77) when a capture is missing.

- LTE: `scripts/gen_lte_captures.sh` — uses srsRAN's `pdsch_enodeb` example
  to write PSS/SSS/PBCH baseband for a chosen PCI straight to a file.
- NR: `scripts/gen_nr_captures.sh` — uses this repo's `gen_nr_ssb_capture`
  tool (built with the project; srsRAN's `srsran_ssb` encoder) to write
  periodic SSBs at a chosen GSCN raster point offset from Fc.

## Format
Raw interleaved complex float32 (`.fc32`), no header. Fs and Fc are required
sidecar metadata — record them in the filename or alongside, e.g.:

```
lte_pci123_fs1.92e6_fc806e6.fc32
nr_pci42_fs11.52e6_fc3610.08e6.fc32
```

Run against a capture:

```
cellsearch_probe captures/lte_pci123_fs1.92e6_fc806e6.fc32 1.92e6 806e6
```
