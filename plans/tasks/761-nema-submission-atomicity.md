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
- `src/display/nema_texture.c`, `src/display/nema_texture.h`
- `src/display/nema_a2le.c`
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
- `tests/unit/test_nema_texture.c`
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

Scope extension authorized by the user on 2026-09-08: the texture readers,
their existing header and focused unit tests above are now owned by this
integration ticket. Fix the E-EMU-NEMA-MEMORY-001 texture-read side effects
using existing bus contracts; preserve native success pins. Status/dependencies
are unchanged. This does not authorize a bus API/policy or format change.

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

### Partial Implementation — Backend Transaction Foundation

2026-09-06. The prior foundation was committed as `ce0e529`. This slice remains
uncommitted and incomplete for whole-ticket acceptance; index/status unchanged.
The existing public display header now defines bounded prepare/commit/abort
operations. The normal backend stages all inherited state, RGB565 pixels,
TSC6A shadow updates and ordered per-child frames before any publication.
Commit allocates nothing and cannot fail; single-list submission delegates
to that path. Original errors survive diagnostic saturation and NULL sinks.
Reset returns CONFLICT while active; callback lifetime/reentrancy rules are
explicit. No persistent encoding or guest timing changes.

Changed files: `include/semu/display.h`, `src/display/nema_backend.c`,
`nema_backend.h`, new `nema_backend_internal.h`, `nema_backend_draw.c`,
`nema_backend_transaction.c`, `nema_state.c`, `nema_state.h`, new
`tests/unit/test_nema_backend_atomic.c`, execution model, current status,
evidence ledger and this handoff. Public machine/GPU callers are intentionally
unchanged in this slice; no Makefile, profile or registry change.

Evidence: E-EMU-NEMA-ATOMIC-001, E-NEMA-RING-001, E-NEMA-LISTS-001,
architecture/execution contracts and follow-up E-EMU-NEMA-BACKEND-001.
Three baseline regressions fail for inherited leakage, empty syntax errors
and lost original draw errors. Seven final backend cases pass, including
later-child and intra-child failure, abort/retry, ordered publication,
independent instances, reentrancy/reset, shadow rollback, NULL sinks,
diagnostic saturation, bounded inputs and deterministic allocation failure.

Commands/results: `make test TEST_FILTER=nema_backend_atomic` (7),
`make sanitize TEST_FILTER=nema_backend_atomic` (7),
`make test TEST_FILTER=nema` (79), `make test TEST_FILTER=transcript` (91),
`make test TEST_FILTER=machine_snapshot` (4),
`make test TEST_FILTER=nema_completion_atomic` (5),
`make test TEST_FILTER=scheduler_batch` (4), `make check-lines`,
`make check-task-contracts` (130), `make check` (796), `make sanitize` (796),
`make sdl` and both exact private commands above pass. Two cold middle runs
and the 700M-prefix resume retain all E-EMU-SAMPLING-CLIP-001 pins. The
read-only observer is unchanged because the convenience callback remains;
its public transaction adaptation is still required at GPU migration.

Remaining: whole-ring GPU use of backend preparation and completion batch,
original refusal/fallback diagnostics through MMIO and bounded CPU proof,
retry/register/generation atomicity, strict framing/tail validation and
machine/frontend/mock callback migration. `nema_gpu_atomic` and `nema_refusal`
are not implemented or claimed run. The original whole-GPU probe still exits
1, now with two failures (later-child frame and partial completion admission).
No extra integration authority is requested. Do not mark ticket 761 done.

### Partial Implementation — GPU/Machine Transaction Integration

2026-09-06. Committed the prior backend slice as `0e8900d`; this continuation
remains uncommitted and ticket status/index are unchanged. Whole-ring GPU
submission now collects children/markers, prepares all children through the
public backend operations and atomically admits completion events before any
frame/register/stop/generation commit. Refusal propagates the first diagnostic
and preserves retry state. Missing configuration, WAIT/invalid results and
reentrant GPU writes/reset/snapshot refuse; machine reset checks the same guard
before changing epochs. Bootstrap, success callback order, marker-only timing,
repeated stops and snapshot encoding retain their prior behavior.

