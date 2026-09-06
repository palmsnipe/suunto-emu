# 759 — Sapporo 2.39 Bounded GPS Awake Integration

**Status:** done
**Phase:** 7
**Dependencies:** 757,758

## Goal

Integrate exactly four synthetic GPIO24 awake pulses as a separate explicit
hash-pinned layer, preserving native IRQ handling and all earlier GPS layers.

## Execution Budget

Two model-days after the pulse failure integration is accepted.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 757 and 758;
E-SAP-0011, E-SAP-COMPAT-GPS-REOPEN-239-001,
E-SAP-COMPAT-GPS-AWAKE-239-001, E-EMU-CXD-AWAKE-FAILURE-001;
every existing Allowed File; `include/semu/{bus,compat,scheduler}.h`,
`src/core/scheduler_internal.h`, `src/core/snapshot_io.h`,
`src/devices/sapporo_cxd5610.{c,h}`, `src/devices/sapporo_devices_snapshot.c`,
`src/compat/sapporo_239_gps_reopen.{c,h}`;
pristine `0x00128ebc..0x00128ed8`, `0x0012907a..0x001290c2`,
`0x001291c4..0x00129258`, `0x00128926..0x0012892e`, `0x000a5e88..0x000a5f00`,
IRQ table `0x001b25dc`, literal `0x00129540`, configuration table `0x000a6040`;
the exact external awake probe source and trace hashes in the evidence entry.

## Current Baseline

The following is the pre-implementation baseline; current acceptance is
recorded in Integrator Acceptance below.

757 and 758 are accepted. Four external diagnostic pulses produce four native awake
IRQs and five successful state-twelve polls without new UART commands. Zero
or late pulses retain the GSTP fault. The production transport now has
758's reviewed admission atomicity and falling-edge error propagation.
The four-pulse compatibility layer is not implemented yet.

## Allowed Files

- `src/compat/sapporo_239_gps_awake.c`, `src/compat/sapporo_239_gps_awake.h`
- `src/devices/sapporo_device_compat.c`, `src/devices/sapporo_devices.c`
- `src/devices/sapporo_devices.h`, `src/devices/sapporo_devices_internal.h`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `src/boards/machine_snapshot.c`, `src/boards/machine_snapshot_layers.c`
- `src/boards/machine_internal.h`
- `profiles/sapporo/2.39.20/profile.semu`
- `tests/unit/test_sapporo_239_gps_awake.c`
- `tests/unit/test_sapporo_239_gps_awake_snapshot.c`
- `tests/unit/test_sapporo_profile_239.c`
- `tests/unit/test_sapporo_239_gps.c` (enumeration assertion only)
- `tests/unit/test_sapporo_239_gps_reopen.c` (enumeration assertion only)
- `tests/integration/test_firmware_sapporo_239_gps_awake.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`, this ticket

## Frozen Interfaces

This ticket owns the machine/device binding, profile metadata and layer
integration, not scheduler/CXD/GPIO changes. Use the accepted 758 transport
contract. Preserve all earlier descriptor identities/counters/logs/timing and
snapshot bytes; no implicit migration from three layers to four. Counter
storage must be instance-owned. No Makefile or runtime dependency change.
Extract snapshot-layer validation into the named file before exceeding 500
lines; do not introduce a private parallel machine API.

## Evidence Inputs

E-SAP-COMPAT-GPS-AWAKE-239-001 proves the exact native pin, callback, successful
branch and four bounded pulses. Delay 100,000,000 ns and high width 1,000,000
ns are synthetic fixture choices, not recovered physical receiver cadence.
Use all three component hashes from E-SAP-0011.

## Implementation

Add explicit `sapporo-2.39-gps-awake`, with one intervention limited to four
hits and aggregate four. Require separately selected startup and reopen
layers; never auto-enable either or replace their UART provider. Both must
have completed their two-hit lifecycles before any pulse.

At `0x001291cc`, require R0=1, R2=12, R4=`0x100588a2`, a complete aligned
driver SRAM span at R8, R5=R8+0x26c, R6=R8+0x314, R7=R8+0x75, callback 12,
pending ten, retry zero, awake byte one and evidenced GPIO24 configuration
`0x93`. Validate the spans before reads. Do not predicate R1 or other native
queue-return values. Schedule one pulse through the existing transport,
then record one hit only after successful admission. Do not write the awake
byte, CPU registers, native event callbacks or GPIO directly from the layer.

Create/reset/restore must bind explicitly and reject duplicate/ambiguous
owners and missing dependencies. Snapshot lifecycle validation requires
enabled layers, attributed hits within four and completed dependencies for
any awake progress. Unknown layer order/identity or malformed pending-pulse
links refuse atomically. No fifth pulse: the fifth successful branch must
refuse before changing queue/device/counters/logs.

