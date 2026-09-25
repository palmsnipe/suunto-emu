# 794 — Sapporo 2.35 Main-Screen Settling And Button Navigation

**Status:** in-progress
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


Lane-oracle census (2026-09-23, `sap235-lane-seconddraw`, FINDINGS sha
`19bfd86031cdef3d9e4f19c7dc49480b35e4ed83673da9d48398121db05e4c88`, every
finding twice byte-identical): the lane NemaGFX model is a tuple
whitelist that never decodes format-17 (its only 0x17 predicates demand
a fixed 480x480 uncompressed shadow), so a lane replay yields
accept/refuse plus the printed draw-state tuple only — never pixels; the
aux-plane usage rule stays UNRESOLVED (third-colour reading as leading
hypothesis, E-RE-SAP235-RESOURCES-INDEX-001). Refusal ladder (hashed to
model lines): REFUSED_AT_PARSE (child word-pair prefix/register
whitelist miss; emits `NEMA_RENDER_REFUSED … command=0x…` with NO
`reason=` and NO draw-state row), REFUSED_AT_TUPLE (`NEMA_DRAW_STATE` +
`reason=unknown-state`), ACCEPTED (`NEMA_CHILD_RENDERED` + completion
irq=1), plus SILENT-DROP when the previous and new ring stops are not
both inside [CMDADDR, CMDADDR+CMDSIZE) (control field 6 = bootstrap
sentinel). The live in-lane 2.35 walk is a CLOSED route: the authentic
boot stalls at assertion `LogbookEntryDb.cpp:53` (BKPT `0x00079424`,
LR `0x000d4809`, re-derives E-SAP-0042) because the lane's MSPI2 lacks
the 64 KiB DC erase; lane-tree edits to fix that are DECLINED as oracle
contamination. The refusal-to-BusFault-to-reset chain is a tree contract
choice, proven CPU-invisible in the lane (census `c575c2dce1b15664…`):
while the state is unsupported, the `0xcdf5a` reset is NOT authentic and
fail-closed stays. Step-2 census capture requirements (replay pipeline
validated, self-test reproduces the accepted submission's pinned census
`407f4d8c…`): CMDADDR/CMDSIZE at the refused kick; ring bytes across the
command span (previous-stop to new-stop inclusive plus base); the
previous accepted CMDRINGSTOP value; the refused kick value and its
low-3-bit control field (must be 4); raw child words at each child
address; completion-marker presence; child TEX1 base/format/stride/
resolution words. Artifacts go to `/tmp/sap235-794census/` (volatile)
with shas.


Ring-span census (2026-09-23, tree-side instance, `/tmp/sap235-794census/`,
pair `f18b2ad5…` twice + pre-regen pair = 4 identical runs, clean-build
provenance chain, pristine rebuild re-reproducing the pinned `b2820edf…`
transcript and reset tuple uninstrumented): the refused kick (ord 7989 of
7989, ctrl=4) executes ring words 92..95 of CMDADDR `0x10143678`
(CMDSIZE `0x400`, previous accepted stop `0x101437EC`) = ONE child-list
push, child `0x100d2800` 250 words (raw `child_100d2800.bin`
`5cd9e2cb…`), NO completion marker in the span. All 125 child pairs are
prefix-clean, 4-aligned, INSIDE the 34-register lane whitelist — lane
verdict is REFUSED_AT_TUPLE, not parse refusal. The child is FIVE DRAW=5
quads over A2LE glyph masks (fmt 0x28, SRAM runtime glyph buffers — NO
content match in the resources partition, NO PXB2 header, ZERO fmt-0x17
textures) plus ONE DRAW=2 TSC6A resolve whose quad coordinates
(`0x005a00ab`..`0x009600e7` = (171,90)-(231,150)) equal the accepted 793
crosshair quad: the post-Done work is a text-glyph pass + a framebuffer
re-compression pass, NOT compressed-asset staging. HONEST NEGATIVE: the
A3/aux-asset hypothesis is FALSE for this tuple (0-of-0 drawn blocks carry
aux bits); A3 stays open and non-blocking. Both models refuse the same
DRAW=2 tuple family; the tree's conversion of that refusal into a
BusFault-on-the-kick-store is contradicted by the lane Q4 proof (guest
store of the same value: CFSR/HFSR zero, core advances, irq=0).
INTEGRATOR RULING (evidence-backed, this round): ring-kick execution
failures become GPU-side only — the 0xEC write commits, the refused child
renders nothing, schedules no completion IRQ, and logs one deterministic
`gpu/draw-refused` line; true argument/range errors still refuse at the
write. Fail-closed is preserved at the DRAWING layer (no pixel is invented
for unsupported states); the CPU-visible fault was the unsupported
behavior, not the fail-closed law. Implementation instance dispatched;
the reset tuple `pc=0x000cdf5a` and the 793 setup-walk pins become
re-derivation candidates for the integrator after review.