Changed files: `include/semu/machine.h`, `src/boards/machine.c`,
`src/boards/machine_internal.h`, `src/frontends/cli.c`,
`src/devices/sapporo_nema_gpu.c`, `.h`, `_internal.h`, new `_submission.c`
and `_snapshot.c`; `tests/devices/test_nema_gpu.c`, `_snapshot.c`, new
`_atomic.c`; `tests/unit/test_transcript.c`,
`tests/unit/test_sapporo_239_gps_awake_snapshot.c`, new
`tests/integration/test_nema_refusal.c`; execution model, status, evidence
ledger and this handoff. No CPU/bus, Makefile, profile or registry edits.

Evidence: E-EMU-NEMA-ATOMIC-001, E-NEMA-RING-001, E-NEMA-LISTS-001,
architecture/execution contracts; results and observer adaptation are recorded
in E-EMU-NEMA-GPU-001. Two GPU regressions fail before migration (MMIO falsely
succeeds and consumes partially published/admitted work). Six final GPU cases
cover later-child refusal/retry/repeat, completion ID/deadline/sequence failure,
marker payloads resembling opcodes, event order/reset, NULL error, callback
diagnostics/results, reentrancy and missing scheduler/backend operations.
The bounded CPU test executes exactly two steps: valid submission continues;
refusal takes the existing precise BusFault/HardFault path, with CFSR `8200`
and BFAR `400900ec`, without modifying CPU fault policy.

The original five-scenario probe, adapted only to the public constructor,
reports zero failures on two normal runs and one sanitizer run. Both exact
private gates above pass. The adapted read-only observer runs two cold middle
probes and the 700M-prefix resume with identical historical trace/pixel/snapshot
pins and endpoint; exact source hashes and commands are in the evidence entry.

Remaining acceptance: strict framing/wrap/odd-tail audit (including direct
fallback and non-power-of-two wrap arithmetic), whole-operation allocation
fault injection beyond the existing component allocator tests, and final
malformed-input/lifecycle acceptance review. Legacy permissive width handling
is not corrected or endorsed by this slice. No additional integration-owned
change or authority is requested. Do not mark ticket 761 done.

Commands/results: all exact ticket commands above pass with actual selection:
backend atomic 7, GPU atomic 6, completion atomic 5, scheduler batch 4, CPU
refusal 1, NEMA 86, transcript 91, machine snapshot 4. `make check` and
`make sanitize` each pass 803 tests; `make check-task-contracts` validates
130 tickets; `make check-lines` has no hard-limit violations. `make sdl`
and both private gate commands pass. Observer/probe commands and unchanged
hashes are recorded in E-EMU-NEMA-GPU-001; no acceptance pin was weakened.

### Partial Implementation — Control Validation and Allocation Coverage

2026-09-06. The prior GPU integration is committed as `1733bb8`. This
continuation remains uncommitted; ticket status/index are not changed.
Changed files: `src/display/nema_framing.c`, `.h`, `nema_backend.h` (stale
contract comment), `src/devices/sapporo_nema_gpu.c`,
`tests/devices/test_nema_gpu_atomic.c`, `tests/integration/test_nema_refusal.c`,
execution model, current status, evidence ledger and this handoff.

Corrected wrap-distance arithmetic for non-power-of-two capacities and checked
complete ring byte ranges. Complete held-control/marker fields now validate
before any callback; malformed targets/opcodes/sizes/truncation refuse.
Unsupported GPU byte/halfword accesses refuse without changing output/state.
Three framing regressions and the access-width regression fail before their
fixes. Framing cases were initially added to the already-large framing unit
test, then moved to the integration test to keep new handwritten growth below
300 lines; the original unit test remains unchanged.

A draft allowing only base-wrap targets failed both private gates. Read-only
2.22/2.39 disassembly proves a second native held-control form targeting the
immediate next word; the first-refusal probe confirms it dynamically. Added
that success regression before correcting the draft. Evidence
E-EMU-NEMA-CONTROL-001 records the exact PCs, component/source hashes and
commands. E-NEMA-RING-001 still supplies the bootstrap/wrap form; no generic
jump acceptance or new register/timing behavior is inferred.

The integration test compiles production framing, backend-transaction and
scheduler-batch sources with only their allocators replaced. It forces each
of the three allocation sites to fail through the real MMIO path while 15
events already exist and two children/two markers are pending. Full GPU codec,
queue bytes/counters, pixels/generation and inherited-color checks pass.
Corrected same-stop retries publish both children and deliver both marker
IDs/IRQs in order; repeated stops do nothing. No production fault hook, private
backend API, Makefile, CPU/bus policy or persistent-format edit.

