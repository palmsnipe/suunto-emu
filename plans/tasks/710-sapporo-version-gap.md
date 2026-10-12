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

Instance 17 (2026-10-08, `sap233-cadence`, E-SAP-0053): the post-wake
cadence is named by a 20,000-slice census (twice byte-identical, from a 5 s
park snapshot). The 2.33.16 idle guest runs a 10-second tickless cycle —
~900k instructions of timekeeping at 0x000a4bdc-0x000a4c06 per wake, 1,516
exact 10.0 s spacings — plus a once-per-60 s two-part housekeeping burst
(~2.6M instructions), while the one-second RTC alarm service is a ~1.2k-
instruction residual and no frame publishes. No engine-seam gap is named
and no src change is authorized; whether the tick accelerates under UI
activity is the next instance's observation. The ticket rolls back to
`ready` for the next named later-Sapporo gap.

Instance 18 (2026-10-09, `sap233-display`, E-SAP-0054): the E-SAP-0053
cadence is re-anchored from a 1 s snapshot — first wake at 1.373 s, the
spacing distribution identical (the parked window was anchor-relative, not
a fixed state). Named gap: the 2.33.16 boot never publishes a frame —
zero frames across 4B instructions / ~36,600 s of guest time in both
censuses, zero GPU/refusal records, and zero display-controller traffic in
the staged 10 s lane run. The display gate (power sequence, UI-task
precondition, or missing input) is unnamed and is the next instance's
census; no change authorized. The ticket rolls back to `ready`.

Instance 19 (2026-10-09, `sap233-bootlog`, E-SAP-0055): the guest's own boot
log (written via MSPI2 TX DMA to 0x00FD0000-0x00FD07FF, 141 records decoded)
shows the 2.33.16 boot completing the fsimage stage (FSS crc/cu) and the
resource stage (Res markers at 0.567-0.568 s), then logging nothing for the
remaining ~59.4 s of a 60 s lane run - no UI stage, no display init, and no
0x400A0xx display access anywhere. The display gate sits in the boot flow
after the resource stage; the 2.35 analogue reaches its setup UI through
the same stages, so the divergence is version-specific. The next instance's
census is the boot flow after resources. No change authorized; the ticket
rolls back to `ready`.

Instance 20 (2026-10-09, `sap233-uifiles`, E-SAP-0056): the display gate is
decoded. The full boot-log records show the 2.33.16 guest requesting
settings/ui.txt and settings/uiv2.txt, both failing with result 3, the
ResourceProvider reporting 403 twice, and WbStoPreload failing 204/500 —
the UI resource stage fails before any display access, so the boot idles
forever (E-SAP-0053/0054/0055 consistent). The empty-cache compat layer
translates only the final WbStoPreload result, not the UI file lookups.
The next instance pins the smallest compat translation: whether a
provisioned 2.33 WbStorage carries those files, or how the 2.35 boot's
equivalent lookups succeed. No change authorized; the ticket rolls back
to `ready`.

Instance 21 (2026-10-09, `sap233-modecmp`, E-SAP-0057): the 2.35 lane boot
log through the same decoder isolates the divergence - the 2.35 staging
fails its own fsimage check, resets, OVERRIDES system mode with 2, and
boots in mode 2 (the E-SAP-0038 limited boot mode) which never attempts
UI files; the 2.33 boot passes fsimage (mode 5, asked 80h) and fails in
the mode-5 resource stage. The 2.35 lane boot also stops logging after
SFL failureMask:e at 1.759s with no display access - the lane never
reaches a display for either version; the 2.35 tree's setup UI comes from
the five-layer staging. The smallest 2.33 candidate is a production-data-
class layer for that profile (its own evidence instance + ticket). No
change authorized; the ticket rolls back to `ready`.

Instance 22 (2026-10-09, `sap233-uimanifest`, E-SAP-0058): the UI files are
provisioned-watch artifacts. The 2.33 resource partition's JSON manifest
DECLares settings/ui.txt (with personal/general/time) but carries no file
content - no ui.txt payload exists in the image; the 2.35 partition has the
same shape. The OTA staging cannot satisfy the UI resource stage; the 403
is the provider refusing the missing files. Candidate fixes (extend the
empty-cache compat to creation-mode opens, or supply via the production
fixture) each need their own instance and ticket under the compatibility
policy. No change authorized; the ticket rolls back to `ready`.

Instance 23 (2026-10-09, `sap233-uiseed`, E-SAP-0059): the seeding
experiment clears the E-SAP-0056 failures - pre-seeding ui.txt/uiv2.txt
through the compat layer's retention dict makes the lookups succeed and
both ResourceProvider 403 lines disappear - but reveals a SECOND display
gate: the boot reaches a new `wuiDu...` stage and never powers on or
writes the display controller in 60 s (zero 0x400A0xx traffic, zero
frames). The seed is a lane-side diagnostic with synthetic content; no
compat intervention is authorized. The next instance's census is the
wuiDu stage and the display power-on path. The ticket rolls back to
`ready`.

