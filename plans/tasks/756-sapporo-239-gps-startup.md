# 756 — Sapporo 2.39 GPS Startup Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 416,615,729

## Goal

Integrate the evidenced initial GPS startup/version exchange as a separate
opt-in `sapporo-2.39-gps-startup` compatibility layer. Preserve native parsing,
state transitions, and the existing one-layer production checkpoints.

## Execution Budget

Two model-days for interface integration, atomic refusals, and exact runs.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 416, 615, 729 and 754;
E-SAP-0011, E-SAP-COMPAT-FILES-239-001, E-SAP-COMPAT-ACTIVITY-239-001,
E-SAP-COMPAT-GPS-STARTUP-239-001;
`include/semu/compat.h`, `include/semu/scheduler.h`, `include/semu/bus.h`,
`src/compat/layer.c`, `src/compat/sapporo_222.c`, `src/compat/sapporo_222.h`,
`src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`,
`src/devices/sapporo_gps_compat.c`, all existing Allowed Files below,
`src/core/snapshot_io.h`, `src/boards/machine_internal.h`,
`src/boards/machine_snapshot.c`, `src/boards/machine_snapshot_identity.c`,
`tests/unit/test_machine_snapshot.c`,
`tests/devices/test_sapporo_cxd5610_snapshot.c`,
`tests/integration/test_firmware_sapporo_239_activity_budget.sh`;
pristine routines `0x00128bb0..0x00128d18`, `0x00128e7c`, `0x00128ebc`,
`0x0012a0c4..0x0012a12c`, `0x0012a728`, `0x0012a764` and the exact
external source/trace hashes recorded in the GPS evidence entry.

## Current Baseline

At initial startup return PC `0x00128d14`, instruction 357,031,764 /
1,878,380,008 ns, driver R4 `0x100366d8` has live UART +0x220, callback four
at +0x272, pending two at +0x273, and retry byte `0x100588a4` zero. R0 is
zero before the native success assignment. With no RX there is no command;
the third timeout asserts at `CXD5610GF-driver.cpp:910`. External diagnostics
prove startup status, exact `@VER` reply, and native states 14/15 only.

## Allowed Files

- `src/compat/sapporo_239_gps.c`, `src/compat/sapporo_239_gps.h`
- `src/devices/sapporo_devices.c`, `src/devices/sapporo_devices.h`
- `src/devices/sapporo_devices_internal.h`, `src/devices/sapporo_device_compat.c`
- `src/devices/sapporo_cxd5610.c`, `src/devices/sapporo_cxd5610.h`
- `src/devices/sapporo_cxd5610_rx.c`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `src/boards/machine_snapshot.c`
- `src/compat/layer.c`, `include/semu/compat.h`
- `src/compat/sapporo_222.c`, `profiles/sapporo/2.22.60/profile.semu`
- `profiles/sapporo/2.39.20/profile.semu`
- `tests/unit/test_sapporo_239_gps.c`, `tests/unit/test_sapporo_239_gps_snapshot.c`
- `tests/unit/test_sapporo_profile_239.c`
- `tests/devices/test_sapporo_cxd5610.c`
- `tests/devices/test_sapporo_cxd5610_atomic.c`
- `tests/unit/test_machine_snapshot_layers.c`
- `tests/integration/test_firmware_sapporo_239_gps_startup.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/756-sapporo-239-gps-startup.md`

## Frozen Interfaces

This is the integration owner for the named layer registry, profile metadata,
and version-scoped device compatibility binding through the existing device
API. It may extend those headers, not invent a parallel machine API. Preserve
snapshot wire format, CPU/scheduler semantics, UART MMIO, successful 2.22 behavior,
existing WbSto descriptor/counters/files, all historical goldens and Makefile.
Split binding/RX responsibilities into the named files before any file exceeds
500 lines; do not expand the nearly-full device files mechanically.

## Evidence Inputs

