# 757 — Sapporo 2.39 GPS Reopen and Restart Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 756,615,729

## Goal

Represent the evidenced later GPS reopen and exact GSR response as a separate
opt-in `sapporo-2.39-gps-reopen` layer, preserving native parsing/state changes
and every historical one-/two-layer checkpoint.

## Execution Budget

Two model-days for bounded integration, atomic refusal and exact private gates.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 756, 615 and 729;
E-SAP-0011, E-EMU-COMPAT-ATOMIC-001, E-SAP-COMPAT-GPS-STARTUP-239-001 and
E-SAP-COMPAT-GPS-REOPEN-239-001; all existing Allowed Files below;
`include/semu/compat.h`, `include/semu/bus.h`, `include/semu/scheduler.h`,
`src/compat/layer.c`, `src/compat/sapporo_239.c`,
`src/core/snapshot_io.h`, `src/core/scheduler_internal.h`,
`src/devices/sapporo_devices_snapshot.c`,
`tests/devices/test_sapporo_cxd5610_atomic.c`,
`tests/devices/test_sapporo_cxd5610_snapshot.c`;
pristine routines `0x00128dc8..0x00128e9a`, `0x00128ebc..0x001290c2`,
`0x001291c4..0x00129258`, `0x0012a0c4..0x0012a12c`, `0x0012a728`,
`0x0012a764`, `0x0012aa7c..0x0012aa8e`, and the exact probe/source hashes in
the reopen evidence entry.

## Current Baseline

Ticket 756's `fa56b33` implementation has passed integrator review; all three
dependencies are done. This ticket is ready for its bounded integration.
The two-layer production baseline reaches the later pending-seven timeout
and refuses the consumed startup hook at instruction 908,321,039 /
14,978,258,084 ns. External experiments prove one later status and exact
GSR response reach native states 10/12 with retry zero, then a distinct GSTP
refusal. No corresponding production response exists yet.

## Allowed Files

- `src/compat/sapporo_239_gps_reopen.c`, `src/compat/sapporo_239_gps_reopen.h`
- `src/compat/sapporo_239_gps.c`, `src/compat/sapporo_239_gps.h`
- `src/devices/sapporo_device_compat.c`, `src/devices/sapporo_devices.c`
- `src/devices/sapporo_devices.h`, `src/devices/sapporo_devices_internal.h`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `src/boards/machine_snapshot.c`, `src/boards/machine_snapshot_layers.c`
- `src/boards/machine_internal.h`
- `profiles/sapporo/2.39.20/profile.semu`
- `tests/unit/test_sapporo_239_gps_reopen.c`
- `tests/unit/test_sapporo_239_gps_reopen_snapshot.c`
- `tests/unit/test_sapporo_profile_239.c`
- `tests/unit/test_sapporo_239_gps.c` (profile enumeration assertion only)
- `tests/integration/test_firmware_sapporo_239_gps_reopen.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/tasks/757-sapporo-239-gps-reopen.md`

## Frozen Interfaces

This ticket owns integration through the existing machine/device interfaces,
layer registration and profile metadata. No private parallel machine API,
snapshot migration, CPU/UART/scheduler changes or Makefile edits. The existing
9-byte CXD request and 64-byte RX limits suffice. Keep files at or below 500
lines; extract snapshot-layer responsibilities into the named file if needed.
Preserve ticket 756's descriptor, two counters, logs, activation and timing.
The new layer must coexist with it, not replace its command provider globally.

## Evidence Inputs

E-SAP-COMPAT-GPS-REOPEN-239-001 proves the exact branch and synthetic responses.
Use all three component hashes from E-SAP-0011. The ten bytes `$PSS0000\r\n`
are parser fixtures, not physical status fields. Cached GNS `0x04cb` and the
version-seen byte explain why this branch needs neither GTIM nor GNS commands.
Wrong-prefix controls distinguish parser acceptance from an injected native
event. External probe outcomes are evidence, not production release goldens.

## Implementation

Add explicit layer `sapporo-2.39-gps-reopen`, two one-use interventions and an
aggregate ceiling of two. Keep lifecycle counters device-instance-owned and
serialized through the unchanged layer codec. Require the separately selected
initial GPS layer; never auto-enable it. Dispatch its original VER response
through its original provider/counters until consumed, and route only the new
exact GSR command to the new provider. Reject duplicate/ambiguous ownership.

At post-arm PC `0x00128e8c`, require completed initial startup/version, unused
reopen responses, aligned complete driver/UART SRAM spans, R5=R4+0x74,
R6=R4+0x270, callback four, pending seven, retry zero, mode 15 at +0x74,
flags two at +0x7f, logging flag zero at +0x7b, cached GNS `0x04cb` at +0x344,
version-seen byte `0x100588a3`=1, UART callback `0x0012890f` and +12=zero.
Do not predicate native scheduling return registers on diagnostic queue IDs.
Queue one ten-byte status after 10,000,000 ns through the existing RX/IRQ path.

