# 710 — One Later Sapporo Observed Gap

**Status:** ready
**Phase:** 7
**Dependencies:** 705

## Goal

Implement one observed later-Sapporo hardware/storage gap or one justified,
hash-pinned compatibility intervention.

## Execution Budget

One to two model-days per gap; one module and focused tests.

## Required Reading

Selected profile handoff, exact failing transcript/evidence entry, relevant
hardware/storage interface, and compatibility policy when applicable.

## Current Baseline

The selected profile stops fail-closed at one recorded behavior. No cache or
logical-storage layer is assumed to be required.

## Allowed Files

One new version-specific device/storage/compat module, its tests and fixture
provenance, plus integration attachment reserved by the selected profile ticket.

## Frozen Interfaces

Prefer real lower-level behavior. A layer must pin exact component hashes,
trigger/state/effect, provenance, and hit count; otherwise refuse.

## Evidence Inputs

Byte-exact transcript or recovered record entry from ticket 700/705. Missing or
ambiguous evidence blocks implementation.

## Implementation

Add one behavior, positive and refusal tests, and rerun until the next distinct
stop. Open a new instance of this ticket for each later gap.

## Tests and Commands

`make test TEST_FILTER=<selected_gap>` selects the focused test and exits 0;
wrong version/state/size/command refuses. Bounded `make test-firmware
FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=sapporo-2.35.34` advances to the
declared next checkpoint twice identically. Run
`make check-lines && make check && make sanitize`.

## Acceptance

Only the evidenced behavior changes, old/new profile regressions pass, and the
new stop is recorded without fallback widening.

## Forbidden Scope

No combined gaps, wildcard layers, guessed cache/storage state, or golden edits.

## Handoff

Report gap ID, implementation class, evidence, exact tests, layer hits if any,
next stop, and whether another instance is required.

## Blocked-state audit note (2026-09-11, no-device constraint session)

Verified on disk (read-only audit; hashes spot-checked): the later-Sapporo
material is present — private bundle `tests/private/sapporo-2.35.34.18929` and
`fixtures/evidence/sapporo/sapporo-2.35.contract.semu`; the audit found traces
blocked only for lack of a profile, and E-SAP-0017's un-recovered state is a
deeper-trace/disassembly job against those files, not a missing observation.
The stated blocker is therefore partially stale (class F + stale; dependency
705 is done). What remains — identifying one exact failing
transaction/record — is read-only RE derivation on files already on disk.
Status is left unchanged for integrator review.

Integrator decision (2026-09-15, delegated): status set `ready` — the
remaining work is the offline RE derivation named above, which needs no
physical device; the first instance must record its derivation as an evidence
entry before any behavior change.

Instance 12 (2026-09-23, `sap235-710-ohr-main`, integrator-reproduced):
E-SAP-0041-EXT landed — the 2.35 OHR startup fixture grew from eight to
thirteen ordered MAIN-state responses (post-Done queries seq 8..12,
lane byte-pinned 14/14 shim verification + twice-identical lane replay
`4dff3a4e…`/`4d6309fc…`; tree pair `245cab82…`). Step 25 SETTLES
(3998/1394c638). REMAINING named gaps for the next instance: (1) command
0x0004 sequence 13 — outside the pinned command set; the lane's zero-
body answer `b72d3ede…` is already pre-captured twice; admission needs a
decision on extending the command census beyond the startup trace;
(2) the terminal `unmapped-access pc=0x001023b0` / `memory address
overflow at 0x10025298` — the address lies INSIDE the pinned SRAM
aperture, so investigate the CPU address-arithmetic/exception-frame path
first (suspected tree defect class: every bug fix starts red-test-first);
(3) after (1)/(2): button navigation acceptance (ticket 794's remaining
scope) across the settling main screen.

Slice 2 (2026-09-23, same instance): command 0x0004/seq 13 admitted by
integrator ruling (lane byte-pinned, MAIN-only, 14th response); the walk
settles steps 24/25 with zero refusals/resets and stops at the CPU
unsupported-instruction wall pc=0x00072f52 (e1bb8c48) — next boundaries
are the 794 CPU family and the GPU law family (788/793/794), not further
OHR admissions; the ticket rolls back to `ready` for the next named
later-Sapporo gap. No fixture growth without a new ruling.

Slice 3+4 (2026-09-23, same instance): slice 3 measured the refused
0x0002/seq14 as a PERIODIC ~955 ms poll (lane 19/19 byte-identical
answers; enumerated growth structurally wrong) and stopped without
implementing, per protocol. Slice 4 implemented the integrator's B'
ruling (E-SAP-0041-EXT3): pinned-14 prefix untouched, MAIN-only
law-governed 0x0002 poll tail (default-ff shape, zero body, cap 16,
logged), full refusal matrix — setup-walk now ends at the scripted
stop=user (360c325a, twice) with the main screen settling steps 24-31.
OHR boundary class CLOSED for this window; ticket stays `ready` for the
next named later-Sapporo gap (GPU law family and CPU census now belong
to 794/788/793).