E-SAP-COMPAT-GPS-STARTUP-239-001 supplies exact native predicates and the
two-line diagnostic, including wrong-prefix refusal. `$PSS0000\r\n` is a
synthetic fixture accepted by the native parser, not a physical capture or a
claim about status-field semantics. All three component hashes must match
E-SAP-0011. The existing one-layer baseline must remain available unchanged.

## Implementation

Add an explicit two-intervention layer: one startup RX hit and one exact
version response hit, aggregate limit two. Both are named, logged, opt-in and
hash-pinned. At the initial `0x00128d14` boundary validate the complete driver
and UART SRAM spans, callback/pending/retry values and UART callback identity
before injecting the ten-byte line after 10,000,000 ns. Do not require R0=1
at this PC, change the CPU, or schedule native state directly.

Accept only six request bytes `@VER\r\n`, once and only after startup injection,
returning the same delayed line through the existing device RX queue/IRQ.
Bind fixtures explicitly on create/reset and snapshot restoration, with no
implicit activation from another 2.39 layer and no dependency on 2.22 fixtures.
Use retained compatibility counters as lifecycle authority; do not introduce
unsaved host-only progress. The existing layer/device snapshot codecs should
suffice; stop and request integration scope if a format change is necessary.

Validate full operation and scheduling capacity/time before mutation. Audit
`semu_sapporo_cxd5610_inject_rx_after`: it currently stages RX bytes before
scheduler insertion. A scheduler failure must not alter device bytes, queue,
counters or logs. Correct only this bounded atomicity problem if the new
regression proves it, preserving successful timing and existing transports.
The user-approved scope extension also covers final-command refusal before
transport buffer/trace mutation and machine snapshot counter validation.
Reject excessive aggregate/per-intervention hits and checked sums greater than
the aggregate; retain valid legacy counters and the wire format. The new GPS
layer must additionally validate its own startup-before-reply lifecycle on
snapshot load. Do not infer ordered lifecycle semantics for unrelated layers.
The prerequisite maintenance also enforces the aggregate bound at intervention
commit and corrects stale 2.22 aggregate metadata to twenty (nine existing
one-shot interventions plus eleven existing awake pulses), without increasing
any per-trigger allowance. Invalid counts now refuse; valid legacy checkpoints
must retain their bytes and successful timing.

## Tests and Commands

Run `make test TEST_FILTER=sapporo_239_gps` before and after implementation;
`make test TEST_FILTER=sapporo_cxd5610`,
`make test TEST_FILTER=sapporo_profile_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check`, `make sanitize`.
Run `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_startup`.
Also rerun the same private command with `TEST_FILTER=sapporo_239_activity_budget`
to preserve the historical one-layer checkpoint. Private tests must validate
every component/full-flash hash before execution; absence skips, mismatch fails.

## Acceptance

Failing-before/passing-after regression. Verify exact activation metadata and
hash refusal; disabled layer, wrong profile, malformed state/pointers,
wrong/repeated command, exhausted hits, RX busy, schedule exhaustion and time
overflow all refuse before state/log mutation. Verify successful delayed RX
and IRQ ordering plus reset binding without activating the layer implicitly.

Two fresh two-layer private runs must reach the native initial states 14/15
with retry zero, exactly one startup hit and one exact command/reply hit,
byte-identical logs and snapshots, and no CPU/firmware edits. Pin the first
post-startup checkpoint from the integrated implementation; external probe
hashes are evidence, not production goldens. Resume snapshots before startup,
with startup RX pending, with response RX pending, and after both hits;
require identical final state/log suffix. Wrong layer identity and malformed
counter/event state reject atomically. Existing one-layer snapshots/goldens
remain unchanged; do not load them into the two-layer configuration silently.

Identify the later pending-seven timeout separately without supplying another
status. Explicit instruction and virtual-time limits apply to every private
run. Record immutable flash hash, exact commands and any skipped condition.

## Forbidden Scope

