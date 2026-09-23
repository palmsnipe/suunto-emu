# 794 — Sapporo 2.35 Main-Screen Settling And Button Navigation

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,761,789,793

## Goal

Attribute the ~2.4 s post-Done guest self-reset at `0xcdf5a`, keep main
alive or reproduce an authentic reset with evidence, and drive native
button navigation on the 2.35 main screen — the direct next step of the
"firmwares fully work with display and buttons" objective.

## Execution Budget

One implementation instance. Diagnostics in volatile workspaces with
explicit instruction/virtual-time budgets; each lane or observer finding
twice byte-identical before it backs code.

## Required Reading

`AGENTS.md`, `README.md`, `docs/current-status.md`, ticket 793 and its
Integrator acceptance, `docs/migration-evidence.md` entries
E-EMU-SAP235-COMPRESSED-001, E-EMU-SAP235-MAIN-TSC6A-001,
E-EMU-SAPPORO-BRANCH-GATES-001, E-SAP-0033, E-SAP-0037, and the tests
named below.

## Current Baseline

Ticket 793 (done): the compressed crosshair renders; common-command
setup-walk transcript `b2820edf…` shows settled step 25 / generation 3994 /
`crc32=6a446900`, then `machine-reset-request pc=0x000cdf5a
instructions=7554756551 virtual_time_ns=32447955715` and the
OHR-fixture refusal `stop=compat-refused pc=0x001be85a
instructions=8135889023 virtual_time_ns=36070752064` (exit 3). Pre-change,
the same PC was reached through the refusal→BusFault chain only.
Integrator decode (2026-09-23, from the pinned app listing): the 2.35
fault analyzer dump is at VA `0x1b0c08` (gate: HFSR bit31; parks tag
`0xFE0E8700` + HFSR/CFSR/MMFAR/BFAR + 8 frame words at `0x1005FFC0+8..`
before tail `b.w 0xcdf48`); the reset log's `lr=0xffffffed`/`xpsr=0x29000003`
fingerprint proves this run reaches the reset through the analyzer branch,
not through any of the eight direct policy-reboot callers of `0xcdf48`
(`a7fb6,a7fdc,ccf68,f50f2,f511a,f55c6,fc54a,10289a,12b106`). The residual
reset is therefore a second, as-yet-unnamed faulting access ~2.4 s after
Done, not authentic post-wizard behavior.

RESOLVED 2026-09-23 (probe pair `/tmp/sap233-fault/235_a.log`=`235_b.log`,
rc 3 both): at the reset `HFSR=0x40000000 CFSR=0x00008200
BFAR=0x400900ec` and the denied access is `write [0x400900ec] w=4
val=0x101437fc pc=0x000c1934 lr=0x000cb087 status=unsupported
text="nema_tsc6a: unsupported resolve state"` — a **second post-Done
TSC6A submit whose state tuple falls outside the accepted ticket-793
tuple**, refused by the fail-closed resolve path, converted by the guest
into the analyzer reset. The submit path is the ring-kick helper pair
(`movs r0,#0xec; bl #0xc1930` at `0xcb080`, `str r1,[r2,r0]` at `0xc1932`,
register `0x400900ec`), and the refusal text names
`src/display/nema_tsc6a_raster.c:265` (`tsc6a_resolve_state` accepted set)
— FINDINGS §6, `/tmp/sap233-fault/FINDINGS.md`, pair sha `861ec177…`, no
drift against the E-EMU-SAP235-COMPRESSED-001 pins. Later in the same run
a second refusal (IOM2 `0x40052120`, 59-byte OHR write) is logged with no
second reset; the run then stops at the known OHR boundary. Not authentic
behavior and not a settling
artifact: once that state's law is evidenced and supported (likely one of
the 57 aux-bit set format-17 assets — A3 territory), the reset should
disappear the way the crosshair refusal did. Next census: instrument the
resolve refusal to dump the full refused tuple (src format, stride, w/h,
size, matrix, clip, target geometry, colors) on this trajectory,
twice-identical.

## Allowed Files

`src/compat/`, `src/boards/`, `src/devices/`, `src/display/` (only if a new
evidenced refusal requires it), the corresponding `include/` headers,
`tests/unit/`, `tests/integration/`, `tools/` SDL live tests,
`docs/migration-evidence.md`, `docs/current-status.md`, and this ticket.

## Frozen Interfaces

Public headers, `Makefile`, `plans/index.tsv`, profiles, registries; the
ticket-793 acceptance tuple and refusal diagnostics; existing evidence
entries; the five-layer common command semantics (layers, budgets) except
where Section-3-style re-derivation is explicitly recorded.

## Evidence Inputs

E-EMU-SAP235-COMPRESSED-001 (boundary + runner Section 3),
E-EMU-SAP235-MAIN-TSC6A-001 (capture tuple, pre-change attribution),
E-EMU-SAPPORO-BRANCH-GATES-001 and ticket 787–789 (native SDL button path),
E-SAP-0033/E-SAP-0037 (manual-clock and scheduler context near `0xcdf5a`).

## Implementation

1. Observer/trace windows between the main-entry settled frame and the
   `0xcdf5a` reset (volatile, twice-reproduced): name the call chain into
   the reset and whether any bus refusal, unimplemented write, or missing
   fixture response precedes it on both the pre- and post-793 paths.
2. If a refusal or missing compat response is found: smallest evidenced
   behavior, evidence entry first or with the change, hit-bounded compat;
   no silent fixture-ceiling increase without a ledger note.
3. If the reset is authentic post-wizard behavior: record it, and pin the
   post-reset second-boot main-entry behavior instead.
4. Native post-settling button navigation (M/L/TAP) asserting observable
   settled-frame transitions only.

## Tests and Commands

New or extended SDL live test under `tools/` following
`test_sdl_sapporo_235_scroll.sh` conventions (skip without private
manifest; twice-identical transcripts; derived pins only);
`make test TEST_FILTER=…`, `make check`, `make check-lines`, `make
sanitize` if devices change, affected 2.35 era scripts, `check-sdl` plus
the 2.35 scroll/startup/snapshot gates unchanged or re-derived with
attribution.

## Acceptance

A ledger entry attributes the `0xcdf5a` reset (fault-path closure or
authentic-behavior observation, with twice-reproduced capture hashes);
the main screen demonstrably persists through at least one evidenced
boundary (settling or post-reset boot); a named button sequence produces
at least one observed navigation transition on the main screen; all
gates above green.

## Forbidden Scope

Other PXB2 format bytes, the A3 auxiliary-plane law, 2.33/2.39 work, GPS
deepening, snapshot codecs (792), product release gate (790), fixture
ceiling changes without evidence.

## Handoff

Report the reset attribution with capture hashes, changed files, exact
commands and results, derived navigation pins, and any integrator-owned
change. Do not claim main "works" beyond settled-frame evidence.
