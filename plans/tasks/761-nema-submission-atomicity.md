# 761 — NEMA Submission Atomicity and Refusal Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 110,500,502,506,513,615,759

## Goal

Make a GPU ring submission fail atomically and report its original diagnostic,
including later-child renderer failure and completion-admission failure.
Preserve every successful renderer publication and deterministic checkpoint.

## Execution Budget

Three model-days for the bounded cross-module transaction and regressions.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; tickets 110, 500, 502, 506, 513, 615 and 759;
E-NEMA-RING-001, E-NEMA-LISTS-001, E-SAP-UI-239-001,
E-EMU-SAMPLING-CLIP-001, E-EMU-NEMA-ATOMIC-001;
every existing Allowed File; `include/semu/types.h`, `include/semu/bus.h`,
`include/semu/frame.h`, `src/core/snapshot_io.h`,
`src/boards/machine_snapshot.c`, `src/boards/machine_snapshot_scheduler.c`,
`src/display/nema_tsc6a.h`, `src/display/nema_tsc6a.c`;
the exact external probe sources and hashes in both refusal evidence entries.

## Current Baseline

All dependencies are done. Commit `c7800be` contains accepted GPS-awake work
and the clipping correction; 780 normal and sanitizer tests pass. The 2.39
language-menu probe now accepts 79/79 submissions and repeats exactly.
E-EMU-NEMA-ATOMIC-001 demonstrates three uncovered failures: refused lists
leak inherited registers, a later child refuses after a frame is published,
and the second completion can refuse after the first event is admitted.
The GPU reports successful MMIO and consumes refused work. Existing opaque
single-list submission and single-event admission APIs cannot roll this back.

## Allowed Files

- `include/semu/display.h`, `include/semu/machine.h`
- `include/semu/scheduler.h`
- `src/core/scheduler.c`, `src/core/scheduler_internal.h`
- `src/core/scheduler_batch.c`
- `src/display/nema_backend.c`, `src/display/nema_backend.h`
- `src/display/nema_backend_internal.h`, `src/display/nema_backend_draw.c`
- `src/display/nema_backend_transaction.c`
- `src/display/nema_state.c`, `src/display/nema_state.h`
- `src/display/nema_framing.c`, `src/display/nema_framing.h`
- `src/display/nema_completion.c`, `src/display/nema_completion.h`
- `src/display/nema_completion_internal.h`, `src/display/nema_completion_snapshot.c`
- `src/display/nema_diagnostics.c`, `src/display/nema_diagnostics.h`
- `src/devices/sapporo_nema_gpu.c`, `src/devices/sapporo_nema_gpu.h`
- `src/devices/sapporo_nema_gpu_internal.h`
- `src/devices/sapporo_nema_gpu_submission.c`, `src/devices/sapporo_nema_gpu_snapshot.c`
- `src/boards/machine.c`, `src/boards/machine_internal.h`
- `src/frontends/cli.c`
- `tests/unit/test_transcript.c`, `tests/unit/test_nema_backend.c`
- `tests/unit/test_nema_backend_atomic.c`, `tests/unit/test_nema_state.c`
- `tests/unit/test_nema_framing.c`, `tests/unit/test_nema_diagnostics.c`
- `tests/unit/test_nema_completion_snapshot.c`, `tests/unit/test_nema_completion_atomic.c`
- `tests/unit/test_scheduler_batch.c`
- `tests/devices/test_nema_gpu.c`, `tests/devices/test_nema_gpu_snapshot.c`
- `tests/devices/test_nema_gpu_atomic.c`
- `tests/integration/test_sapporo_display.c`, `tests/integration/test_nema_refusal.c`
- `tests/unit/test_sapporo_239_gps_awake_snapshot.c`
- `docs/execution-model.md`, `docs/current-status.md`, `docs/migration-evidence.md`
- This ticket

## Frozen Interfaces

This is the integration owner for the smallest transactional extension of
the existing display backend/machine and scheduler contracts. Define ownership,
bounded preparation, abort, commit and callback lifetime in the existing public
interfaces. Do not bypass opaque contexts, type-check function pointers to
special-case the normal backend, or introduce a private parallel backend API.
A single-list convenience entry point may delegate to the same transaction.

