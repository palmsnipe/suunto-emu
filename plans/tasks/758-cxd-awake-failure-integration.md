# 758 — CXD Awake Pulse Failure Integration

**Status:** done
**Phase:** 7
**Dependencies:** 110,416,615,757

## Goal

Make awake-pulse failure atomic at admission and observable during dispatch,
preserving successful pulse timing and historical deterministic checkpoints.

## Execution Budget

Two model-days for the bounded scheduler/device interface and regressions.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 110, 416, 615 and 757;
E-EMU-CXD-AWAKE-FAILURE-001, E-EMU-COMPAT-ATOMIC-001,
E-SAP-COMPAT-GPS-AWAKE-239-001; every existing Allowed File;
`src/devices/sapporo_devices_snapshot.c`, `src/boards/machine_snapshot.c`,
`src/boards/machine_snapshot_scheduler.c`, `src/core/snapshot_io.h`,
`src/compat/sapporo_222.c`, `src/compat/sapporo_239_gps_reopen.c`;
the exact external atomic probe and hash recorded in the evidence entry.

## Current Baseline

All dependencies are done. `9f8c121` passes 759 normal/sanitizer cases and
ticket 757's private gates. A separate six-case host probe demonstrates
admission failures changing device state/signals and falling-edge scheduling
failures returning success with a zero-duration pulse. Production 2.39 has
no awake fixture; the existing 2.22 success path must remain reproducible.

## Allowed Files

- `include/semu/scheduler.h`
- `src/core/scheduler.c`, `src/core/scheduler_internal.h`
- `src/devices/sapporo_cxd5610.c`, `src/devices/sapporo_cxd5610.h`
- `src/devices/sapporo_cxd5610_internal.h`
- `src/devices/sapporo_cxd5610_snapshot.c`
- `src/cpu/armv7m/cpu.c`, `src/cpu/armv7m/sleep.c`
- `src/boards/machine_run.c`
- `tests/unit/test_scheduler_callback_failure.c`
- `tests/unit/test_cpu_sleep.c`
- `tests/devices/test_sapporo_cxd5610_atomic.c`
- `tests/devices/test_sapporo_cxd5610.c`
- `tests/devices/test_sapporo_cxd5610_snapshot.c`
- `docs/execution-model.md`, `docs/current-status.md`
- `docs/migration-evidence.md`, this ticket

## Frozen Interfaces

This is the integration owner for the smallest scheduler callback-failure
reporting contract. Define it in the existing scheduler interface; do not
create a private parallel API or change every device callback ABI. Preserve
normal instruction cost, WFI behavior, event IDs/order, pulse delay and width,
and all successful snapshot encodings. No persistent-format migration,
profile, layer, registry or Makefile changes. The 493-line CXD implementation
must be split by responsibility into the named files if needed for the limit.

## Evidence Inputs

E-EMU-CXD-AWAKE-FAILURE-001 supplies the six reproducible failures.
The architecture/execution contracts require validation before mutation and
fail-closed execution. E-SAP-COMPAT-GPS-AWAKE-239-001 motivates the downstream
fixture but authorizes no new pulse trigger in this ticket.

## Implementation

Add the narrowest public callback-error propagation needed for dispatch to
report a device's failure to its caller, including CPU instruction and WFI
advancement. First failure wins, error text/code survive, and remaining due
events are not drained after failure. Define reset/reentrancy behavior and
avoid retaining a borrowed error pointer. Normal callbacks remain unchanged.

For awake admission, validate/schedule before changing transport state or
calling the signal sink. An admission failure preserves full snapshot bytes,
queue/counters and signal transcript. Reject a known unrepresentable complete
pulse before admission. At the rising event, validate/arrange the falling
edge before emitting high; later ID/sequence/allocation failure must return
an explicit scheduler/device refusal rather than a truncated pulse or silent
success. Do not consume a fresh ID/sequence earlier on successful paths merely
to avoid this check. Preserve reset cancellation and callback-triggered reset.

No general batch/reservation framework is required. If correct propagation
cannot fit the frozen format/interfaces, stop with the smallest additional
integration request instead of weakening refusal or a historical golden.

