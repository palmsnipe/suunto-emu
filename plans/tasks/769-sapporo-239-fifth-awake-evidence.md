# 769 — Sapporo 2.39 Fifth GPS Awake Evidence

**Status:** done
**Phase:** 7
**Dependencies:** 759,768

## Goal

Measure one further synthetic awake lifecycle after native time persistence,
its native IRQ/flag/poll effects, deterministic restore and next independent
boundary. Distinguish diagnostic acceptance from physical GPS evidence.

## Execution Budget

One model-day; every experiment caps total execution at six billion
instructions, 45 billion virtual ns, 5,000 frames and 256 observation records.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
ticket 768; E-SAP-COMPAT-GPS-AWAKE-239-001, E-SAP-GPS-FIFTH-239-001;
`src/compat/sapporo_239_gps_awake.{c,h}`;
`src/boards/{machine_internal.h,machine_snapshot_layers.c}`;
`src/devices/sapporo_cxd5610.{c,h}`;
`include/semu/{machine,compat,bus,scheduler}.h`;
`tests/integration/sapporo_239_personal_probe.c`;
`src/frontends/cli_snapshot.c`, `src/display/nema_backend.h`.

## Current Baseline

Dependencies are done. Commit `44ee4d6` natively retains both time saves and
refuses the fifth pulse at `001291cc / 3960123530 / 32455738919`, GPS
`2,2,4`. Remaining driver predicates match; no pulse event is pending.

## Allowed Files

- This ticket, `docs/current-status.md`, `docs/migration-evidence.md`.
- External temporary observational probes and isolated diagnostic source
  copies; no firmware, modified runtime library, snapshots or pixels in Git.

## Frozen Interfaces

Production code, headers, Makefile, registry, profiles, four-layer identity,
four-pulse/file ceilings, snapshots, rendering and prior goldens are unchanged.
No counter replenishment, CPU/RAM repair or parallel production API.

## Evidence Inputs

Exact E-SAP-0011 components; full flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`;
native-time final snapshot
`ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`.
Existing independently recovered IRQ callback `00128926`, flag `100588a2`,
poll `001291cc`, 100-ms synthetic delay and 1-ms pulse width are the hypothesis.

## Implementation

Repeat the unchanged production refusal. An isolated copy of the awake module
may use five total hits, keeping all lifecycle predicates and pulse semantics.
Validate components, flash and input snapshot before execution. Trace native
callback entry and STRB, the next poll, immutable counters and the next refusal.
Compare two runs and restores with the rise pending and after the callback.
No-pulse and six-second-late controls may omit only the exhausted-bound
admission in the isolated module to observe natural firmware recovery; do not
replace a CPU instruction, native state change or unsupported UART response.
An optional MIDDLE press/release at 34 seconds/70 ms later may expose the next
navigation boundary, but stop at its first independent refusal without changing
file limits. Record all actual input edges; do not infer a new UI milestone.

## Tests and Commands

Record exact external compile/run commands and source/trace/snapshot hashes.
Use production-only refusal, repeated five-hit diagnostics, pending/completed
pulse restores and missing/late controls; source flash must remain unchanged.
Run `make test TEST_FILTER=sapporo_239`, `make test TEST_FILTER=machine_snapshot`,
`make check-lines`, `make check-task-contracts`, `make check`, `git diff --check`.
Run the external diagnostic with ASan/UBSan when practical. No private input
skip may be reported as completion.

## Acceptance

One additional native IRQ/STRB/poll sequence demonstrated without guest repair;
exact repeated/resumed final state and log suffix; refusal controls; minimum
proposed integration scope and unsupported cases explicitly recorded.
Leave ticket status unchanged during implementation.

## Forbidden Scope

No production fifth pulse, indefinite heartbeat, physical-cadence claim, GPS
fix/time/coordinates, NMEA or GSTP response, storage-budget change, firmware
patch, private-state repair, new runtime dependency or weakened golden.

## Handoff

2026-09-08: local acceptance observations pass; status remains ready for
separate integrator review. E-SAP-GPS-FIFTH-239-002 records source/trace hashes,
all repeat/restore checkpoints, controls and the additional navigation boundary.
Changed this ticket and `docs/{current-status,migration-evidence}.md` only;
the preceding separate planning review accepted 768 and added this index row.
Commit `44ee4d6` contains the prior time-routing work; new evidence is uncommitted.

The isolated fifth pulse invokes the native GPIO callback and STRB, permitting
the next successful state-twelve poll with retry zero. Two runs and three
phase restores match final `127214e55e966741d3cc3acb5fd5fad50988b3cb4bdadb78788e590b91f8df28`
at `001291cc / 4071207676 / 37929735196`; a sixth admission refuses atomically.
ASan/UBSan produces the identical complete trace/log/final image with no finding.
Both missing and six-second-late controls reach the same native UART fault,
not a successful poll. Wrong snapshot hash and production loading of a five-hit
snapshot refuse. Original production refusal and source flash remain unchanged.

Two normal MIDDLE-click diagnostics reach HEIGHT (170 cm), 69 publications,
CRC `cd4c0a99`, before mode-two `settings/personal` exceeds the unchanged
file budget. An independent live-run capture and snapshot inspection confirm
that path and frame. Native-time start and pulse intermediate hashes are in
Evidence Inputs/the ledger; every run checks all component/full-flash/input
identities before execution. All artifacts remain outside Git.

### Exact reproduction commands

The external source pins are in E-SAP-GPS-FIFTH-239-002. The following uses
the same explicit arguments as the completed runs; `diag_root` is a task-local
alias, never a firmware source directory. Bounds are compiled into `probe.c`.

```sh
diag_root=/tmp/semu-769.py1saC
start_image=/tmp/semu-768.42dyOp/native-a.final.sems
start_hash=ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/probe.c" "$diag_root/awake.c" build/libsemu.a -o "$diag_root/probe"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/probe.c" build/libsemu.a -o "$diag_root/production"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -DDIAG_NO_PULSE -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/probe.c" "$diag_root/awake.c" build/libsemu.a -o "$diag_root/no-pulse"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -DDIAG_DELAY_NS=6000000000ULL -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/probe.c" "$diag_root/awake.c" build/libsemu.a -o "$diag_root/late-pulse"
for attempt in first second; do
    "$diag_root/probe" "$diag_root/$attempt" "$start_image" "$start_hash" idle >"$diag_root/$attempt.trace" 2>"$diag_root/$attempt.log"