Atomicity includes all children and markers in one active CMDRINGSTOP write,
not merely one draw. Stage inherited NEMA state, RGB565 pixels, TSC6A shadows
and ordered per-child publications. Admit all completion events atomically
before any irreversible commit/callback. Commit must not allocate or fail
after scheduler admission. A scheduler batch/reservation must preserve existing
success-path IDs, insertion sequences, deadlines and equal-time ordering.
No fake dry run against a mutating backend, snapshot-based rollback of an
unknown backend, or deferred error after publishing partial output.

Preserve the existing machine/GPU/completion snapshot encoding, successful
frame generations and callback order, bootstrap/no-IRQ semantics, marker-only
behavior and 100-us completion delay. Transient transaction state is not
serialized. Reject reentrant submission/reset conflicts before mutation;
document callback restrictions. Split the existing 486-line GPU and 455-line
backend by responsibility into the named files as necessary. No Makefile,
profile, registry, CPU/bus fault policy or persistent-format change is allowed.

## Evidence Inputs

E-EMU-NEMA-ATOMIC-001 supplies synthetic success controls and three exact
failures. E-SAP-UI-239-001 supplies the swallowed single-child refusal and
consumed-stop reproducer. Architecture/execution require fail-closed
transactions and validation before mutation. E-NEMA-RING-001 and
E-NEMA-LISTS-001 define the existing successful framing/completion behavior.
No new physical GPU register, program or timing claim is inferred.

## Implementation

First promote the external failures into narrow regressions and run them
against the baseline. Retain the first failing code/text throughout draw,
diagnostic, backend and GPU layers. Diagnostic recording must not turn REFUSE
into an empty outward error. Define a bounded fallback error for a callback
that refuses without one, unexpected WAIT and invalid results; never consume
such a submission as success.

Prepare and validate the complete ring operation before committing inherited
state, pixels, GPU registers/pointers/generation or completion queue/counters.
Refusal leaves those states byte-identical and emits no frame or IRQ; bounded
failure diagnostics may change. A corrected retry of the same ring stop must
execute once, proving refusal did not consume it. Normal repeated stops remain
no-ops. Do not reset all inherited state as a substitute for rollback.

Cover malformed framing, missing backend and completion configuration, draw
errors after earlier successful draws/children, marker capacity/ID/sequence/
deadline/allocation failure, null error sinks and callback lifecycle. Preserve
known successful native command interpretation; do not globally narrow rings
to one child or marker to evade the missing transaction boundary. Observe the
normal CPU/bus refusal path with a bounded synthetic guest; do not change fault
semantics or fabricate guest recovery. If the allowed interface cannot meet
atomicity and preserved success pins, stop with the smallest additional request.

## Tests and Commands

```sh
make test TEST_FILTER=nema_backend_atomic
make test TEST_FILTER=nema_gpu_atomic
make test TEST_FILTER=nema_completion_atomic
make test TEST_FILTER=scheduler_batch
make test TEST_FILTER=nema_refusal
make test TEST_FILTER=nema
make test TEST_FILTER=transcript
make test TEST_FILTER=machine_snapshot
make check-task-contracts
make check-lines
make check
make sanitize
make sdl
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
```

Require actual selection in every focused group. Rebuild/adapt the external
E-EMU-SAMPLING-CLIP-001 observer to the public transaction contract without
altering guest state/input timing, then run two cold middle-button probes and
the 700-million-prefix resume. Record its revised source hash and exact commands.
Validate all private components before execution; missing may skip, mismatch
must fail. No private artifact enters Git.

## Acceptance

All new regressions fail before implementation and pass afterward. Include
first-child refusal, successful first/failed second child, a real backend list
with a valid color write followed by invalid syntax then an inherited draw,
and two markers with only one remaining event ID. Compare full state, queue
contents/IDs/sequences, frames and IRQ transcripts, including corrected retry,
reset/reuse, pending-event snapshot round trips and at least two instances.
Force allocation failure deterministically rather than exhausting host memory.

The old diagnostic failures become explicit refusals with zero partial output.
Successful controls preserve two child publications and both completion events
in order. Single-list callers get the same atomic inherited-state guarantee.
Strict malformed-input refusals, optional no-backend configuration and bounded
CPU-path behavior are documented and tested without inventing hardware support.