## Tests and Commands

```sh
make test TEST_FILTER=scheduler_callback_failure
make test TEST_FILTER=sapporo_cxd5610
make test TEST_FILTER=cpu_sleep
make test TEST_FILTER=machine_snapshot
make check-task-contracts
make check-lines
make check
make sanitize
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_reopen
```

Run new regressions before implementation and require actual test selection.
Also run the available unchanged authentic 2.22 awake/startup gate, recording
its exact command and preserved checkpoints; validate all private components
before execution. A missing private component may skip, mismatch must fail.

## Acceptance

Admission time/ID/sequence exhaustion, busy transport and malformed arguments
leave transport/queue/IDs/sequences/signals unchanged. Cover time sufficient
for rise but insufficient for fall. Cover ID and sequence exhaustion after
admission but before rise; no high edge is emitted and advancement reports
failure. Cover direct scheduler dispatch, advance, CPU tick and WFI paths,
first-failure ordering, reset/reuse, and successful callbacks scheduled at the
same deadline. No silent continuation or fabricated guest exception repair.

Existing successful low/high/low timing, reset-during-high behavior, snapshot
round-trips and event-link refusals remain unchanged. Historical GPS/layer
goldens are not re-pinned. No new firmware fixture is enabled.

## Forbidden Scope

No GPS awake compatibility layer, additional UART status, GSTP response,
NMEA/time/fix, generic sensor behavior, instruction repair, arbitrary GPIO,
hit-budget increase, host time, snapshot migration or runtime dependency.

## Handoff

Implementation, 2026-09-06; acceptance pending, status/index unchanged.

Changed runtime files: `include/semu/scheduler.h`, `src/core/scheduler.c`,
`src/core/scheduler_internal.h`, `src/cpu/armv7m/sleep.c`,
`src/devices/sapporo_cxd5610.c`, `.h`, `_internal.h`, and `_snapshot.c`.
Changed tests: `tests/unit/test_scheduler_callback_failure.c`,
`tests/unit/test_cpu_sleep.c` and all three allowed CXD test files. Changed
documentation: this ticket, execution model, current status and evidence.
The pre-existing planning changes are preserved. No Makefile, persistent
format, profile, layer or registry change is needed.

Evidence: E-EMU-CXD-AWAKE-FAILURE-001 and E-EMU-COMPAT-ATOMIC-001 plus the
architecture/execution contracts; E-SAP-COMPAT-GPS-AWAKE-239-001 remains only
downstream motivation. Admission validates the complete pulse and queues rise
before state/signals. Fall is scheduled at the historical rising deadline
before high. A copied first callback error returns through dispatch/advance,
CPU tick and WFI/WFE; remaining due events stay queued. Reset cannot erase an
active failure. Recursive dispatch/advance refuses before mutation. Transient
diagnostics are not serialized. Reset from high cancels the pending fall.

Before implementation, `make test TEST_FILTER=sapporo_cxd5610_atomic` selected
five cases and failed the two new admission/dispatch regressions;
`make test TEST_FILTER=cpu_sleep` selected four and failed the new CPU case.
After implementation every command in Tests and Commands exits 0:
scheduler callback failure 4 cases; CXD 13; CPU sleep 4; machine snapshot 4;
129 task contracts; line checks; `make check` 768 cases and `make sanitize`
768 cases. Deterministic ID/sequence exhaustion and reported `NOMEM` are
covered; actual host allocator exhaustion is not forced. The unchanged
private 2.39 reopen gate passes without skips. Its first state-12 checkpoint
remains 825147087 instructions / 10875951888 ns, snapshot SHA-256
`0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`,
log `f63cab509a2da82a867580bf9eac35b3e764df53d08155bb11c47c6dfa328007`;
later GSTP remains refused at 940963736 instructions / 16349008531 ns.

The available authentic 2.22 SDL gate was not skipped, and was not re-pinned:

```sh
make sdl
build/suunto-emu validate --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
```