Remaining: strict inline-ring/padding and odd-tail interpretation, plus final
malformed-input/lifecycle acceptance review. The native tail interpretation
still needs evidence reconciliation with existing success pins; this slice
does not read beyond a declared child list or silently invent a missing value.
No extra integration authority is requested. Do not mark ticket 761 done.

Final control-slice verification is recorded in E-EMU-NEMA-CONTROL-001:
backend atomic 7, GPU atomic 8, completion atomic 5, scheduler batch 4,
refusal integration 5, framing selection 15, NEMA 92, transcript 91, machine
snapshot 4; every focused command selects tests and passes. `make check` and
`make sanitize` pass 809 tests; task contracts validate 130 tickets and line
checks pass. `make sdl`, both exact private gates, two cold middle probes and
the prefix resume pass with all historical hashes unchanged. The original
atomicity probe reports zero failures twice normally and once under sanitizers.

### Partial Implementation — Complete Paired Lists

2026-09-06. Committed the prior control/allocation slice as `611d3c4`
(`Validate NEMA ring controls and allocation refusal paths`), then continued
ticket 761. This continuation remains uncommitted; index/status unchanged.
Changed files: `src/display/nema_framing.c`, `.h`,
`src/display/nema_backend_transaction.c`, `nema_backend.h`,
`tests/unit/test_nema_backend_atomic.c`, `tests/devices/test_nema_gpu_atomic.c`,
`tests/integration/test_nema_refusal.c`, execution model, current status,
evidence ledger and this handoff.

E-EMU-NEMA-TAIL-001 reconciles E-NEMA-LISTS-001 with the later read-only native
research: the rounded-tail theory was superseded by the CMDSIZE entry-count
correction. Supported lists contain complete register/value pairs. Framing
and backend range validation now refuse odd counts before emitting callbacks
or staging renderer work. No tail word is dropped, no value is fetched beyond
the declared list, and no hardware opcode/register or compatibility hook is
invented. Empty backend lists retain their no-publication behavior.

Three regressions failed on `611d3c4` before the implementation, with logs in
`/tmp/semu-761-syntax.Tuw7Uw/{backend,gpu,framing}-before.log`. They now pass,
covering ordinary/held unmatched tails, retained inherited blue color and
pixels/generation, zero framing callbacks, unchanged complete GPU snapshot,
zero frames/IRQs/completion events and a corrected same-stop paired-list retry.
All three changed handwritten tests remain below 300 lines.

The external `syntax-probe.c` SHA-256 is
`7d517824bf4f331791d983a43a97ac85a77bdf9be387c3017a8f7e3d28907282`.
It only observes active guest-RAM ring words before delegating to the production
submission function. The included UI observer remains SHA-256
`6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`;
manifest/full-flash validation and bounded input timing are unchanged.
No observer source or private artifact is copied into Git.