Private 2.39 awake log/snapshot pins remain ticket 759's exact values, including
the intentional fifth-hit refusal. The short 2.22 SDL gate keeps all
E-SAP-ONBOARD-EMU-012 pins. The corrected 2.39 middle run retains 79 successful
submissions, frame CRC `6b6aa2dc`, final pixel/snapshot hashes and exact endpoint
from E-EMU-SAMPLING-CLIP-001. Any newly exposed authentic refusal is evidence
to investigate, not permission to skip the error or repin the acceptance gate.

## Forbidden Scope

No firmware writes/bytes, new GPS pulse or command, file/hit-budget extension,
physical-panel claim, opcode repair, permissive MMIO or draw fallback,
global read-as-zero, profile/registry update, snapshot migration, host-time
input or runtime dependency. No language-selection workflow extension yet.

## Handoff

Planning-only, 2026-09-06. Ready for integration, not implemented or accepted.
E-EMU-NEMA-ATOMIC-001 records the unchanged-runtime probes, exact hashes and
validation. The required integration is the transactional display contract
plus atomic completion admission; no private workaround is authorized.
Implementation must append changed files, exact commands/results, preserved
checkpoints, unsupported cases and any remaining integration request here.

### Partial Implementation — Scheduler and Completion Foundation

2026-09-06. Ticket remains incomplete; status/index are not changed by this
implementation. The existing display/GPU submission API has not yet migrated.

Implemented `semu_scheduler_schedule_batch` in the existing public scheduler
contract; single admissions delegate to it. Count/deadline/identity/allocation
refusal changes no queue, clock, counter or output ID. NEMA completion batch
admission stages all entries and deduplicates pending/repeated IDs before one
atomic scheduler call. Single completion failures now preserve full snapshots.
Reset/cancel/destroy remove owned callbacks; checked cancellation compares
ID/callback/context to avoid cancelling a new owner after scheduler ID reuse.
The completion codec is split without changing encoded bytes; GPU snapshot
restore rebinds the transient target-scheduler pointer.

Changed files for this part: `include/semu/scheduler.h`,
`src/core/scheduler.c`, `src/core/scheduler_batch.c`,
`src/display/nema_completion.c`, `.h`, `_internal.h`, `_snapshot.c`,
`src/devices/sapporo_nema_gpu.c` (snapshot rebind only),
`tests/unit/test_scheduler_batch.c`, `tests/unit/test_nema_completion_atomic.c`,
`tests/unit/test_nema_diagnostics.c` (destruction order), execution model,
current status, evidence ledger and this handoff. Existing planning changes
are preserved; no further index, profile, Makefile or persistent-format edit.

Evidence: E-EMU-NEMA-ATOMIC-001, E-NEMA-RING-001, E-NEMA-LISTS-001 and
the architecture/execution contracts. The narrow single-admission snapshot
regression fails before implementation; an ID-reuse cancellation regression
also failed during development before checked ownership was added. New API
tests cover two-entry ID/sequence/deadline/capacity refusals, no-op duplicates,
successful FIFO/identity/snapshot equivalence, two owners, cancellation/reset/
destruction and deterministic realloc failure in the production batch code.

Remaining acceptance: backend prepare/commit/abort and inherited-state/pixel/
shadow staging; whole-ring GPU use of the completion batch; preserved refusal
diagnostics and retry pointer; display/machine/frontend callback migration;
bounded CPU refusal proof and reentrancy. The `nema_backend_atomic`,
`nema_gpu_atomic` and `nema_refusal` groups are not implemented or claimed run
in this partial handoff. The original whole-ring three-failure probe is still
expected to fail. No additional integration authority is requested; continue
within this ticket's existing Allowed Files. Do not mark the ticket done.

Verification: `make test TEST_FILTER=nema_completion_atomic` (5),
`make test TEST_FILTER=scheduler_batch` (4), `make test TEST_FILTER=nema` (72),
`make test TEST_FILTER=transcript` (91, including five transcript codec cases),
`make test TEST_FILTER=machine_snapshot` (4), `make check-task-contracts`
(130 tickets), `make check-lines`, `make check` (789 tests), `make sanitize`
(789 tests), `make sdl`, and both exact private firmware gate commands above
pass. Two cold middle-button runs and the 700M-prefix resume retain every
E-EMU-SAMPLING-CLIP-001 trace/pixel/snapshot pin and the exact endpoint.
E-EMU-NEMA-BATCH-001 records the full commands, unchanged SHA-256 values,
observed pre-fix failures, evidence boundaries and remaining acceptance gaps.
