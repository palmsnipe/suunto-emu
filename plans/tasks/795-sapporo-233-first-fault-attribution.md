# 795 — Sapporo 2.33.16 First-Fault Attribution And Boot Refusal Closure

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,761,762,764

## Goal

Name the first faulting bus access of the in-tree 2.33.16 boot whose only
known symptom is the reset at `0x000c97f2` (E-SAP-0015), and close the
refusal it exposes so boot advances past that boundary.

## Execution Budget

One implementation instance. Lane probes read-only under
`../suunto-firmware`, volatile logs, explicit budgets, every finding twice
byte-identical before it backs code.

## Required Reading

`AGENTS.md`, `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`
entries E-SAP-0050, E-SAP-0015, E-SAP-0017, E-SAP-0018, E-SAP-0030, the
2.33 profile (`src/boards/sapporo_profile.c`), the fault-handling analysis
in `/tmp/sap233-reset/FINDINGS.md` (volatile, cited by hash in E-SAP-0050),
and the 2.33 tests named below.

## Current Baseline

The 2.33.16 profile resets ~11.6 ms into boot at `0x000c97f2` with
`xPSR 0x29000003`, `LR 0xffffffe9`, `R3 0x49000000` — the HardFault-analyzer
tail into CMSIS `NVIC_SystemReset`, not the lane's footer-policy branch
(E-SAP-0050). The lane with authentic storage staging boots past the footer
gate with zero warnings; the tree already backs `0x14000000`.

Integrator finding (2026-09-23, `src/cpu/armv7m/scb.c:376` + the pinned 2.35
analyzer at VA `0x1b0c08`): the tree's escalation sets HFSR bit30
(VECTTABLE per ARMv7-M; the snapshot invariant keys on the same bit) while
the analyzer's park gate tests bit31 (DEBUGEVT) with `bmi`, and ARM
escalation FORCED is bit1. The gate therefore skips parking in-tree, so
fault attribution must read the SCB/CFSR values (spec-correct FORCED bit,
BFSR precise bits, BFAR) directly; whether the escalation bit itself should
become bit1 is a guest-visible CPU-semantics question requiring its own
lane observation (probe binary forcing BusFault→HardFault and reading HFSR
via UART) — name it in the evidence entry, change it only if observed.

CAPTURED 2026-09-23 (probe pair `run_a.log`/`run_b.log`, byte-identical):
at the reset the SCB reads `HFSR=0x40000000 CFSR=0x00008200
BFAR=0x40054120` (PRECISERR+BFARVALID, and the parked region was in fact
present with tag `0xFE0E8700`, contradicting the skip-path reading — the
gate semantics need the lane ground truth). The faulting instruction at
`0x1438b2` is `str.w r1,[r5,#0x120]` — a **precise WRITE to IOM4 offset
`0x120`** (base selected `0x40054000` via the driver-instance field
`<<12`), not a read; the caller chain composes bits 8..11/13..15 (enum
values 3/6/0xc, `0xe000` variants) — an IOM4 CS/transaction-style register
write by the external-device driver (IOM4 hosts haptic + MAX17050 per
E-SAP-HAPTIC-001/E-SAP-MAX17050-001). Task 3 is now: lane census of the
IOM4 `0x120` write law (value semantics, response) and the matching
in-tree behavior in `src/soc/apollo4/iom*`/`sapporo_iom4_regs`; the PWRCTRL
hypothesis is retired by observation.

Reference located (read-only, `shasum -a 256` `b1d1dc8ee41ed84a…`):
`../suunto-firmware/emulator/renode/iom4/SapporoApollo4Iom4.cs` models
offset `0x120` as **`CommandOffset`** — the IOM4 DMA command register
(`PrepareDma`, DMACFG/TotalCount/TargetAddress/Status neighbors, OHR2
pressure-bus special case, command-size low nibble with read opcode,
CMDSTAT retire semantics). The in-tree 2.33 IOM4 register model lacks
`0x120` entirely; the closure is the lane-consistent DMA-command law
(plus its IRQ/CMDSTAT completion), not a bare store-accept.

## Allowed Files

`src/devices/`, `src/cpu/armv7m/`, `src/boards/`, `src/core/bus*` (only if
the named access lives there), corresponding `include/` headers,
`tests/unit/`, `tests/integration/`, `docs/migration-evidence.md`,
`docs/current-status.md`, and this ticket.

## Frozen Interfaces

Public headers, `Makefile`, `plans/index.tsv`, profiles, registries; the
lane tree and `tests/private/`; evidence entries; the interpreter's
fail-closed exception model; the footer gate (no synthetic footers, ever).

## Evidence Inputs

E-SAP-0050 (footer law, analyzer parking tag `0xFE0E8700` with
HFSR/CFSR/MMFAR/BFAR + 8 frame words at RAM `0x1005FFC0`, literals
`0x1ab0d8`/`0x1ab0dc`), E-SAP-0015 (in-tree reset registers), E-SAP-0017
(MSPI/XIP), E-SAP-0018 (2.39 write family — candidate only).

## Implementation

1. In-tree capture at the `0xc97f2` reset: read the analyzer region and
   decode HFSR/CFSR/BFAR/MMFAR to the exact faulting access; new ledger
   entry with twice-reproduced hashes.
2. Lane cross-check using the proven E-SAP-0050 storage staging to
   localize the divergence between warn-and-continue (lane) and fault
   (tree); no presupposition about which access it is.
3. Implement the named behavior in the owning device only after the entry
   exists, bound to what CFSR/BFAR names — the register evidence names the
   branch, not the access.
4. Re-derive only the moved 2.33 gate transcripts, with attribution.

## Tests and Commands

Focused register unit tests with success and refusal cases, `make test
TEST_FILTER=…`, `make check`, `make check-lines`, `make sanitize` (device
protocol change), updated 2.33 firmware runner with twice-derived
post-boundary transcripts.

## Acceptance

A ledger entry names the first faulting access (decoded CFSR/BFAR, twice
reproduced); the named behavior is implemented in the owning device with
success/refusal unit coverage; the 2.33.16 boot boundary demonstrably moves
past `0x000c97f2` with a re-derived pin; all listed commands green.

## Forbidden Scope

Full 2.33 UI parity, 2.33.12 packaging differences, synthetic footers, any
change to the interpreter's fault semantics beyond what the named access
requires, 2.35/2.39 work.

## Handoff

Report the decoded CFSR/BFAR fault, new evidence entry id, changed files,
exact commands and results, every re-derived pin with attribution, and any
integrator-owned change needed.