```sh
probe_dir=/tmp/semu-761-syntax.Tuw7Uw
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -DUSE_UI_OBSERVER -Iinclude -Isrc -Isrc/devices "$probe_dir/syntax-probe.c" build/libsemu.a -o "$probe_dir/syntax-239"
"$probe_dir/syntax-239" cold 1 "$probe_dir/paired-239" > "$probe_dir/paired-239.trace" 2> "$probe_dir/paired-239.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices "$probe_dir/syntax-probe.c" build/obj/src/frontends/cli.o build/obj/src/frontends/main_headless.o build/libsemu.a -o "$probe_dir/syntax-headless"
"$probe_dir/syntax-headless" run --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu --layer sapporo-2.22-no-device --max-instructions 1300000000 --max-time 22000000000 > "$probe_dir/paired-222.log" 2>&1
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-gpu.aQow5X/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

The 2.22 observer exits 3 at the expected budget, PC `000d4a8c`, instructions
`746431481`, time `22090668071`: 10 active stops, 5 child lists, zero odd counts.
The 2.39 observer exits 0: 158 stops, 79 child lists, zero odd counts. Both
observe 162 inline words across initialization prefixes. This is a bounded
inventory, not evidence that arbitrary inline words may be ignored. Remaining
work: strict inline-ring/padding grammar, ordered inline-state treatment and
final malformed-input/lifecycle acceptance review. No additional integrator
authority is requested and ticket 761 must not be marked done yet.

Two cold middle-button renderer probes and the 700M-prefix resume pass. The
renderer observer source remains E-EMU-NEMA-GPU-001's exact SHA-256
`b59e73c8220beaade48cee629a1da3b5f8b93b8b47a1cc6cd0be1a2e4ee32ab7`.
Cold runs accept 79 submissions; resume accepts 76. All retain final frame
CRC `6b6aa2dc`, pixels SHA-256
`f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`,
snapshot SHA-256
`4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`,
PC `000a7abc`, instructions `1300000000`, time `22286110403`.
Both cold traces retain SHA-256
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.

Final verification: every command in this ticket's Tests and Commands section
passes without skipped selection. Focused counts: backend atomic 8, GPU atomic
9, completion atomic 5, scheduler batch 4, refusal integration 6, NEMA 95,
transcript 91, machine snapshot 4. `make check` and `make sanitize` each pass
812 tests; `make check-task-contracts` validates 130 tickets and
`make check-lines` has no hard-limit violations. `make sdl` passes.
The exact private SDL live-input and 2.39 GPS-awake commands both pass,
including cold/resume/IRQ checks and their unchanged log/snapshot hashes:
2.22 log `9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`;
2.39 log `06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
The 2.39 gate still ends at its intentional fifth-hit compatibility refusal,
PC `001291cc`, instructions `1272353867`, time `32770943068`; this is not a
new renderer refusal. Logs are retained in the probe directory above.

### Partial Implementation — Shared Inline Ring Plan

2026-09-07. Continued without committing; the preceding paired-tail changes
remain intact and uncommitted. This slice changes `include/semu/display.h`,
GPU submission, backend list/transaction code, framing `.c`/`.h`, state `.c`/`.h`,
backend-atomic/completion-atomic/framing/transcript unit tests, GPU snapshot
tests, execution model, current status, evidence ledger and this handoff.
No index/status, Makefile, profile, firmware, CPU/bus policy or codec changes.

E-EMU-NEMA-INLINE-001 supplies the paired inline/NOP/bootstrap evidence. The
parser now produces one validated plan for ordered command spans and marker
IDs. The GPU no longer rescans values as potential marker opcodes, ignores
inline state, or submits the whole ring when a direct stream wraps. Known
inline register validation shares the state module's existing register map.
Child-only callbacks retain their contract and share the same parser.

The public display descriptor gains a flags field: zero retains ordinary
publication, `SEMU_DISPLAY_LIST_INLINE` stages commands without publication,
and other values refuse. All in-tree initializers are migrated. The backend
admits up to 64 spans; framing retains 32 children and 64 markers. A synthetic
32-child/32-inline plan succeeds, and the next span refuses without modifying
the output plan. The existing prepare/commit/abort ownership contract applies
to every span. Inline work after the last child commits final working pixels
without adding a child frame; commit remains allocation-free and infallible.

The mixed inline/child/completion regression fails before the fix: an unknown
inline command after a child is accepted. Final coverage includes ordinary and
held unknown registers, nonexact NOPs, a failing inline draw after two children,
unchanged GPU codec/pixels/frames/events, command-shaped values, ordered child
colors, unpublished trailing inline pixels and corrected/repeated stops. Two
old synthetic padding inputs also fail new negative assertions under the old
parser: an unmatched zero register and unmatched held graphics word. Their
corrected retries use exact NOPs; no native golden was changed. Additional
plan tests cover wrapped complete runs, truncated markers, unchanged output
on refusal, physical pair-split refusal and capacity boundaries. The framing
implementation is now 171 lines and GPU submission is 84 lines.

External evidence/log directory: `/tmp/semu-761-inline.JJ1n7Q`. The old framing
source extracted read-only from `611d3c4` has SHA-256
`24ec7912e86351503f17dffe17c0ca59f378de6f9cca91e230472cc7b6e4d1c3`.
Compiling the final framing tests with it yields exactly the two new padding
refusals as failures; `before.log` records the earlier mixed-command failure.
The revised renderer observer has SHA-256
`03aeafb4e9eb1e8a568aa7c024fe1cca57e3c77f5d0ddb7d1e18622abc33184a`.
Its only behavioral instrumentation change is to number/trace published child
descriptors, not the newly surfaced initialization spans. Every span still
passes unchanged to the production backend. The included UI observer remains
SHA-256 `6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`;
component/full-flash validation and bounded input timing are unchanged.

