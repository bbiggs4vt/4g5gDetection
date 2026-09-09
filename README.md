# Cellular Cell-Search Library (LTE + 5G NR, MIB-level)

A reusable library that ingests a **generic IQ stream** and detects the presence of
an LTE or 5G NR cell on that channel, decoding down to **MIB level** — enough for
presence detection and cell fingerprinting, not full L3.

> **Open design decision — input model.** This spec is written for the
> **per-channel probe** model: the core takes a fixed-`Fc` IQ stream for one
> channel's bandwidth and fully analyzes it. Continuous multi-frequency **sweep**
> is treated as an *optional outer orchestration layer* that retunes an SDR and
> feeds the core one channel at a time. If the intended primary use is a
> continuous sweeper on a single wideband stream, flip this: make the ring
> buffer/flow-control the core concern and the per-channel analysis a callee.
> Decide before the framing layer is written.

---

## Scope

### In scope
- **RATs:** LTE (FDD + TDD) and 5G NR.
- **Depth:** cell search → PBCH/**MIB** decode only.
- **Function:** detect whether an LTE or NR cell is present on the channel, and
  emit a normalized fingerprint record.
- **Input:** a generic complex-IQ stream described by `(Fs, Fc)` metadata. No
  direct SDR/driver coupling in the core — files, sockets, and live radios are
  all "just a stream."

### Explicitly out of scope (do not implement)
- **No PDCCH / PDSCH decode.** No blind DCI search.
- **No SIB1 or any SIB.** (This means no full cell identity, PLMN, or TAC — see
  "What MIB gives you" below.)
- **No ciphered / dedicated signaling, no user plane.**
- **No SDR driver code in the core** (no UHD/SoapySDR/UHD calls inside the
  decode library). Any radio glue lives in a separate optional layer.

Stopping at MIB is the deliberate scoping that makes this a days-to-weeks glue
project rather than a months-long stack. Keep it there.

---

## What MIB level gives you

Because we stop at MIB, the "identity" is a **fingerprint**, not the globally
unique cell ID (that lives in SIB1, which is out of scope).

**LTE (after PSS/SSS + PBCH):**
- PCI (0–503)
- system bandwidth
- PHICH configuration
- SFN
- CP type
- antenna port count

**NR (after SSB search + PBCH/MIB):**
- PCI (0–1007)
- SSB index (→ beam, in multi-beam deployments)
- common subcarrier spacing (SCS)
- SFN
- `pdcch-ConfigSIB1` (a pointer to CORESET#0 — captured but **not followed**)

So the emitted fingerprint is roughly:
`(RAT, Fc, PCI, [SSB index for NR], bandwidth/SCS, SFN)`.

---

## Dependencies

- **srsRAN** is the PHY engine for **both** RATs. Reuse its sync and PBCH/MIB
  blocks; **do not reimplement DSP**. srsRAN is the single dependency that spans
  LTE and NR, which is the whole reason the two branches share a codebase.
  - LTE: `srsran_ue_sync` (PSS/SSS timing/CFO) and `srsran_ue_mib` (PBCH/MIB).
  - NR: the NR SSB search + `srsran_ue_sync_nr` path and the NR PBCH/MIB decode.
  - These operate on **sample buffers**, not radio handles — feed them from the
    ring buffer directly.

---

## Pipeline

```
IQ stream (Fs, Fc)
      │
      ▼
  ring buffer  ──►  [optional NCO freq-shift]  ──►  decimate/resample to sync rate
                                                          │
                             ┌────────────────────────────┴───────────────────────┐
                             ▼                                                      ▼
                   LTE branch                                              NR branch
             PSS/SSS  → PBCH/MIB                                   SSB search → PBCH/MIB
                             └────────────────────────────┬───────────────────────┘
                                                           ▼
                                          normalized cell record (see schema)
```

### Blocks the core owns (that an SDR abstraction would otherwise hide)
1. **Resampling / decimation.** Incoming stream is at the channel BW's rate
   (e.g. 30.72 Msps for 20 MHz LTE). Cell search only needs the band center —
   ~1.92 Msps for LTE (central 6 RBs / PSS-SSS region), a narrow SSB region for
   NR (rate depends on SCS). Decimate to the sync rate before the correlators.
2. **Framing.** Buffer enough samples for one sync attempt: PSS/SSS correlation
   plus a few PBCH periods — tens of ms for LTE, ~one SSB burst period (~20 ms)
   for NR. Ring buffer in, fixed-size analysis frames out.
3. **NCO frequency shift (needed for off-center targets).** If `Fc` is not the
   cell center, mix the target sync region to baseband *before* decimating.
   **NR routinely needs this** even on a "centered" carrier, because the SSB sits
   at a GSCN raster point that can be offset from `Fc`. LTE's PSS/SSS *is* at
   carrier center, so LTE usually skips this. This is the one place NR asks more
   of the core than LTE.

### Dispatcher
Run an LTE cell-search attempt and an NR SSB-search attempt over the framed
samples. Each RAT has a distinct sync-signal structure, so "detect either" falls
out of trying both correlators and reporting whichever locks. Normalize the
result regardless of which branch fired.

---

## Normalized cell record (target schema)

```
CellRecord {
    rat            : enum { LTE, NR }
    fc_hz          : double        // center freq of the analyzed stream
    pci            : uint16        // 0–503 (LTE) / 0–1007 (NR)
    ssb_index      : optional<uint8>   // NR only
    bandwidth_scs  : variant       // LTE system BW | NR common SCS
    sfn            : uint16
    // LTE-only extras
    phich_config   : optional<...>
    cp_type        : optional<enum>
    n_ports        : optional<uint8>
    // NR-only extras
    pdcch_config_sib1 : optional<uint8>   // captured, NOT followed
    // metadata
    timestamp      : ...
    snr_or_metric  : optional<float>      // lock confidence, if available
}
```

Both branches flatten into this one struct.

---

## Practical gotchas (carry these into implementation)
- **LTE sync rate is cheap:** 1.92 Msps regardless of the cell's real bandwidth.
- **NR needs the right GSCN + SCS hypothesis;** srsRAN's NR search iterates SCS
  candidates. Use the GSCN sync raster to limit where SSBs can be — big speedup
  vs. scanning continuously (relevant to the sweep layer).
- **Retune settling** (sweep layer): let the LO settle after each tune or you get
  phantom misses. This is the classic live-scanner bug.
- **Frequency offset:** srsRAN estimates/corrects CFO, but a TCXO/GPSDO radio
  makes NR much more reliable. NR's higher SCS is more offset-tolerant than LTE.
- **TDD vs FDD** doesn't matter for presence/MIB — sync doesn't care. (It would
  only matter if we chased PDCCH, which we don't.)

---

## Suggested build order
1. **Skeleton:** IQ stream input interface + `CellRecord` struct; stub LTE/NR
   branches and dispatcher; build against srsRAN.
2. **Framing layer:** ring buffer + resampler (+ NCO hook).
3. **LTE branch end-to-end** against a recorded IQ file — verify it *locks*, not
   just compiles.
4. **NR branch** — adds SSB-offset/GSCN handling on top of the same shape.
5. (Optional) **Sweep orchestration layer** + SDR glue, kept out of the core.

## Testing
Files are just streams — develop and regression-test the core against known
LTE/NR captures under `captures/`. "It compiles" is not the bar; "it locks onto
the cell in the capture and emits the expected PCI/SFN" is.
