# 796 — Sapporo 2.39 Native Storage JSON Write Law

**Status:** in-progress
**Phase:** 7
**Dependencies:** 729, 768

## Goal

Extend the Sapporo 2.39 compat logical-file law so the guest's native
storage JSON persistence (`storage/<key>/data.jsn`, observed key prefix
`38d123`) is modeled with the same named, hash-pinned, opt-in, hit-bounded,
logged treatment as the existing table paths, unblocking the thirteen era
scripts that currently end at the fail-closed unknown-writable-path
refusal wall (six GPS/settings scripts plus the seven windows listed in
the baseline).

## Execution Budget

Two to three model-days. Expected: one compat evidence entry (offline RE
per the 2026-09-23 owner decision or a lane probe), table/capacity edits in
`src/compat/sapporo_239_files.c` and the file hook, focused unit cases, and
the six era scripts re-derived green twice.

## Required Reading

`AGENTS.md`; `docs/migration-evidence.md` E-SAP-COMPAT-FILES-239-001,
E-SAP-TIME-NATIVE-239-001, and the personal-settings entry carrying the
`pc=000920b4 ... path=settings/time` refusal census;
`src/compat/sapporo_239_file_hook.c`, `sapporo_239_files.c`,
`sapporo_239_files_internal.h`; ticket 777's classification record;
`tests/integration/test_firmware_sapporo_239_{gps_awake,gps_reopen,gps_startup,gps_five,general_budget,personal_budget}.sh`.

## Current Baseline

Every 2.39 GPS/settings era window ends at
`compat-refused pc=0x000920b4 instructions=446660148 virtual_time_ns=2831939375
detail=unknown Sapporo 2.39 writable file path` — the file hook refuses a
mode-2 open of `storage/38d123/data.jsn` (path captured 2026-09-27 with a
temporary, reverted diagnostic; capture noted in the 777 classification
entry). The path is absent from `semu_s239_file_paths`; the refusal is
correct fail-closed behavior for an unmodeled path. The six scripts' pinned
choreographies (GPS awake 32.7 s, five-pulse 37.9 s, settings budgets) are
unreachable past 2.83 s guest time.

Batch classification (2026-09-27, re-pin stage final report): the wall
extends beyond the six — `activity_budget` (logo window, rc-0-expected),
`ctimer13_inten`, `ongoing`, `quiet_read`, `widgets`, `zip_read`, and
`wbsto_cache` all carry pinned windows past the same refusal (wall
fingerprint cold+layer+normal-frame: first.log sha
`47e8aaa79662999c941660034c956fcad2befe9e26c7b53395d5199c3e7dca8d`,
first.sems sha
`d1582e694a5132475d935332f654380ec0035218e84fc6b43ac90148adf7a0e7`;
refusal tuple 442856246 / 2176971322 at pc 0x000920b4). Thirteen era
scripts total are unreachable past the wall; their windows recover only
when this ticket lands. A separate OHR2 BSL semantics change (ticket 797)
and the cap-redistribution windows are NOT this ticket's scope.

Addendum 2026-09-27 (797/777-B2 completions — scope update): ticket 797
closed showing the OHR2 movement was session relocation, no device law.
The 777 B2 stage then proved three further windows are wall-entangled, so
this ticket's unblock list grows from 13 to 16 scripts. New findings
(±1-verified negative probes below the wall, census in
`/tmp/sap239-era/b2/notes.md`, wall-1 probe log sha
`67cd6109ab5c41e85dab870a2362d94dcf0f8aa8f541178aa02e4e5039339cf8`):
(a) `history_budget` — the 35,712-record `sleepln/sleep.bin` read scan
occurs NOWHERE below the wall (0 `result=72` reads through 442856245; 2
header reads + 2 seeks; the guest never begins the sleep scan) and its
pinned unknown-path refusal boundary IS the wall (old anchor
439081594/1978038136 → 442856246/2176971322): the scan must reappear
naturally past the modeled data.jsn write.
(b) `preload1` — `wbsto-preload1-result` (registered
`src/compat/sapporo_239.c:34`) fires nowhere below the wall: the exact
`WbStoPreload` command-1 callback with result 500 (chain: provider
0x001c0ed8, cmd byte 1, r3==500, session-cache + preload-result hits) is
never reached; its FILE_OPEN budget-exceeded boundary (logical-file
budget now 76667) is likewise unreachable at the current 865 max hits.
(c) `logical_files` — `wbsto-preload-result` (registered
`src/compat/sapporo_239.c:27`, cmd-0 callback result 500) fires nowhere
below the wall; session-cache trigger and census 118 intact at the old
cap. Acceptance for this ticket therefore also includes: after the law
lands, the three windows' asserted events either reappear (re-derive per
E-SAP239-OHR2-REPIN-001) or remain absent as a PROVEN post-wall guest
change, recorded as a negative observation — never by weakening a guard.
This is consistent with the lane's own note (read-only
`../suunto-firmware/docs/research/sapporo-2.39-logical-storage-adapter.md`)
that native `storage/` opens sit beyond the WbStoPreload boundary the
synthetic layer currently ends at.