Lane replay of the authentic refused submit (2026-09-23, FINDINGS §9,
sha `b792cc8f…`, pair `417c2983…` byte-identical): the 250-word child is
fully whitelist-parseable; ladder verdict REFUSED_AT_TUPLE with exactly
ONE `NEMA_DRAW_STATE` row (DRAW=2 resolve, offset 48/word 12,
start `0x005A00AB` end `0x009600E7`) — the model aborts the child at the
first refusal so the five A2LE quads never print; zero pixel writes
in-machine provenance (span hashes `de2f2560…`/`17ff4b59…` before==after,
covering the crosshair destination and TSC6A bases); NO completion row
(marker scan only over the executed span — push-with-no-marker faithful);
CFSR/HFSR/ISPR0 all zero again — the authentic refused submit is
CPU-invisible in the lane. RECIPE FIX for all future replay specs: the
register block must end with `0x400900FC = 0` or the model records but
never executes (found by zero-edit verbatim replay, NO_SUBMISSION_OBSERVED
`72bfbad9…`, honestly reported first). Limit (§9.4): the lane walker does
NOT apply ring-inline register writes, so no lane run can pin the
authentic inherited resolve state — the refusal FORM is proven, the state
authority is the tree's own refusal site. Offline twice-derived quad
census (`c494b014…`): five A2LE quads, code `0x941E8000`, inst
`0/0x004E0002/0x804B1286`, clip `0x00510000`/`0x00A200F0`, no fmt-0x17
pointer anywhere. Control skipped as not representable from captured
bytes (three named blockers; no fabricated renders).


Implementation landed (2026-09-23, commit `655e3cd`, E-EMU-SAP235-
RINGKICK-CPU-INVISIBLE-001): refused ring-kick draw states are now
GPU-side only; the setup-walk shows ZERO resets, two `gpu/draw-refused`
lines, and terminates at the E-SAP-0041 OHR-fixture ceiling
(MAIN-state sequence 8, `pc=0x001be85a instructions=7572236241 vt=
32551364960`, transcript `344973205de1…`, integrator-reproduced on a
fresh build; compressed era runner re-pinned green twice). REMAINING
scope for this ticket (still in-progress): (1) OHR-fixture budget/
evidence for post-Done MAIN-state transactions (the terminal refusal);
(2) the DRAW=2 resolve-law extension — witness detail recorded (accent
`0xff55aaff` fails the predicate, IMEM triple matches), pixels lane-
unobservable, requires an owner-decision evidence route; (3) button
navigation acceptance across the settled main screen once (1) opens;
(4) parse-level refusal CPU-invisibility known-divergence recorded in
the ledger, deliberately out of this change's scope.

Resolve-law extension landed (2026-09-23, integrator, E-SAP-0041-EXT4):
the 124 per-window resolve refusals are one law family — every line
carries tex/draw color `0xff55aaff`, firmware-native per the twice-
reconciled offline-RE census (theme/style table, four VAs in the pinned
application) and the SOLE failing predicate of the tuple. RED-first unit
case `test_resolve_accepts_firmware_native_accent`, then the masked
accent `0x0055aaff` admitted in `tsc6a_resolve_state`. Window effect
(twice byte-identical, transcript `a4a04c53…`): resolve refusals 124→0,
draw-refused 227→103 (exactly the 788 compressed-source family), the
accent-tinted 60x60 blit at clip (171,90)-(231,150) rasters into the
resolve targets and composites — settled frames change from step 25
(new crcs `74e8d4f5`/`a43f1010`/`ddbafcf2`/`636e9f75` alternating with
`7ef957e9`; step 24 `3991/1c1f9064` unchanged).
Instructions and virtual time `9487528672`/`37414100700` are unchanged —
pure display gain with zero CPU-timeline perturbation. Compressed
Section 3 re-derived to the EXT4 state (twice, integrator); `make
check`, `make sanitize` green; ledger entry E-SAP-0041-EXT4 records the
census, tooling, and scope. Remaining for acceptance: (3) button
navigation acceptance across the settled main screen — slice in flight
(nav tool per E-SAP-BUTTONS-001 golden policy); (4) parse-level refusal
CPU-invisibility known-divergence recorded in the ledger, deliberately
out of scope.

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
