# 753 — Sapporo 2.39 Native Empty Widgets Cache Value

**Status:** done
**Phase:** 7
**Dependencies:** 728, 729, 615

## Goal

Correct the synthetic Widgets cache value using the native empty-object ABI,
preserving the serializer's bounds checks and all compatibility budgets.

## Execution Budget

One model-day for ABI recovery, strict regression and exact firmware runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; ticket 728 and 752 handoffs;
E-SAP-SERIALIZER-ARRAY-239-001, E-SAP-COMPAT-WIDGETS-NATIVE-239-001,
E-SAP-COMPAT-WBSTO-239-001 and E-SAP-0011;
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`include/semu/compat.h`, `include/semu/bus.h`, `src/compat/layer.c`;
`tests/unit/test_sapporo_239_compat.c`, `tests/unit/test_sapporo_239_preload1.c`,
`src/boards/machine_snapshot_identity.c`,
`tests/integration/test_firmware_sapporo_239_ctimer13_inten.sh`;
`src/frontends/cli.h`, `src/frontends/cli_checkpoint.c`, `include/semu/frame.h`,
`include/semu/hash.h` for read-only frame verification;
read-only research `sapporo-2.39-widgetsettings-serialization.md`,
`sapporo-2.39-wbsto-value-abi.md`, `sapporo-2.39-synthetic-wbsto-runtime.md`;
the pristine copy/walker/schema addresses named in the new evidence entry.

## Current Baseline

Ticket 752 reaches `ChunkSerializer.cpp:38` at instruction 608,140,267.
The cache mistakenly supplies fallback JSON where native type `0xa412`
expects a 12-byte object. JSON bytes become a bogus nonzero array count.

## Allowed Files

- `src/compat/sapporo_239.c`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_239_widgets.c`
- `tests/integration/test_firmware_sapporo_239_widgets.sh`
- `tests/integration/sapporo_239_widgets_frame.c`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/753-sapporo-239-native-widgets.md`

## Frozen Interfaces

Keep the existing layer ID, opt-in/hash gating, triggers, all per-trigger/total
budgets, entry count/order/offsets, arena extent, other three values, file table,
headers, CPU/device logic and snapshot format. No implicit snapshot migration.

## Evidence Inputs

E-SAP-COMPAT-WIDGETS-NATIVE-239-001 pins the native type-15 copy, 12-byte
object, empty array layout and successful native copy/status in a separately
labeled diagnostic. JSON is only a fallback API response, not cache data.

## Implementation