No status injection on arbitrary opens/retries, pending-seven support,
`@SLP`/other command handling, physical status semantics, NMEA/fix/time data,
GPIO guesses, assertion bypass, general read-as-zero, 2.22 hook transplant,
file-budget expansion, renderer changes, snapshot migration or firmware bytes.
Do not claim full GPS, settled 2.39 setup or a functional release from states
14/15 alone. Existing ticket statuses remain integrator-owned.

## Handoff

At ticket creation the new GPS layer was not implemented. Dependencies 416, 615 and 729
are done in the index. The initial integration contract is grounded in the
new GPS evidence; later pending-seven behavior remains a separate gap.
Maintenance evidence capture changes no production code, layer or checkpoint.

## Integration Handoff — 2026-09-06

Implementation and local acceptance pass; status remains integrator-owned.
The explicit hash-pinned GPS layer supplies only the initial startup and exact
version-response fixtures. Its two counters are device-instance-owned, and
create/reset/restore binding uses the existing machine/device contracts.
Snapshot format is unchanged; reply-before-startup, unattributed GPS hits,
disabled GPS lifecycle and layer-identity mismatches reject atomically.
The old one-layer production path retains all historical checkpoints.

Changed files: `src/compat/sapporo_239_gps.c` and `.h`;
`src/devices/sapporo_device_compat.c`, `sapporo_devices.c`, `.h`, and
`sapporo_devices_internal.h`; `src/boards/machine.c`, `machine_run.c`, and
`machine_snapshot.c`; `profiles/sapporo/2.39.20/profile.semu`;
`tests/unit/test_sapporo_239_gps.c`, `test_sapporo_239_gps_snapshot.c`, and
`test_sapporo_profile_239.c`; the new private GPS script; this ticket,
`docs/current-status.md`, and `docs/migration-evidence.md`. The device binding
split preserves existing 2.22 behavior and keeps every file below 500 lines.
References: E-SAP-0011, E-SAP-COMPAT-GPS-STARTUP-239-001,
E-SAP-COMPAT-ACTIVITY-239-001, and E-EMU-COMPAT-ATOMIC-001.

Exact verification commands and results:

```sh
make test TEST_FILTER=sapporo_239_gps       # 6 pass; activation failed before addition
make test TEST_FILTER=sapporo_cxd5610      # pass
make test TEST_FILTER=sapporo_profile_239  # 5 pass; profile count failed before addition
make test TEST_FILTER=machine_snapshot    # pass
make check-task-contracts                 # 126 tickets validated
make check-lines                          # pass, review-threshold warnings only
make check                                # 752 tests pass, all gates pass
make sanitize                             # 752 tests pass, no sanitizer findings
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_startup
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
git diff --check                           # pass
```

Both private gates pass without skips. Two cold GPS runs reach state 15,
retry zero at `pc=0x00128ed8 instructions=393785845 virtual_time_ns=2564070074`.
Log SHA-256 `b5b23c9f6a96d9ecfbf4f17aa4f3b70801d08cd9b9636330d11c08f2e0647123`,
snapshot `bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9`.
Four exchange-phase snapshots reproduce the exact final image/log suffix.
Completed exchanges resume directly or through the later pending-seven
checkpoint to the same retry image. The later retry at instruction 908,321,039 /
14,978,258,084 ns refuses without another response or hit. The old one-layer
logo/activity/halt gate passes unchanged; full flash remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
No acceptance item was skipped and no firmware artifact was added to Git.

Remaining gap: reverse-engineer the separate pending-seven reopen response;
no extra status, commands, physical payload semantics, fix/time data or settled
2.39 UI are implemented. Requested integrator action: review acceptance and
update the ticket index separately; assign new evidence-scoped work for the
later lifecycle. No additional public-interface change is requested.