```sh
probe_dir=/tmp/semu-761-inline.JJ1n7Q
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/display -Itests/support tests/unit/test_nema_framing.c tests/support/test.c "$probe_dir/framing-before.c" build/libsemu.a -o "$probe_dir/framing-before"
"$probe_dir/framing-before" > "$probe_dir/framing-before.log" 2>&1
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices "$probe_dir/render-probe.c" build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

Both cold traces remain SHA-256
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.
Cold and prefix-resumed final pixels remain
`f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`,
snapshot `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`,
frame CRC `6b6aa2dc`, PC `000a7abc`, instructions `1300000000`,
time `22286110403`; 79 cold and 76 resumed child publications.

Remaining: final malformed-input/lifecycle acceptance review, including
side effects of command-memory reads and callback ownership. Graphics pairs
split across the physical ring end, arbitrary held jumps, IRQ-clear behavior
and fragment-processor ISA execution are explicitly unsupported. The current
64-span/32-child bound is a declared software limit, not a physical GPU claim.
No additional integrator authority is requested. Do not mark ticket 761 done.

Final inline-slice verification: every command in Tests and Commands passes
with actual selection: backend atomic 8, GPU atomic 9, completion atomic 6,
scheduler batch 4, refusal integration 6, NEMA 98, transcript 91, machine
snapshot 4. The additional `make test TEST_FILTER=nema_gpu_snapshot` selects
4 passing cases. `make check` and `make sanitize` each pass 815 tests;
`make check-task-contracts` validates 130 tickets and `make check-lines` passes
with only existing review-threshold warnings. `make sdl` and both exact private
gate commands pass without skips. The 2.22 cold live-input log retains
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c` and
the exact `000bacf4 / 774081920 / 6520939902` stop tuple. The 2.39 gate verifies
four awake pulses, native IRQs, four snapshot phases and the intentional fifth
refusal, retaining log
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`
and snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.

### Command-memory audit continuation, 2026-09-07/08 (partial)

This slice changes only `src/display/nema_framing.c`/`.h`,
`src/display/nema_backend.c`, `tests/unit/test_nema_framing.c`,
the execution model, current status, evidence ledger and this handoff.
Earlier uncommitted tail/inline work is preserved; no commit is made.
E-EMU-NEMA-MEMORY-001 and the architecture's validation-before-mutation rule
support memory-only command fetches. The existing bus copy API suffices:
four-byte reads decode explicitly little-endian and refuse device mappings
without invoking callbacks, retaining the original error and output word.
No bus API/policy, public header, profile, registry or codec changes this slice.

External evidence/log directory: `/tmp/semu-761-memory.KpbRMR`.
`make test TEST_FILTER=nema_framing` fails before implementation: 15 unit cases,
four failures, each at the zero-device-read assertion. They cover a device ring,
device child, direct device list and RAM register/device value. After the fix
all 15 pass; the filter also selects six passing refusal integration cases.
ROM little-endian commands and corrected RAM retries remain successful.
Shared fixture setup keeps the framing test at 418 lines versus the prior 403;
the parser implementation is 186 lines. No expected native stop or golden changes.

The read-only texture audit reproduces a remaining atomicity gap. Source
`texture-probe.c` in that external directory has SHA-256
`9c0c29f72931d2c1888638f589786739f55786b2fa9fc87db125bd1302d089ce`.
Its synthetic device increments a read counter before returning an error:
texture validation refuses after one callback, RGB565 sampling after two,
and A2LE sampling after one. Mapped-ROM white-pixel/opaque-alpha controls pass.
The program returns zero only when all three gaps and both controls reproduce;
this is evidence of unfixed behavior, not passing atomicity acceptance.

```sh
probe_dir=/tmp/semu-761-memory.KpbRMR
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc "$probe_dir/texture-probe.c" build/libsemu.a -o "$probe_dir/texture-probe"
"$probe_dir/texture-probe" > "$probe_dir/texture-probe.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-inline.JJ1n7Q/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