## Tests and Commands

```sh
make test TEST_FILTER=sapporo_239_gps_awake
make test TEST_FILTER=sapporo_239_gps
make test TEST_FILTER=sapporo_cxd5610
make test TEST_FILTER=scheduler_callback_failure
make test TEST_FILTER=sapporo_profile_239
make test TEST_FILTER=machine_snapshot
make check-task-contracts
make check-lines
make check
make sanitize
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_reopen
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_startup
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_activity_budget
```

Regress before implementation. Private gates validate every component and
full-flash hash before execution; absent evidence may skip, mismatch fails.

## Acceptance

Success/refusal cases cover exact hashes/profile, layer-off, dependency
presence/progress, wrong registers/spans/state/GPIO config, busy pulse,
time/ID/sequence failure, duplicate ownership, all four hits and fifth-hit
refusal, reset/rebind and machine isolation. The normal GPIO IRQ invokes
the native callback; a direct RAM-state change is not an equivalent test.

Two fresh four-layer runs reach the fifth successful state-twelve boundary
with exactly four pulses, four IRQ callbacks, unchanged startup/reopen hits
and retry zero. The diagnostic expectation is instruction 1,272,353,867 /
32,770,943,068 ns, PC `0x001291cc`; capture production hashes independently.
Limit each run to at most 1.3 billion instructions and 35 billion virtual ns.
The next attempted hook must refuse exactly there without another pulse/hit.
Snapshots before the first pulse, waiting for rise, high waiting for fall,
and after four completed pulses resume to the identical final image/log suffix.
Malformed pulse links/lifecycles and incompatible layer sets fail atomically.
All earlier private gates retain their hashes; do not re-pin to hide a change.

## Forbidden Scope

No GSTP or other UART responses, extra startup statuses, fabricated NMEA/time/
fix, indefinite heartbeat, physical cadence claim, expanded file budgets,
guest repair, assertion bypass, snapshot migration or settled/full-GPS UI claim.

## Handoff

Planning-only, 2026-09-06; blocked on 758. External four-pulse proof and both
negative controls are recorded in E-SAP-COMPAT-GPS-AWAKE-239-001, with no
production behavior change. The fifth-hook refusal is intentional evidence-
bounded behavior, not permission to keep generating pulses. Integrator must
review 758 before making this ticket ready.

Integrator update, 2026-09-06: ticket 758's source review and all mandatory
gates pass, including the corrected 2.22 SDL gate under E-SAP-ONBOARD-EMU-012.
Both dependencies are done; this ticket is now ready. Its scope, four-hit
bound and acceptance conditions are unchanged. No production layer is enabled
by this planning update.

## Implementation Handoff

2026-09-06; implemented and acceptance-tested, status/index unchanged for
separate integrator review. The earlier uncommitted work is preserved.

Changed files: `src/compat/sapporo_239_gps_awake.{c,h}`;
`src/devices/sapporo_device_compat.c`, `sapporo_devices.c`,
`sapporo_devices.h`, `sapporo_devices_internal.h`;
`src/boards/machine.c`, `machine_run.c`, `machine_internal.h`,
`machine_snapshot.c`, `machine_snapshot_layers.c`;
`profiles/sapporo/2.39.20/profile.semu`;
`tests/unit/test_sapporo_239_gps_awake.c`,
`test_sapporo_239_gps_awake_snapshot.c`, `test_sapporo_profile_239.c`,
`test_sapporo_239_gps.c` and `test_sapporo_239_gps_reopen.c` (the last two
only update enumeration); `tests/integration/test_firmware_sapporo_239_gps_awake.sh`;
this ticket, current status and migration evidence. No scheduler/CXD/GPIO,
CPU implementation, Makefile, external firmware or persistent format changes.

References: E-SAP-0011, E-SAP-COMPAT-GPS-REOPEN-239-001,
E-SAP-COMPAT-GPS-AWAKE-239-001, E-EMU-CXD-AWAKE-FAILURE-001 and the
architecture/execution/compatibility contracts. The pristine instruction
ranges, tables and literal in Required Reading were inspected read-only.
The external awake probe source, positive trace and log match their ledger
SHA-256 pins. Only synthetic cadence is established.

The layer validates every specified register/span/state/GPIO predicate and
both completed GPS dependencies. R1 is deliberately not a predicate. One
pulse is admitted through the existing transport before committing its hit;
the native instruction still executes. The single counter and descriptor are
instance-owned, reset/rebound explicitly, and serialized in the existing
layer encoding. VER/GSR retain their original providers and budgets. Binding
validates every owner before mutation; snapshot validation rejects disabled,
unattributed or excessive awake progress and pending pulses without a hit.
Layer set/order and existing pulse event links remain strict. The snapshot
layer codec was extracted into the named file without changing encoded bytes.
Every new C/header file is below 300 lines; all files satisfy the hard limit.

