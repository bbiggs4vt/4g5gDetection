# CLAUDE.md

Guardrails for this repo. Read `README.md` for the full design and rationale.
This file is the short list of constraints that must hold every session.

## What this project is
A library that takes a **generic IQ stream `(Fs, Fc)`** and detects an **LTE or
5G NR** cell on that channel, decoding to **MIB level only**. Output is one
normalized `CellRecord` per detected cell.

## Hard constraints — do not violate
- **Scope stops at MIB.** No PDCCH, no PDSCH, no blind DCI search, no SIB1/SIBs,
  no ciphered or user-plane decode. If a task seems to require these, stop and
  flag it — it's out of scope, not a gap to fill.
- **srsRAN is the PHY engine. Do not reimplement DSP.** Reuse:
  - LTE: `srsran_ue_sync`, `srsran_ue_mib`.
  - NR: the NR SSB search / `srsran_ue_sync_nr` path + NR PBCH/MIB decode.
  These take sample buffers — feed them from the ring buffer.
- **No SDR/driver code in the core.** No UHD/SoapySDR/radio-handle calls inside
  the decode library. The input contract is a stream of complex samples plus
  `(Fs, Fc)` metadata. Any live-radio glue lives in a separate optional layer.
- **`Fc` is required sidecar metadata.** A raw sample buffer carries no center
  frequency; results are meaningless without `Fc`, and NR needs it for GSCN.

## Architecture the code must follow
```
IQ stream (Fs, Fc) → ring buffer → [optional NCO shift] → decimate to sync rate
                     → { LTE: PSS/SSS→PBCH/MIB | NR: SSB→PBCH/MIB } → CellRecord
```
- Decimate to the **sync region** rate before correlating (LTE ~1.92 Msps; NR a
  narrow SSB region sized by SCS). Don't run correlators at the full channel rate.
- **NR often needs the NCO frequency shift** even on a centered carrier, because
  the SSB sits at a GSCN raster point offset from `Fc`. LTE's PSS/SSS is at
  carrier center and usually doesn't.
- The dispatcher tries **both** RAT correlators and reports whichever locks.

## Current input model
Core = **per-channel probe** (fixed `Fc`, analyze one channel fully).
Continuous sweep is an **optional outer layer**, not the core. (If this changes,
update `README.md` first, then the framing layer.)

## Build / verify order
Skeleton + interfaces → framing (ring buffer + resampler + NCO hook) → **LTE
branch end-to-end against a file capture** → NR branch → (optional) sweep + SDR
glue. Land LTE fully before starting NR.

## Definition of done for a decode branch
Not "it compiles." It must **lock onto the cell in a known capture** under
`captures/` and emit the expected fingerprint (PCI, SFN, bandwidth/SCS). Test
against real IQ files early — files are just streams here.

## When unsure
If a request pushes past MIB, couples the core to a specific SDR, or asks you to
hand-roll sync/PBCH DSP that srsRAN already provides — pause and confirm rather
than proceeding.