Renderer observer/UI source hashes were verified against the preceding
inline-slice pins before recompilation. Full-flash and prefix hashes likewise
match the existing pins; all components are validated before execution.
Both cold traces retain
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`,
resume trace `1136b75b42117100c287048cb5282eb14987f99f63b27f987a8f050beb9061fe`.
All three final pixel images retain
`f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`,
snapshots `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.
There are no refused child submissions: 79 cold and 76 resumed publications;
the final tuple remains `000a7abc / 1300000000 / 22286110403`, frame CRC
`6b6aa2dc`. Firmware/resource/frame/snapshot bytes remain outside Git.

Requested integrator-owned change: explicitly extend this ticket's Allowed
Files to `src/display/nema_texture.c`, `src/display/nema_texture.h`,
`src/display/nema_a2le.c` and `tests/unit/test_nema_texture.c` for the demonstrated
texture-source side effects and focused refusal/success regressions. No private
parallel read API, backend-only duplicate preflight or permissive fallback was
added to evade scope. Callback lifecycle acceptance remains separate unfinished
work within the current scope. Physical ring-end split pairs, arbitrary held
jumps, IRQ-clear semantics and fragment-processor ISA remain unsupported.
Ticket 761 is not complete; its index/status is unchanged.

Final command-memory-slice verification: every exact command in Tests and
Commands passes without a skipped private gate. Actual focused selection is
backend atomic 8, GPU atomic 9, completion atomic 6, scheduler batch 4,
refusal integration 6, NEMA 103, transcript 91 and machine snapshot 4.
`make check` and `make sanitize` each pass 820 tests with zero failures.
`make check-lines` passes with existing review-threshold warnings;
`make check-task-contracts` validates 130 tickets and is rerun after this
handoff. `git diff --check` passes. `make sdl` succeeds.

The exact 2.22 live-input command passes its transcript/frame/stop assertions,
retaining log `9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`
and stop tuple `000bacf4 / 774081920 / 6520939902`. The exact 2.39 awake gate
passes four pulses, native IRQs, four snapshot phases and intentional fifth-hit
refusal. Its log remains
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`,
stop tuple `001291cc / 1272353867 / 32770943068`. Long 2.22 onboarding is not
revalidated in this slice. Passing these gates does not resolve the texture
side-effect gap or complete callback lifecycle acceptance.

### Texture-reader continuation, 2026-09-08 (partial)

The user approved the preceding four-file scope request. This ticket's
Allowed Files now include `src/display/nema_texture.c`/`.h`,
`src/display/nema_a2le.c` and `tests/unit/test_nema_texture.c`; status and
dependencies are unchanged. This slice changes those files, the already-owned
`tests/unit/test_nema_backend_atomic.c`, execution model, current status,
evidence ledger and this handoff. All earlier uncommitted work is preserved;
no commit, public bus/header, profile, registry, build or codec changes.

E-EMU-NEMA-TEXTURE-MEMORY-001 supplies the synthetic evidence and architecture
contract. Texture validation and RGB565/A2LE samples now use byte-wide mapped
memory copies without device callbacks. The byte width matters: the bus's
existing wider-copy lookup would bypass a partial one-byte overlay. RGB565
also retains support for a pixel split across adjacent mapped memory regions.
The A2LE wrapper stages alpha before assigning the whole output texel, fixing
RGB-field mutation on read refusal. Bilinear taps retain staged alpha output.

Validation remains a bounded last-byte accessibility check after descriptor
arithmetic validation; the header no longer incorrectly promises full-range
preflight. Every sampled byte is checked independently, and rendering callers
stage their target writes. Validation now preserves the original memory-copy
error instead of replacing it with generic UNSUPPORTED. The existing synthetic
out-of-memory-range test consequently expects RANGE; this is a diagnostic
correction supported by the new exact-code/text assertions, not a native repin.

External evidence/log directory: `/tmp/semu-761-texture.SVO7Ta`.
`make test TEST_FILTER=nema_texture` before the implementation runs 17 cases,
with six failures: validation, RGB565 low/high bytes, A2LE, a later bilinear
tap all invoke MMIO; unmapped A2LE with a null error sink alters output RGB.
All ten prior cases and the new adjacent-ROM success control pass. Final
texture tests pass all 17, including overlay removal/corrected RAM retries.
The old fixture setup is compacted without removing existing assertions;
this test is 309 lines versus 301 before this slice. Production texture/A2LE
implementations are 163/118 lines.

The backend regression has an earlier successful child draw and a RAM-backed
texture whose last byte is accessible but second texel contains a device byte.
It requires zero callbacks, original error text, unchanged full frame and
snapshot count, then an inherited draw that confirms refused source registers
did not leak. Removing the overlay permits the same two-child transaction to
publish both frames. The corrected fixture includes required TEX_COLOR;
its read-only baseline replay fails the zero-read assertion, while all eight
previous backend cases pass. Final backend selection is nine passing cases.

The baseline texture/A2LE files extracted read-only from `611d3c4` have hashes
`3de0a763b0613c36b200a2611a2ce5a22c608f850bf0622402e3ffc5d208c3a2` and
`25c61165a0e06a964c6996695ea38601c76818d93ac1761e66f0ae08c88f4fc8`.
Reproduction commands (baseline executable intentionally exits 1):

```sh
probe_dir=/tmp/semu-761-texture.SVO7Ta
make test TEST_FILTER=nema_texture
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/display -Itests/support tests/unit/test_nema_backend_atomic.c tests/support/test.c "$probe_dir/before-nema_texture.c" "$probe_dir/before-nema_a2le.c" build/libsemu.a -o "$probe_dir/backend-before"
"$probe_dir/backend-before" > "$probe_dir/backend-before.log" 2>&1
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-inline.JJ1n7Q/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

