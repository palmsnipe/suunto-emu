# 777 — Sapporo Opt-in Era Gate Re-derivation

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729

## Goal

Restore every Sapporo 2.39 opt-in era script under `tests/integration/` to a
green, re-derived state against the current engine, and add a `make check-era`
aggregation target so era-gate drift can no longer accumulate silently between
`TEST_PROFILE`-selected runs.

## Execution Budget

Two to three model-days; six era scripts plus one Makefile aggregation target
and its documentation. No new source modules.

## Required Reading

E-ULS-0041 (drift attribution and bisect table), ticket 729/728 handoffs,
`tests/integration/test_firmware_sapporo_239_file_seek.sh`,
`test_firmware_sapporo_239_file_size.sh`,
`test_firmware_sapporo_239_ohr2_command2.sh`,
`test_firmware_sapporo_239_ctimer13_inten.sh`,
`test_firmware_sapporo_239_logical_files.sh`,
`test_firmware_sapporo_239_wbsto_cache.sh`, and `docs/execution-model.md`
era-gate sections.

## Current Baseline

`make check` is green (902 PASS, 2 SKIP, diff-stable), but the six era scripts
above fail their artifact-hash pins independently of the E-ULS-0041 adoption:
byte-identical first failures occur on the pre-adoption halt-semantics binary
and on commits `963bdee`/`c7800be`/`59d6168`. Bisect attribution: the probe
cold run reproduces its pin exactly at `cd6b67d` (`stop=budget
pc=0x00079e1c`, log hash `4f8e749e…`); the drift batch is `d311da0..6555d38`
(`710f82a8…`, sentinel `0x000a7dda`); the current artifact hash is stable from
`963bdee` onward (`c5376d7e…`). `file_size.sh` is structurally stale since
2026-09-05: `cd6b67d` moved the guest sentinel from instruction 405895302 to
416256852 but re-pinned only `file_seek.sh`. The era scripts are opt-in and
outside `make check`, which is how the drift stayed invisible.

## Allowed Files

The six `tests/integration/test_firmware_sapporo_239_*.sh` scripts named above,
their referenced expected-artifact fixtures, the `Makefile` `check-era` target
and its `.PHONY`/help lines, `docs/execution-model.md`, and
`docs/migration-evidence.md`.

## Frozen Interfaces

The era runner contract is unchanged: opt-in behind `TEST_PROFILE=sapporo-2.39.20`
plus `SEMU_SAPPORO_239_FULL_FLASH` (hash `37134845…c4cb`) and
`SEMU_FIRMWARE_MANIFEST`. Script exit semantics unchanged (0 pass, 3 budget).
No engine behavior may change to make a pin pass; pins move only to recorded
observations.

## Evidence Inputs

E-ULS-0041 drift attribution (lane-independent failure reproduction on the old
halt binary; bisect log hashes above); the retained era-run logs of the
E-ULS-0041 session; E-ULS-0040/E-ULS-0041 rows of
`docs/migration-evidence.md`. Re-derived pins must cite the fresh two-run
evidence recorded during this ticket.

Full-set audit 2026-09-23 (read-only instance, HEAD `ab791ea`, report
`/tmp/sap239-era/REPORT.md`, 43 scripts x 2 serial rounds, emulator
`a765d3bd…6ff45` sha-stable across the run): the synthetic full-flash fixture
is REPRODUCIBLE from read-only inputs per this ticket's recovered recipe
(component-05 `49a3936f…` FF-padded to 16 MiB, lane
`tools/build_production_data_fixture.py` manufacturing sector `c0881606…`,
patched at `0x00FFF000`, FF upper half) and hashes exactly to the script pin
`37134845…c4cb`. Census: 12 PASS-pin-held / 25 new-pin candidates (shared
deltas: cold log `47e8aaa7…` for `ctimer13_inten`/`ongoing`/`quiet_read`/
`zip_read`; Δvt -1,556,396 ns at unchanged instruction counts across the
`0x0014e8ea` cap family; `activity_budget`+`widgets` share
`stop=compat-refused pc=0x000920b4@442856246/2176971322`) / 0 unexplained
failures / 6 environment-blocked (`gps_awake`/`gps_reopen`/`gps_startup`/
`gps_five`/`general_budget`/`personal_budget` — stale inspector binaries and
`libsemu.a` coupling under a concurrent build owner; need a quiesced-tree
re-run, not re-pins). One green-to-red flip vs the E-SAP-0045 census:
`timer_pattern` (same park PC `0x000e955a`, -62 instructions, -62 ns) —
attribution open (candidates `cd1de52`/`2f22137`/`d8bfba9`), requires a
rebuild-bisect on a quiesced tree before any re-pin; this is exactly the
E-ULS-0041 drift-class question this ticket exists to settle. The audit also
records a structural coupling: 7 of 43 gates (and `make check-era`) run
in-tree `make` or link `$(dirname SEMU_EMULATOR)/libsemu.a` and are unsafe
under a concurrent build owner.

