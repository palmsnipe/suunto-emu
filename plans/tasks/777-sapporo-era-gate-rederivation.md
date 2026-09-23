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