The unchanged renderer/UI observer source hashes were rechecked against
the inline-slice pins before compilation, as were the full-flash and prefix
snapshot hashes. All private components are validated before execution.
Two cold runs and the prefix resume retain cold trace
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`,
resume trace `1136b75b42117100c287048cb5282eb14987f99f63b27f987a8f050beb9061fe`,
pixels `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`,
and snapshot `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.
All 79 cold / 76 resumed child submissions succeed. Final frame CRC remains
`6b6aa2dc`; endpoint `000a7abc / 1300000000 / 22286110403` is unchanged.

The texture scope request is resolved. Remaining work is final callback
lifecycle acceptance, not a new texture or bus interface. Device-backed
textures, physical ring-end split pairs, arbitrary held jumps, IRQ-clear
semantics and fragment-processor ISA remain unsupported. Descriptor validation
does not claim full-range memory preflight. No further integrator-owned scope
change is requested by this slice; ticket 761 remains incomplete.

Final texture-slice verification: every exact Tests and Commands invocation
passes with actual selection: backend atomic 9, GPU atomic 9, completion
atomic 6, scheduler batch 4, refusal integration 6, NEMA 111, transcript 91,
machine snapshot 4. The extra `make test TEST_FILTER=nema_texture` selects
17 passing cases. `make check` and `make sanitize` each pass 828 tests with
zero failures. `make check-lines` passes with review-threshold warnings
(backend-atomic test 342 lines, texture test 309; no file exceeds 500).
`make check-task-contracts` validates 130 tickets, including the user-authorized
scope extension. `make sdl` and `git diff --check` pass.

Both exact private gate commands pass without skips. The 2.22 live-input gate
retains log `9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`,
all four frame CRCs, and stop `000bacf4 / 774081920 / 6520939902`.
The 2.39 gate retains log
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`,
four awake pulses/native IRQs/four snapshot phases and the intentional
fifth-hit refusal at `001291cc / 1272353867 / 32770943068`.
The longer 2.22 onboarding gate is not revalidated in this slice.

### Callback-lifecycle continuation, 2026-09-08

Committed the previously verified tail/inline/command-memory/texture work as
`cfce2a1` (`Validate inline NEMA submissions and keep source reads memory-only`)
on the user's request. The worktree was clean afterward. This continuation
changes only `src/display/nema_completion.c`/`.h`,
`tests/unit/test_nema_completion_snapshot.c`, execution model, current status,
evidence ledger and this handoff. These new lifecycle edits remain uncommitted.

E-EMU-NEMA-CALLBACK-001 records the discovered slot-lifetime defect. Dispatch
marked a completion inactive, then repeatedly read callback pointers from that
reusable entry. Scheduling from its CLID callback reused the slot and redirected
INTERRUPT/IRQ to the next recipient. Reset could clear its callback pointers;
completion-owner destruction could free the entry while dispatch still used it.
The regression fails before the fix: six selected cases, one failure at the
original recipient's three-notification count. The first recipient gets only
CLID, while the replacement receives the remaining calls.

Dispatch now copies the accepted notification tuple before retiring the slot
and uses only that local copy after calling out. Queued work remains cancellable;
the in-flight CLID/INTERRUPT/IRQ sequence is not revocable. The existing header
documents that callback contexts and scheduler must remain alive through that
sequence, even if the standalone completion owner is reset/destroyed. This does
not authorize destruction of a callback context's machine/device owner or
recursive scheduler dispatch. No scheduler, public ABI, delay, event identity,
codec, frame callback restriction, profile or compatibility change is made.

The new case covers slot reuse, reset, cancel and completion-owner destruction
from the CLID callback with independent live stack contexts. It verifies exact
CLID/INTERRUPT/IRQ values and recipient order, cancellation of the second pending
completion, final pending flags/counters, and delivery of an older equal-time
completion before the replacement at the next 100-us deadline. Existing snapshot
round trips and separate-instance ownership tests remain intact. The completion
implementation is 178 lines and the expanded snapshot/lifecycle test is 295.

External evidence/log directory: `/tmp/semu-761-lifecycle.hA9RV8`.
In addition to every command in Tests and Commands, run:

```sh
make test TEST_FILTER=nema_completion_snapshot
probe_dir=/tmp/semu-761-lifecycle.hA9RV8
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-inline.JJ1n7Q/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