After that intervention, accept exactly six bytes `@GSR\r\n` once and queue
the same ten-byte line after 10,000,000 ns. Unknown/repeated commands, wrong
state, disabled layers, exhausted budgets, RX busy and scheduling failures
refuse before queue/transport/counter/log mutation. Do not change CPU, RAM,
native state, requested mode, GPIO, event callbacks or source firmware.
Explicit create/reset/restore binding must preserve both layers' ownership.

Snapshot validation rejects reply-before-reopen, unattributed/excess hits,
missing initial GPS dependency and reopen progress before both initial hits.
No migration from one/two layers into three; counters and pending RX events
must round-trip across every phase. Historical snapshots retain their bytes.

## Tests and Commands

```sh
make test TEST_FILTER=sapporo_239_gps_reopen
make test TEST_FILTER=sapporo_239_gps
make test TEST_FILTER=sapporo_cxd5610
make test TEST_FILTER=sapporo_profile_239
make test TEST_FILTER=machine_snapshot
make check-task-contracts
make check-lines
make check
make sanitize
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_reopen
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_startup
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
```

Run the new regression before and after implementation; each filter must
select at least one test. Private gates validate every component/full-flash
hash before execution. Absent evidence may skip; a mismatch must fail.

## Acceptance

Success/refusal coverage for exact activation hashes, missing initial layer,
wrong profile/state/pointers, dependency progress, unknown/repeated commands,
per-trigger/aggregate bounds, busy RX and time/ID/sequence exhaustion. Refusal
must preserve device/scheduler/counters/logs. Verify machine-instance isolation,
delayed UART byte/IRQ ordering, reset and restore without implicit activation.

Two fresh three-layer runs reach native state ten and state twelve with retry
zero, one reopen hit, one GSR hit and the unchanged two initial GPS hits.
Capture and pin new production log/snapshot hashes; do not copy probe hashes.
Compare full logs/images byte-for-byte. Snapshot before reopen, with reopen
RX pending, with GSR RX pending and with both responses complete; all resume
to the same final state/log suffix. Reject malformed lifecycle/event state
and wrong layer sets atomically. Existing one-/two-layer private gates pass
without re-pinning. Explicit limits: at most one billion instructions and
30 billion virtual ns per private run.

Identify and retain the later precise GSTP refusal separately; neither this
monitor failure nor full receiver liveness is solved by the two responses.

## Forbidden Scope

No additional status on arbitrary opens/retries, GSTP/GTIM/GNS/GUSE/other
command responses, real GPS status semantics, awake pulses, NMEA/fix/time
payloads, CPU/native-state repair, assertion bypass, file-budget expansion,
renderer changes, implicit activation or snapshot migration. No proprietary
bytes, pixels or private artifacts in Git. Do not claim full GPS or settled UI.

## Handoff

Integrator planning update, 2026-09-06: 756 is accepted and this ticket is
ready. Updating the existing startup test's profile enumeration assertion is
explicitly in scope when the third optional layer is added; no startup
behavior or historical golden may change.

Planning/evidence only; no new production behavior. Ticket 756's implementation
and uncommitted changes are preserved. This ticket remains blocked until the
integrator reviews 756 and updates its status separately. Evidence and exact
external source/trace/checkpoint hashes are in E-SAP-COMPAT-GPS-REOPEN-239-001.

2026-09-06 evidence-maintenance verification: `make check` passes all 752
normal cases and its build/line/CLI gates; `make check-task-contracts`
validates 127 tickets; `git diff --check` passes. The exact private startup
command in Tests and Commands passes without skips and retains both cold-run
goldens, four-phase resumes and the later retry refusal. `make sanitize` was
not rerun for this documentation/planning-only step; production C is unchanged
from ticket 756's verified working tree. New-ticket implementation acceptance
above remains outstanding.

The bounded external probe was built with:

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices -Isrc/compat /tmp/semu-gps-reopen.qNanQr/reopen-probe.c build/libsemu.a -o /tmp/semu-gps-reopen.qNanQr/reopen-probe
cmp /tmp/semu-gps-reopen.qNanQr/proof-a.trace /tmp/semu-gps-reopen.qNanQr/proof-b.trace
cmp /tmp/semu-gps-reopen.qNanQr/proof-a.log /tmp/semu-gps-reopen.qNanQr/proof-b.log
shasum -a 256 /tmp/sapporo-239-full-flash-exact.bin
```

Both comparisons pass, both negative-prefix controls refuse, and full flash
retains `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Changed repository files in this evidence step: `docs/migration-evidence.md`,
`docs/current-status.md`, `plans/index.tsv`, and this new ticket. All earlier
uncommitted files are preserved; existing ticket statuses are unchanged.
Requested integrator action: review 756, then unblock 757. No permission to
implement the later GSTP/liveness behavior is inferred from this evidence.