Instance 24 (2026-10-09, `sap233-assert`, E-SAP-0060): the second display
gate is an ASSERT - with the UI files seeded, the boot's UI resource
provider fails `wuiDump Assert ngsProvider.cpp:228` at 0.563s and begins
a `wui_dump.bin` dump that never completes; no display access or frame
occurs in 60s. Naming the line-228 condition is a bounded offline-RE
instance on the pristine 2.33 application (the assert call site for the
ngsProvider.cpp string). No compat intervention authorized until that
condition is named; the ticket rolls back to `ready`.

Instance 25 (2026-10-09, `sap233-assertfile`, E-SAP-0061): the assert
translation unit is SettingsProvider.cpp - the app has no standalone
ngsProvider.cpp string; the recorded text is exactly
"SettingsProvider.cpp"[5:] (the firmware renders the __FILE__ pointer
five bytes in). The assert formats Assert %s:%zu / Assert ..%s:%zu
compose the record through wuiDump %s; no word-aligned pool entry equals
the string VA, so the exact call site needs a data-flow pass. Naming the
line-228 condition remains the open offline-RE continuation. No change
authorized; the ticket rolls back to `ready`.

Instance 26 (2026-10-09, `sap233-callsite`, E-SAP-0062): the assert
call-site census is bounded but not closed. The record text is byte-exact
(wuiDump Assert ngsProvider.cpp:228); the app's only provider string is
SettingsProvider.cpp whose [5:] is exactly that text; a literal pool at
0x8c760 holds the pointer amid the storage-stage strings; and an app-wide
LDR-literal scan finds zero direct consumers - the pool is reached via a
base register, so the call site needs a constant-propagation pass over
the storage-stage functions (one bounded offline-RE instance). The
assert is in SettingsProvider.cpp:228. No change authorized; the ticket
rolls back to `ready`.

Instance 27 (2026-10-10, `sap233-assert-cond`, E-SAP-0063): the
line-228 assert condition is NAMED. The assert helper 0x70b20 decodes
the `adds r0, r0, #5` quirk (E-SAP-0061 confirmed in code); the site
is 0x8be24 (r1=0xe4, r0=r8 loaded from the 0x8c760 pool entry via a
far ldr.w — the E-SAP-0062 scan gap). The condition: after opening and
parsing settings/ui.txt, the provider requires a "wfn" record with a
"fuwe" field ("daily"/"sport" in the same chain) and asserts when
absent; the file-open failure path skips the assert entirely, which
explains why the assert appeared only after the E-SAP-0059 seed. The
candidate fix (schema-valid ui.txt seed) is NOT authorized: the
record's exact shape is the next bounded census and needs its own
instance + ticket. No change authorized; the ticket rolls back to
`ready`.

Instance 28 (2026-10-10, `sap233-schema`, E-SAP-0064): the record
schema is decoded - "<key>:<value>\r\n" lines (sscanf table), ui.txt
needs wfn (byte) + fuwe (char 'T'/'F'), uiv2.txt needs the full
0xac860 chain (18 keys + the daily list). Lane experiment: the
schema-valid ui.txt seed CLEARED the line-228 assert and advanced the
gate to the line-242 uiv2 validator (the provider deletes the seed
file). The uiv2 seed cannot work through the overlay: its validator
walks a parsed document object only the guest's own Settings flow
creates (the thin overlay handle lacks the parsed fields). Next
route: guest-authored uiv2.txt via the native Settings path (the
proven E-SAP-COMPAT-FILES-239-001 class) - its own evidence instance
and ticket. No change authorized; the ticket rolls back to `ready`.

Instance 29 (2026-10-10, `sap233-fuwe-clear`, E-SAP-0065): seeding
schema-valid ui.txt (wfn:00 + fuwe:T) CLEARS the line-228 assert on
the lane (twice byte-identical, no wuiDump at all). The guest deletes
the seed and opens uiv2 read-mode; the 242 validator is SKIPPED with
a flag (no boot-time creation - that is the Settings app's job). The
boot lands on the exact E-SAP-0056 record set: ResourceProvider 403s
for general/personal/CMGRPLST/ble-device and WbStoPreload 500, then
idle. The frontier is the provisioned-settings family (E-SAP-0058
class) plus the empty-cache hook's non-firing in this staging. Next
census: decode the general/personal schemas from the same reader
family and reconcile the hook. No change authorized; the ticket
rolls back to `ready`.

Instance 30 (2026-10-10, `sap233-hook`, E-SAP-0066): the stock
empty-cache layer's non-firing is root-caused - it pins the 2.33.12
app (hook 0x11978C, provider 0x1b9024) while the 2.33.16 registry is
(WbStoPreload, name ptr 0x1b8f44, id 0x1b1b08) at 0x113AA0 and the
failure path is the function at 0x119734 (client r0, status r3,
provider field = name pointer). A corrected v4 lane layer fires once
and the 2.33.16 boot advances past its former terminal idle through
the whole first-boot provider wave to a new sleepln gate. The
corrected layer stays a volatile artifact: the 2.33.16 empty-cache
translation is the integrator's ticket to scope (with the general
user-profile key table recorded; the guest writes general itself).
No change authorized; the ticket rolls back to `ready`.