done
"$diag_root/production" "$diag_root/production" "$start_image" "$start_hash" idle
"$diag_root/no-pulse" "$diag_root/none" "$start_image" "$start_hash" idle
"$diag_root/late-pulse" "$diag_root/late" "$start_image" "$start_hash" idle
"$diag_root/probe" "$diag_root/resume-pending" "$diag_root/first.pending.sems" 3a8d9d5e24ccd74df852681b8e80097272095e2b2b55443ed543280c8a74d674 idle
"$diag_root/probe" "$diag_root/resume-high" "$diag_root/first.high.sems" 03712d1dea6acbb028b5a3b04fd2c1b5d6f2df848bcdf96ece85a72f70f274e4 idle
"$diag_root/probe" "$diag_root/resume-fallen" "$diag_root/first.fallen.sems" f545b28f0ac7575c3ce29294487231136210512a64a3db505a77d6a85eb27452 idle
for attempt in middle middle-repeat; do
    "$diag_root/probe" "$diag_root/$attempt" "$start_image" "$start_hash" middle
done
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/probe.c" "$diag_root/awake.c" build/sanitize/libsemu.a -o "$diag_root/probe-sanitize"
"$diag_root/probe-sanitize" "$diag_root/sanitize" "$start_image" "$start_hash" idle
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices "$diag_root/inspect.c" "$diag_root/awake.c" build/libsemu.a -o "$diag_root/inspect"
"$diag_root/inspect" "$diag_root/inspect" "$diag_root/middle.final.sems" 00432bcc97bc988da8370e9e2a298a39bdfa86971e5a8000ccd36dafaaf5a286 idle
"$diag_root/inspect" "$diag_root/middle-image" "$start_image" "$start_hash" middle
sips -s format png "$diag_root/middle-image.ppm" --out "$diag_root/middle-image.png"
cmp "$diag_root/first.trace" "$diag_root/second.trace"
cmp "$diag_root/first.log" "$diag_root/second.log"
for attempt in first second resume-pending resume-high resume-fallen sanitize; do
    cmp "$diag_root/first.final.sems" "$diag_root/$attempt.final.sems"
    cmp "$diag_root/$attempt.final.sems" "$diag_root/$attempt.refused.sems"
done
cmp "$diag_root/production.final.sems" "$start_image"
cmp "$diag_root/middle.trace" "$diag_root/middle-repeat.trace"
cmp "$diag_root/middle.final.sems" "$diag_root/middle-repeat.final.sems"
cmp "$diag_root/middle.final.sems" "$diag_root/middle-image.final.sems"
shasum -a 256 /tmp/sapporo-239-full-flash-exact.bin
```

All listed normal/focused/snapshot/line/task-contract/check/diff commands pass:
832 normal cases, 47 Sapporo 2.39 cases, four snapshot cases, 137 contracts.
Sanitizers were run on the complete isolated firmware diagnostic using the
existing instrumented library, not rerun across the unchanged production suite.
The prior ticket's 832-case sanitizer and private-gate results remain historical,
not claimed as fresh runs. No production behavior changed after the commit.

### Remaining scope and integration request

Only a finite fifth synthetic pulse is demonstrated; no sixth pulse, physical
receiver cadence, GPS fix/time, setup completion or watch face is supported.
Any implementation must preserve the existing four-pulse layer identity and
its atomic fifth-refusal goldens. A separately selected extended fixture can
reuse the current hook/transport, but requires a named integration ticket
owning layer selection/registry, lifecycle validation and snapshot tests.
Do not silently raise the existing bound or migrate diagnostic snapshots.
After that, measure the complete additional personal-save suffix before
changing its file ceiling. Neither a larger storage allowance nor runtime
GPS implementation is included in this evidence task.

### Integrator acceptance, 2026-09-08

Separate planning review accepts evidence commit `15c2a53`: native fifth IRQ,
three phase restores, repeated/sanitized traces and missing/late controls all
meet acceptance. No production behavior changed. Ticket 771 owns the optional
five-pulse integration; the personal-save suffix remains a later evidence task.