Instance 13 (2026-09-30, `sap235-710-vscroll`, ticket 710, E-SAP-0041-EXT7):
the E-SAP-0041-EXT6 named residual — the distinct vertically-scrolling
draw family of the 2.35 upper navigation window — is closed. Census
twice byte-identical on HEAD a481cef (pre-admission log = the pinned
upper golden be0e2e1c…): exactly 4 refused draws, same 60x60 fmt-17
asset, mm12 = -dstY (-151.0000153/-151/192/220), clip-cut heights
11/49/48/20. Admission via the composer band law (RE 0xc1b5e set-matrix
API, MM12 = -dstY, quad = strip∩clip): mm12 moves from the bit-pinned
pair to v(y0) >= -1 / v(y1) <= 60*65536+1 in 16.16 units with the
one-ULP bias class admitted exactly; height 1..60 requires a clip-cut
end. Post-admission: zero draw-refused, guest stop bytes unchanged,
navigation frame crc 9b554fd9 unchanged, step-28 generation 4345 -> 4349
(host-side), upper golden re-pinned to 20aecbc7… twice. Red-first unit
coverage 12/12 + 7/7 (one superseded near-miss re-pinned with
justification: one ULP at 90 is half a 16.16 unit). The ticket rolls
back to `ready` for the next named later-Sapporo gap; the vertical
family's remaining unknowns (emitting guest function, scroll-animation
values) are recorded as observed-not-derived in the evidence entry.

Instance 14 (2026-09-30, `sap235-710-ticktrail`, ticket 710,
E-EMU-SAP235-TICKTRAIL-001/002): the interactive-defect residual named
by 001 — the 2.35 main-screen seconds-sweep trail — is closed by the
per-resolve frame lifecycle (source-side application of the pinned
2.22 per-frame-fresh ruling; 001's blocked-fix theory is dead, the
span is byte-stable, sha 373b538e…, twice): the fmt-17 shadow's resting
state is the decode of the guest span through the 788 block law, each
frame begins at the first fmt-17 draw, and each resolve returns the
shadow to the resting state. Undecodable aux-bit blocks (5,441 of
14,400, all outside every pinned resolve region) keep the pre-fix
transparent-black content. Post-fix census: sweep-region pixels hold
at 131/129/140/120 across ticks (pre-fix 152 -> 278 -> 380 -> 477
growing); guest stop lines byte-identical. Goldens re-pinned with
dated notes: nav (three transcripts + steps 25/27/31), compressed
main entry (steps 25/27/29/31 + transcript), restore (snapshot
5f21f7d1…, first frame 500b350f), and the 2.39 general_budget
terminal-image pin bb17b7a8… -> 24d5a4dd… (probe + runner; serialized
shadow moved to the resting state, no 2.39 frame/stop/transcript
moved; the only 2.39 census red). Verification: 2.35 firmware sweep
10/10 with every 2.35 SDL/integration gate green, 2.39 era census
re-run green at the final HEAD.
Remaining unknowns: the aux-bit codec population (never read by a
pinned resolve) and the [resolve, strokes, resolve] RMW-writeback
shape (no pinned window uses it) are recorded as observed-not-derived
in E-EMU-SAP235-TICKTRAIL-002. The ticket rolls back to `ready` for
the next named later-Sapporo gap.


Instance 15 (2026-10-07, `sap235-710-hrate`, E-SAP-0051): the E-SAP-0049
named frontier — the post-55th-admission high-rate region at pc
0x000ccac4 — is closed as a census. The 26B/400s frontier was reproduced
twice byte-identically; a 3,500-slice x 2M-instruction resume census from a
1-ns-past-the-55th-hit prefix (both passes byte-identical) shows a persistent
guest CPU-bound spin at the engine's 1 ns/instruction law: an ordered
deadline-list insert at 0xbdce4 (26.8%), the tick seqlock/timekeeping family
(a66e6/a6540/ccaac/ccac4), and a rolling mean at 0x1b9cce, settling into a
slice-exact 866 ms two-phase cycle (512M + 354M instructions). Zero resets,
refusals, layer hits or frames in the window; the 64-admission awake budget
is untouched. No engine-seam gap is named and no src change is authorized;
the deadline-list growth intent is recorded as observed-not-derived. The
ticket rolls back to `ready` for the next named later-Sapporo gap.

Instance 16 (2026-10-08, `sap233-rtc`, E-SAP-0052): the 2.33.16 boot park is
RTC-waked. The lane census (full E-SAP-0050 staging, upstream RTC
unregistered, logging peripheral at 0x40004800, twice byte-identical)
captured the exact E-SAP-0032 13-transaction arm block, so
`semu_apollo4_select_profile` now takes the live block for
`sapporo-2.33.16` (IOM4 stays 2.35-only; pressure235 unchanged). The inert
RTC-test exemplar moves to sapporo-2.39.20. Result twice byte-identical:
1.2B/310s ends at the same park PC with the guest alive on the one-second
alarm cadence, zero resets/refusals/trace records; the instruction curve
(1s 83.7M ... 30s 89.2M, ~185k/s periodic) records the service cadence and
the parked 2-5 s window as the next instance's census. No other profile's
stub or pin moved; check/check-lines/sanitize/check-era green.