Instance 31 (2026-10-10, `sap233-sleepgate`, E-SAP-0067): the
corrected-layer boot extended to 30 s terminates at the same
sleepln failure (guest 2.159 s; no UI stage, no Settings, no
further records for ~27.8 s). The terminal state is the first-boot
provider wave's completion condition: the sleepln translation is
gated on observing a native 17888-byte sleep.bin write that the
guest never creates in this staging. Next census: the 2.33.16
sleepln provider contract (same decode shape as WbStoPreload).
No change authorized; the ticket rolls back to `ready`.

Instance 32 (2026-10-10, `sap233-firstboot`, E-SAP-0068): the
0x119734 checker is SHARED - one SRAM client (0x10028f04) whose +12
name pointer rotates per module (78-entry probe), so translations
must be name-based; the sleepln status is already 500 at entry.
Iterative name-based layers cleared the entire first-boot wave
(sleepln/TrainingTss/PoiP-cmd1/actitmln - 6 translations) and the
boot reaches NEMA_INITIALIZED (GPU ring up) with normal post-boot
operation. Twice byte-identical. The v7 layer stays volatile; the
2.33.16 first-boot compat ticket is now fully evidenced (hit counts,
mechanism, next census = post-NEMA panel bring-up). No change
authorized; the ticket rolls back to `ready`.


Instance 33 (2026-10-11, `sap233-panel-size`, E-SAP-0069): the post-NEMA
lane gate is ActivityTimelineStore.cpp:368, caused by the logical-file
overlay omitting native size operation 0xbb204. The guest-written 247.bin
header is valid and its retained size is 46112, but the native length check
receives zero and leaves count 0xffffffff. A one-file diagnostic moves the
same assertion to ongoing.bin (152 bytes); the two-file diagnostic clears
both asserts and removes the need for the actitmln result translation.
At 8 s: PC 0xdbc0a, 118163913 instructions, no asserts/DebugMonitor records
or GPU command submissions; every experiment reproduced twice identically.
No production behavior or pin changed. Integrator must scope the 2.33.16
session-file size ABI, identity/budgets/refusals/snapshot policy and boot
staging before adopting the v7 translations. Next census: the post-load UI
scheduling/precondition and the concurrent bounded GPS startup timeout.
Ticket remains ready; implementation acceptance is not claimed.


Instance 34 (2026-10-11, `sap233-ui-gps`, E-SAP-0070): extending the
instance-33 corrected-size staging to 11.775 s reaches the third native GPS
timeout and `CXD5610GF-driver.cpp:889` (call 0x11dbcc, tick 11769858500).
State 5 increments retry byte 0x1005b8f8 from 0 to 3; no GPS UART TX/RX or
awake pulse occurs. Control and read-only observer pairs are byte-identical;
final PC/LR/SP 0x7890c/0x11dbd1/0x10033050, 143209642 instructions,
CFSR/HFSR zero. A native UI census point runs 23 times from 2.4951565 s,
but GPU submissions/frame generation stay zero. The 8 s checkpoint follows
WFI and does not establish a scheduler defect; GPS's causal relation to the
no-draw condition is unproven. Production and pins unchanged; session-file/
boot integration request from instance 33 remains. Next separate GPS census:
2.33.16 initial UART callback/arming boundary and status-line recognizer,
with successful and wrong-prefix/state controls before fixture integration.
Ticket remains ready; no production implementation acceptance is claimed.


Instance 35 (2026-10-11, `sap233-gps-startup`, E-SAP-0071): native
initial-arm PC 0x11d990, callback 0x11d593, state/pending 4/2, retry zero
and the $PSS consumer are derived. Five probe pairs reproduce identically.
Two delayed synthetic status lines produce six @VER TX bytes, twenty RX
bytes and states 2/14/15/18; at 11.775 s PC 0xdbc0a, 130924409
instructions, no assertions/refusals. Wrong prefix causes native rejection;
its causal control repeats line-889 assertion, bounded control refuses the
first retry, and premature state 0 refuses before RX. The new version-specific
component src/compat/sapporo_233_gps.{c,h} and its four-case unit suite pass
(37 atomic refusal configurations); make check 1051, sanitize 1046,
lines/contracts171 and existing paired 2.35 GPS firmware gate pass.
No registry/profile/public header/Makefile/snapshot/golden or index change.
Machine layer registration/device binding/reset/refusal and snapshot policy
need integrator-owned attachment; authentic 2.33 advancement is not claimed.
E-SAP-0069 boot/session-file prerequisite remains separate. Pending-7 reopen
is observed but not supplied; GPS success still emits no GPU submissions.
Ticket remains ready; full implementation acceptance is outstanding.