Before implementation, `make test TEST_FILTER=sapporo_239_gps_awake` selected
one activation regression and failed because the fourth profile layer was
absent. After implementation every command in Tests and Commands exits 0,
without private skips: awake 7 cases, all GPS 20, CXD 13, callback failure 4,
profile 5, machine snapshots 4; 129 task contracts; line checks;
`make check` 775 cases and `make sanitize` 775 cases. Final full-suite logs
are `/tmp/semu-759-final-{check,sanitize}.log`; private gates are recorded at
`/tmp/semu-759-private-{awake,reopen,startup,activity}.log`.

Two fresh production runs match at the intentional fifth-hook refusal:
`stop=compat-refused pc=0x001291cc instructions=1272353867 virtual_time_ns=32770943068`
(CLI exit 3). Callback 12, pending ten, retry zero, awake one, GPS hits
`2,2,4`, GPIO24 low and no pending awake event. Complete log SHA-256
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`;
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
Four saved phases (before first hook, pending rise, high pending fall, and
four pulses completed) reproduce that exact image and concatenated log suffix.
The observer verifies all four native callback entries and their STRB results
without injected signals or guest writes, then reaches the same final image.
Repeating the refused boundary preserves the whole snapshot and emits no hit.
Wrong layer sets/order, missing dependencies and duplicate owners refuse.

The unchanged private startup, reopen and activity gates all pass with their
original hashes. A supplementary 2.22 first-awake comparison also passes:

```sh
build/suunto-emu validate --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu
build/suunto-emu run --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu --layer sapporo-2.22-no-device --max-instructions 1500000000 --max-time 12000000000 --snapshot-save /tmp/semu-759-222.sems > /tmp/semu-759-222-headless.log 2>&1
cmp /tmp/semu-758-baseline-222-headless.log /tmp/semu-759-222-headless.log
cmp /tmp/semu-758-baseline-222.sems /tmp/semu-759-222.sems
git diff --check
```

Validation/comparisons/diff check exit 0; the bounded run has expected budget
exit 3. Log SHA-256 remains
`1672376e5f2ba81738ebcf131c29be0dda5db4141bc5e2b859ba9885d0dca92c`,
snapshot `9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.
The short/long SDL walks were not rerun in this ticket. No artifact containing
firmware, guest snapshots or frame pixels is committed.

Remaining scope: no fifth pulse, GSTP response, NMEA/time/fix, physical GNSS
cadence or settled/full-GPS UI claim. No additional integration interface is
requested. Next action is integrator review, then separately evidenced work;
the intentional bound is not authority to add an indefinite heartbeat.

## Integrator Acceptance

2026-09-06; separate integration-review/planning maintenance accepts 759.
Dependencies 757 and 758 are done. Review found no blocking issue in the
guarded pulse admission, instance ownership, reset/rebind, preserved UART
providers, snapshot codec extraction or lifecycle/event-link validation.
The synthetic tests cover successful operations and atomic refusals; the
private verifier observes native callback execution without guest writes.
References remain E-SAP-0011, E-SAP-COMPAT-GPS-REOPEN-239-001,
E-SAP-COMPAT-GPS-AWAKE-239-001 and E-EMU-CXD-AWAKE-FAILURE-001.

Every exact command in Tests and Commands was rerun and exited 0. Focused
groups selected 7/20/13/4/5/4 cases respectively; `make check` and
`make sanitize` each passed 775 cases. All four private firmware gates
validated the exact components and full flash and passed without skips.
The awake gate rechecked two cold runs, four snapshot resumes, all four
native IRQ callbacks and the mutation-free fifth-hook refusal at
1,272,353,867 instructions / 32,770,943,068 ns / PC `0x001291cc`.
Its complete log and snapshot retain the production hashes recorded above;
the startup, reopen and activity gates retain their earlier pins.
129 task contracts and the hard line limit pass. Review logs are
`/tmp/semu-759-review-{check,sanitize,lines,awake,reopen,startup,activity}.log`.

This review changes only `plans/index.tsv`, this ticket and
`docs/current-status.md`; all pre-existing implementation work is preserved.
No new runtime behavior, fixture, hash pin or firmware artifact is added.
The supplementary 2.22 headless comparison remains the implementation-turn
result above; SDL walks were not rerun during this review. No integration
interface change is requested. Further work must gather evidence within the
accepted bound before proposing any new behavior; GPS fix/time, physical
cadence and a settled 2.39 UI remain unsupported.
