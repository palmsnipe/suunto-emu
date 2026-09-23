# 795 — Sapporo 2.33.16 First-Fault Attribution And Boot Refusal Closure

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,761,762,764

## Goal

The 2.33.16 profile resets ~11.6 ms into boot at `0x000c97f2`
(E-SAP-0015). E-SAP-0050 proved the lane-side `0xc97f2` reset is the CMSIS
`NVIC_SystemReset` helper reached by two distinct branches: the
fsimage-VSF-footer policy reboot (only when the `0x14000000` aperture is
unbacked — not our tree, which already maps the resource) and the firmware
HardFault analyzer tail branch, whose register fingerprint matches the
in-tree reset exactly (`xPSR 0x29000003`, `LR 0xffffffe9`,
`R3 0x49000000` = stacked xPSR). Name the first faulting bus access of the
in-tree run and close the refusal it exposes, so 2.33 boot advances past
the E-SAP-0015 boundary.

## Evidence basis

E-SAP-0050 (footer law, analyzer parking tag `0xFE0E8700` + HFSR/CFSR/
MMFAR/BFAR + 8 frame words at RAM `0x1005FFC0`, literals `0x1ab0d8/
0x1ab0dc`; do-not-fabricate-footer note), E-SAP-0015 (in-tree reset
registers), E-SAP-0017 (MSPI/XIP context), E-SAP-0018 (2.39 PWRCTRL-family
write family — candidate but labeled hypothesis, not authority).

## Implementation

1. In-tree capture at the `0xc97f2` reset: read the analyzer region
   `0x1005FFC0..0x1005FFF8` (headless diagnostic or trace, volatile probe
   first) and decode HFSR/CFSR/BFAR/MMFAR to the exact faulting access and
   access type. Twice-reproduced, hashes in a new evidence entry.
2. Lane cross-check with the storage-staged 2.33.16 wrapper
   (`suunto-sapporo-storage.repl` + component-05 at `0x14000000` +
   production fixture at `0x14fff000`, the proven E-SAP-0050 mechanism) to
   confirm the same access behaves (warns vs. faults) under lane semantics.
   Note: in the staged lane the footer gate passes and no fault fires, so
   the faulting access is most plausibly absent-or-handled there; the
   cross-check localizes the divergence, it does not presuppose it.
3. Implement the named behavior in the owning device only after the entry
   exists. The in-tree register evidence names only the branch (HardFault
   analyzer tail), never the access: the lane's tolerated PWRCTRL-family
   writes (`0x40020000` offsets `0x58/0x60/0x78/0x80`, `[no-name]` `0x2C0`,
   offset `0x4`) and any unbacked XIP read are equally plausible
   candidates; bind the implementation to what the CFSR/BFAR actually
   names, not to the candidate list.
4. Re-pin only what the change moves; the 2.33 profile gate transcripts
   derive fresh boundaries with ledger attribution.

## Tests and Commands

Focused unit refusals/accepts for the named registers (success + refusal
case), `make test TEST_FILTER=…`, `make check`, `make sanitize` (device
protocol change), and a 2.33 firmware runner update with twice-derived
post-boundary transcripts; lane probes stay volatile with cited hashes.

## Out of Scope

Full 2.33 UI parity, 2.33.12 packaging differences (E-SAP-0050: identical
validator/analyzer), synthetic footers (forbidden), 2.35/2.39 work.

## Handoff

Report the decoded CFSR/BFAR fault, the new evidence entry id, changed
files, exact commands, and every re-derived pin with attribution.