## Allowed Files

`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_files_internal.h`,
`src/compat/sapporo_239_file_hook.c`, focused unit modules under
`tests/unit/`, the six era scripts named above (pin re-derivation only),
`docs/migration-evidence.md`, `docs/current-status.md`, this ticket.
`Makefile`, registries, profiles, and other scripts are integration-owned.

## Frozen Interfaces

The existing table-owned state machine, capacities, quiet-read bypass,
settings/time native retention, ordinal intervention accounting, refusal
message prefixes pinned by other scripts, deterministic checkpoints, and
firmware safety remain. Unknown paths keep refusing fail-closed; this
ticket admits only the lane/RE-evidenced storage JSON path family with its
measured capacity and size law.

## Evidence Inputs

The refusal census in ticket 777's classification entry (twice-identical
across all six scripts, integrator-reproduced manually). Required next:
the native storage write semantics for `storage/<key>/data.jsn` — block
size/capacity, write/commit sequence, key derivation — from offline RE of
the pinned 2.39 application partition or an equivalent lane probe, named
with SHA-256 inputs and a twice-reproduced census.

## Implementation

Derive the storage JSON write law from the pinned application image;
extend the file table and hook with the measured capacity and operation
sequence; keep the refusal for every still-unmodeled path. Red-first unit
cases: admitted-path success, capacity boundary refusal, unknown-key
refusal. Then re-derive the six era scripts' pins from two byte-identical
runs each; the choreography goldens (stop reason, later windows) must
RECOVER their original shapes, not be re-pinned to the early refusal.

## Tests and Commands

`make test TEST_FILTER=sapporo_239` (or the focused module), `make check`,
`make sanitize`, and the six era scripts twice each with
`SEMU_EMULATOR/TEST_PROFILE/SEMU_FIRMWARE_MANIFEST/SEMU_SAPPORO_239_FULL_FLASH`
set; `make check-era` census improving from 24 red toward green as this
ticket and the 777 follow-up stages complete.

## Acceptance

A hash-pinned offline-RE (or lane) evidence entry for the storage JSON
path; unit success+refusal coverage; all six era scripts exit 0 twice with
re-derived pins whose choreography shapes match their pre-file-hook goldens
at unchanged CRCs; zero new refusal classes introduced.

## Forbidden Scope

No blanket unknown-path admission, no fabricated capacity, no guessed
serializer behavior, no weakening of the six scripts' choreography goldens
to the early refusal, no silent ordinal-budget growth, no engine-file
changes outside the compat layer named above.

## Handoff

Integrator-created 2026-09-27 during the 777 classification batch: the six
era scripts are blocked on exactly this law; everything else about the 777
re-derivation is reproducible without it.

## Implementation Note 2026-09-27 (awaiting integrator review)

Evidence: E-SAP239-REPO38D123-001. Law: `storage/<key>/data.jsn` key =
lowercased FNV-1 (prime-multiply-before-xor) of the lower-cased resource
path; open admits only on guest mode 2 (write-create) with a fail-closed
shape check; fixed 34-byte capacity (observed payloads end exactly at 34);
append-only name table bounded by the handle index space (63 slots, file
ids 12..74); repeat open reuses retained bytes; unknown mode-2 paths keep
the verbatim refusal. Snapshot codec v2 (name table + present flags +
payloads); v1 artifacts byte-identical; open storage handles legal only in
v2. `ac100d90` key preimage not cracked (admitted by family shape, as
evidence records). Unit coverage: shape refusals, mode-1 fallthrough,
capacity boundary, pool exhaustion, reopen reuse, v2 round-trip
(`test_sapporo_239_files`, 6/6 pass; `make test TEST_FILTER=sapporo_239`
53/53; `make sanitize TEST_FILTER=sapporo_239_files` clean).
Post-admission wall (from pre-wall save, cross-wall resume verified
identical to direct runs): `stop=compat-refused pc=0x000920b4
instructions=474153646 virtual_time_ns=2208268722 detail=unknown Sapporo
2.39 writable file path` — NOT a storage path. Era scripts not re-pinned
here; the 16 wall-entangled windows re-derive under 777. `make check`
PASS; sanitize clean. Status spot-check (committed binary, stashed
baseline): pre-admission windows green (profile, file_size, file_seek,
chip_identity, power, rstgen, watchdog); the 16 wall-entangled scripts
remain red exactly as classified pre-implementation (they assert stops
below or at the moved wall). Note for 777: scripts capping beyond the
first wall (wbsto_cache, quiet_read, ongoing at 500M) now stop at the
NEW wall 474153646/2208268722 instead of the old 442856246 wall; the
layer-off pins of wbsto_cache/logical_files drift is
`pc=0x00070378` at cap 72774982 (pre-existing at f413e23, not caused by
796; bisection: the layer-on path at that cap changed when the storage
wall moved). Ticket stays in-progress until 777 re-derivation lands.