2026-09-06 maintenance verification: `make check-task-contracts` validates
126 tickets; `make check` passes, including line checks with existing review
warnings only; `git diff --check` passes. The exact private activity-budget
command above passes both cold starts, historical logo/continuation hashes,
intermediate snapshot resume and native GPS halt. Full-flash SHA-256 remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Two external GPS diagnostic trace/log pairs compare byte-identically using
`cmp`; the wrong-prefix control still reaches the native assertion. Exact
hashes and limits are in E-SAP-COMPAT-GPS-STARTUP-239-001. These diagnostics
are not the future production acceptance tests. No sanitizer run was needed
for this documentation/planning-only change; integration acceptance remains
entirely outstanding. Changed files: this ticket, `plans/index.tsv`,
`docs/current-status.md` and `docs/migration-evidence.md`. No firmware,
pixels, snapshots, traces or probe source entered Git.

## Prerequisite Maintenance Handoff — 2026-09-06

The user authorized necessary integration changes, including breaking changes.
E-EMU-COMPAT-ATOMIC-001 records the completed bounded prerequisite fixes:
atomic final-fragment refusal/WAIT and delayed RX scheduling, legacy startup
provider hit/log ordering, runtime aggregate-budget enforcement, checked
snapshot counter validation, and correction of stale 2.22 aggregate metadata
to the sum of its unchanged per-trigger limits. No new GPS layer, hardware
response or firmware-state intervention is enabled. This maintenance does not
complete ticket 756; its new-layer acceptance conditions remain outstanding.

Changed implementation/contracts: `src/devices/sapporo_cxd5610.c` and `.h`,
`src/compat/sapporo_222.c`, `src/compat/layer.c`, `include/semu/compat.h`,
`src/boards/machine_snapshot.c`, `profiles/sapporo/2.22.60/profile.semu`.
New tests: `tests/devices/test_sapporo_cxd5610_atomic.c`,
`tests/unit/test_machine_snapshot_layers.c`. Documentation: this ticket,
`docs/current-status.md` and `docs/migration-evidence.md`. Index/status is
unchanged. The allowed-file extension above records the approved integration
ownership; no private parallel API or snapshot format was added.

Commands/results:

- `make test TEST_FILTER=sapporo_cxd5610_atomic`: original two cases fail
  before transport fixes; added legacy-provider case fails before its fix;
  all three pass afterward.
- `make test TEST_FILTER=machine_snapshot_layers`: all three cases fail
  before snapshot/budget corrections and pass afterward.
- `make test TEST_FILTER=sapporo_cxd5610` and
  `make test TEST_FILTER=machine_snapshot`: all selected cases pass.
- `make check` and `make sanitize`: each passes all 746 cases.
- `make check-task-contracts`: 126 indexed tickets validate;
  `make check-lines` and `git diff --check` pass.
- The exact private activity-budget command in Tests and Commands passes,
  preserving both historical logo hashes and the pre-BKPT snapshot
  `8d9b262474b00c6c0a2b5423ce4100363eb4582a96205eae8d045b70c8ae50a7`.
- Independently compiled `cb875c4` and current CLI: validate exact 2.22
  manifest, then `run --profile sapporo-2.22.60 --firmware
  tests/private/sapporo-2.22.60/firmware.semu --layer sapporo-2.22-no-device
  --max-instructions 450900000 --max-time 30000000000 --snapshot-save PATH`.
  Both return budget (exit three), with byte-identical logs/snapshots. Both
  resume the old snapshot using `--snapshot-load PATH --max-instructions
  450910000 --max-time 30000000000 --snapshot-save NEXT_PATH`, again matching.
  Exact checkpoints/hashes are in E-EMU-COMPAT-ATOMIC-001; this is not a new
  release golden. Full flash retains its pinned hash.

Deliberate compatibility change: malformed counter snapshots and aggregate
excess now refuse, and rejected final TX fragments are no longer traced or
discarded. Valid tested snapshots and successful guest timing are preserved.
No private firmware/pixels/snapshots enter Git. Layer-specific lifecycle order,
new fixture binding, two-layer snapshots and the later pending-seven gap are
still the next implementation work. No remaining scope authorization is
needed for the currently identified 756 integration changes.