Renderer observer/UI sources retain their verified hashes
`03aeafb4e9eb1e8a568aa7c024fe1cca57e3c77f5d0ddb7d1e18622abc33184a` and
`6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`.
The full-flash and prefix identities were also rechecked before execution;
all private components are validated by the observers/gates. Two cold runs
and the prefix resume retain cold trace
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`,
resume trace `1136b75b42117100c287048cb5282eb14987f99f63b27f987a8f050beb9061fe`,
pixels `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`,
snapshot `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.
All 79 cold / 76 resumed child submissions succeed; CRC `6b6aa2dc` and endpoint
`000a7abc / 1300000000 / 22286110403` are unchanged.

Acceptance review maps the requirements to the existing, executed regressions:

| Requirement | Coverage |
| --- | --- |
| Whole-list/child state and pixel atomicity, retry | Backend/GPU atomic tests; inline completion transaction; texture later-child regression |
| Queue IDs, sequence, deadline and allocation refusal | Scheduler-batch, completion-atomic and composed allocation integration tests |
| Original diagnostics and CPU-visible refusal | Backend diagnostic saturation/null-sink cases, GPU callback-result cases, precise guest GPU-store fault |
| Strict framing and memory reads | Framing, wrapped-plan/capacity GPU snapshot, unmatched-tail and command/texture MMIO-counter tests |
| Publication/notification order and lifecycle | Backend batch publication, GPU reentrancy, completion snapshot/rebind and new in-flight lifecycle case |
| Persistent and authentic success pins | Machine/GPU snapshot tests, both exact private gates, two cold middle runs and prefix resume |

Unsupported cases remain explicit: device-backed command/texture sources,
physical ring-end split pairs, arbitrary held jumps, inferred IRQ-clear behavior,
fragment-processor ISA, destroyed callback contexts and recursive dispatch.
No such support is required by the pinned native acceptance runs. The long 2.22
onboarding test is outside this ticket's exact gate and is not rerun. The ticket
status/index remain unchanged; integrator acceptance is separate from implementation.

Final lifecycle-slice results: all exact Tests and Commands invocations pass
with actual selection. Backend atomic 9, GPU atomic 9, completion atomic 6,
scheduler batch 4, refusal integration 6, NEMA 112, transcript 91, machine
snapshot 4; the extra completion-snapshot filter selects six passing cases.
`make check` and `make sanitize` each pass 829 tests. `make check-lines` passes
with existing review-threshold warnings; `make check-task-contracts` validates
130 tickets. `make sdl` and `git diff --check` pass.

Both private gates run without skips and retain their exact pins. The 2.22
live-input gate retains log
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`,
four frame CRCs and stop `000bacf4 / 774081920 / 6520939902`.
The 2.39 awake gate retains log
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`,
four pulses/native IRQs/four snapshot phases and intentional fifth-hit refusal
at `001291cc / 1272353867 / 32770943068`.

The implementation candidate has passing evidence for the recorded acceptance
requirements and is ready for integrator review. No further integrator-owned
interface change is requested. Do not treat this handoff as a status/index
update or as evidence for unsupported physical GPU behavior.