Install twelve zero bytes for Widgets, with logical/bounded lengths twelve
and unchanged alignment/padding. Reuse complete cache validation at both
preload translations, and update the existing installation event provenance.
Refuse old JSON or any altered installed byte/length before translating status.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_compat` before/after the correction;
`make test TEST_FILTER=sapporo_239_widgets`, `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_widgets`.

## Acceptance

Failing-before/passing-after native layout regression; byte-exact wrong-layout
and length refusal at both preload checks without RAM/CPU/hit/log mutation.
Two fresh private logs/snapshots match and native Widgets copy returns 200.
If a visible frame is reached, verify its dimensions, format and pixel hash
through a read-only CLI frame callback without committing pixels.
The pre-install baseline remains exact, corrected snapshots resume identically,
source flash remains immutable and the next independent boundary is identified.
All components validate before execution. Report any skipped gates.

## Forbidden Scope

No serializer/assertion bypass, buffer growth, extra intervention or budget,
guessed nonempty widgets, JSON/parser/file replacement, recovered-default claim,
firmware-byte copy, profile/header/Makefile edit or historical golden edits.
Existing post-install JSON snapshots/checkpoints are historical evidence of
the defective fixture, not goldens to re-pin silently; require regeneration.

## Handoff

Implemented and verified; status remains in-progress for integrator review. Changed files
are exactly the Allowed Files above. References: E-SAP-0011,
E-SAP-COMPAT-WBSTO-239-001, E-SAP-SERIALIZER-ARRAY-239-001,
E-SAP-COMPAT-WIDGETS-NATIVE-239-001 and E-SAP-BOOT-LOGO-239-001.
Only the Widgets payload/length and existing installation event's effect/
provenance change production code. There are no new hooks, counters, budgets,
formats, public interfaces, files, default-state claims or shader/device edits.

The native layout regression failed before the fix at the old 16-byte entry
length, then passed with the twelve-byte zero object and four-byte alignment
padding. The new Widgets tests cover both preload stages, all sixteen slot
bytes (including padding), both entry lengths, and a complete old JSON/length
restore. Refusals preserve CPU, complete cache RAM, per-trigger/total hits and
the log position. Successful preload validation preserves RAM and logs the
new installation provenance once. Existing hash/profile/disabled-layer,
prerequisite, unrelated-trigger and per-trigger budget checks remain intact.

Commands/results: `make test TEST_FILTER=sapporo_239_compat` passes four cases;
`make test TEST_FILTER=sapporo_239_widgets` passes two cases;
`make test TEST_FILTER=sapporo_239` passes 23 cases;
`make test TEST_FILTER=machine_snapshot` passes one case.
`make check` and `make sanitize` each pass all 739 cases.
`make check-task-contracts` validates 124 indexed tickets;
`make check-lines` has existing warnings only. `git diff --check` and
`sh -n tests/integration/test_firmware_sapporo_239_widgets.sh` pass.
Direct private-runner checks skip an absent full-flash variable (exit zero)
and reject a known wrong full-flash input, the application component (exit
two), before execution. The frame helper compiles as warning-clean C99 with
the existing CLI object/library; it writes hashes only, never pixels.

Production tracing from a freshly generated corrected-cache checkpoint uses
no diagnostic RAM/CPU/budget edits. It reproduces native copy to `0x10033e50`
at instruction 608,140,162, with zero scalar/count and firmware-relocated
pointer `0x10033e5c`, then native status 200 at 608,140,325 for LID `0xa432`.
The exact trace/prefix/log hashes are recorded in the evidence entry.

The fresh first-visible checkpoint is
`stop=user pc=0x00093be2 instructions=609300000 virtual_time_ns=2148256583`.
Log SHA-256 `63eb4997ff645958e70ed0586613762f88ee5e6e699434c1fbae48f0f435528b`;
snapshot `30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
It renders a 240x240 Suunto boot logo, generation two, CRC32 `4979f432`,
pixel SHA-256 `3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373`.
This is the first nonblack `normal-frame` checkpoint, not a settled setup UI.
No resets/refusals precede it; logical-file hits stay 76,258, all other limits
are unchanged, and full flash retains hash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.

The pre-install 40-million-instruction checkpoint retains old log hash
`253ffdd99ca7b8fb972518ad7e50701f37306436d67a5114085d94bda01ae11b` and snapshot
`d2ae7cd38834b3488f9a5785235bd69005e773b129497bf0fa68ac6fa0b47fce`.
The private runner compares two fresh captures and both pre-install and
corrected-cache resumes, verifies the actual frame hash, then continues past
the logo to the unchanged file-budget refusal. The exact private command in
Tests and Commands passes all these gates. Its preframe checkpoint uses
607,100,000 instructions to align the existing 100,000-instruction CLI frame
polling grid. The initial nonaligned checkpoint had a different CLI stop
boundary, not a demonstrated state-restoration defect; no CLI change is made.

Next production boundary from the captured logo: `actitmln/247.bin` update-open
mode three, LR `0x000b9e0d`, PC `0x000920b4`, instruction 610,599,945,
time 2,149,556,528 ns, logical-file budget exhausted. A separate no-observer
preframe diagnostic reaches that same request at 609,821,432; retain the
rendered-logo lineage for the next task instead of mixing those boundaries.
Trace the finite post-logo activity operations before growing any budget.

Layer-off execution remains exactly E-SAP-0029 (log hash
`db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`).
No snapshot format or migration changes: an old JSON-bearing ticket-751
snapshot still reaches the native serializer halt at instruction 608,140,267;
if resumed before either preload translation it instead fails the strict
cache check. Regenerate these checkpoints from reset or the pre-install
prefix. Historical JSON-bearing goldens remain unchanged in Git and describe
the defective fixture, not current acceptance. No nonempty Widgets, provisioned
storage contents, interactive 2.39 setup or physical-panel support is claimed.
No additional integrator-owned interface change is requested.