Build/validation exit 0; the gate exits 1. A clean `git archive HEAD` checkout
of starting commit `9f8c121` in `/tmp/semu-758-baseline.Qa0cHe` was built with
`make -j4 sdl`, then the same gate ran there with the absolute manifest path.
It fails identically: all CRCs match but both builds stop at PC `0x000bacf4`,
774081920 instructions / 6520939902 ns, not the historical 804398304 /
9504428769 tuple. Complete gate logs compare byte-identically, SHA-256
`a39f0b54d2ce24e6d420b2f83152b53826aadc6649d915fd0ed088c46c4e8124`.
This is a pre-existing baseline drift, not an introduced pulse difference.

An independent bounded headless comparison exercises the actual first awake
intervention. After validation and `make -j4 all` in the clean baseline, run
from this repository with `EMU` set first to
`/tmp/semu-758-baseline.Qa0cHe/build/suunto-emu`, then `build/suunto-emu`, and
`OUT` set to `/tmp/semu-758-baseline-222`, then `/tmp/semu-758-current-222`:

```sh
"$EMU" run --profile sapporo-2.22.60 --firmware /Users/cyril/projects/suunto-emu/tests/private/sapporo-2.22.60/firmware.semu --layer sapporo-2.22-no-device --max-instructions 1500000000 --max-time 12000000000 --snapshot-save "$OUT.sems" >"$OUT-headless.log" 2>&1
cmp /tmp/semu-758-baseline-222-headless.log /tmp/semu-758-current-222-headless.log
cmp /tmp/semu-758-baseline-222.sems /tmp/semu-758-current-222.sems
```

Both runs exit 3 (`stop=budget`), with the first awake hit at 10317472799 ns
and PC `0x000d4a8c`, 599774578 instructions / 12027701702 ns at stop. Both
comparisons exit 0. Log SHA-256
`1672376e5f2ba81738ebcf131c29be0dda5db4141bc5e2b859ba9885d0dca92c`,
snapshot `9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.
All private images/logs remain external. Unsupported GPS commands, physical
GNSS behavior and any 2.39 awake fixture remain unchanged.

Requested integrator action: investigate/resolve the pre-existing 2.22 SDL
stop-checkpoint drift in separately scoped work before accepting this ticket.
No golden change is authorized here, and ticket 759 remains blocked. Do not
mark 758 done while that mandatory authentic gate fails.

## Integrator Acceptance

2026-09-06; separate review/planning maintenance, no runtime edits. The
separately scoped E-SAP-ONBOARD-EMU-012 investigation resolves the earlier SDL
gate blocker to the existing haptic fixes, not this scheduler integration.
Source review confirms copied first-error ownership, fail-closed CPU tick and
sleep propagation, atomic pulse admission, unchanged successful event IDs and
snapshot encoding, and reset/reentrancy behavior. All dependencies are done.

Every command in Tests and Commands was rerun and exits 0: focused groups
4/13/4/4 cases, 129 task contracts, line checks, 768 normal and 768 sanitizer
cases, and the exact private 2.39 reopen gate without skips. Its state-12
snapshot/log hashes and later GSTP refusal remain those recorded above.
The three SDL commands above now all exit 0; the corrected gate enforces
the unchanged frame CRCs and cold-log SHA-256
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`.

The bounded 2.22 headless command above was also rerun with
`OUT=/tmp/semu-758-accept-222`: expected budget exit 3, identical stop and
first-awake boundary. Both log and snapshot compare byte-identically with
the clean baseline; the recorded hashes are unchanged. Acceptance logs are
external at `/tmp/semu-758-accept-{check,sanitize,private,live}.log`.

An external allocator-fault probe supplements the in-tree exhaustion tests;
E-EMU-CXD-AWAKE-FAILURE-001 records its exact source hash and build commands.
All three cases pass normally and under ASan/UBSan: actual scheduler allocation
refusal preserves admission state, falling-edge insertion reuses the popped
rise's capacity without early ID consumption, and allocation refusal inside
a callback preserves the error and queued tail. This is deterministic fault
injection, not physical host-memory exhaustion.

Ticket 758 is accepted; ticket 759 is ready, not implemented. No additional
integration interface is requested. The longer manual-entry SDL onboarding
gate remains a separate audit, not a claimed validation result here. No new
GPS trigger, UART response, fix/time data or physical receiver claim is made.