## Implementation

For each of the six scripts: rerun against the current binary with explicit
instruction budgets, verify the failure is the recorded drift (not a new
class), re-derive every stale hash/sentinel/budget pin from the new
two-identical runs, and record each script's before/after pins as one evidence
entry. Then add `check-era` running the full `TEST_PROFILE=sapporo-2.39.20`
era set (skipping cleanly when the private manifest or flash fixture is
absent, failing when present and red) and wire it into the `check` target's
dependency list only if its runtime is bounded and fixture-clean locally.

## Tests and Commands

Fixture provenance re-derived (2026-09-23, integrator): the volatile 2.39
full-flash fixture was lost to a machine event and rebuilt byte-exactly from
the recorded recipe (current-status 2.39 boundary section) plus the lane's
read-only tools — `../suunto-firmware/tools/build_production_data_fixture.py`
on the 2.22.60 application image (`component-04-type-4-v2.raw`, CRC table at
`0x199EE8` VA) yields the synthetic 4 KiB manufacturing sector, then
`component-05-type-1-v3.raw` (2.39, `0xFC1000` bytes) is placed at offset 0,
FF-padded to 16 MiB with the sector patched at `0x00FFF000`, and FF-extended
to 32 MiB; `shasum -a 256` again equals the pinned
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`. The
first probe with the restored fixture (`file_seek`) passes the fixture gate
and manifest validation and fails at artifact comparison — the expected
PIN-DRIFT entry state; full six-script classification is queued on the final
post-788 binary so display-side transcript movement is re-derived once.

`SEMU_EMULATOR=$PWD/build/suunto-emu SEMU_FIRMWARE_MANIFEST=… SEMU_SAPPORO_239_FULL_FLASH=…
TEST_PROFILE=sapporo-2.39.20 sh tests/integration/test_firmware_sapporo_239_<name>.sh`
for each of the six names, twice each, exit 0 both times; `make check-era`
twice (fixture-absent: clean skip, rc 0; fixture-present: all green);
`make check-lines && make check`.

## Acceptance

All six era scripts exit 0 on two consecutive runs with re-derived pins;
`make check-era` exists, is documented, and behaves as specified; every pin
change traces to a recorded two-run observation in a new evidence entry;
engine sources are byte-identical before and after the ticket.

## Forbidden Scope

No engine changes, no weakened assertions, no deleting or merging scripts, no
re-pinning without a fresh two-run derivation, no changing the full-flash
fixture or manifest, no claims about the Sapporo 2.35 profile.

## Handoff

Report per-script old→new pin tables with the two-run proof, the `check-era`
runtime, evidence entry IDs, and any script whose drift class turns out not to
match E-ULS-0041 attribution (which would open a new gap instead of a re-pin).

## Integrator classification (2026-09-27, final post-788 binary 7984ebd)

The queued final-binary classification ran as `make check-era` on 7984ebd
(census `/tmp/sap239-era/check-era-1.log`, fixture sha re-verified
`37134845…`): 12 of 43 green — exactly the audit's device-register pin-held
family (chip_identity, ctimer7_intclr, ctimer7_inten, nor_program, power,
profile, rstgen, timer_outcfg26, usb_clkctrl, watchdog_inten,
watchdog_restart, watchdog); 31 red with NO new drift class since the
audit. Root cause of the six environment-blocked scripts established: every
GPS/settings window ends at
`compat-refused pc=0x000920b4 instructions=446660148 virtual_time_ns=2831939375
detail=unknown Sapporo 2.39 writable file path`; the refused path,
captured 2026-09-27 with a temporary (applied, run, reverted) diagnostic is
`storage/38d123/data.jsn` mode 2 — an unmodeled native storage JSON write.
The refusal is correct fail-closed law; the six scripts are blocked on new
ticket 796 and are NOT re-pinned (that would weaken their choreography
goldens). Secondary fix applied in this batch: the three 2.39 GPS snapshot
inspectors (`tests/unit/test_sapporo_239_gps_awake_snapshot.c`,
`test_sapporo_239_gps_reopen_snapshot.c`, `test_sapporo_239_gps_snapshot.c`)
registered the display snapshot codec the CLI uses, mirroring ticket 791's
snapshot format — with it, a real 446M-state `.sems` loads and verifies
(all three inspector round-trips green). The remaining 25 timing-only
re-pin candidates are being re-derived on this final binary with
twice-identical runs per script; `timer_pattern` keeps its
rebuild-bisect-before-re-pin requirement. Scope add (integrator, 2026-09-27):
the three inspector sources above are in-scope for this ticket as the
smallest change making the six scripts runnable at all.

## Integrator record: re-pin batch complete (2026-09-27, HEAD 947f4bb)

The mechanical re-pin stage ran over the final binary (twice-identical
runs per script, integrator review + independent spot-verification of
`timer_pattern` twice byte-identical, full `make check-era` census
reproducing the classification: 24 of 43 red = 18 blocked + 6 gps/budget,
19 green = 12 device-register + 7 re-pinned).

Re-pinned green twice (7, value-only substitutions, all refusal guards
and choreography goldens intact): ctimer_combined_inten (boundary pc
0x00079e1e→0x00070378, count-preserving), file_seek, haptic,
haptic_calibration, lps22 (hash + cap-pc/vt drift, censuses intact),
ohr2_command2 (hash + pc/vt + at-cap census 452→193 per its own
E-ULS-0047 precedent), timer_pattern (idle 84856118/6372873793→
84856056/6372873731, park pc 0x000e955a unchanged; ATTRIBUTED by
rebuild-bisect to exactly d8bfba9 "gauge: align MAX17050 AvgVCell fixture
0x19 to current lane table" — cd1de52, 2f22137, and d8bfba9^ reproduce
the old value; not the 788 display commit).

Blocked, untouched (18): three root causes. B1 ticket-796 storage-JSON
wall at 442856246/2176971322 (fingerprint first.log 47e8aaa7… /
first.sems d1582e69…) blocks activity_budget, ctimer13_inten, ongoing,
quiet_read, widgets, zip_read, wbsto_cache in addition to the six
gps/budget scripts. B2 choreography redistribution past pinned caps —
file_size (595→456 ordinals, tss.bin ops absent), history_budget
(75764→865, sleep reads 0 vs 35712), preload1 (FILE_OPEN boundary moved,
trigger count 0 vs 1), logical_files (wbsto-preload-result trigger never
fires through 440M, still registered at src/compat/sapporo_239.c:27) —
needs a cap re-derivation stage once 796/797 settle. B3 OHR2 BSL
refuse→ok semantic change (7 scripts) — split to new ticket 797 with the
nine-commit bisect candidate list. A 69b1b35 control build reproduces the
old pin sets exactly for ctimer13_inten and history_budget, proving all
drift is accepted-batch era movement, not nondeterminism.

Integrator record 2026-09-27 (ticket 797 attribution stage, clean builds):
the B3 "device-law flip" premise is SUPERSEDED by the clean-build bisect
recorded in plans/tasks/797 Attribution Record. The earlier pass (and my
first probes) suffered build-directory object pollution (a
duplicate-symbol link failure exposed it); with `make clean` per commit:
the ohr2 cold stop stays pinned-exact (pc 0x0014e8ea vt 1881138282)
through 0c84673, relocates (pure instruction-count movement, vt fixed)
exactly at cd1de52, then drifts pc-only within the same vt lineage
(0x000d2084 era → 0x000a7ac4 on 06e3c2c, the ohr2_command2 re-pin
lineage); the pinned refuse goldens RELOCATE, they do not die — with the
scripts' own one-instruction resume shape, HEAD advances a single
instruction at the boundary with zero transactions, while the transcripts
show the same command/sequence family completing ok at the relocated
moments (refuse capability intact; guard greps hold). At 69b1b35 the
log/snapshot hashes already differ from the pins while the cold stop is
pinned-exact (Sep-5 file-law intervention/state drift, counts and hashes
only). 1bc1ce8/6ae8ce5/7c8bb59 exonered. No
engine change is warranted; the seven OHR2 scripts re-derive
mechanically under 797. The timer_pattern attribution to d8bfba9 stands
separately (its own rebuild-bisect, value -62).

The 2026-09-23 audit's "25 timing-only re-pin candidates" classification
is superseded by this record: only 7 of the 25 were mechanically
re-pinnable; the rest are the B1/B2/B3 evidence gaps above. 777 remains
in-progress: the remaining stages are (1) 796 lands → re-derive the 13
wall-blocked windows, (2) 797 lands → re-derive the 7 OHR2 windows,
(3) B2 cap re-derivation stage, then (4) full-green twice-run census.

Update 2026-09-27: stage (2) is DONE — the seven OHR2 scripts re-pinned
green twice (E-SAP239-OHR2-REPIN-001); census 26 of 43 green. Stage (3)
B2 is now UNBLOCKED from the 797 result: the technique (advance caps to
the boundary's first-appearance instruction via ±1 bisection, re-pin
stops/hashes/transcripts value-only, guards/census intact, stay below the
796 wall) applies directly to file_size, history_budget, preload1,
logical_files; note logical_files' absent trigger may need re-examination
of its registration condition rather than a cap move.

B2 stage result (integrator-reviewed, twice-verified): file_size RE-PINNED
green twice (tss.bin size op relocated to vt 2038505656; cold cap
405895301→414252829 by ±1 bisection; census 595→506; guard byte-identical;
new cold stop vt = event vt + 1 exactly). history_budget, preload1, and
logical_files are NEGATIVE observations — their asserted events occur
NOWHERE below the 796 wall (±1-verified wall-1 census in
/tmp/sap239-era/b2/notes.md): the 35,712-record sleep.bin scan never
begins, and neither WbStoPreload cmd-0/cmd-1 result-500 callback fires.
Scripts untouched (no guard may be weakened); all three are folded into
ticket 796's unblock list (13→16 windows) — the events must reappear past
the modeled storage-JSON write or be recorded as proven post-wall guest
changes. Stage (3) therefore concludes awaiting 796; remaining stages:
796 + 16 windows, then the full-green twice-run census.

Stage note 2026-09-27 (post-796 census, committed binary 5f1bf4c):
full 43-script census run 1 recorded (`/tmp/sap239-era/census1/`,
per-script logs + run1.txt): 27 green / 16 red. The 16-red set is
BYTE-IDENTICAL to the red set re-run on the pre-796 binary built from
f413e23 in a separate worktree (`/tmp/sap239-era/census-base/` and
`/tmp/sap239-era/re796/base-*.log`), so the 796 storage admission flipped
NO era script green-to-red or red-to-green. Classification of the 16:
(a) 11 wall-entangled windows asserting stops at or beyond the first
wall - history_budget, logical_files, preload1, wbsto_cache, quiet_read,
ongoing, widgets, zip_read, personal_budget, general_budget, and the
gps_five-family (gps_awake, gps_five, gps_reopen): the wall-crossing
runs now stop at the E-SAP239-REPO38D123-001 new wall
`stop=compat-refused pc=0x000920b4 instructions=474153646
virtual_time_ns=2208268722` (widgets confirms the pair exactly: baseline
442856246/2176971322 -> new 474153646/2208268722); re-derivation
proceeds by ±1 first-appearance bisection from the new wall as in
E-SAP239-OHR2-REPIN-001, plus re-observation of the next unknown
writable path at the new wall (next RE target). (b) 5 pre-existing
BKPT-cascade reds identical on both binaries - activity_budget,
ctimer13_inten, gps_startup and the layer-off halves of wbsto_cache and
logical_files (pin pc=0x00079e1e sentinel vs observed 0x00070378 at cap
72774982, verified identical at f413e23): the documented E-ULS-0041
sentinel-relocation drift, re-pin mechanically (stop line + artifact
shas, guards intact). [Superseded by the correction below - keep for
the record.] Census run 2 completed: run1.txt = run2.txt byte-identical
(`diff` clean), so the 27/16 split and the red-set identity against the
pre-796 binary are twice-reproduced. (b) correction (same day, direct
observation): the split is 14 wall-entangled + 2 wall-crossing
layer-off, NOT 11+5. activity_budget and ctimer13_inten cap past the
wall WITH the wbsto layer: at f413e23 their cold runs refused at the OLD
wall 442856246 (rc=3 == their pinned exit codes, so both were red on
"artifact differs", not on exit code); on 796 the refusal moves to
474153646 keeping rc=3 - same failure line, wall-moved artifacts.
gps_startup is purely pre-existing (assert-layer cold run, no file layer,
identical failure on both binaries - the documented sentinel/gate drift).
Every
re-derivation lands with twice byte-identical runs per E-SAP239-OHR2-
REPIN-001 method.

Stage note 2026-09-27 (B3 first re-derivation, wbsto_cache): wbsto_cache
re-derived green twice (third run confirmatory) on 5f1bf4c: (a) layer-off
sentinel re-pin 0x00079e1e -> observed 0x00070378 at the unchanged
72774982/521257564 boundary (E-ULS-0041 drift class, identical at
f413e23); (b) layer-on advanced checkpoint re-derived from the old
fault-free 500M-cap stop to the E-SAP239-REPO38D123-001 second-wall stop
`stop=compat-refused pc=0x000920b4 instructions=474153646
virtual_time_ns=2208268722 detail=unknown Sapporo 2.39 writable file
path` (cold-run log sha a1e305b5…, twice byte-identical); guards held
(session-cache trigger==1, preload-result now pinned ABSENT - verified 0
at f413e23 as well, reset guard, underflow 0x0f676e34 guard). Census
28/43 green. REPROBING RESULT for the remaining file-window scripts
(ongoing/quiet_read/zip_read/widgets/history/gps family): their asserted
native lifecycles (ongoing.bin 20 ops at vt ~2186664635, ZIP/general/
personal scans) occur in the full admission run only in the FINAL
~0.7% of pre-wall log lines - instruction-wise beyond every tested cap
473500000/474000000/474100000/474153645 (0 occurrences) - because the
four admitted storage JSON sessions INSERT instructions before them on
the post-796 path, so pinned (instruction, vt) pairs moved by a
non-uniform delta (the vt-instruction law holds per pristine segment,
not across the inserted sessions). RE-OBSERVATION RESULT for the new
wall: the next unknown writable path materializes in guest RAM at the
post-admission save as `storage/f8572579/data.jsn` (string offset
1505113 in rt-i.sems); nearby snapshot strings reveal NESTED keying
(`storage/%s` + `<key>/<subkey>/data.jsn` forms: storage/9ea0dac9/
91b4709a, storage/2e3fa8d2/b51799fe/data.jsn) - the FNV-1 top-level
family extends to sub-keyed paths; none of f8572579/9ea0dac9/2e3fa8d2
cracked from the 6710-token resource/app string census yet (cracking
scripts /tmp/sap239-era/re796/*.py). Next B3 steps: (i) crack or
model the nested-key law from the builder at 0x00198960 callers
[update same day: the second-wall path cracked —
`storage/f8572579/data.jsn` = FNV-1("/dive/surfacetimesnapshot"),
i.e. it IS a storage-family key; census + bounded negative for
ac100d90/faed64e2 recorded in E-SAP239-REPO38D123-001. The wall is a
NESTED storage path storage/<key>/<sub>/data.jsn, so the next law
question is whether the builder admits the nested form as a storage
path (pathjoin %s%x composes key1/key2) and whether 34-byte capacity
and the name-table shape extend to nested names];
(ii) ±1-bisect each window's relocated boundary by scanning log-line
fractions against capped runs (ongoing events sit at line fraction
0.9934 of the 474153646-line pre-wall log); (iii) re-pin or fold each
window per the E-SAP239-B2-CAPSTAGE-001 negative-observation rule.

Stage note 2026-09-27 (B3 wall-entangled re-observation): direct cap
runs on 796 confirm the wall-entangled windows cannot be mechanically
re-pinned: at cap 439081594 (history_budget "first") the new binary
reports stop=budget pc=0x000d160e instructions=439081594
virtual_time_ns=2173196670 with ZERO sleep.bin record reads (old anchor:
35,712 reads, stop vt 1,978,038,136, pc 0x000920b4 refusal) - the guest
no longer reaches the sleep scan before this cap because the four
admitted storage sessions change the post-boot flow that previously
stalled at the first wall. The pre-wall prefix at cap 435333559 is
unchanged (twice byte-identical log c1461588…, sems 30139dde…, and
byte-identical to the f413e23 baseline - the prefix predates the storage
family). Consequence: the history_budget/ongoing/quiet_read/zip_read/
general/personal/widgets/ctimer13/activity_budget windows each need the
RE-observed NEW choreography (the relocated lifecycle anchors) before
re-pinning; the vt-instruction offset is NOT a global constant across
the wall (vt-instruction = 216172480 at 439M new vs 1519791848 on the
pristine segment), so each anchor is an independent observation, not an
offset. gps_five baseline vs new: identical PRE-EXISTING red (fails the
unknown-writable-path gate on both binaries; new wall vt 2862236294 vs
old 2820942891). The general_budget/personal_budget probes fail in the
cold 700M stage (prefix hash) because that stage now crosses the wall.
Next: RE-observe the relocated sleep/ongoing/general anchors under the
new choreography (log-line fraction scan + ±1 bisection per E-SAP239-
OHR2-REPIN-001) and decide re-pin vs B2-style negative fold.
