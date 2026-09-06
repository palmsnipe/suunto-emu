# Migration Evidence Ledger

## Purpose and Format

This ledger records facts migrated from `suunto-firmware`, firmware traces, documentation, and synthetic experiments. It contains summaries and hashes, never copyrighted firmware bytes. Each implementation and hardware status change cites stable evidence IDs from this file.

Each future entry must contain: ID, date, source kind and location, product/firmware hashes, observation, confidence, affected modules, validation test, and unresolved questions. Local-only source locations must be described symbolically, such as `$FIRMWARE_ROOT`, rather than with a user path.

Rows that describe an earlier blocked probe remain historical evidence. In
particular, E-SAP-0013 records the pre-705 state in which later-version
profiles were unavailable; E-SAP-0014 and E-SAP-0017 supersede its current
profile and gap conclusions without changing the original observation.

## Seed Evidence

### E-EMU-NEMA-GPU-001

2026-09-06; partial ticket 761 integration on backend commit `0e8900d`.
Sources: E-EMU-NEMA-ATOMIC-001's original synthetic probe, architecture/
execution contracts and the in-tree GPU/backend/completion implementation.
E-NEMA-RING-001 and E-NEMA-LISTS-001 remain the native interpretation evidence.
No new register, command, physical timing or panel behavior is inferred.

Two narrow GPU tests fail before migration: later-child refusal and second
completion admission both return successful MMIO with partial effects.
GPU/machine/CLI now use the public prepare/commit/abort operations. A submission
collects children and markers, prepares all child output, admits all completion
events in one batch and only then commits frames/registers/stop/generation.
Refusal preserves retry state and returns the original diagnostic. Empty-error
REFUSE, unexpected WAIT/invalid results and missing backend refuse explicitly.
Missing completion scheduling configuration refuses before backend preparation.
NULL error sinks work; reentrant GPU write/reset/snapshot and machine reset
conflict before mutation. GPU snapshot bytes and native success paths remain.

Six GPU cases pass, covering those failures plus corrected same-stop retry,
repeated-stop no-op, marker payload/opcode separation, ID/sequence/deadline
exhaustion, event order/reset, original/fallback callback errors, reentrancy and
missing configuration. A synthetic STR/BKPT guest tests the unchanged CPU/bus
path in exactly two steps: success continues at PC 0x102; refusal enters the
installed HardFault handler at 0x180, with CFSR 0x8200 and BFAR 0x400900ec.
The refused GPU snapshot is unchanged; no CPU/bus policy was edited.
Confidence is high for these tested transaction invariants, not all malformed
ring inputs. The ticket handoff lists every changed file.

Validation (every focused group selects tests; all pass):

```sh
make test TEST_FILTER=nema_backend_atomic       # 7
make test TEST_FILTER=nema_gpu_atomic           # 6
make test TEST_FILTER=nema_completion_atomic    # 5
make test TEST_FILTER=scheduler_batch           # 4
make test TEST_FILTER=nema_refusal              # 1
make test TEST_FILTER=nema                      # 86
make test TEST_FILTER=transcript                # 91
make test TEST_FILTER=machine_snapshot          # 4
make check-task-contracts                      # 130 tickets
make check-lines                              # no hard-limit violations
make check                                    # 803 tests
make sanitize                                 # 803 tests
make sdl
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
```

The original five-scenario probe is copied externally and changes only its GPU
constructor argument from the convenience callback to the public ops table.
Its new SHA-256 is
`f171b64a7f67e1c7e00ba0e1fbc43ab75007f0bba05f4d46891a4fd02ca7ee59`.
Two normal runs and one ASan/UBSan run report identical results:

```text
refused-list result=2 error=6 pixel=001f frames=1
inherited-after-refusal pixel=001f expected=001f
children refuse=0 status=0 error=0 frames=2 pixel=001f
children refuse=1 status=6 error=6 frames=0 pixel=0000
markers exhaust=0 status=0 error=0 pending=2 next_id=3
markers exhaust=1 status=4 error=4 pending=0 next_id=18446744073709551614
atomicity-failures=0
```

The read-only observer is adapted to prepare/commit/abort without changing
guest input, timing or state. Its renderer wrapper records draws during prepare
and successful submission records after commit; the authentic single-child
submissions retain their exact historical trace ordering. Revised source pins:

- `render-probe.c`: `b59e73c8220beaade48cee629a1da3b5f8b93b8b47a1cc6cd0be1a2e4ee32ab7`.
- Included `ui-probe.c`: `6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`.

Exact external commands (sources/artifacts stay outside Git):

```sh
probe_dir=/tmp/semu-761-gpu.aQow5X
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc /tmp/semu-761-gpu.aQow5X/atomic-probe.c build/libsemu.a -o "$probe_dir/atomic-probe"
"$probe_dir/atomic-probe"
"$probe_dir/atomic-probe"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc /tmp/semu-761-gpu.aQow5X/atomic-probe.c build/sanitize/libsemu.a -o "$probe_dir/atomic-probe-san"
"$probe_dir/atomic-probe-san"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-gpu.aQow5X/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
```

Private components are validated before execution; firmware identities remain
the existing manifest pins. Full-flash SHA-256 remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
The 2.22 gate retains stop `user`, PC `000bacf4`, 774081920 instructions,
6520939902 ns and all E-SAP-ONBOARD-EMU-012 frame/log pins. The 2.39 awake
gate retains all four pulse/snapshot phases and the intentional fifth-hit
refusal: PC `001291cc`, 1272353867 instructions, 32770943068 ns,
log SHA-256 `06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot SHA-256 `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.

All 79 cold and 76 resumed language submissions succeed. Both cold traces and
all three final pixel/snapshot artifacts retain E-EMU-SAMPLING-CLIP-001 pins:

- Cold trace: `a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.
- RGB565: `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`.
- Snapshot: `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.

Last changed cold frame: generation 79, CRC `6b6aa2dc`, instruction
1088274630 at 12335112985 ns. Endpoint: 1300000000 instructions,
22286110403 ns, PC `000a7abc`, GPS hits `2,2,3`. Cold frames/changes
remain 79/62; resume 76/60. No private bytes or new goldens enter Git.

Remaining: strict ring/wrap/unmatched-tail validation, non-power-of-two wrap
arithmetic and direct fallback review; composed whole-operation deterministic
allocation-failure coverage beyond the component tests; malformed-input and
lifecycle acceptance audit. Legacy unsupported-width handling is unchanged,
not endorsed. Callback guest execution/destruction/bus mutation is unsupported.
No extra integration authority is requested. Ticket 761 is not complete.

### E-EMU-NEMA-BACKEND-001

2026-09-06; partial ticket 761 implementation on foundation commit `ce0e529`.
Sources: E-EMU-NEMA-ATOMIC-001's unchanged synthetic probe, the in-tree
backend/state/TSC6A contracts, architecture/execution atomicity requirements
and new `tests/unit/test_nema_backend_atomic.c`. E-NEMA-RING-001 and
E-NEMA-LISTS-001 remain the native interpretation evidence. No new GPU program,
register, physical timing or panel claim is inferred.

Three narrow regressions fail before implementation: a refused list leaks
red inherited color into the next draw; a syntax refusal returns an empty
error; a TSC6A draw failure loses its original code/text. The public display
contract now supplies bounded prepare/commit/abort operations. The normal
backend stages inherited registers/counters, RGB565 pixels, lazy-cloned
TSC6A shadows and ordered per-list frame images. No frame escapes preparation.
Commit allocates nothing and cannot fail. The single-list convenience uses
the same implementation; diagnostic saturation cannot replace the first error.
NULL error sinks are supported. Reset/reentrant prepare conflict before
mutation, and callback lifetime/restrictions are explicit. Transient staging
does not alter snapshots; successful interpretation/generation rules remain.

Seven backend tests pass for those regressions plus intra-child/later-child
failure, abort/retry, exact two-frame order/generations, independent instances,
TSC6A refusal/abort/commit/reset, bounds, empty lists and deterministic frame
allocation failure. A test compiles the production transaction source with
only its allocator replaced; no runtime allocator hook is introduced. Commit
still succeeds with that allocator disabled and makes zero allocation calls.
Confidence is high for these synthetic backend invariants, not for whole-ring
GPU behavior. The original external probe still exits 1, now reporting:

```text
refused-list result=2 error=6 pixel=001f frames=1
inherited-after-refusal pixel=001f expected=001f
children refuse=0 status=0 error=0 frames=2 pixel=001f
children refuse=1 status=0 error=0 frames=1 pixel=001f
markers exhaust=0 status=0 error=0 pending=2 next_id=3
markers exhaust=1 status=0 error=0 pending=1 next_id=18446744073709551615
atomicity-failures=2
```

Affected files are the public display contract, backend lifecycle/list/draw/
transaction implementation and internal header, state-copy implementation/
header, the new regression and documentation listed in ticket 761's handoff.
No GPU/machine caller, profile, registry, Makefile or persistent format changes.
The handoff records exact focused commands: backend 7, NEMA 79, transcript 91,
machine snapshots 4, completion 5, scheduler batch 4; all pass. `make check`
and `make sanitize` each pass 796 tests; line checks and 130 task contracts
pass. The final expanded intra-child regression also passes the targeted
normal and sanitizer commands. Logs/artifacts: `/tmp/semu-761-backend.vQf9p6/`.

Authentic validation uses unchanged exact manifest/profile component hashes
and full-flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
All components are validated before execution. `make sdl` and these exact
gates pass without re-pinning:

```sh
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
```

The 2.22 stop remains PC `000bacf4`, 774081920 instructions, 6520939902 ns
with all E-SAP-ONBOARD-EMU-012 frame/log pins. The 2.39 gate retains all four
native IRQ/pulse/snapshot phases and ticket 759's exact fifth-hit refusal.
Two cold middle-button runs and the unchanged 700M-prefix resume are rebuilt
and reproduced with the still-compatible, read-only observer (source SHA-256
`d96fbfbbae59a1696794a291ff5d6e8ddb81ea9358044fbf2c7fb5b789f57190`):

```sh
probe_dir=/tmp/semu-761-backend.vQf9p6
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-239-ui.690H4Q/render-probe.c build/libsemu.a -o "$probe_dir/render-probe"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/middle-b" > "$probe_dir/middle-b.trace" 2> "$probe_dir/middle-b.log"
"$probe_dir/render-probe" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/middle-resume" > "$probe_dir/middle-resume.trace" 2> "$probe_dir/middle-resume.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc /tmp/semu-nema-atomic.3Ec7wr/probe.c build/libsemu.a -o "$probe_dir/atomic-probe"
"$probe_dir/atomic-probe" # expected exit 1: two GPU integration failures remain
```

All 79 cold submissions and 76 resumed submissions succeed. Cold traces match
each other and E-EMU-SAMPLING-CLIP-001 byte-for-byte; all final pixels and
snapshots match each other and that baseline. SHA-256 pins remain:

- Cold trace: `a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.
- RGB565: `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`.
- Snapshot: `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.

The last changed cold frame remains generation 79, CRC `6b6aa2dc`, instruction
1088274630 at 12335112985 ns. Endpoint: 1300000000 instructions, 22286110403
ns, PC `000a7abc`, GPS hits `2,2,3`. No private bytes enter Git.

Unresolved: GPU/machine/frontend migration to the new contract, whole-ring
backend/completion admission, MMIO diagnostic/retry state and bounded CPU
refusal proof, strict framing/tail validation and caller reentrancy. The
`nema_gpu_atomic` and `nema_refusal` groups are not yet implemented or run.
The observer must migrate with the GPU callback contract. Ticket 761 remains
incomplete; no new authority, budget or evidence exception is requested.

### E-EMU-NEMA-BATCH-001

2026-09-06; partial ticket 761 implementation, following
E-EMU-NEMA-ATOMIC-001 and the architecture/execution atomicity contracts.
Sources: in-tree scheduler/completion implementations, new synthetic unit
regressions and existing private firmware gates. No physical timing or new
hardware behavior is inferred; E-NEMA-RING-001 / E-NEMA-LISTS-001 remain the
framing evidence. Affected modules: scheduler admission/cancellation, NEMA
completion lifetime/codec and GPU snapshot scheduler rebinding only.

The narrow single-completion regression fails before the fix: a refused
deadline admission changes encoded completion state. Atomic batch reservation
now stages all completion entries before one scheduler call. Four scheduler
and five completion regressions cover success order/identities, duplicate
markers, deadline/ID/sequence/slot limits, allocation failure, unchanged
output IDs/queues/snapshots and cancellation/reset/destruction. A second
regression failed during development when scheduler reset reused event ID one:
checked cancellation now requires the callback and context as well as the ID.
Transient scheduler ownership is not encoded; successful snapshot bytes and
single-event ordering are unchanged. Confidence is high for these bounded
synthetic invariants, not for the still-unimplemented whole-ring transaction.

Exact verification commands (repository root), all successful:

```sh
make test TEST_FILTER=nema_completion_atomic
make test TEST_FILTER=scheduler_batch
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

Normal and sanitizer totals: 789 tests, zero failures. Task contracts: 130.
The authentic gates validate every component before execution, using unchanged
manifest/profile hashes; the full-flash hash remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
The short 2.22 SDL stop remains PC `000bacf4`, 774081920 instructions,
6520939902 ns with all E-SAP-ONBOARD-EMU-012 frame pins. The 2.39 awake gate
passes all four pulse/IRQ and snapshot phases and its fifth-hit refusal;
ticket 759's log/snapshot pins are unchanged.

The external read-only E-EMU-SAMPLING-CLIP-001 renderer observer is rebuilt
against this library; source hash remains
`d96fbfbbae59a1696794a291ff5d6e8ddb81ea9358044fbf2c7fb5b789f57190`.
Reproduction and outputs:

```sh
probe_dir=/tmp/semu-761-foundation.xSooSd
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-239-ui.690H4Q/render-probe.c build/libsemu.a -o "$probe_dir/render-probe-final"
"$probe_dir/render-probe-final" cold 1 "$probe_dir/final-middle-a" > "$probe_dir/final-middle-a.trace" 2> "$probe_dir/final-middle-a.log"
"$probe_dir/render-probe-final" cold 1 "$probe_dir/final-middle-b" > "$probe_dir/final-middle-b.trace" 2> "$probe_dir/final-middle-b.log"
"$probe_dir/render-probe-final" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/final-middle-resume" > "$probe_dir/final-middle-resume.trace" 2> "$probe_dir/final-middle-resume.log"
```

Both cold runs accept 79 submissions; the 700-million-instruction prefix
resume accepts the remaining 76. No renderer refuses. Cold traces match each
other and the corrected baseline byte-for-byte; all final pixels/snapshots
match each other and that baseline. SHA-256 pins:

- Cold trace: `a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.
- Final RGB565: `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`.
- Final snapshot: `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.

Last changed cold frame remains generation 79, CRC `6b6aa2dc`, instruction
1088274630 at 12335112985 ns. Final endpoint remains 1300000000 instructions,
22286110403 ns, PC `000a7abc`, GPS hits `2,2,3`. No private bytes enter Git.

Unresolved: backend inherited-state/pixel/shadow staging, multi-child
prepare/commit/abort, GPU whole-ring marker admission, original-error
propagation, retry state, caller migration and bounded CPU refusal proof.
The original three-failure whole-ring probe is not fixed by this slice.
The `nema_backend_atomic`, `nema_gpu_atomic` and `nema_refusal` groups are not
implemented or claimed run. Ticket 761 remains incomplete; no additional
integration permission or persistent-format change is requested.

### E-EMU-NEMA-ATOMIC-001

2026-09-06; synthetic diagnostic and integration-planning maintenance on
unchanged runtime commit `c7800be`. No firmware is used by this probe; no
physical GPU behavior is inferred. Source: existing display backend, framing,
NEMA state/completion and scheduler interfaces, plus an external C99 probe at
`/tmp/semu-nema-atomic.3Ec7wr/probe.c`, SHA-256
`05443c3a7325e04a30016102710b150dca24856f35dc05dccf06398565e48979`.
Architecture/execution require complete validation before rendering mutation
and an outward diagnostic on refusal. E-SAP-UI-239-001 already demonstrates
the GPU swallowing a single-child backend refusal and consuming its stop.

The real in-tree backend exposes three further atomicity failures:

- A successful one-pixel blue draw establishes inherited state. A second list
  sets red draw color then contains an invalid prefix. It returns REFUSE with
  outward error code zero and preserves blue pixels and publication count.
  However, a subsequent draw-only list paints red (`f800`), not the committed
  blue (`001f`): rollback restores pixels, but not inherited register state.
- One ring contains two syntactically valid child lists. The first paints
  blue and publishes; the second has unsupported DRAW_CMD `deadbeef`. MMIO
  returns OK with empty error while one frame/pixel change has escaped. The
  all-valid control publishes twice and succeeds. Merely returning REFUSE
  from the second child cannot retract the first callback.
- One ring contains two completion markers. With next event ID restored to
  `UINT64_MAX - 1`, the first schedules successfully and consumes that ID;
  the second exhausts the ID space. MMIO still returns OK with empty error,
  one event remains pending and next ID becomes `UINT64_MAX`. The ordinary
  control admits both events with next ID three. Cancelling the first event
  alone would not restore the consumed ID/sequence or completion bookkeeping.

The bounded five-scenario probe uses SRAM-only synthetic command lists,
normal GPU/backend callbacks and the scheduler's existing restore interface
to create ID exhaustion. It exits 1 for the three violated expectations;
exit 2 is a setup/control failure. Two normal runs and one ASan/UBSan run
produce identical output, SHA-256
`c8abd72aef450df5d89b9f706c5d1b24c5e825961070116590e044fb811b1b5f`.
Both success controls pass; sanitizer stderr is empty. Exact output:

```text
refused-list result=2 error=0 pixel=001f frames=1
inherited-after-refusal pixel=f800 expected=001f
children refuse=0 status=0 error=0 frames=2 pixel=001f
children refuse=1 status=0 error=0 frames=1 pixel=001f
markers exhaust=0 status=0 error=0 pending=2 next_id=3
markers exhaust=1 status=0 error=0 pending=1 next_id=18446744073709551615
atomicity-failures=3
```

Reproduction commands from the repository root:

```sh
probe_dir=/tmp/semu-nema-atomic.3Ec7wr
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc "$probe_dir/probe.c" build/libsemu.a -o "$probe_dir/probe"
"$probe_dir/probe" > "$probe_dir/normal-a.log"; test $? -eq 1
"$probe_dir/probe" > "$probe_dir/normal-b.log"; test $? -eq 1
cmp "$probe_dir/normal-a.log" "$probe_dir/normal-b.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc "$probe_dir/probe.c" build/sanitize/libsemu.a -o "$probe_dir/probe-sanitize"
"$probe_dir/probe-sanitize" > "$probe_dir/sanitize.log" 2> "$probe_dir/sanitize.err"; test $? -eq 1
cmp "$probe_dir/normal-a.log" "$probe_dir/sanitize.log"
test ! -s "$probe_dir/sanitize.err"
make check-task-contracts
make check
git diff --check
```

The existing suite still passes 780 tests, line checks and now 130 task
contracts; it does not yet contain these failing regressions. Full sanitizers
and authentic firmware are not rerun for this documentation/planning-only
step. Prior `c7800be` results remain applicable to unchanged runtime bytes;
the new probe itself was run under sanitizers as specified above.

Confidence is high for these synthetic failures and the interface gap.
The current opaque single-list backend can commit and invoke callbacks, but
cannot prepare/abort a multi-child batch. The scheduler/completion path admits
one event at a time. AGENTS.md therefore stops the proposed narrow GPU fix:
the smallest required integration is a transactional extension of the existing
display contract plus atomic completion admission, with staged inherited
register/pixel/shadow state and publication only after all checks succeed.
Ticket 761 is ready and explicitly owns those public interfaces, caller
migration, focused regressions and exact historical checkpoints. No runtime
fix or private parallel API is introduced by this entry. Planning changes only
the ticket/index, status and this ledger. File-budget/UI continuation remains
behind the failure-correctness gate; no firmware or physical-panel claim changes.

### E-EMU-SAMPLING-CLIP-001

2026-09-06; bounded maintenance implementation and synthetic/private replay
follow-up to E-SAP-UI-239-001. Sources: the shared helper's intersection
contract, that entry's exact native glyph and external hash-pinned observer,
and `tests/unit/test_sampling_clip.c`. Private inputs are the same exact
E-SAP-0011 firmware components and full flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`;
each probe validates all components before execution. Firmware stays read-only.
Working-tree library SHA-256 after correction:
`69a59afe14b167d0ec1769a524165485ef8a0f8a269154ba872b1237602c193b`.

The initial two committed regression cases both fail against the old helper.
Correction stages outputs, validates layout and endpoint arithmetic, computes
the intersection, and returns all-zero bounds/offsets for empty coverage.
Nonempty offsets must fit `int32_t`; malformed inputs refuse before changing
outputs. No signed geometry, texture format, command or compatibility hook
is introduced. Five final test cases cover the observed offscreen glyph,
disjoint/zero-size rectangles, 1,728 independent pixel-membership comparisons,
14 atomic refusal cases, and RGB565/A2LE/affine draw consumers with visible
success and no-pixel-change controls. Affine format/matrix refusals remain.

Two cold middle-button observer runs now accept all 79 submissions (zero
REFUSE), instead of 27 accepted / 52 refused. Their traces, final snapshots
and RGB565 pixels are byte-identical. A resume from the unchanged 700-million
instruction prefix accepts all 76 remaining submissions and converges to the
same final snapshot/pixels; its frontend publication counter starts afresh.
Last changed frame: 1,088,274,630 instructions / 12,335,112,985 ns, cold
generation 79, CRC32 `6b6aa2dc`, RGB565 SHA-256
`f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`.
Visual inspection shows the language list with English selected, without the
stale green icon. The bounded endpoint remains 1,300,000,000 instructions /
22,286,110,403 ns / PC `0x000a7abc`, GPS hits `2,2,3`.
Cold trace SHA-256:
`a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`;
corrected final snapshot:
`4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.
The prior incomplete-render snapshot is historical, not re-pinned as a
release golden. The 700-million prefix remains `7650d82f...a904d2` as recorded
in E-SAP-UI-239-001. All pixels and probe artifacts remain outside Git.

Exact verification commands (all pass after correction):

```sh
make test TEST_FILTER=sampling_clip
make test TEST_FILTER=sampling
make check
make sanitize
make check-lines
make sdl
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
probe_dir=/tmp/semu-239-ui.690H4Q
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices "$probe_dir/render-probe.c" build/libsemu.a -o "$probe_dir/render-probe-clip"
"$probe_dir/render-probe-clip" cold 1 "$probe_dir/clip-middle-a" > "$probe_dir/clip-middle-a.trace" 2> "$probe_dir/clip-middle-a.log"
"$probe_dir/render-probe-clip" cold 1 "$probe_dir/clip-middle-b" > "$probe_dir/clip-middle-b.trace" 2> "$probe_dir/clip-middle-b.log"
"$probe_dir/render-probe-clip" "$probe_dir/cold-a.prefix.sems" 1 "$probe_dir/clip-middle-resume" > "$probe_dir/clip-middle-resume.trace" 2> "$probe_dir/clip-middle-resume.log"
cmp "$probe_dir/clip-middle-a.trace" "$probe_dir/clip-middle-b.trace"
for other in clip-middle-b clip-middle-resume; do
  cmp "$probe_dir/clip-middle-a.final.sems" "$probe_dir/$other.final.sems"
  cmp "$probe_dir/clip-middle-a.rgb565" "$probe_dir/$other.rgb565"
done
git diff --check
```

Results: five new cases; 27 focused sampling cases; 780 normal and 780
ASan/UBSan cases across 141 suites; 129 task contracts and line checks pass.
Private 2.39 four-awake/two-cold/four-resume gate preserves the ticket 759
log/snapshot pins, native IRQ checks and fifth-hit refusal. The short 2.22
SDL gate preserves all E-SAP-ONBOARD-EMU-012 frame/stop/log pins. Full long
onboarding is not rerun. Logs: `/tmp/semu-clip-fix.hdaNVI/`.
Confidence is high for clipping and repeatability, not physical-panel fidelity.
Affected files: sampling implementation/header, the new unit test, status and
this ledger. No profile, budget, persistent format or integrator-owned change
is needed. Swallowed backend errors remain a separate demonstrated bug;
logical-file investigation must wait for that fail-closed boundary correction.

### E-SAP-UI-239-001

2026-09-06; bounded observational maintenance after ticket 759 acceptance.
The in-tree interpreter uses all four explicit 2.39 layers, the normal NEMA
backend, exact E-SAP-0011 components and full flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Every private probe validates the components and full flash before loading or
executing. Firmware inputs remain read-only. No runtime, budget, profile,
snapshot format, native callback, guest RAM or register patch is made.
The observed working-tree library SHA-256 is
`9e1bba2db9c6b42c3836a19f2931732ed39549cf54ecf960057b13a988a3e056`.

Two cold idle runs publish six frames, five with distinct consecutive CRCs.
The final change, instruction 712,863,377 / 5,805,310,840 ns / generation six,
visually reads `Select language`; CRC32 `3bd12ac8`, RGB565 SHA-256
`07944160817f67bb02efd0fdcebe6e0e38353d1950adb112f34a912d1a40396f`.
There is no later publication before the accepted fifth-awake refusal.
Both final machine images are byte-identical to ticket 759's production
`da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
The idle frame trace SHA-256 is
`70c25cbe75473a7ca3b364d9675a8240eb1f84ea3a6361422c150209e79e6d38`.
A four-layer prefix at 700,000,000 instructions / 5,792,348,681 ns /
PC `0x000be50a` has snapshot SHA-256
`7650d82fe72e58d544dc0043df99ab756ece41092d39e94d7c0b2b7460a904d2`.

A middle press at instruction 847,389,017 / 12,010,884,552 ns and release at
850,221,528 / 12,096,961,147 ns advances to a language menu. Requested
12.000/12.070-second deadlines are exceeded by bounded WFI event jumps;
the actual times above, not a claimed exact 70-ms hold, define the experiment.
Upper/lower controls from the same prefix produce no new frame after the
initial tile and retain the four-hit GPS bound. Cold and resumed middle runs
have identical final pixels and machine snapshot: budget stop at
1,300,000,000 instructions / 22,286,110,403 ns / PC `0x000a7abc`, GPS hits
`2,2,3`; image SHA-256
`a890e7bab7203ac3b22891f49e9bd575e44dd749d8d85ca1d4bab76a6c0cfeb5`.
Last menu pixel SHA-256 is
`ff97cac3de4ae996e06d9417c18e62b2f655e0d831c09e6f22c4a6d6ee9c41c1`
(CRC `64e76882`). The centre retains the prior green icon. This incomplete
image is diagnostic, not a correct language-menu golden. Snapshot restore
does not restore the frontend backend surface: early partial pixels and frame
generations differ, although these runs converge to the same final pixels.

The read-only draw observer identifies **52 refused backend submissions out
of 79** in the cold middle run; only 27 publish. First failure: list 11,
DRAW_CMD five at `0x100d0b1c`, target `0x10121d40`, A2LE source `0x10063d38`,
code `0x941e8000`, white tint, rectangle `[298,307) x [15,27)` and clip
`[239,240) x [0,42)`. This is a fully clipped-out animated glyph.
`draw_mask_affine` reaches `sampling_compute_clip`, which rejects an origin
beyond the 240-pixel target instead of returning an empty intersection.
A synthetic clip probe reproduces that range refusal; another disjoint case
(x=150, width=9, clip x=0..100) returns inverted bounds `150,15,100,27` with
success. A partly visible x=239 control succeeds with `239,15,240,27`.
These contradict the helper's documented intersection contract. Signed
geometry and malformed/overflow cases still need a bounded correction; no
new texture format or guessed command is needed to reproduce these bugs.

Separately, the backend retains an unsupported-draw diagnostic but loses its
detailed local draw error. `submit_child` receives REFUSE;
`trigger_rendering` discards it and advances `previous_ring_stop`, while the
MMIO writer returns OK. An independent synthetic backend returning REFUSE
with `synthetic backend refusal` reproduces successful MMIO, empty outward
error and a consumed submission: repeating the same stop pointer does not
invoke the backend again. A successful-backend control also runs once.
Normal and ASan/UBSan reproducers exit 1 on this violated fail-closed
expectation, with no sanitizer finding. Correcting clipping alone must not
be represented as fixing this distinct failure-propagation contract.

Read-only resident disassembly at `0x0001dd5a..0x0001dd78` confirms active-low
reads through `0x0001e020`: pins 57/58/59. The native-navigation reference
`$FIRMWARE_ROOT/emulator/display/sdl/s239/navigation-trace.resc` identifies
publication `0x0010ace2` and view-open `0x00073898`; pristine application
disassembly confirms these call boundaries. A separate single-step observer
records native middle-button events 2, 5 and 1 for each of two properly held
presses. It observes view token `0x8a7f9b55` before input and `0x9cdbd4e2`
after the second click, without calling the view opener itself.

Two repeated navigation probes stop identically at the existing logical-file
ceiling: 1,376,488,437 instructions / 14,401,737,146 ns /
PC `0x000920b4`, LR `0x000ad079`, path `settings/general`, mode two.
GPS hits remain `2,2,1`. No budget was enlarged. Trace SHA-256
`e4008fb8e24c6abb63f199f4136b9a9d496b276bb045d8bb1cb8ff94c231e296`,
final snapshot `1e5e2c8aa9e22925ada70558bf7dd072046522238cc9198ad19c9c000059c734`.
This later diagnostic boundary follows swallowed renderer failures, not
correct intervening frames. Measure the native file sequence only after the
renderer/refusal issues are addressed; do not raise its bound from this
observation alone.

External artifacts are in `/tmp/semu-239-ui.690H4Q/`. Source SHA-256 pins:

| Source | SHA-256 |
| --- | --- |
| `ui-probe.c` | `6d920dfd7b44f47b3aee1f1b50f23711792a8cabf45f303602a5c1144481fc9f` |
| `render-probe.c` | `d96fbfbbae59a1696794a291ff5d6e8ddb81ea9358044fbf2c7fb5b789f57190` |
| `nav-probe.c` | `508c684cc26eb1781d2c5618488b4636d3343a955aa14fb6013f8d68c5cb3233` |
| `refusal-probe.c` | `65ad0fbdc804006855109d18cd74ea7765852e8089b813631116eba2226bb81e` |
| `clip-probe.c` | `53cbec1e1798966569caeb6510297e08820b1d54f9c83123533e136b3a590d5b` |

Reproduction commands (run from the emulator root):

```sh
probe_dir=/tmp/semu-239-ui.690H4Q
for name in ui render nav refusal clip; do
  cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices "$probe_dir/$name-probe.c" build/libsemu.a -o "$probe_dir/$name-probe"
done
"$probe_dir/ui-probe" cold - "$probe_dir/cold-a" > "$probe_dir/cold-a.trace" 2> "$probe_dir/cold-a.log"
"$probe_dir/ui-probe" cold - "$probe_dir/cold-b" > "$probe_dir/cold-b.trace" 2> "$probe_dir/cold-b.log"
"$probe_dir/ui-probe" cold 1 "$probe_dir/middle-cold" > "$probe_dir/middle-cold.trace" 2> "$probe_dir/middle-cold.log"
"$probe_dir/ui-probe" "$probe_dir/cold-a.prefix.sems" 1 "$probe_dir/middle-a" > "$probe_dir/middle-a.trace" 2> "$probe_dir/middle-a.log"
"$probe_dir/render-probe" cold 1 "$probe_dir/render-detail" > "$probe_dir/render-detail.trace" 2> "$probe_dir/render-detail.log"
"$probe_dir/nav-probe" "$probe_dir/nav-b" > "$probe_dir/nav-b.trace" 2> "$probe_dir/nav-b.log"
"$probe_dir/nav-probe" "$probe_dir/nav-c" > "$probe_dir/nav-c.trace" 2> "$probe_dir/nav-c.log"
"$probe_dir/nav-probe" inspect "$probe_dir/nav-b.final.sems"
"$probe_dir/clip-probe"
"$probe_dir/refusal-probe"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc "$probe_dir/refusal-probe.c" build/sanitize/libsemu.a -o "$probe_dir/refusal-sanitize"
"$probe_dir/refusal-sanitize"
make check
git diff --check
```

Builds/private probes exit 0; clip/refusal reproducers intentionally exit 1.
The draw observer preserves the cold-middle final image/pixels byte-for-byte;
its detailed trace SHA-256 is
`e2ba4216396b0c6fa8afa1d3b59ba1143f1d270cf62eb0f86f46a40a011c0224`.
Existing `make check` passes 775 cases, line checks and 129 task contracts;
it does not yet contain these external failing regressions. Full sanitizers
and SDL walks were not rerun for documentation-only maintenance. Confidence
is high for native input/events, exact refusal and the two synthetic bugs;
complete UI, physical display fidelity, GPS fix/time and a justified extra
file budget remain unproven. Affected components: sampling/affine draw, NEMA
backend/device error propagation and the separate logical-file workflow.
Next work must promote these reproducers into regressions and resolve the
renderer failures before extending that workflow.

### E-SAP-ONBOARD-EMU-012

2026-09-06; bounded maintenance investigation of the 2.22 SDL gate mismatch
reported during ticket 758. Exact `2.22.60.3383-P` components are independently
validated in every clean build (E-SAP-0005..0007). All runs use the in-tree
interpreter, normal NEMA backend, dummy SDL, and `sapporo-2.22-no-device`.
No firmware, CPU, device, renderer, input timing or release frame golden is
changed by this maintenance correction.

Clean `git archive` builds of `50f7697` and `fbe8d48` reproduce the complete
E-SAP-ONBOARD-EMU-011 log SHA-256
`55d96468b4b41a938f98ab9db500dabc99ad491d7cc1e5595b933119a7b1f72b`,
including its 804398304-instruction / 9504428769-ns stop. Thus that historical
measurement was valid, not a host-time artifact or fabricated checkpoint.

External, observational IOM instrumentation in the `fbe8d48` build records
32 one-byte haptic reads with command `0x22000112` but selector `0x08` from
the obsolete `dma_target - 8` lookup. Each returns zero. The first two occur
at 1213352815 and 1213355468 ns; the last is at 4193067310 ns. This reads
the status register instead of the requested autotune register `0x22`, forcing
the firmware's nonfatal timeout. The read-only research
`$FIRMWARE_ROOT/docs/research/feedback-startup-haptic.md`, SHA-256
`64858799bdfe96e56b918051e05051ea52cc7c4729f11e0c11a9f70219d6a13e`,
independently identifies 2.22's native poll, 31 tries separated by 100 ms,
and nonfatal timeout. Immediate completion followed by calibration reads is
the documented successful path (E-SAP-HAPTIC-001).

Commit `d6b4235` corrects the shared address-`0x50` selector under
E-SAP-IOM4-HAPTIC-239-001; `df93397` supplies the separate zero calibration
reads under E-SAP-HAPTIC-CAL-239-001. A clean `d6b4235` build alone reaches
the missing calibration read and later resets/halts; it is not a valid UI
checkpoint. Applying only `df93397`'s haptic source to that external build
and observing the IOM shows five successful reads: `0x22` returns `0x00`,
then `0x03`, then `0x03`; `0x23` and `0x24` each return `0x00`, finishing
at 1213359491 ns. Removing only the diagnostic lines gives a log byte-equal
to the current build. On the old build, removing the same diagnostics gives
the exact old log. This isolates the timing correction to those two already
implemented changes, not scheduler ticket 758 or SDL event pacing.

The diagnostic artifacts are external under `$SDL_DRIFT_ROOT` (local run:
`/tmp/semu-sdl-drift.RmgaNg`). Instrumented old/new IOM source SHA-256:
`e562c6c55fdf3070e609209acdd2bf5c8ad98278dc3d8566eaa6cfd077d566ac` /
`8ee2a8765c230ecbdb8864c204079a1a1d64cf4f39786e8b8e7398b54a1553c2`.
Complete diagnostic log SHA-256:
`a5e9b7deaf127bed02870a5175c46119779aec17c34af9f1b73f8a17937adc00` /
`ee4c32b353f6d75485bb001c4189067da14c418f089ecf5806d74345c83b1624`.
Instrumentation only prints command, selected register, response, status and
virtual time; it changes no request or result. No private bytes enter Git.

Two fresh uninstrumented current runs compare byte-identically, SHA-256
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`.
Their unchanged initial/settled frames are generation 1 CRC `2a01c517`,
generation 3 `4979f432`, generation 5 `629da47e`, generation 63 `d4ed66c7`.
They stop through synthetic SDL quit at `stop=user pc=0x000bacf4
instructions=774081920 virtual_time_ns=6520939902`. Current SDL binary hash
`a9ce82fc29e07201bd0266066d07dc06aab2d9354fdff7e8b649a2b8fddadf14`;
library `a5bda58c8065faf59e9fb85107ceb39593296f3408114c65e7b8dbd71b0b56ee`.

The existing authentic `tools/test_sdl_live_input.sh` regression first fails
its old stop check on this corrected runtime. Maintenance updates that exact
tuple and adds full-log SHA-256 verification for cold boots, retaining all
existing frame, configuration-refusal and exit-status checks. Snapshot runs
retain the frame/stop checks; their startup log is a suffix, not the cold log.
Confidence is high for the isolated cause and repeatability. This supersedes
011 only as the current SDL smoke expectation, not as historical evidence.
No broad release-frame re-pin or physical calibration claim is authorized.
The longer manual-entry onboarding gate is a separate, unrevalidated contract.

Verification: the original focused authentic gate exits 1 before this smoke
correction and 0 afterward. An external harness replays the exact current log
with one additional line: frame and stop checks still pass, but the cold hash
check exits 1 with `SDL live input cold-start transcript mismatch`. Thus the
new expectation is not an allowlist of both behaviors or a relaxed tolerance.
`make check-sdl` without a manifest passes five synthetic SDL cases and its
configuration refusals (firmware walks explicitly skip); the short authentic
gate is run separately with all three validated components. `make check`
passes 768 cases, line checks and 129 task contracts; `git diff --check`
and `sh -n tools/test_sdl_live_input.sh` pass. No C changed,
so the preceding 768-case sanitizer result remains applicable.

Reproduction from each clean archive/current tree (`FW` is the same absolute
validated private manifest, `LOG` is an external output path):

```sh
make -j4 all sdl
build/suunto-emu validate --profile sapporo-2.22.60 --firmware "$FW"
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=middle-language build/suunto-emu-sdl run --profile sapporo-2.22.60 --firmware "$FW" --layer sapporo-2.22-no-device --until middle-language --max-instructions 14000000000 --max-time 22000000000 >"$LOG" 2>&1
```

### E-EMU-COMPAT-ATOMIC-001

2026-09-06; synthetic C regressions against `cb875c4`, in-tree interpreter,
no firmware bytes required. `tests/devices/test_sapporo_cxd5610_atomic.c`
demonstrates that a refusing exchange previously cleared a buffered five-byte
`@VER\r` prefix and traced the rejected final newline. WAIT had the same
problem. Delayed-RX time/ID/sequence exhaustion changed serialized event
context even though insertion failed and the scheduler retained no event.
The 2.22 startup response provider also consumed its one-hit counter and log
before discovering that the RX deadline overflowed.

`tests/unit/test_machine_snapshot_layers.c` demonstrates that machine snapshot
load accepted excessive aggregate/intervention counts, a sum larger than the
aggregate, and a wrapping sum. Intervention commit also ignored the aggregate
limit and could wrap its unsigned total. These are emulator bookkeeping and
transaction-boundary defects, not observations about physical GPS hardware.
The unchanged 2.22 table has nine one-shot interventions and eleven awake
pulses: aggregate twenty, while stale runtime/profile metadata said seven/one.
The fix aligns metadata with those existing limits; no individual allowance,
fixture body, firmware hook or event delay is expanded.

Transport now calls the exchange before committing/tracing the final fragment;
non-success retains the prefix for a retry. Exchange callbacks own their side
effects and must schedule responses, not deliver synchronous RX. Delayed RX
commits its bytes/context only after successful scheduler insertion, which
does not invoke callbacks inline. The legacy startup provider preflights its
budget and records its hit only after scheduling succeeds. Snapshot restore
checks budgets and sums without changing the wire format; direct layer hits
remain allowed, so intervention sum may be less than aggregate. The future
2.39 GPS layer still needs its own startup-before-reply lifecycle validator.

All six new cases fail on the corresponding old behavior and pass after the
corrections. `make check` and `make sanitize` each pass 746 tests; task
contracts validate 126 tickets, and line checks pass with existing review
warnings. Full-machine section comparisons verify rejected loads leave CPU,
RAM, time, events, devices, storage, compatibility and renderer state intact.
Transport tests cover refusal/WAIT retry, time/ID/sequence exhaustion, busy RX,
successful delayed delivery, and legacy-provider counter/log atomicity.
Allocator failure itself is not fault-injected; the same scheduling-error
return precedes RX mutation for that failure as well.

An independently built `cb875c4` CLI and the changed CLI, using validated
Sapporo 2.22 components and `--layer sapporo-2.22-no-device`, produce identical
logs/snapshots with `--max-instructions 450900000 --max-time 30000000000`:
PC `0x000bf0ee`, time 3,760,805,450 ns, log SHA-256
`fb1019a0bdcd1bf03d38fb562e5193cb0d2eddb6d0c929713d967f95e11de1d9`,
snapshot `66bf69fbde073741659ea277cc67f6ef54156f6920a0a9980facef14d98f940b`.
Both binaries load that old snapshot and continue identically to instruction
450,910,000, PC `0x0009aaa4`, time 3,760,815,450 ns. Continuation log/snapshot
SHA-256 are `bc67d51cc5923cf7a1767071391c5d24ba031b75d0f8b6da811862f781bde382`
and `90f1e00e7a37ecf2ad0c06ee1dbcce0a98f197564d147815e3a89e9d5fd76345`.
These are current-build equality probes, not replacement release goldens.
Local logs, snapshots and the comparison checkout stay outside Git.

The exact private `sapporo_239_activity_budget` runner also passes: both cold
starts, boot-logo and continuation hashes, intermediate snapshot resume, and
native GPS halt retain E-SAP-COMPAT-ACTIVITY-239-001. All three components are
validated before execution; full flash remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Confidence is high for these synthetic refusals and retained private
checkpoints. Unsupported commands, later GPS lifecycle behavior and physical
receiver fidelity remain unchanged; this maintenance does not implement 756's
new GPS layer or authorize a new startup-state response.

### E-SAP-COMPAT-GPS-AWAKE-239-001

2026-09-06; pristine Sapporo 2.39 application disassembly and bounded external
experiments against emulator `9f8c121`. All three E-SAP-0011 component hashes
are validated; application SHA-256 is
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
Full flash remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
The normal NEMA backend and all three ticket-757 layers remain attached.
No source firmware, CPU/RAM state, compatibility counter, UART response,
native event or production code is modified. External candidate pulses use
only the existing CXD awake-output API and normal GPIO IRQ path.

The external-IRQ table at `0x001b25dc` has entry two at `0x001b25ec`:
pin 24 and Thumb callback `0x00128927`. State ten registers entry two with
mode zero at `0x001290ac..0x001290b0`. Registration `0x000a5e88` maps that
mode through `0x000a6040` to configuration bits [7:6]=2; the live GPIO24
configuration at `0x40010060` is `0x93`. Callback `0x00128926..0x0012892e`
sets byte `0x100588a2` to one through the literal at `0x00129540`.
State ten bootstraps the same byte at `0x0012909e`. State twelve reads it
at `0x001291c8`, branches to missing-awake recovery when zero, and clears it
at `0x00129250` after rearming. Therefore `@GSTP` is a consequence of absent
awake input, not evidence that another PSS acknowledgement is required.

The successful state-twelve boundary is `0x001291cc`: R0=1, R2=12,
R4=`0x100588a2`, R8=driver=`0x100366d8`, R5=driver+0x26c,
R6=driver+0x314, R7=driver+0x75, callback 12, pending ten, retry zero.
Both GPS layers have exactly two hits. R1 is a scheduling return value and
varies across polls; it must not become a fixture predicate. The first
boundary is instruction 825,147,127 / 10,875,951,928 ns, reached from the
production state-twelve image
`0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`.

One external pulse queued there for +100,000,000 ns, with the existing
1,000,000-ns high duration, invokes the native callback at instruction
827,008,206 / 10,975,952,369 ns. The byte is zero on entry and one after
the native STRB at 827,008,209 / 10,975,952,372 ns. No RAM write is made by
the probe. The second poll succeeds rather than sending GSTP. With no further
pulse, the third poll correctly recovers and faults at 1,051,151,991 /
21,823,117,037 ns, BFAR `0x4001d000`, stacked PC `0x00171798`.

Four pulses at successive successful polls produce four real callback entries
and five successful state-twelve checks, all retry zero. Their queue times
are 10,875,951,928; 16,348,760,381; 21,822,867,048; 27,296,867,731 ns.
The fifth success, with four pulses consumed, is 1,272,353,867 /
32,770,943,068 ns at `0x001291cc`. A four-hit production fixture must refuse
there if execution attempts a fifth pulse, not silently extend its budget.
The diagnostic stops injecting after four and reaches ordinary RTOS idle:
1,315,882,442 / 35,000,617,152 ns, PC `0x000e955e`, both GPS counts still two,
no new UART command or recovery. This is the first instruction step crossing
its 35-billion-ns guard (WFI can advance to the next event), not a new golden.
The probe also caps instructions at two billion and trace records at 200.

Controls: zero pulses retains the precise 940,963,736 /
16,349,008,531-ns GSTP fault. Delaying the single pulse by six billion ns
instead of 100 million produces the same fault, with zero native awake IRQs.
The two four-pulse runs have byte-identical traces and logs. External source
`$AWAKE_PROBE_ROOT/awake-probe.c` SHA-256
`463479cea885123e4fb81dddd9ec5d8712c3e399b283c22e76608c6a0d466062`;
positive trace/log
`ca3527d679f889242849f6bfe52ed726cc0c817cb329b45630c6bc2a5ad72010` /
`5bdb32dc4a6a417d5b16d78d4348681c5ee1cedd47e170ca8dd7bc825171a212`;
zero-pulse trace/log
`941cd82481f4865b1f7cfca32ed2873e1cc1a84c56b7efd44584d8290fedd998` /
`b9f5090948d2bfec71690c0c60a2f369cc9ad53875771e23ff456771ff95a9af`;
single-pulse trace/log
`b91ffcd898e566d4bb574b68dbaeceed6fe6886d8b481c0bc29aa7f9dcc41465` /
`9571cc4016bff919160ca45d8bbbaf394736a3988a0264010c69b207274f0f21`;
late-pulse trace/log
`d8264deafd53e0b2a36e11c86dd9735c9a8123932087c97b3f9dc8a7dbab1c50` /
`4b9eb60fbb53f7f01b61d331dacee58d1548d031ffc1032469efbbd7b83f2a4f`.

Cross-build research files are hypotheses only: `$FIRMWARE_ROOT/docs/research/`
`cxd5610-gps-boundary.md` hash
`17f9f1e38ba16a38d43f945b0240b72046802fa063afcc8bed00b1ac5ea46eba`
and `cxd5610-live-epoch-cross-build.md` hash
`8b22fb8d660364668c929e174e742c3fb649d16db81da4b5b99b1bd0b5c846e4`.
The exact 2.39 table, callback and dynamic controls above establish this
build's wiring independently. Confidence is high for synthetic pulse
acceptance and native liveness checks, not physical receiver cadence, a
fix/time source, NMEA acceptance, settled UI or indefinite operation.
Affected modules: CXD awake scheduling, device compatibility, machine binding
and snapshots; validation is assigned to tickets 758/759. Four pulse timings
are explicitly synthetic and must remain named, hash-pinned and opt-in.

External reproduction (set `AWAKE_PROBE_ROOT` to the local evidence directory
and `STATE12_IMAGE` to the exact hash-pinned ticket-757 image above):

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices -Isrc/compat "$AWAKE_PROBE_ROOT/awake-probe.c" build/libsemu.a -o "$AWAKE_PROBE_ROOT/awake-probe"
"$AWAKE_PROBE_ROOT/awake-probe" "$STATE12_IMAGE" 4 normal >"$AWAKE_PROBE_ROOT/positive-a.trace" 2>"$AWAKE_PROBE_ROOT/positive-a.log"
"$AWAKE_PROBE_ROOT/awake-probe" "$STATE12_IMAGE" 4 normal >"$AWAKE_PROBE_ROOT/positive-b.trace" 2>"$AWAKE_PROBE_ROOT/positive-b.log"
cmp "$AWAKE_PROBE_ROOT/positive-a.trace" "$AWAKE_PROBE_ROOT/positive-b.trace"
cmp "$AWAKE_PROBE_ROOT/positive-a.log" "$AWAKE_PROBE_ROOT/positive-b.log"
"$AWAKE_PROBE_ROOT/awake-probe" "$STATE12_IMAGE" 0 normal
"$AWAKE_PROBE_ROOT/awake-probe" "$STATE12_IMAGE" 1 normal
"$AWAKE_PROBE_ROOT/awake-probe" "$STATE12_IMAGE" 1 late
```

Production integration, ticket 759 (2026-09-06): the separately selected
hash-pinned `sapporo-2.39-gps-awake` layer validates the recorded successful
poll predicates and both completed two-hit GPS dependencies. It admits one
100-ms-delayed/1-ms-high pulse through the unchanged transport, then commits
one attributed hit. Four hits maximum; no instruction replacement, RAM write,
UART response or physical-cadence claim. Lifecycle/ownership validation and
the extracted layer snapshot codec preserve previous encodings and reject
unattributed pending pulses, malformed layer sets/order and excess progress.

The checked-in private awake gate validates all components and full-flash
hash before execution. Two fresh production runs match byte-for-byte at
the fifth attempted hook, with expected CLI exit 3:
`stop=compat-refused pc=0x001291cc instructions=1272353867 virtual_time_ns=32770943068`.
Callback 12, pending ten, retry zero, awake one, initial/reopen/awake hits
`2,2,4`, no remaining pulse and GPIO24 low. Production log SHA-256
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`;
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
These are independently captured production hashes, not diagnostic re-pins.

Snapshot boundaries at instructions 825147127 (before first hook), 825147128
(rise pending), 827008207 (high, inside the first native callback), and
1165000000 (four pulses complete) all resume to that exact final snapshot
and full event-log suffix. The read-only verifier observes native callback
entries at instructions 827008206, 941844404, 1052945626 and 1163113462,
each with awake zero; three native instructions later PC is `0x0012892e`
and awake is one. It uses the normal NEMA backend and production hooks,
without signal injection, callback replacement or guest writes, and its
terminal image equals the cold CLI image. Repeated fifth-hook refusal
preserves the entire image and produces no new intervention log.

All exact ticket commands pass: 775 normal/sanitizer cases; 129 task contracts;
line checks; awake and unchanged startup/reopen/activity private gates, without
skips. A bounded 2.22 run through first awake retains the log and snapshot
hashes in E-EMU-CXD-AWAKE-FAILURE-001. Commands, changed files and external
log paths are in ticket 759's handoff. Status remains pending separate review.
No proprietary artifact enters Git. The evidence still supports only this
four-pulse synthetic liveness fixture, not indefinite operation or GPS fix/time.

### E-EMU-CXD-AWAKE-FAILURE-001

2026-09-06; synthetic host-only probe against `9f8c121`. No firmware input.
`src/devices/sapporo_cxd5610.c` SHA-256
`5cb26a4bb23c8fa4f6738342291ff6b6a9ce6144c7c06395c1c6c69f94486a90`,
`src/core/scheduler.c` SHA-256
`0b361210676f6bc5090060ab1d445f0897b62ed3e6cc3749f5d71eb8519405fc`.
Awake scheduling writes its event context and calls the low-level signal
before scheduler insertion. Exhausting time, event ID or insertion sequence
returns `SEMU_ERR_RANGE` but changes the serialized transport and emits one
signal callback. This violates the full-operation-before-mutation contract.

The rising callback then drives high before attempting to schedule the
falling edge, and ignores a failed insertion except for immediately lowering
the output. With rise representable but fall overflowing, or ID/sequence
exhausted between arm and rise, scheduling and scheduler advancement both
report success. The observed callbacks are low/high/low with no remaining
event: a zero-duration pulse rather than the configured one millisecond.
The void scheduler callback interface has no current failure-reporting path;
this cannot be fixed by silently returning low or by adding a private API.

External `$AWAKE_PROBE_ROOT/atomic-probe.c` hash
`f8fac4c55b992b1647813841ad95dfb171d740fccbdf62aa0c0eb1497f463e26`,
six-case output hash
`6ef1212316e087324754a3c2c204bffd3d28d9814cca2216f75b3bdbebb61ecc`.
Cases 0..2 report `arm=4 signals=1 high=0 events=0 changed=1`; cases 3..5
report `arm=0 advance=0 signals=3 high=1 events=0 changed=1` (enum zero is
success, four is range error). Confidence is high for these reproducible
in-tree failures. Ticket 758 owns the minimal scheduler/device failure
integration and regression tests; 759 must wait for its acceptance. Normal
success timing, event ordering and historical snapshot bytes must remain
unchanged. No fix or new production pulse is included in this evidence step.

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc "$AWAKE_PROBE_ROOT/atomic-probe.c" build/libsemu.a -o "$AWAKE_PROBE_ROOT/atomic-probe"
"$AWAKE_PROBE_ROOT/atomic-probe"
```

Ticket-758 implementation observation, 2026-09-06: the unchanged external
probe rebuilt against the working library reports cases 0..3 as
`arm=4 advance=0 signals=0 high=0 events=0 changed=0`; cases 4..5 now report
`arm=0 advance=4 signals=1 high=0 events=0 changed=1`. The latter state change
is the accepted rise being consumed, not a failed admission; no high occurs.
The in-tree admission/dispatch and CPU regressions first failed on `9f8c121`,
then passed with the public copied-first-error callback contract. Normal pulse
width, equal-deadline IDs/order, high-reset cancellation and rise/fall snapshot
round-trips pass. The snapshot codec was moved unchanged apart from its
internal callback symbol. No firmware trigger or response was added.

Both full suites pass 768 cases. Ticket 757's unchanged authentic reopen gate
passes with its existing hashes and precise GSTP refusal. A clean `git archive
HEAD` build of `9f8c121` and the changed headless build independently validate
all three 2.22 components, then run with `sapporo-2.22-no-device`,
`--max-instructions 1500000000 --max-time 12000000000`. Both consume the
existing first awake intervention at 10317472799 ns and stop at
`pc=0x000d4a8c instructions=599774578 virtual_time_ns=12027701702`,
`stop=budget` (CLI exit 3; sleeping advancement crosses the time limit).
Their complete logs and snapshots compare byte-identically: log SHA-256
`1672376e5f2ba81738ebcf131c29be0dda5db4141bc5e2b859ba9885d0dca92c`,
snapshot `9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.
Artifacts remain external under `/tmp/semu-758-{baseline,current}-222*`.

The unchanged `tools/test_sdl_live_input.sh` gate was also run with the exact
2.22 manifest on both builds. Both fail its historical stop tuple identically:
all expected frame CRCs pass, but the stop is `user`, PC `0x000bacf4`,
774081920 instructions / 6520939902 ns instead of E-SAP-ONBOARD-EMU-011's
804398304 / 9504428769. The complete failed-gate logs compare byte-identically,
SHA-256 `a39f0b54d2ce24e6d420b2f83152b53826aadc6649d915fd0ed088c46c4e8124`.
This is evidence of a pre-existing baseline mismatch, not authority to weaken
or re-pin a golden. Ticket 758 acceptance remains pending that separate
integration review; ticket 759 is not enabled. The implementation handoff
contains exact reproduction commands. No physical GPS accuracy is inferred.

Integrator observation, 2026-09-06: E-SAP-ONBOARD-EMU-012 separately resolves
the historical SDL mismatch; the corrected strict gate now passes. Ticket
758's review reruns every mandatory command successfully, including both
768-case suites and the unchanged private 2.39 gate. A fresh 2.22 headless run
through first awake compares byte-identically with the clean baseline log
and snapshot above. Ticket 758 is accepted and 759 becomes ready, without
enabling a new production fixture.

Additional host-only allocator fault injection uses external source
`/tmp/semu-758-accept.aEyOPs/allocation_probe.c`, SHA-256
`58d493482c775498d87875463cc2d05ebfbd0d93f5eb34b4407848284e1095b3`.
Its scheduler-only `realloc` replacement deterministically returns NULL.
Three assertions pass: failed first heap growth preserves complete transport
snapshot, signal count and queue/ID/sequence state; a full queue's rising
event frees the slot reused by its falling edge, so no allocation or early
ID reservation is needed; a callback that fills the freed slot then triggers
failed heap growth returns `NOMEM` with owned `cannot grow scheduler` text,
leaving all queued tail callbacks unexecuted. The callback clears its local
error after reporting, checking ownership. This is fault injection, not an
attempt to exhaust host memory. No firmware is used.

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Drealloc=semu_review_realloc -c src/core/scheduler.c -o /tmp/semu-758-accept.aEyOPs/scheduler.o
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc /tmp/semu-758-accept.aEyOPs/allocation_probe.c /tmp/semu-758-accept.aEyOPs/scheduler.o build/libsemu.a -o /tmp/semu-758-accept.aEyOPs/allocation_probe
/tmp/semu-758-accept.aEyOPs/allocation_probe
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc -Drealloc=semu_review_realloc -c src/core/scheduler.c -o /tmp/semu-758-accept.aEyOPs/scheduler-sanitize.o
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc /tmp/semu-758-accept.aEyOPs/allocation_probe.c /tmp/semu-758-accept.aEyOPs/scheduler-sanitize.o build/sanitize/libsemu.a -o /tmp/semu-758-accept.aEyOPs/allocation_probe-sanitize
/tmp/semu-758-accept.aEyOPs/allocation_probe-sanitize
```

Both builds and both three-case executions exit 0. The fall-capacity check is
an implementation invariant, not evidence that all callbacks are allocation-
free; ID/sequence exhaustion remains explicitly covered by in-tree tests.

### E-SAP-COMPAT-GPS-REOPEN-239-001

2026-09-06; bounded external in-tree-interpreter experiments on the exact
E-SAP-0011 components, with the normal NEMA backend and both ticket-756 layers.
All components validate; full-flash SHA-256 remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
No production code, CPU/RAM state, compatibility budget, GPIO input or source
firmware is modified in this evidence-capture step. Candidate RX uses only
the existing transport API; it is explicitly synthetic, not physical capture.

From ticket 756's two-layer state-15 image
`bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9`,
the unchanged CLI saves the later UART-open return at instruction 672,044,883 /
4,758,944,162 ns, PC `0x00128bf4`. Pre-open image SHA-256
`0fee8567ae023b8cab0bb682724646069e1678b4f5519cc3ee2f55788fdda8bc`, log
`cb9c3dc744bb2ca5c45353d693817418fb09e4101dc723351037cdae86528735`.
Driver R4=`0x100366d8`, live UART=`0x10046188`, callback +4=`0x0012890f`,
UART +12=zero. Pristine `0x00128dc8..0x00128e9a` observes requested mode 15
at driver +0x74 and flags two at +0x7f, follows the reopen branch, sets pending
seven (+0x273), then arms timeout four for 10,000 ms. The post-arm boundary
is `0x00128e8c`, instruction 672,045,164 / 4,758,944,443 ns: callback +0x272=4,
pending=7, retry `0x100588a4`=0, R5=driver+0x74, R6=driver+0x270. Logging
flag +0x7b=0, cached GNS halfword +0x344=`0x04cb`, and version-seen byte
`0x100588a3`=1. Both initial GPS interventions have already been consumed.

Exactly one synthetic `$PSS0000\r\n` line queued at that boundary with a
10,000,000-ns delay reaches the normal parser branch `0x0012a128`. It schedules
pending seven at 673,001,215 / 4,768,967,430 ns, enters state seven at
674,839,841 / 4,778,862,352 ns, state eight at 675,767,890 / 4,788,304,461 ns,
and state nine at 676,717,923 / 4,797,799,071 ns. Pristine
`0x00128f72..0x00129078` explains the exact path: version-seen skips `@GTIM`,
the matching cached `0x04cb` skips `@GNS`, and the selected branch loads
literal `0x00129384`, exactly `@GSR\r\n`. No command-length expansion is
required. The native TX helper `0x00128c34`, LR=`0x00129075`, is reached at
676,717,951 / 4,797,799,099 ns with pending ten already set.

Status-only execution hits the existing unsupported-command refusal: precise
UART DR fault at 676,735,188 / 4,797,816,336 ns, handler `0x001c0db4`, BFAR
`0x4001d000`, stacked PC=`0x00171798`. A second experiment installs an external
one-use exchanger accepting only those six `@GSR` bytes and returning the same
synthetic line after ten ms, committed at 4,797,816,335 ns. Native parsing
schedules ten at 681,351,968 / 5,396,262,689 ns; state ten is entered at
688,672,230 / 5,405,077,968 ns, and the 5,500-ms state-12 monitor is armed at
688,672,285 / 5,405,078,023 ns. State 12 is entered at 825,147,086 /
10,875,951,887 ns with retry zero. Startup layer hits remain exactly two;
the two extra diagnostic responses do not alter its counters.

The next gap is separate receiver-liveness recovery. Pristine
`0x001291c4..0x00129258` consumes the live flag and later takes its
`Missing awake signal` / `Waking up attempt` branch. It calls helper
`0x0012aa84`, which selects literal `0x0012ad48`, exactly `@GSTP\r\n`.
The TX helper is reached at 940,946,122 / 16,348,990,917 ns, LR=`0x0012921f`;
the one-use GSR exchanger refuses. Precise UART fault follows at 940,963,736 /
16,349,008,531 ns with the same handler/BFAR/stacked PC. No GPIO pulse, GSTP
reply, NMEA epoch, fix/time payload or full GPS/UI behavior is established.

Reproducibility: external `reopen-probe.c` SHA-256
`f4e1b3b3a8598dfcb0936ae4b02c78f7a590769af68319894afd14f0011e3e3f`, built
against `build/libsemu.a` with `cc -std=c99 -Wall -Wextra -Werror -pedantic -O2
-Iinclude -Isrc -Isrc/devices -Isrc/compat`. It validates all firmware and flash
before loading the pre-open image, single-steps with absolute limits of one
billion instructions / 30 billion ns / 200 trace records, and stops at a
precise fault or refusal. Argument two optionally supplies one reopen line;
argument three optionally supplies the one-shot exact-GSR reply. Two positive
runs compare byte-identically: trace SHA-256
`fa47f38f463a2d9c4f29e78a53111a16678163743b92e0645d36e723637b39eb`, log
`0dc6b27a302cebafea021ec995c09e05d80f32fab21fa0afc87ee1035f0ec494`.
Status-only trace/log are
`66bcdccc097392cbe73f590cd3c6a62c669187ffc46f185208b6fb70aa5f0aa4` /
`57e08963c57008b6c0b2d61fc74a4e39ddaf30c269d04c5b58cba9f6f9caac96`.

Negative controls use `$BAD0000\r\n`. A wrong reopen prefix causes no GSR
and reaches the consumed-startup refusal at 908,348,332 / 14,978,074,970 ns;
trace/log hashes are
`35c263ace43f3b59baf694e3a665415063af0adffb887a905b4b5e1d8125cf35` /
`a7d3fd3516e97595886d08a10a87315fef154c6e75629703c5e0e41170e0cf10`.
A wrong GSR-response prefix reaches the same refusal at 926,139,813 /
15,611,043,530 ns without entering state ten; trace/log hashes are
`3acea87f41a9b8a7ddd87e667de91e2e82b751dd00c00f1787460b0a15bd841b` /
`e9650a72600c73a3e068d2359a2656a7efcb20471838b3873b5ea0e073563a13`.
The no-injection baseline retains ticket 756's 908,321,039-instruction refusal;
trace/log hashes are
`8e63bcfa87b1da179b5c9d6d7996c8404faeffcfd78dfb6d60de621274885e9e` /
`9aa06b7ac991b2fc5344272ab8a31cde62e3757d88c2bdd5e59b2de08071cd47`.
All source, trace, log and image artifacts remain external to Git. Confidence
is high for these exact native transitions and synthetic acceptance only.
Ticket 757 specifies separate opt-in integration; no production behavior or
existing golden changes in this maintenance step.

Production integration, ticket 757 (2026-09-06): the separate hash-pinned
`sapporo-2.39-gps-reopen` layer now owns exactly one post-arm status and one
exact-GSR reply, each delayed 10,000,000 ns. Initial startup/VER keep their
original provider, two counters, logs and timing. Reopen progress requires
both initial hits; wrong state, missing dependency, duplicate ownership,
busy RX, overflow/exhaustion and malformed snapshot lifecycles refuse.
The snapshots retain their existing wire format and require the exact layer
set/order. No diagnostic queue ID becomes a register predicate.

Two fresh production runs reach native state 12 at instruction 825,147,087 /
10,875,951,888 ns, PC `0x00128ed8`, callback 12, pending ten, retry zero,
R2=12, initial counts `(2,1,1)`, reopen counts `(2,1,1)`, no CXD RX event.
Both complete logs and images compare byte-identically: log SHA-256
`f63cab509a2da82a867580bf9eac35b3e764df53d08155bb11c47c6dfa328007`, snapshot
`0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`.
The checked-in private gate reproduces these cold runs and resumes from
672,045,164 (before reopen), 672,045,165 (status pending), 676,735,188
(GSR reply pending), and 688,672,231 (native state ten, retry zero). Every
resume matches the final image and full event-log suffix. Initial two-layer
and historical one-layer private gates pass without re-pinning.

The production gate separately reads exact `@GSTP\r\n` at the native TX
helper at 940,946,122, then pins the precise fault at 940,963,736 /
16,349,008,531 ns, PC `0x001c0db4`, BFAR `0x4001d000`, stacked PC
`0x00171798`; both GPS layers retain two hits. A direct state-12 resume to
that fault has snapshot SHA-256
`fa8436d239c147fe152b9e00e8db410efdd29d4e062a735e742c197181efdc96`
and log `e8f69f75c2f9669ce963e0af559d658f8e2e047c6c7f97ea735074b8aeb63fca`.
This is a preserved unsupported-command boundary, not receiver liveness.
No physical status semantics, awake signal, NMEA/time/fix, extra commands,
firmware bytes or frame pixels are added. Artifacts remain outside Git.

### E-SAP-COMPAT-GPS-STARTUP-239-001

2026-09-06; read-only disassembly of Sapporo `2.39.20.22297-P` application
SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`,
all three validated components E-SAP-0011, immutable synthetic full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Local diagnostic artifacts below live in `$GPS_PROBE_ROOT` (external to Git).
The read-only research `$FIRMWARE_ROOT/docs/research/cxd5610-gps-boundary.md`
(`17f9f1e38ba16a38d43f945b0240b72046802fa063afcc8bed00b1ac5ea46eba`)
and `cxd5610-live-epoch-cross-build.md`
(`8b22fb8d660364668c929e174e742c3fb649d16db81da4b5b99b1bd0b5c846e4`)
supplied hypotheses only; the following observations use the exact 2.39 guest.

The production GPS failure is a missing unsolicited startup status, not a
failed transmitted command. With the normal NEMA backend attached, the first
UART open returns through `0x00128bf4` at instruction 357,025,317 /
1,878,373,561 ns. Driver base is `0x100366d8`; its +0x220 UART object is
`0x100472e8`, with callback `0x0012890f`. Startup helper `0x00128c8a`
sets pending state two (+0x273) and arms timeout state four for 3,000 ms via
`0x0012a764`. At `0x00128d14`, instruction 357,031,764 / 1,878,380,008 ns,
R4 is the driver, +0x272 callback state is four, +0x273 pending state is two,
and byte retry counter `0x100588a4` is zero. R0 is **zero**, not the helper's
eventual success value: `0x00128d14` itself sets R0 to one. Do not predicate
this boundary on R0 already being one.

Without RX, timeout/close/reopen repeats without any GPS TX. The retry branch
at `0x00128f30..0x00128f54` increments the counter and asserts at three:
`0x00079e56`, LR `0x00128f55`, R0 points to `CXD5610GF-driver.cpp`, R1 is
910. Ticket 754's unmodified production BKPT remains instruction 932,397,950 /
11,388,431,927 ns, PC `0x00079e1e`.

The exact native line consumer `0x0012a0c4..0x0012a12c` requires length >5,
`$` at byte zero and compares the next three bytes with the `PSS` literal at
`0x0012a348`. It reads byte seven for logging. In the observed normal-mode
branch, `0x0012a128` calls `0x0012a728`, which loads the pending byte and
schedules that state after ten ms via `0x0012a764`. Other mode branches exist;
this is not evidence for a general GPS status parser or physical payload.

An external in-tree-interpreter diagnostic supplies exactly ten synthetic
ASCII bytes `$PSS0000\r\n`, delayed 10,000,000 ns through the existing CXD RX
queue/IRQ path at the first startup boundary. The four zero characters are a
synthetic fixture, **not captured physical status bytes**. Native execution
schedules state two at instruction 367,054,580 / 1,888,402,824 ns (LR
`0x0012a12d`), enters it at 376,256,001 / 1,897,604,245 ns, then calls TX helper
`0x00128c34` at 376,256,008 / 1,897,604,252 ns with exactly `@VER\r\n`.
Status-only execution refuses at the existing no-response UART transport:
HardFault entry `0x001c0db4`, instruction 376,273,275 / 1,897,621,519 ns,
BFAR `0x4001d000`, stacked PC `0x00171798`. This is not an unknown UART map.

A second experiment adds only a one-use, exact-six-byte `@VER\r\n` exchanger
returning the same synthetic line after ten ms. Unknown/repeated commands
refuse. The unmodified consumer schedules pending state 14 at 381,814,268 /
2,495,305,973 ns, enters 14 at 390,944,539 / 2,504,436,244 ns, closes UART,
and enters state 15 at 393,785,844 / 2,564,070,073 ns; retry remains zero.
There is no observed `@SLP` command in this branch. This demonstrates initial
startup/version lifecycle progress without CPU, SRAM, file or budget edits.

The next independent gap is a later UART reopen through `0x00128e7c`, not the
initial startup helper. At instruction 672,044,891 / 4,758,944,170 ns,
`0x0012a764` (LR `0x00128e8d`) arms a 10,000 ms timeout with pending state
seven. No fixture covers that event. It times out at 902,701,905 /
14,711,349,514 ns and retries. The diagnostic ends at its explicit one-billion
instruction limit, PC `0x000a7b50`, time 19,479,370,023 ns, with one startup
injection and one reply; it does not demonstrate full GPS or settled UI.

Reproduction/provenance: build the external `gps-probe.c` against
`build/libsemu.a` with `cc -std=c99 -Wall -Wextra -Werror -pedantic -O2
-Iinclude -Isrc -Isrc/devices -Isrc/compat`; source SHA-256
`9a6e8d4dcf0b62f0cb20e238b70fca3d4275e7b795db61e809d25855deba4ada`.
It validates the manifest, attaches the normal NEMA backend, enables only
`sapporo-2.39-synthetic-wbsto`, loads the pre-arm snapshot, then runs one
instruction at a time with absolute caps of one billion instructions and
30 billion ns. Its second argument is the literal CRLF-terminated candidate;
third argument `version` enables the strict single reply. No argument supplies
an additional compatibility layer or alters native CPU/RAM state.
The production CLI generated `gps-prearm.sems` by resuming the clean 40M
prefix `d2ae7cd38834b3488f9a5785235bd69005e773b129497bf0fa68ac6fa0b47fce`
to absolute instruction 357,031,764. Pre-arm snapshot SHA-256
`2e78d1d551fddec22297d023e336c156d4d9f48d55319dca00b1e061fc3189c3`;
creation log `36f71116521599508022535497b349e2f3c70266b32501627580b0e1c0f2b56b`.

Two independently loaded pre-arm runs (`gps-proof-a` / `gps-proof-b`) have
byte-identical traces and logs: SHA-256 respectively
`d8f05419e9fb73575efb94cb91749144d8e41ad6e28ae510d75e1b4396b48e15` and
`15e04c2e34c648384e02952ae5077381ba848efc14e414eb0f1e8db4be0daad6`.
The negative candidate `$BAD0000\r\n` produces no TX and no version reply;
native retry assertion still halts at instruction 932,397,861 /
11,388,431,861 ns, PC `0x00079e1e`. Its trace/log SHA-256 are
`79ae01c911c20016936071f02392fb7f9ea6413f1cdbc58905a8f9c4cde57092` /
`77b0d55758c52b9775594a0eec533e9a3d123065f61c731ae76b40ced8f96730`.
Status-only trace/log (`gps-first-status`, from the earlier clean prefix) are
`c2fd74140444806f0331a4522aa3294e36077b68d9d317aeff1b789c82611571` /
`0c3a3ac0fa1fa87f61f44bb8e231c40753fbd36f64c3e0c107e6bcba7b50b5e3`.

Confidence is high for these exact native branches and synthetic experiments,
not for physical receiver fidelity. Affected modules for future integration:
2.39 compatibility descriptor, device fixture binding, CXD delayed RX, machine
dispatch/reset, and snapshot restoration. Ticket 756 requires separate opt-in,
three-hash-pinned, two-hit compatibility with success/refusal and snapshot
tests. The current factory/binding exposes only 2.22 GPS fixtures; no 2.22 hook
may be transplanted or silently enabled for 2.39. Delayed-RX schedule failure
must also be tested for full pre-mutation refusal. No production implementation
or new golden is authorized by the external probe alone; later state seven,
physical status fields, further commands, fix/time data and final UI remain gaps.

Ticket 756 integrated verification (2026-09-06): the user-authorized, separate
`sapporo-2.39-gps-startup` layer now queues exactly those two lines. The pristine
startup routine additionally confirms R5=R4+0x270, R6=R4+0x338, writes through
driver +0x346, and UART callback +4=`0x0012890f` / +12=zero. The hook validates
the entire aligned 0x348-byte driver span and 16-byte UART span in SRAM before
the queue operation. R0 remains untouched. Startup and version hit times are
1,878,380,008 and 1,897,621,518 ns, respectively. Both delayed responses reach
the native parser; no native event scheduling, status return, assertion or
CPU instruction is intercepted. Lifecycle authority is the two instance-owned
intervention counters, serialized by the existing machine codec.

Two cold production CLI runs with both layers and normal NEMA rendering reach
instruction 393,785,845 / 2,564,070,074 ns, PC `0x00128ed8`, native callback
15, pending 14, R2=15, retry zero, exactly two aggregate GPS hits and no pending
CXD RX event. Log SHA-256
`b5b23c9f6a96d9ecfbf4f17aa4f3b70801d08cd9b9636330d11c08f2e0647123`, snapshot
`bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9`.
These are new two-layer goldens, not replacements for the one-layer goldens.
Private snapshots at instructions 357,031,764 (unused), 357,031,765 (startup
RX pending), 376,273,275 (version RX pending), and 390,944,539 (native state
14, retry zero) all reproduce the same final image and exact log suffix.
The checked-in C inspector validates all firmware/flash inputs and only reads
snapshots; it performs no guest execution or guest mutation.

Continuation observes pending seven at 672,044,891 with no extra RX/hit. At
908,321,039 / 14,978,258,084 ns the retry byte is one and PC is `0x00128d14`.
The next attempted instruction stops `compat-refused` at the same checkpoint:
the initial one-shot response cannot be reused for this distinct lifecycle.
Direct and intermediate-snapshot continuations have identical retry images.
The older one-layer image refuses when loaded into a two-layer configuration.

Verification: the activation regression failed before the profile addition;
six new synthetic tests cover exact hashes, instance isolation, both delayed
responses, 31 atomic refusal variants (including time/ID/sequence exhaustion
for either response), UART byte/IRQ ordering, reset/rebind, four snapshot
phases, and atomic malformed identity/lifecycle rejection. All 752 normal and
sanitizer cases pass. Exact ticket commands, both private GPS/activity gates,
line/contract checks and results are recorded in ticket 756. Historical
one-layer logo/activity/halt hashes remain exact; source flash retains the
hash above. No firmware, pixels, snapshots, traces or private bytes enter Git.
Later pending-seven support, other commands and full GPS/UI remain unsupported.

### E-SAP-COMPAT-WIDGETS-NATIVE-239-001

2026-09-05; exact components E-SAP-0011, immutable full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`,
ticket-751 snapshot `15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
Read-only application inspection confirms that `0x000ca258` is fallback JSON
used by `0x000ca194`, not the raw type-15 cache representation. The earlier
synthetic-runtime research explicitly left the Widgets consumer unvalidated;
E-SAP-SERIALIZER-ARRAY-239-001 now proves its JSON fixture was incompatible.

The native cache reader at `0x000c9b58..0x000c9b7e` binds the arena bytes to
named type `0xa412`, then calls `0x000e3c7c`. Its type-15 branch dispatches
to `0x001b22e2`; schema resolution yields internal type `0x045f`, schema
`0x0004ddde`. The recursive walker `0x001a03e4` and copy `0x001b2818`
allocate/copy the 12-byte, four-byte-aligned object. Member descriptors at
`0x0004a85e` and `0x0004a864` place the leading four-byte scalar at offset
zero and array at offset four. Array schema `0x0004ddd8` resolves element
size eight. `0x001b21a4` reads the array's 16-bit count at object+4 and pointer
at object+8; object+6 is padding. The synthetic empty object uses a zero
leading scalar, zero count/padding and null input pointer. It contains no
elements, ownership allocation or copied firmware bytes.

A separately labeled disposable diagnostic changes only the emulator-owned
Widgets arena slot to twelve zero bytes (plus four zero alignment bytes),
and entry logical/bounded lengths to twelve. It starts from the pinned
checkpoint, edits no source image or CPU instruction, and leaves every
compatibility budget unchanged. At instruction 608,139,972 the native array
allocation requests zero bytes with cursor/capacity both twelve. Native
copy returns destination `0x10033e50` at PC `0x000c9b7e`, instruction
608,140,162. Its output words are zero, zero and `0x10033e5c`: firmware
relocates the empty array pointer to the end of its destination allocation.
The source remains the cache-owned null-pointer representation. At PC
`0x000c9bb2`, instruction 608,140,325, native status is 200 for LID `0xa432`.
Execution continues beyond the former assertion, eventually exhausting the
unchanged logical-file budget. This diagnostic is not a production golden.

Confidence is high for this empty native cache value, synchronous copy and
successful consumer result. Ticket 753 may replace only that installation
payload/length and its complete validators, without a new hook or budget.
Affected module: `sapporo_239.c`; regression `sapporo_239_compat`/`widgets`
and exact private `sapporo_239_widgets` runs. Nonempty arrays, provisioned
32-byte files and generic JSON/storage serialization remain unproven.
Old post-install JSON checkpoints must be regenerated, not silently migrated;
pre-install and layer-off execution must remain unchanged.

The diagnostic layout trace SHA-256 is
`795966231270d8c65bea08e0bae1177b1d3fc61df409e4baae4b7bd272afaad6`,
log `14830646dbcd9e7593bc1a2dec3f1880c7e44a26a3537f093a150bd4d6ebbce2`.
After correcting installation, a new cold-prefix-derived snapshot at
instruction 607,105,617 has hash
`b17a3b485f89779a3dc8191f1417c6d225a65fdcc41f0681d1cb068c1ad24d90`.
Unmodified production execution from it reproduces the entire same trace,
including native copy output and status 200, with no diagnostic writes.
Its log hash is
`b38a60f1f5a64430ce074d63c601e85eb278cdad92dd0631f18a202595738328`.

### E-SAP-COMPAT-ACTIVITY-239-001

2026-09-06; exact application E-SAP-0011, pristine disassembly and bounded
in-tree interpreter trace from rendered-logo snapshot SHA-256
`30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
All three components validate and full flash retains SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
An external diagnostic executable raises only its process-local logical-file
ceiling to 200,000, with absolute limits of one billion instructions and
30 billion virtual nanoseconds; no guest RAM/CPU, mode, path or ABI edits.
This initial diagnostic did not attach the CLI's NEMA rendering backend;
its later dump request is not the production-renderer checkpoint.
Trace SHA-256 `625a0d344fd50260f946d26b221384be85210732fe912dd89783ff47d4a241f8`,
log `353df500ddaedd28c4f042db3273c8ad3ee4bdc9cd6d75c1177564b6cd75729f`.

The suffix begins at instruction 610,599,945 / 2,149,556,528 ns. Database
`actitmln/247.bin` performs nine operations: update-open, seek 32, two
40-byte writes, seek zero, read 24, seek zero, write 24, close.
`actitmln/ongoing.bin` performs twelve: the same sequence with an additional
seek 72 and two 40-byte writes before the header read/update. Neither file
grows: sizes remain 46,112 and 152. Both handles close, the last at instruction
610,601,105 / 2,149,557,688 ns. Exactly 21 operations yield 76,279 file hits
and 76,282 aggregate hits. No additional handled file operation occurs before
the distinct refusal at instruction 639,161,545 / 3,146,465,889 ns:
PC `0x000920b4`, LR `0x000843e9`, R0 `0x00079df4`, R1=10, `wui_dump.bin`.

Pristine `0x000b9df8..0x000b9e30` opens the store path at +64 and closes
after header synchronization. Record writer `0x000b9518..0x000b9584` checks
capacity, seeks `32 + index * 40` via `0x000b94e0`, verifies each 40-byte
write and advances the ring index before writing the next empty record.
Header reader `0x000b938e..0x000b93b6` requires seek-zero success and 24
bytes read; `0x000b932a..0x000b9356` validates and rewrites 24 bytes. The
trace follows these success branches, not retries or an adapter ABI failure.

Confidence is high for this finite update suffix and its measured ceiling.
Ticket 754 may add exactly 21 hits, not diagnostic headroom. Preserve all
native-created bytes, paths/capacities, ABIs, other budgets and frame/snapshot
contracts. The dump request remains unsupported; its owner and native mode
semantics need separate recovery. This establishes activity-update progress,
not settled UI, provisioning or physical-panel behavior. Diagnostic source,
trace and private snapshots stay outside Git. Validation uses synthetic
activity-budget success/refusal tests and an exact private continuation gate.

With the normal NEMA backend attached, a read-only single-step continuation
from the production 639,161,545-instruction snapshot reproduces the CLI halt
at instruction 932,397,950 / 11,388,431,927 ns, PC `0x00079e1e`.
Trace SHA-256 `aa84252ae748aacf46edd677fc619b586e48420405b85d43f10d6f37c04ba21a`,
log `8e1e8b051f5b7db1a111954e40c168bd7c52f6557618dd08d0bef7c07f07ccf8`.
At instruction 932,397,654, assertion entry `0x00079e56` receives R0
`0x00129d98` (native `CXD5610GF-driver.cpp`), R1=910, LR `0x00128f55`.
Pristine `0x00128f30..0x00128f54` handles state/event value five, increments
a byte counter at R4+2 and asserts when it reaches three. This independently
identifies a GPS-driver failure, not a file-budget or dump-path refusal.
The exact missing exchange/retry cause remains unproven; no GPS behavior is
changed here. The missing backend also explains why earlier diagnostic-only
paths must not be interchanged with rendering-backed CLI evidence.

Production pre-BKPT checkpoint:
`stop=budget pc=0x00079e1c instructions=932397949 virtual_time_ns=11388431926`.
Log SHA-256 `3a625809c79c1fdb8937ac36cd6e912b026ffcdc8fbc80c7ed888680d8bb11a7`,
snapshot `8d9b262474b00c6c0a2b5423ce4100363eb4582a96205eae8d045b70c8ae50a7`.
All 21 activity operations have identical times/results in the diagnostic
and production paths; no later logical-file operation or reset is observed.

Ticket 754 verification: all 740 normal and sanitizer cases pass, including
the failing-before/passing-after 21-operation synthetic regression and atomic
budget refusal. The private runner validates all components before execution;
two independently cold-started logo snapshots retain the exact historical
hashes, both continuations match the new log/snapshot above, and a checkpoint
after both updates resumes identically. One-step resume executes the real
BKPT; source flash stays immutable. There are no new interventions or formats,
and historical private goldens remain unchanged. No GPS fix is included.

### E-SAP-BOOT-LOGO-239-001

2026-09-05; ticket 753 exact production runs using components E-SAP-0011,
the explicit `sapporo-2.39-synthetic-wbsto` layer and full flash pinned above.
After native Widgets copy succeeds, the unmodified renderer publishes a
240x240 RGB565-LE Suunto boot logo, stride 480, size 115,200, generation two.
Pixel SHA-256 is
`3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373`,
CRC32 `4979f432`. A private local rendering was visually inspected; the logo
is white on black. No frame pixels or firmware bytes enter Git. This is a
visible boot frame, not settled setup, button interaction or physical-panel
completion. The CLI's existing `normal-frame` gate means first nonblack frame.

Fresh production logs/snapshots match at
`stop=user pc=0x00093be2 instructions=609300000 virtual_time_ns=2148256583`:
log SHA-256 `63eb4997ff645958e70ed0586613762f88ee5e6e699434c1fbae48f0f435528b`,
snapshot `30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
There is no reset/refusal before the logo and file hits remain exactly 76,258.
The 40-million-instruction pre-install prefix stays exact: log
`253ffdd99ca7b8fb972518ad7e50701f37306436d67a5114085d94bda01ae11b`, snapshot
`d2ae7cd38834b3488f9a5785235bd69005e773b129497bf0fa68ac6fa0b47fce`.
The private runner compares two fresh runs, pre-install and corrected-cache
resumes, exact pixel hash through a read-only CLI callback, and source flash
immutability. Its preframe checkpoint is aligned to the CLI's existing
100,000-instruction polling grid so resumed frame stops use the same boundary.
No frontend or execution semantics are changed.

Resuming the captured logo without the frame stop reaches the unchanged
file-budget refusal: PC `0x000920b4`, instruction 610,599,945, time
2,149,556,528 ns. The attempted operation is update-open mode three for
`actitmln/247.bin`, LR `0x000b9e0d`. No synthetic operation is performed and
the budget is not increased. Recover this post-logo activity sequence and its
bounded requirements before authorizing more logical-file operations.
The independent preframe single-step diagnostic without a frame observer
reaches the same request at instruction 609,821,432 / 2,148,778,015 ns;
that diagnostic bound is not interchangeable with the rendered-logo snapshot
continuation. The latter is the production private gate and next work baseline.
Its read-only trace SHA-256 is
`955c2d0c237dfdc442559a1fec8404606762febec6c8ccb110a09b80cdcbc8dd`, log
`b2c682699eb527144e55807d3d7e6d90094a1ee042e858cc4b06a22b103b9a6e`.

Confidence is high for native empty Widgets decoding and the deterministic
logo frame. The corrected fixture is still explicitly synthetic, not recovered
watch state. Layer-off execution retains the E-SAP-0029 log hash
`db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`.
An old JSON-bearing ticket-751 snapshot still reaches its native serializer
halt at instruction 608,140,267 when resumed; no silent repair takes place.
Both preload validators refuse such stale JSON/lengths atomically. Historical
JSON-bearing checkpoint hashes remain recorded unchanged, but must not be
used as current corrected-fixture goldens. Generate new snapshots from reset
or from the validated pre-install prefix.

### E-SAP-CTIMER13-INTEN-239-001

2026-09-05; exact components E-SAP-0011 and immutable full flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
The E-SAP-COMPAT-QUIET-READ-239-001 pre-fault snapshot hash is
`15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
Pristine PC `0x000cb84c..0x000cb852` reads CTIMER INTEN into R1,
ORs R4, then stores R4 at R0=`0x40008060`. At instruction 607,105,617,
time 2,146,062,159 ns, R1=`0x00004001` and R4=`0x08004001`.
The next instruction takes the precise HardFault vector, not an invalid opcode.

The independently rehashed Apollo4 Plus PAC 1.0.0 `timer.rs` SHA-256
`5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`
places INTEN at offset `0x60`. Its `timer/inten.rs` SHA-256
`52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`
defines bit 27 as `TMR131INT`, Timer13 CMP1, with read/write access and reset
zero. Bits zero and 14 remain Timer0 CMP0 and Timer7 CMP0 (E-SAP-0029).
The existing model already accepts `0x08000000` and `0x08000001`; the new
observation authorizes only the additional whole-register value `0x08004001`.
Keep the trace-derived compare scheduling, IRQ gates/clear and reset behavior.
This is not evidence for general hardware-complete interrupt-mask semantics.

Confidence is high for the exact retained value and bit identity. Affected
modules: `timer.c` and `timer_snapshot.c`; ticket 752 validation uses
`apollo4_timer_inten13` and the fixture-gated `sapporo_239_ctimer13_inten`.
Neighboring values, widths, offsets and malformed snapshots must refuse
atomically. No additional timer or compatibility behavior is authorized.

Both narrow regressions fail before the change (MMIO acceptance and independent
snapshot import) and pass afterward. The combined value retains readback while
the existing channel-13 compare deadline and clear remain exact. Rejected
neighboring values, widths, offsets, malformed/truncated snapshots preserve
complete serialized state. Reset cancels the event and restores INTEN zero.
All 737 normal and ASan/UBSan cases pass. Two fresh authentic runs match at
`stop=budget pc=0x00079e1c instructions=608140266 virtual_time_ns=2147096849`:
log SHA-256 `2223de22981528b7cd2df049be68ea2e4022627763da13aab9293ef1fbbf7e16`,
snapshot `74e45df965216d809cf41e090ee0fc56b740affbc6d32aec35413dd65db1aa0c`.
The ticket-751 prefix retains both hashes and resumes to the identical new
snapshot. There are no resets/device refusals or additional compatibility hits;
logical-file operations stay 76,258 and source flash is unchanged. One more
instruction executes native BKPT, halting at PC `0x00079e1e`, instruction
608,140,267, time 2,147,096,850 ns. No normal frame has been reached.

### E-SAP-SERIALIZER-ARRAY-239-001

2026-09-05; exact components E-SAP-0011 and full flash/prefix hashes from
E-SAP-CTIMER13-INTEN-239-001. Read-only in-tree single-step tracing from the
ticket-751 checkpoint, bounded by 610,000,000 instructions / 30,000,000,000
ns with no guest-state or budget edits, reaches a native serializer assertion.
Trace SHA-256 `efbd4d7e3fb23af4a1c9c769b90ecc32d3fca942e031aeaa9f52ad6de2d73ecd`;
log `d503f9effc5be7778ed5b593e82b244a49fb799db145a42948987e33469d9828`.
Pristine PC `0x001957b2..0x001957b6` passes line 38 and the string
`ChunkSerializer.cpp` at `0x001957d4` to the normal fatal path `0x00079e56`.

The allocator `0x00195778` checks an aligned allocation against its context's
capacity. Context `0x10033c60` has buffer `0x10033e50`, capacity 16, cursor
12. At instruction 608,139,972, caller `0x001b21ca` requests 199,568 bytes
(`0x00030b90`), alignment four. The allocator returns `0xffffffff` when the
new cursor becomes 199,580, triggering the assertion. The earlier 12-byte
allocation succeeded. The preceding serialization path carries LID `0xa432`
and synthetic value pointer `0x100002b0`.

At instruction 608,139,955, `0x001b21a4` receives R1=`0x0004ddda`,
R2=`0x100002b4`. It loads a 16-bit count from R2 and multiplies by the
schema element size derived from `[R1+2] >> 6` (eight). The retained synthetic
JSON value starts at `0x100002b0`; its bytes at `0x100002b4` represent `ra`,
little-endian `0x6172` (24,946), giving exactly 199,568. This is direct evidence
of an incompatible synthetic representation at this native array consumer,
not evidence that the serializer needs a larger buffer or a success override.

Confidence is high for assertion identity, bounds failure, pointer/count/size
provenance and deterministic halt. Affected future scope: the exact synthetic
WbStorage value ABI for LID `0xa432`; recover its complete native object,
array and lifetime contract before changing the cache or its validators.
Validation here is ticket 752's private pre-halt/one-step halt gate. The
required replacement layout is unresolved; no cache repair, assertion bypass,
snapshot change or new compatibility hit is authorized by this observation.

### E-SAP-COMPAT-QUIET-READ-239-001

2026-09-05; read-only pristine application disassembly at
`0x000bdfd0..0x000be062`, exact components E-SAP-0011 and full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`;
ticket-749 snapshot SHA-256
`13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
The mode-string construction depends only on R1's mode bits, not the path in
R0. Exact mode nine always passes `r` to native open `0x000cd1ae`; bit mask
`0x08` only suppresses the later failed-open diagnostic. The filename is
forwarded unchanged to the native filesystem. E-SAP-COMPAT-ZIP-READ-239-001
already demonstrates native ZIP handle `0x30`; the next identical mode request
is `ui/js/config.js`, PC `0x000920b4`, LR `0x000843e9`, instruction
459,796,107, time 1,998,752,649 ns. The ZIP-only adapter guard is narrower
than the recovered native read ABI.

Confidence is high for filename-independent native quiet reads. Ticket 751
may generalize not-handled routing to exact mode nine for validated paths
outside the twelve-entry synthetic file table, using its existing case-folded
lookup. This follows existing non-table mode-one native routing: path/volume
resolution, content validation and missing-file return remain firmware-owned.
This is not a wildcard synthetic file, host-file adapter or successful-open
override. Table-owned paths still refuse mode nine even when absent, avoiding
an inconsistent native view of retained logical data. Modes 10/11, extra
flags and malformed path syntax remain refused; mode 1/2/3 is unchanged.
Affected module: `sapporo_239_file_hook.c`; validation:
`sapporo_239_quiet_read` unit/private tests and retained ZIP regression.
No budget, descriptor, handle or snapshot change is authorized. The native
UI result and next independent blocker remain to be traced.

Read-only in-tree tracing from that snapshot, bounded at 610,000,000
instructions / 30,000,000,000 ns with no guest-state or budget edits, reaches
native open `0x000e74c4` for `ui/js/config.js` at instruction 459,796,462.
The public return to `0x000843e8` at instruction 459,848,032 carries native
handle `0x40`; firmware closes it at instruction 459,866,463. It next opens
`ui/js/fonts.js`, then many UI scripts/styles. The first 600-million-
instruction segment records 134 opens over 105 unique paths; its trace hash
is `2c70bf261784dc54a959b143801a1bb84fd5b3fb2646e8a6629dc729bc53e4fe`.
The full pre-fault trace hash is
`1bf92ecee2ef656ffad8916a1c81299657967195ba11e02c51d9047d05aec29c`;
its log hash `db09b5ae5c799d3fda8a727460ed0cca518004d37c84b982c336dffaf8903495`
reports unchanged total file hits 76,258. No synthetic content is supplied.

The next independent boundary is a native `STR r4,[r0]` at `0x000cb852`:
R0=`0x40008060` (CTIMER INTEN), R1=`0x00004001`, R4=`0x08004001` after
the preceding OR. The one-instruction continuation takes precise fault vector
`0x001c0db4` at instruction 607,105,618, time 2,146,062,160 ns, with fault
address `0x40008060` and PSP `0x10034720`. Native fault handling later requests
reset at instruction 607,105,697, time 2,146,062,239 ns. This evidence does
not authorize the newly requested interrupt-enable bit; recover its timer
contract in a separate ticket. No normal frame is reached before the fault.

Verification: the native-read routing regression fails before the correction
and passes afterward, including absent/present protection of all twelve
logical paths, retained contents/handles, and normal mode-one rereads. The
existing ZIP regression preserves register/RAM/file/counter/log atomicity,
unknown-mode/flag/syntax/disabled-layer refusals and exhausted-budget native
execution; its non-table mode-nine cases now follow the recovered native ABI.
Two fresh exact logs/snapshots match before the timer fault: log SHA-256
`740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec`,
snapshot `15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
The ticket-749 prefix stays byte-identical and resumes to this same new state;
one-step continuation reaches the precise fault vector. The private runner
validates all components, unchanged 76,258 file hits, no earlier reset/device
refusal, and immutable source flash. Full normal and ASan/UBSan suites each
pass 735 cases. Historical private runners and goldens are untouched; only
their later non-table quiet-read refusal is superseded by native execution.

### E-SAP-COMPAT-ZIP-READ-239-001

2026-09-05; read-only pristine application disassembly, exact components
E-SAP-0011; ticket-748 snapshot SHA-256
`42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`, full
flash `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
The native caller `0x000843da..0x000843e8` selects mode nine for a clear
object flag and ten for a set flag. Public open forwards unchanged mode/path
through `0x000920b4 -> 0x000bdfd0`. The latter validates `(mode & 7)` in
1..4, initializes the mode string to `r` from `0x000be22c`, changes it for
write/update/append, and calls `0x000cd1ae`. Mode nine keeps `r` unchanged.
Mask `0x08` is tested only after an unsuccessful open at `0x000be036`; when
set, it skips the failure logging branch. This is read plus quiet failure,
not a request for synthesized archive contents or an alternate volume.

Read-only `$FIRMWARE_ROOT/docs/research/sapporo-2.39-zapp-installer.md`,
SHA-256 `a49b393d1c9a91d2cc525aaf47588fdb53d6ce94192c4bfba2cf7633c823ab6b`,
independently records the native directory scanner discovering
`zapp/zwspee01.zip` and opening it in mode nine. This reference is supporting
evidence, not authorization to import its separate Editor WFA overlay.
The in-tree request is independently observed at instruction 451,511,675,
time 1,990,468,217 ns, PC `0x000920b4`, LR `0x000843e9`.

Confidence is high for this exact request's native read semantics. Ticket
749 may let only normalized `zapp/zwspee01.zip` / mode nine fall through the
existing logical-file adapter. It must not mutate guest/file/counter state or
supply a return value. All native filesystem checks still execute; all other
unknown modes remain refused. Affected module: `sapporo_239_file_hook.c`.
Validation: `sapporo_239_zip_read` unit/private tests, unchanged historical
checkpoint and dual-run/resume checks. Native ZIP result and next boundary
are measured below; no archive completeness or installation is claimed.

Bounded, read-only instruction tracing from the old snapshot (600,000,000
instructions / 30,000,000,000 ns; no diagnostic budget or guest-state change)
reaches native open `0x000e74c4` at instruction 451,512,009. The HCC wrapper
returns real handle `0x30` at `0x000be01e`, instruction 451,622,396; the public
call returns that handle to `0x000843e8` at instruction 451,622,704. Native
close later receives the same handle at instruction 451,773,751. No logical
ZIP slot, host file or synthetic return value was involved. Trace SHA-256
`721ca166681243b1cf0ca941d6c22fe84022f4755c5694b30dba5a7d57385321`,
diagnostic log `b513c2ccbde63696cd74b93efd1b385abe767a32fb849c6144394affe6c6cc74`.
The next refusal is mode nine for `ui/js/config.js`, PC `0x000920b4`, LR
`0x000843e9`, instruction 459,796,107, virtual time 1,998,752,649 ns. File
hits remain 76,258. That distinct path is outside this bounded exception.

Verification: the new regression fails before the change on the first exact
ZIP read and passes afterward. Full check and ASan/UBSan each pass 734 cases.
The exact private runner compares two fresh complete logs/snapshots, verifies
76,258 file hits, no reset/device refusal, immutable full flash and unchanged
ticket-748 prefix hashes. Resuming that prefix produces the same new snapshot.
New checkpoint log SHA-256
`ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732`,
snapshot `13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
One attempted instruction from the new snapshot preserves the mode refusal at
the same PC/instruction/time. Only the historical next-step ZIP refusal is
superseded; no earlier golden or expected stop is weakened.

### E-SAP-COMPAT-ONGOING-239-001

2026-09-05; read-only pristine application disassembly, exact Sapporo 2.39
components E-SAP-0011; immutable ticket-747 snapshot SHA-256
`3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1` and
full-flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
At PC `0x000920b4`, LR `0x000b944d`, R1=2, R5=`0x1003e070`, the path
at store+64 is `actitmln/ongoing.bin` and the little-endian word at store+36
is three. Creation `0x000b943c..0x000b94d6` opens enum two, writes a 24-byte
header, eight zero padding bytes, and loops exactly store+36 times writing
40-byte native empty records, then closes. The header includes 16-bit value
195 at +4, record width 40 at +6, record capacity at +8, zero at +12 and a
native time value at +16. Validation `0x000b93b8..0x000b93f2` checks the
header and requires file size minus 32 to equal capacity times 40.

Confidence is high for exact path and maximum native creation size
`32 + 3 * 40 = 152`. Ticket 748 may append only that path/capacity and
retain native-written bytes; no initial data or cursor repair is authorized.
Affected modules: exact-build file table/snapshot codec and bounded layer
descriptor. Validation: `sapporo_239_ongoing` unit/private tests, historical
snapshot compatibility and atomic parser/capacity refusals. Later lifecycle
is bounded by the following native execution; no later mode is authorized.

An isolated interpreter executable with only a process-local diagnostic file
ceiling of 200,000 resumes that snapshot with 600,000,000-instruction and
30,000,000,000-ns limits. The appended path is the only new behavior; there
are no guest-state edits. Diagnostic trace SHA-256
`55a30554caabebc4c3ed1485bdca8a9bc2bbb44e61561b59276069b55c2dfc93`,
log `8f9ff2ce056409b4484550782fa0a66b663af87168224a86d4a8e9e1f65adf98`.
Twenty operations create, reopen, validate and read the ongoing file; five
writes are exactly 24, 8, 40, 40, 40 bytes. The native size check returns 152.
The rest are 408 operations on the already-evidenced `settings/personal` and
66 on `zapp/storage.sbm`, without capacity or ABI changes. Exactly 494 new
operations yield a total ceiling of 76,258 (aggregate 76,261). The next
wrapper call refuses mode nine for `zapp/zwspee01.zip`, PC `0x000920b4`,
LR `0x000843e9`, instruction 451,511,675, virtual time 1,990,468,217 ns.
Production may use only that measured ceiling, not diagnostic headroom.
The ZIP path and mode-nine semantics require separate evidence/ticket.

Production verification: two exact fresh runs match at `stop=budget`, PC
`0x000920b4`, instruction 451,511,675, time 1,990,468,217 ns; log SHA-256
`33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e`,
snapshot `42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`.
Ticket 747's eleven-slot snapshot/log retain their exact hashes and resume
to the same new snapshot. Its next-step unknown-path refusal is historical;
the current next step refuses mode nine without advancing instruction/time.
The unit regression fails before path support and passes after, including
synthetic byte-preserving lifecycle, exact-capacity excess refusal, unknown
mode/path, old/new snapshot round trips and atomic malformed-state refusal.
Private source flash is unchanged; no reset, device refusal or normal frame
is reported before the new boundary.

### E-SAP-COMPAT-HISTORY-239-001

2026-09-05; exact application E-SAP-0011, read-only disassembly at
`0x000ba180`, `0x000ba22a`, `0x000ba266`, `0x000ba2d8`, `0x00134386`,
`0x001344bc`, `0x00134ef6`, and bounded C interpreter diagnostics using full
flash `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Ticket 746's 2,671-operation stop occurs inside record reader `0x000ba22a`,
returning to `0x000ba2ac`. Seek compares the requested `32 + index * 72`
against its success return, then a 72-byte read succeeds. Search `0x000ba266`
iterates at most the store's 248 records. Empty records cause no match and
the outer query returns 416 normally at `0x000ba324`; there is no file retry.

The caller at `0x00134386` initializes a day counter to 59 and decrements
through zero at `0x001344bc`; the other caller at `0x00134ef6` starts at 41.
The dynamic order is 42, 60, 42 days, with successive query timestamps
differing by 86,400,000 milliseconds. All 144 queries read all 248 records:
35,712 reads and 35,712 seeks. Each returns 416 without translation. The
historical reference's 2,671 total in E-SAP-COMPAT-FILES-239-001 predates the
corrected size/seek ABI and does not cover these now-reachable native scans.

An isolated executable linked to the unchanged interpreter raises only its
process-local file counter ceiling to 200,000 (explicit diagnostic log), with
600,000,000-instruction / 30,000,000,000-ns bounds, no guest-state edits and no
path/operation relaxations. It starts from instruction 405,860,000 snapshot
`2414e607e40dc665d6a0615d3d38c92580e1499365f51e6b0bfbf5af40f02116`.
Trace SHA-256 `59661304ee0c1e736f679dfd8b6d22cb6cc6839192bb1facbff35754488fa32c`;
log `4d4f8efa2fbc74ee9908dabfc5dc6985d9d32108b8352a961baf9a2967cc8359`.
It completes the scans, creates and reads the existing allowlisted Activity
Timeline database, and refuses `actitmln/ongoing.bin` enum-create at
PC `0x000920b4`, LR `0x000b944d`, instruction 439,081,594, time
1,978,038,136 ns, after exactly 75,764 logical operations. No record content,
return ABI, filesystem path, or provider status was changed by the probe.

Confidence is high for the finite loop and measured count. Ticket 747 may
raise the existing logical-file ceiling to exactly 75,764 (aggregate 75,767),
not the diagnostic ceiling. All other guards remain unchanged. The unknown
ongoing-file layout/capacity requires separate evidence and remains refused.
Validation: `sapporo_239_history_budget` unit/private tests; exact old-prefix
and dual-run snapshot checks. Diagnostic sources and outputs remain outside Git.

Ticket 747 verification passes the failing-before/passing-after narrow test,
full check, 17 Sapporo 2.39 sanitizer cases and the exact private runner.
Two fresh runs match at PC `0x000920b4`, instruction 439,081,594, time
1,978,038,136 ns: log SHA-256
`6f47fad1eeb3b6032955b463e2c4ba26310dbf5ddc453ae3f0f350acf15a9348`,
snapshot `3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1`.
The old ticket-746 prefix retains both original hashes and resumes to that
identical new snapshot. The next attempted instruction refuses the unknown
write path without advancing PC/time/count; source flash remains immutable.
There are 35,714 total sleep seeks including two earlier header rewinds.
All normal operations retain their original result/state semantics. No normal
frame or additional file schema is claimed.

### E-SAP-COMPAT-PRELOAD1-239-001

2026-09-05; direct read-only C interpreter trace and pristine application
disassembly, exact Sapporo 2.39 components E-SAP-0011 and full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
From instruction 405,860,000, bounded trace SHA-256
`3c06f813eac80e86875570c82d8cd5ab6f4b2cfe69c2cba4007fae655815700c`
records native reader `0x000c9d54` with descriptor addresses `0x00192f94`,
`0x00192fb8`, `0x001930b4`, `0x001932d0`: the same four LIDs established
by E-SAP-COMPAT-WBSTO-239-001. Each logs status 204 at `0x000c9da0`.
The final callback is `0x00124844`, instruction 416,223,667, R0
`0x10025634`, R3=500; descriptor provider `0x001c0ed8`, command one.
Read-only inspection of ticket-744 snapshot SHA-256
`92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16`
confirms all 40 context, 80 tree and 40 data bytes still match the installed
synthetic cache. The missing record reads do not populate or alter that cache.

The read-only reference `$FIRMWARE_ROOT/emulator/renode/sapporo-2.39-preload-compat.resc`,
SHA-256 `72dd2d7f7fd2640004c55e9da058cdf8b82d96d2c5a8904a1a7f7896d0ca7dc2`,
explicitly translates both preload commands; no other reference override is
adopted. Persisted payload sectors are absent below the compact OTA fragment,
as established by the cache-map research in E-SAP-COMPAT-WBSTO-239-001.
Thus a faithful lower-level record representation remains unavailable.
Ticket 746 authorizes only an additional, separately logged one-hit command-one
translation, gated by both prior interventions and exact cache revalidation.
Existing synthetic values remain disposable, not recovered defaults. Confidence
is high for this exact boundary; no general preload semantics are inferred.
Validation: `sapporo_239_preload1` unit/private tests and snapshot migration tests.

Ticket 746 verification: two fresh logs/snapshots match at PC `0x000921dc`,
instruction 435,333,559, virtual time 1,974,290,101 ns. Log SHA-256
`476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71`;
snapshot `27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f`.
The new intervention fires once. At the next attempted instruction the existing
logical-file budget refuses without retiring the read; no budget was increased.
Resuming a genuine older three-counter snapshot produces the identical new
snapshot. Full check, 16 Sapporo 2.39 sanitizer cases, and the private dual-run
checkpoint/refusal/immutable-flash runner pass. No normal frame is claimed.

| ID | Source | Product/version | Observation | Confidence / validation |
| --- | --- | --- | --- | --- |
| E-SAP-COMPAT-SEEK-239-001 | 2026-09-05; read-only exact application disassembly at `0x00092182`, `0x000be0a4`, `0x000922d6`, `0x0016fab0`; bounded in-tree C trace from instruction 405,860,000 to ticket-743 BKPT | Sapporo `2.39.20.22297-P`, application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; components E-SAP-0011 and full flash E-SAP-COMPAT-FILES-239-001 | At instruction 405,861,738 the public seek wrapper receives handle `0x1015fd00`, offset 32, origin zero. The adapter moves the cursor but returns zero; guest `0x0016fac2` compares R0 with requested 32 and returns false without a record read, ultimately yielding TrainingTss result 500. Native helper `0x000be0a4` preserves the input offset in R5 across the lower seek and returns that original value on success; wrapper epilogue preserves it into R0. Thus the public return is the requested offset bit pattern, not zero and not the computed absolute cursor, for all accepted origins. | Direct static and dynamic evidence authorizes only the successful seek return correction using existing checked cursor/hit/refusal logic. No TrainingTss status translation or record fabrication is needed for this comparison. Ticket 744 regression fails before and passes after, with unchanged atomic refusals; full check and 12 Sapporo 2.39 sanitizer tests pass. Native TrainingTss reads all 42 records and reports status 200 at instruction 405,884,883. Two authentic runs match at PC `0x00079e1c`, instruction 416,256,851, time 1,955,213,393 ns: log SHA-256 `4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e`, snapshot `92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16`. There are 595 logical-file hits, no reset/device refusal and immutable source flash. One-step resume halts at PC `0x00079e1e`, instruction 416,256,852, time 1,955,213,394 ns. The next failure is WbStoPreload command one: descriptor `0x10025634` stores provider pointer `0x001c0ed8` and command byte one; saved LR `0x001248b9` and result 500 at SP `0x10025340` identify the StartupClient failure path. Missing preload state remains unresolved; no normal frame or additional status translation is claimed. |
| E-SAP-COMPAT-FILE-SIZE-239-001 | 2026-09-05; read-only exact application disassembly at `0x00092244`, `0x000be13c`, `0x000b9ff8`, `0x000ba058`; bounded C instruction trace from instruction 393,118,000 to ticket-742 BKPT | Sapporo `2.39.20.22297-P`, application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; components E-SAP-0011; full flash E-SAP-COMPAT-FILES-239-001 | Native sleep header checks all pass at `0x000b9ff8`. At instruction 393,119,087, wrapper `0x00092244` receives synthetic handle `0x1015f900` in R0; missing interception sends it to native stat helper `0x000be13c`, returning zero at instruction 393,143,528 / PC `0x000ba084`. Validator `0x000ba058` subtracts 32 from this file length and compares against 248 records times 72. The session adapter has already retained exactly 17,888 guest-written bytes, so the missing size wrapper, not the header or empty-record contents, causes this check to fail. Integrate this exact public ABI using retained size and existing logical-file hit/log/refusal semantics. | Direct read-only execution plus disassembly; this corrects the cause of E-SAP-STARTUP-SLEEP-239-001 and explains why the reference's first-boot result translation is unnecessary for this check. No status translation or fabricated records authorized. Ticket 743's two narrow regressions fail before the fix and pass after; full check and Sapporo 2.39 sanitizers pass. Post-fix native sleep startup returns 200 at instruction 393,135,988. Two authentic logs/snapshots match at PC `0x00079e1c`, instruction 405,895,301, time 1,927,243,545 ns: log SHA-256 `8139068b549a4e2be4baf57c94bc3b8eff385cb2bbb8efca506a8b30469de0d8`, snapshot `0fa411dde053a15ef42d1b4ce2bf7282ad1990f88532b059dcf1c14ca9824193`. There are 512 logical-file operations, no reset/refusal, and unchanged source flash. Size calls return 17,888 for sleep and 2,384 for training; the subsequent assertion belongs to TrainingTss command zero (descriptor `0x10025634`, name pointer `0x001c2468`, result 500 at SP `0x10025340`). One-step resume halts at PC `0x00079e1e`, instruction 405,895,302, time 1,927,243,546 ns. Training's later failure cause remains unresolved; no normal frame yet. |
| E-SAP-OHR2-CMD2-239-001 | 2026-09-05; read-only native-firmware reference capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log`, SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`, lines 150–155; E-SAP-OHR2-ECHO-239-001 pre-request checkpoint; exact-hash in-tree ticket 742 runs | Sapporo `2.39.20.22297-P`, exact components E-SAP-0011; synthetic full flash SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | After echo, the reference records command `0x0002`, sequence seven in MAIN, with fifty `ff` request data bytes and CRC `a3 3b ef 8c`. Its 58-byte response echoes command/sequence, has fifty zero data bytes, and CRC `f3 cc ff f0`. The shared enum/registry admits MAIN and the 2.39 provider supplies this exact body; BSL and absent/legacy providers remain refused. Two authentic runs and snapshots match at PC `0x00079e1c`, instruction 393,235,868, virtual time 1,914,584,112 ns: log SHA-256 `9161895c12da70077ec78fb76bae6062196194a80f1df5b8c9609876fa20b17a`, snapshot `c36512287d4bf7d5a06762334ba261d984d0259a1466ec83076e73ebb253dcb0`. The request/response completes at 1,890,385,573/1,890,388,462 ns, with 449 logical-file operations by the later checkpoint, no reset/refusal, and immutable source flash. | Exact reference startup transcript, successful/refused packet and snapshot tests, full check and OHR2 sanitizers, and private dual-run/one-step runner pass. Physical module semantics and the meaning of command 2 are unknown. No normal frame yet; the next firmware breakpoint is E-SAP-STARTUP-SLEEP-239-001. |
| E-SAP-STARTUP-SLEEP-239-001 | 2026-09-05; read-only exact application disassembly at `0x00079e1c`, `0x00079e56`, `0x00124844` and read-only ticket-742 snapshot SHA-256 `c36512287d4bf7d5a06762334ba261d984d0259a1466ec83076e73ebb253dcb0`; one-instruction C resume | Sapporo `2.39.20.22297-P`, exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; components E-SAP-0011 | Resume executes BKPT at `0x00079e1c` and halts at PC `0x00079e1e`, instruction 393,235,869, time 1,914,584,113 ns. Saved LR `0x001248b9` links the assertion helper `0x00079e56` to StartupClient's failure path. R4 `0x10025634` references a descriptor whose module pointer at +12 is `0x001c3924` (`sleepln`) and command byte at +16 is zero. SP `0x10025340` holds result 500, matching the failure-path stack argument at `0x001248a0`; the path passes source line 67 and filename `StartupClient.cpp` at `0x001248d4` to the helper. | High for breakpoint, registers, descriptor and saved result; read-only control-flow inference identifies the startup failure. Private command-2 runner pins the breakpoint. Root cause of result 500 is unresolved; this authorizes investigation, not a guessed success response or bypass of BKPT. |
| E-SAP-OHR2-ECHO-239-001 | 2026-09-04; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; ticket-739 snapshot SHA-256 `d7c30abd8ff1744c1644b2730953d45012c547977b24c905873b37fa2533b8e0`; exact-hash in-tree ticket 741 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records echo command `0x0006`, sequence six with data prefix `fc 60 e8 83 d5 01 00 00 00 00` and forty `ff` bytes; its reply returns all fifty data bytes unchanged and shares request/response CRC `30 4f fc d9`. Read-only inspection of the exact pre-request ticket-739 snapshot at SRAM `0x1002fa0c` records the deterministic guest's distinct data prefix `00 f4 51 c2 8c 01 00 00 00 00`, the same forty-byte `ff` tail, and request CRC `29 e8 3c ef`. The profile-selected physical provider accepts only those two complete MAIN-state bodies and returns the data unchanged. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 369,037,329, virtual time 1,890,385,573 ns: log SHA-256 `2ef900dbf79d08a83e94c2e6d8e642c53b51fa40d3faaca977319ac64f4d59cf`, snapshot SHA-256 `5168ba1e48997e23553370705d49f4ea0f83c407337576bb3c7a56cacb308686`. They complete echo without reset, preserve 118 logical-file interventions, and leave source flash unchanged. One-instruction continuation submits command `0x0002`, sequence seven; the generic command registry refuses it before provider dispatch and the precise fault vector is PC `0x001c0db4`. | High for both complete observed request bodies, exact echoed data/CRC semantics, wrong-state/prefix/tail refusal atomicity, retained existing bodies, deterministic dual runs, unchanged snapshot format and compatibility count, and immutable source flash. This is not a permissive echo implementation. Command `0x0002` remains unsupported and requires a separate interface-owning ticket despite its native bytes being visible. |
| E-SAP-OHR2-RESULT14-239-001 | 2026-09-04; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; exact-hash in-tree ticket 739 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records result command `0x000e`, sequence five with all fifty data bytes `0xff`. Its reply echoes command/sequence, has fifty zero data bytes, and has little-endian CRC `81 df dd 28`. The dedicated profile-selected physical provider validates MAIN state and the complete request fill before mutation and supplies that exact zero body. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 369,026,992, virtual time 1,890,375,236 ns: log SHA-256 `8e1e68584a5d869f91c07216add9d6d1befbc36aac26718113359b76298fd1c4`, snapshot SHA-256 `d7c30abd8ff1744c1644b2730953d45012c547977b24c905873b37fa2533b8e0`. They complete command 14 without reset, preserve 118 logical-file interventions, and leave source flash unchanged. One-instruction continuation submits echo command `0x0006`, sequence six; the provider refuses it before the precise fault vector at PC `0x001c0db4`. | High for exact command/state/fill, zero response body/CRC, wrong-state and malformed-fill refusal atomicity, retained existing bodies, deterministic dual runs, unchanged snapshot format and compatibility count, and immutable source flash. Echo `0x0006` and command `0x0002` remain unsupported even though their native bytes are visible; each requires a separate bounded ticket. |
| E-SAP-OHR2-RESULT13-239-001 | 2026-09-04; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; exact-hash in-tree ticket 738 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records result command `0x000d`, sequence four with all fifty data bytes `0xff`. Its reply echoes command/sequence, has fifty zero data bytes, and has little-endian CRC `56 d9 ff 43`. The dedicated profile-selected physical provider validates MAIN state and the complete request fill before mutation and supplies that exact zero body. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 368,995,288, virtual time 1,890,343,532 ns: log SHA-256 `eb76c862ba97bd1b0f5ae569b62dcfd3544ecf39d06e3b791de22ce57c2f2331`, snapshot SHA-256 `5359e0cdf8f62514c88b6a90cb381e40c55811a748fcf5b510319268680100f4`. They complete command 13 without reset, preserve 118 logical-file interventions, and leave source flash unchanged. One-instruction continuation submits result command `0x000e`, sequence five; the provider refuses it before the precise fault vector at PC `0x001c0db4`. | High for exact command/state/fill, zero response body/CRC, wrong-state and malformed-fill refusal atomicity, retained existing bodies, deterministic dual runs, unchanged snapshot format and compatibility count, and immutable source flash. Result command `0x000e`, echo `0x0006`, and command `0x0002` remain unsupported even though their native bytes are visible; each requires a separate bounded ticket. |
| E-SAP-OHR2-ID-MAIN-239-001 | 2026-09-04; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; exact-hash in-tree ticket 737 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records MAIN identity command zero, sequence three with all fifty data bytes `0xff`. Its reply echoes command/sequence, is otherwise zero except `MAIN\0` at payload offsets 9..13, and has little-endian CRC `b4 1f 2b 76`. The dedicated profile-selected physical provider validates the complete identity fill before mutation and selects `BSL\0` or `MAIN\0` solely from modeled OHR state. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 368,958,374, virtual time 1,890,306,618 ns: log SHA-256 `d82ebc5b061787b8cefad7f64f7b70168858bc8da29adb644cd486211a8bfc22`, snapshot SHA-256 `352cdedcec47360eb478c6eec3649534025c7373b19c9c35c90e3922549c8a81`. They complete MAIN identity without reset, preserve 118 logical-file interventions, and leave source flash unchanged. One-instruction continuation submits result command `0x000d`, sequence four; the provider refuses it before the precise fault vector at PC `0x001c0db4`. | High for exact request fill, response body/CRC, modeled-state selection, malformed-fill and later-command refusal atomicity, retained BSL/boot-mode behavior, deterministic dual runs, unchanged snapshot format and compatibility count, and immutable source flash. Result commands `0x000d`/`0x000e`, echo `0x0006`, and command `0x0002` remain unsupported even though their native bytes are visible; each requires a separate bounded ticket. |
| E-SAP-OHR2-ID-BSL-239-001 | 2026-09-04; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; exact-hash in-tree ticket 736 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records BSL identity command zero, sequence one with all fifty data bytes `0xff`. Its reply echoes command/sequence, is otherwise zero except `BSL\0` at payload offsets 9..12, and has little-endian CRC `f7 33 0c 4b`. A dedicated profile-selected 2.39 physical body provider validates the complete fill before mutation and supplies only that BSL response plus the existing boot-mode body; it never falls through to the 2.22 compatibility fixture. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 368,947,987, virtual time 1,890,296,231 ns: log SHA-256 `c5599a2faf3016cdeb85bbb2cd6951f70fad49d6732639bbda861d7f5348c1ed`, snapshot SHA-256 `2a823cb69c1bdb7463233c553a2e55312c462bca99aa1715246cb1fd3866d690`. They complete BSL identity, the existing fire-and-forget reboot, and the second boot-mode exchange in MAIN, contain no reset, preserve 118 logical-file interventions, and leave source flash unchanged. One-instruction continuation submits MAIN identity command zero, sequence three; the provider refuses it before the precise fault vector at PC `0x001c0db4`. | High for exact request fill, response body/CRC, profile and BSL-state selection, malformed/cross-state refusal atomicity, retained boot-mode behavior, 2.22 isolation, deterministic dual runs, unchanged snapshot format and compatibility count, and immutable source flash. MAIN identity and every later result/echo body remain unsupported even though their native bytes are visible; each requires a separate bounded ticket. |
| E-SAP-OHR2-BOOT-239-001 | 2026-09-03; read-only native capture `$FIRMWARE_ROOT/emulator/display/captures/sapporo-239-native-full-ui/trace.log` SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; read-only OHR transport research `$FIRMWARE_ROOT/docs/research/ohr2-startup-handshake.md` SHA-256 `105c9835dd6dd6e5375db1665a3f3582b3c1eb4764aea89eef4722ab5b1c93fc`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; exact-hash in-tree ticket 735 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native capture records command `0x0010` twice: sequence zero before BSL identity and sequence two after fire-and-forget reboot but before MAIN identity. Both requests contain data byte `0x01` followed by forty-nine `0xff` bytes; both replies echo command/sequence, zero all fifty data bytes, and carry the valid CRC. The transport accepts that exact payload in BSL or MAIN, refuses any mode/fill mismatch before mutation, and the profile-selected 2.39 device provider supplies the zero reply without a compatibility layer or hit. OHR reset now actively drives GPIO62 low even when internal ready was already low, allowing the first low-to-high ready transition. Two fresh exact runs and snapshots are byte-identical at PC `0x0014e8ea`, instruction 359,790,038, virtual time 1,881,138,282 ns: log SHA-256 `b8977bf8911cc5435e19afc109205c267e822249c17e19fba3809779d046664e`, snapshot SHA-256 `b7d1d84e2be435635cc6031b8424ece436b6557d3ba3883c59f92b7550916f86`. They record ready high, the successful request, ready low, and the successful response; contain no reset; preserve 118 logical-file interventions; and leave source flash unchanged. One-instruction continuation submits identity command zero, sequence one, which the unbound 2.39 body provider still refuses before the precise fault vector at PC `0x001c0db4`. | High for both native command forms, exact payload/reply/CRC, BSL and MAIN transport coverage, reset/ready order, 2.39-only body selection, malformed and 2.22 refusal, deterministic dual runs, unchanged snapshot format, compatibility count, and immutable source flash. Only the first command-`0x0010` exchange is reached by the C firmware run; the second remains behind the independently evidenced 2.39 identity/reboot startup bodies. No other OHR command or state transition is authorized by this entry. |
| E-SAP-GPIO-WT1-239-001 | 2026-09-03; Apollo4 Plus PAC 1.0.0 crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`, `gpio.rs` `a73c178c00164f03a17ccb192364d306fe7fb61922ae524e660bbe5bd8817c03`, and `gpio/wt1.rs` `37bb353d62d8094028dffaa60a9ec3101a78d4495d0f76abcdb013fb5ec71410`; read-only `$FIRMWARE_ROOT/emulator/results/sapporo-apollo4-gpio-edge.trace` SHA-256 `d8e7c7a7d73583525e6a10a7e794bce3698050889c3b1060754fbbad110fe2c6`; exact application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`; pristine disassembly `0x000cce98..0x000ccebe`; exact-hash in-tree ticket 734 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The PAC identifies GPIO offset `0x218` as aligned 32-bit WT1, the read/write output-state word for pins 63–32, reset zero; reads reflect current output state including WTS/WTC effects. The native trace records WTS1/WTC1 operations, including set values `0x00040000` and `0x00200000`. Pristine firmware operation 1 selects WT base `0x40010214`, adds the pin bank, reads the word, shifts by the pin index, and returns one bit. At the ticket-733 fault, the invocation queries pin 53 and the already modeled WTS/WTC state is WT1 `0x00040000`, so bit 21 is low. Exposing only WT1 readback advances two fresh authentic runs byte-identically to PC `0x0014e8ea`, instruction 359,772,704, virtual time 1,881,120,948 ns: log SHA-256 `48c514ba4504a25122c60e71e2b3966fba9463edc9a3641b4ba3ab5854ff9e02`, snapshot SHA-256 `20febdf889a8d8baf7d146f6a1f1bcac6182d009b4ad3ed4b4ace4bd9f28d81a`; neither run resets and the logical-file compatibility count remains 118. One-instruction continuation emits a refused OHR2 request (`command=0x0010`, sequence zero, BSL state) and enters the precise fault vector at PC `0x001c0db4`. | High for register identity/reset/readback semantics, exact guest pin attribution, WTS/WTC-derived value, strict positive/refusal coverage, deterministic dual runs, one-instruction boundary, unchanged snapshot layout, and immutable source flash. Direct WT writes and WT0/WT2/WT3 remain unsupported. OHR2 command `0x0010` is the next independent protocol evidence gate; no response or state transition is authorized here. |
| E-SAP-HAPTIC-CAL-239-001 | 2026-09-03; read-only `$FIRMWARE_ROOT/docs/research/feedback-startup-haptic.md` SHA-256 `64858799bdfe96e56b918051e05051ea52cc7c4729f11e0c11a9f70219d6a13e`; haptic endpoint `$FIRMWARE_ROOT/emulator/renode/haptic/SapporoHapticPmic.cs` `fc7533bbcca2d6cb15d22edf0b40bb884c7df702d58f9f2bac0bbd76b291e968`; native startup probe `$FIRMWARE_ROOT/emulator/results/sapporo-startup-request-probe.log` `c987d798436da654ab17955e6830fd93f288086e42ceac5c5811ea56f0995357`; exact-hash in-tree ticket 733 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | After firmware observes autotune complete bit 1 in register `0x22`, the native path performs separate one-byte reads of calibration registers `0x23` and `0x24`. The reference endpoint resets its 256-byte register array to zero and the native transcript does not write either calibration register, defining zero-valued, read-only synthetic fixture bytes without a physical-calibration claim. The strict endpoint accepts only a separate one-byte read of either register and refuses writes and multi-byte spans before mutation; the values add no state or snapshot bytes. Two fresh authentic runs and snapshots are byte-identical at PC `0x000cceb2`, instruction 357,033,113, and virtual time 1,878,381,357 ns: log SHA-256 `3aee5f2f3271f54448ab2ca681908e6dfa766348b4dfbe0e2099add4c7b24ca7`, snapshot SHA-256 `c287c2c1e256e55c100b083a6db1b35730a646ad9aabeea21600347873a9e95e`; neither run resets and the logical-file compatibility count remains 118. One-instruction continuation enters the fault vector at PC `0x001c0db4`, identifying the next strict boundary as a word read at GPIO address `0x40010218` by guest PC `0x000cceb2` (`LDR r1, [r1]`). | High for the register order, reset-zero reference fixture, strict positive/refusal coverage, deterministic dual runs, unchanged compatibility count, one-instruction boundary, and immutable source flash. Calibration values are synthetic reference defaults, not measured physical values. GPIO offset `0x218` remains unsupported; no GPIO meaning or response is authorized without independent evidence. |
| E-SAP-IOM4-HAPTIC-239-001 | 2026-09-03; read-only `$FIRMWARE_ROOT/emulator/results/sapporo-apollo4-iom-dma-boundary.trace` SHA-256 `f9a02ffdc2ac41bab2cf32b58c5ee1723fa5468de01e3ee9b03f14b5075d95ea` and summary `69fbd57f07e350b67d25f846b730b5a85f28c36313b23dc9d9464aa4d6e2daed`; `$FIRMWARE_ROOT/docs/research/feedback-startup-haptic.md` `64858799bdfe96e56b918051e05051ea52cc7c4729f11e0c11a9f70219d6a13e`; haptic endpoint `SapporoHapticPmic.cs` `fc7533bbcca2d6cb15d22edf0b40bb884c7df702d58f9f2bac0bbd76b291e968`; IOM wrapper `SapporoApollo4Iom4.cs` `b1d1dc8ee41ed84a52d22e3dd510ffb88950619638c93fc57cb43facfe8f1be7`; exact-hash in-tree ticket 732 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The native haptic autotune poll is a one-byte IOM4 P2M request to I2C address `0x50` with command `0x22000112`; the command high byte is register selector `0x22`. Reference offset descriptors vary relative to the DMA target, so the former fixed `dma_target - 8` recovery is invalid for this shape. The post-731 C fault confirms it delivered stale byte `0xa0`, causing the strict endpoint refusal and forced HardFault that preceded `SYSRESETREQ` at instruction 122,452,650. Address-scoped command selector forwarding completes the `0x22` poll and advances 5,258 instructions to the next command at PC `0x0014e8ea`: `0x23000112`, the separately evidenced calibration-register `0x23` read. Two fresh runs and snapshots are byte-identical at instruction 122,457,908 and virtual time 1,230,996,595 ns: log SHA-256 `f1c41ec3d40617174d8cbb299883c69b028a6bfb445b44a0bb7aaf2622915354`, snapshot SHA-256 `629ba604acfbb1eb1265a755283c6133b9650d45dcf2c44d9439752365e22247`; one-instruction continuation enters the fault vector at PC `0x001c0db4`. | High for the exact address/direction/command selector, contradictory-adjacent-SRAM regression, unchanged non-`0x50` selector behavior, deterministic dual runs, one-instruction boundary, and immutable source flash. The haptic endpoint still intentionally refuses calibration registers `0x23`/`0x24`; their values and span are the next independent evidence gate. No generic selector inference or compatibility behavior is authorized. |
| E-SAP-LPS22-239-001 | 2026-09-03; read-only `$FIRMWARE_ROOT/docs/research/sapporo-2.39-lps22-pressure.md` SHA-256 `356bf18b1c47a7ca92a3f155a8cbaede850dabaf45fe4db461f8cffde7b5579f`; `$FIRMWARE_ROOT/emulator/renode/pressure/SapporoLps22.cs` `d65ec2bc2978ee4a75c3b35c55a3853891f05257dfd05ea1bb8fc33675eac936`; focused reference test `test_sapporo_239_lps22.py` `cae4f6f4d8692322e4116b607ab046f0e2cdf6d38ad341997002534e66a76182`; versioned fragment `sapporo-2.39-lps22.repl` `8e9f2eabb126be1dd3da015fa6e8d50cfaa7afbf9beee67f6895a8b87bc03de5`; IOM wrapper `SapporoApollo4Iom4.cs` `b1d1dc8ee41ed84a52d22e3dd510ffb88950619638c93fc57cb43facfe8f1be7`; exact-hash in-tree ticket 731 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | Firmware probes LPS22 at IOM2/I2C address `0x5c`. Its exact fourteen-command transcript reads registers `0x0f`, `0x11`, `0x10`, and `0x33`; writes `11 80`, `11 04`, `10 02`, `10 0e`, and `10 1e`; accepts LPS22HB identity `0xb1`; observes immediate self-clear of CTRL_REG2 bits `0x80` and `0x04`; and retains CTRL_REG1. The version-scoped endpoint and exact IOM selector forwarding remove the E-SAP-COMPAT-FILES-239-001 HardFault without a compatibility hit. Two 79,000,000-instruction runs and snapshots are byte-identical: log SHA-256 `fa74015d06b9aa988787724e666f4e37c1223c14fc36f996cd773b2a93d61592`, snapshot SHA-256 `75f0f534bfc9aae60adabd642f0d4fa982146ed9a743abb7e584aa0e1664e290`; both stop at PC `0x000a7b2e`, virtual time 520,829,069 ns, with no reset. One-instruction snapshot continuation stops at PC `0x000a7b30`, instruction 79,000,001, virtual time 520,829,070 ns. The source flash remains unchanged. | High for the hash-pinned identity/transcript, strict positive/refusal coverage, 2.39-only wiring, unchanged 2.22/2.33 device snapshot bytes, older 2.39 snapshot restore, deterministic dual runs, and immutable source flash. Register `0x33` returns only the reference's zero fixture; physical pressure/temperature, LPS22HH, conversions, FIFO, IRQ, alternate addresses, and unobserved register spans remain unsupported. A longer exploratory run reaches a distinct firmware reset request at instruction 122,452,650 and virtual time 1,230,991,337 ns; its cause remains the next evidence gate. |
| E-SAP-COMPAT-FILES-239-001 | 2026-09-03; read-only `$FIRMWARE_ROOT/docs/research/sapporo-2.39-startup-wbsto-preload.md` SHA-256 `3a14f05280e28093b8fcb8d7e5cad9ac152057b2a40239c7463df328f4da4722`; executable reference `$FIRMWARE_ROOT/emulator/renode/sapporo-2.39-preload-compat.resc` SHA-256 `72dd2d7f7fd2640004c55e9da058cdf8b82d96d2c5a8904a1a7f7896d0ca7dc2` and runner SHA-256 `a68c8a5d3b28758255bfae5af6fb36e70e1e3a1cf9c7269e80a1e6bc21d6908f`; pristine application disassembly at the public file wrappers; exact-hash in-tree ticket 729 runs | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | Public wrapper entries are open `0x000920b4`, close `0x000920f4`, tell `0x00092146`, `f_truncate` `0x00092162`, seek `0x00092182`, write `0x000921a8`, read `0x000921dc`, and flush `0x0009221a`. The bounded reference performs 2,671 logical-file operations (2,549 writes, 26 opens, 26 closes, 19 reads, 34 seeks, 16 tells, and one flush) over exactly eleven writable paths with observed native size ceilings: `settings/sync.txt` 0, `settings/uiv2.txt` 235, `settings/general` 1505, `logs/entries.bin` 12, `message/history.bin` 10800, `sleepln/sleep.bin` 17888, `tssln/tss.bin` 2384, `pois/poi.bin` 68272, `actitmln/247.bin` 46112, `settings/personal` 1727, and `zapp/storage.sbm` 64 bytes. The opt-in exact-build layer retains only firmware-created bytes for those paths in machine-owned session memory; missing read/update files continue to native storage, while unknown create paths, modes, synthetic handles, ranges, and capacity excess refuse before mutation. Two exact C runs are byte-identical (log SHA-256 `82fe5769ed4c6d886a8adca29ac4cda61b5a7b8bc1e63045ca42c26bd4fcb5b9`; snapshot SHA-256 `d3f7d317553d689ca7595631a84193d788c4cc95563acbefeaeb241aa9364024`) with 118 logical-file hits, retain `sync.txt` at 0 bytes, `uiv2.txt` at 235, and `general` at 1505, and remove the former `0x0f676e34` FAT underflow. They stop one instruction before the next deterministic reset at PC `0x000d2f6c`, instruction 78,868,137, virtual time 520,697,206 ns; snapshot resume reproduces the reset on the next instruction. The reset is caused by a precise HardFault on IOM2 address `0x40052120`; its PSP exception frame has stacked PC `0x0014e8ee`, LR `0x000a5d13`, r0 `0x10`, r1 `0x0f000112`, r2 `0x0f`, and r3 `1`, matching the reference's IOM2 transaction at PC `0x0014e8ea` while probing I2C address `0x5c`. | High for exact wrapper/path/capacity pins, complete preflight and atomic snapshot refusal coverage, deterministic dual runs, immutable source flash, preserved layer-off halt, and one-instruction snapshot continuation. The retained files are session-local native outputs, not recovered persisted data. This entry authorizes no FAT repair, host-file access, wildcard path, fabricated content, generic IOM behavior, or pressure-sensor response; the IOM2/`0x5c` transaction is the next independent evidence gate. |
| E-SAP-COMPAT-WBSTO-239-001 | 2026-09-03; exact-hash in-tree ticket 728 runs; read-only `$FIRMWARE_ROOT/docs/research/sapporo-2.39-startup-wbsto-preload.md` SHA-256 `3a14f05280e28093b8fcb8d7e5cad9ac152057b2a40239c7463df328f4da4722`, `sapporo-2.39-wbsto-cache-identities.md` `9a5e0ddf2532b6eecac449c308880b7853667cc707074fc4dbc87f44001fc0bb`, `sapporo-2.39-wbsto-cache-map.md` `d263d04545e595f6cf16a9d68437822efc4445f1456bef1b11cec48b57c8adb9`, `sapporo-2.39-wbsto-value-abi.md` `d90bc3f4690647157e3b4480030eae54f734cd5606043691caf62c081e9a860c`, `sapporo-2.39-synthetic-wbsto-runtime.md` `280db874100b0bc4a03e908a2a3fed197677ebf7be31295ad9066ec8380403b5`, disposable `wbsto239/synthetic-runtime.resc` `352595abda4c4b7077017914da37deeb53ac01ad453120771802a9b7f80e711f`, and exact preload compatibility source `sapporo-2.39-preload-compat.resc` `72dd2d7f7fd2640004c55e9da058cdf8b82d96d2c5a8904a1a7f7896d0ca7dc2` | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The failed `StartupClient.cpp:67` request is `WbStoPreload` command zero at shared client `0x10025634`, provider `0x001c0ed8`, status 500. It follows successful `WbStoManager` provider `0x001c0ec8`. Four native file-backed preloads return 204 for LIDs `0xa431`, `0xa42a`, `0xa432`, and `0xa427`; their omitted persisted bytes belong below the compact OTA fragment, so no FAT or `data.jsn` bytes are recoverable. The opt-in `sapporo-2.39-synthetic-wbsto` layer validates the exact empty native context at `0x1000021c`, then installs four 20-byte session-cache entries at `0x10000004` and aligned synthetic values `zwwatc01`, `0`, `{"arrayData":[]}`, and `yellow` at `0x100002a0`. These are explicitly disposable values, not recovered defaults. The four native preload reads remain 204; only the exact final command-zero 500 result is translated to 200, after the installed cache is revalidated byte-for-byte. One layer-off run retained E-SAP-0029 exactly. Two layer-on logs were byte-identical (SHA-256 `b1156669803cbd2c09e16599fa3719ff2adeecb493eb3749e20fcec34b8f37c0`), each with exactly two intervention hits and no reset, advancing from the firmware halt to `stop=unmapped-access`, PC `0x0007038c`, instruction 78,496,951, virtual time 526,979,533 ns, on the native 32-byte FAT-cache write to `0x0f676e34`. | High for exact component/profile pins, provider/status sequence, cache context/layout, full preflight/no-partial-mutation tests, bounded translation, layer-off stability, immutable source flash, and deterministic advanced checkpoint. The values remain synthetic and session-local. The new stop confirms that the compact OTA fragment is not a coherent writable filesystem; it authorizes no broad memory mapping or guessed FAT state. |
| E-SAP-0029 | 2026-09-03; ticket 727 exact-hash production-path runs; Apollo4 Plus PAC 1.0.0 at crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`; `timer.rs` SHA-256 `5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`; `timer/inten.rs` SHA-256 `52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`; pristine disassembly at `0x000f7d4c..0x000f7d84` and `0x00124870..0x001248d4`; pre-halt snapshot SHA-256 `552c0ef371008e455199e1c13dcb82ee69efcaf727d2641009c7fc53f3fc758f` | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The PAC identifies INTEN bits zero and 14 as Timer0 CMP0 and Timer7 CMP0. Accepting and retaining only the observed combined whole-register value `0x00004001` removes the E-SAP-0028 precise fault and all reset events from the bounded run. Two complete one-line logs are byte-identical (SHA-256 `db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`) and stop at `stop=halt`, PC `0x00079e1e`, instruction 72,774,982, virtual time 521,257,564 ns. The preceding instruction at `0x00079e1c` is `BKPT #0`; a snapshot one instruction earlier records LR `0x001248b9`. Pristine code at `0x001248b0..0x001248b4` passes line 67 and the string `StartupClient.cpp` to the fatal path at `0x00079e56`, which disables interrupts and reaches the breakpoint. | High for exact INTEN readback/refusal atomicity, existing-format snapshot round trip, immutable source, two-run no-reset checkpoint, and static identity of the terminal fatal call. Ticket 726's runner is bounded before the combined write at instruction 49,456,350, PC `0x000f7afc`, virtual time 441,085,024 ns. The halt is firmware-owned rather than an unsupported instruction or device transaction; its triggering StartupClient state requires separate reverse engineering before any new behavior is authorized. |
| E-SAP-0028 | 2026-09-03; ticket 726 exact-hash production-path runs; Apollo4 Plus PAC 1.0.0 at crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`; `timer.rs` SHA-256 `5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`; `timer/inten.rs` SHA-256 `52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`; pristine disassembly at `0x000cb84c..0x000cb854` and `0x000f7d4c..0x000f7d84`; bounded pre-reset snapshot SHA-256 `f10a9c8363a96d0eade5a22188ca265b90bbb4e045768af89ef9315f4959f4a6` | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The PAC identifies CTIMER offset `0x60` as read/write INTEN, reset zero, and bit 14 (`0x00004000`) as Timer7 CMP0 interrupt `TMR70INT`. Accepting and retaining only that additional whole-register value removes the E-SAP-0027 precise fault without changing the trace-derived scheduler or IRQ gates. Two complete logs are byte-identical (SHA-256 `5e0d8dd23c863aaa00b44489d9235b4967183d44d26d49f538f094e55e6affe0`): the first reset advances to instruction 49,456,422 at 441,085,096 ns, and the final halt is PC `0x00079e1e`, instruction 165,500,890, virtual time 885,004,292 ns. The new precise fault is again at INTEN: while handling external IRQ21, pristine PC `0x000f7d76..0x000f7d7e` reads `0x00004000`, ORs bit zero, and writes `0x00004001`. A pre-reset snapshot records CFSR `0x00008200`, HFSR `0x40000000`, BFAR `0x40008060`, and an MSP exception frame with r1 `0x40008060`, r2 `0x00004001`, stacked LR `0x000f7d59`, stacked PC `0x000f7d80`, and xPSR `0x41000025`. | High for offset/bit identity, reset/readback, neighboring-value refusal atomicity, existing-format snapshot round trip, and two-run checkpoint. Ticket 724's runner is bounded before INTEN at the byte-identical 41,435,600-instruction checkpoint `pc=0x000cb84c`, virtual time 278,677,209 ns. The combined `0x00004001` value is a separate fail-closed boundary; no generic INTEN mask or IRQ-model redesign is authorized. |
| E-SAP-0027 | 2026-09-03; ticket 724 exact-hash production-path runs; Apollo4 Plus PAC 1.0.0 at crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`; `timer.rs` SHA-256 `5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`; `timer/intclr.rs` SHA-256 `b82119d005e0ba5f231ec456df38d7af458443fb51945335c290114acf483519`; pristine disassembly at `0x000cb830..0x000cb86e`; bounded pre-reset snapshot inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The PAC identifies CTIMER offset `0x68` as write-one-to-clear INTCLR and bit 14 (`0x00004000`) as Timer7 CMP0 interrupt `TMR70INT`. Accepting only that newly observed whole-register value and mapping it to the model's channel-7 pending/IRQ state removes the E-SAP-0026 precise fault. Two complete logs are byte-identical (SHA-256 `32a5bc1df226ca20da0c94aa90dc121fea14125f45890397f3671d6cb95c0b33`): the first reset moves 22 instructions and 22 ns to instruction 41,435,683 at 278,677,292 ns, and the final halt is PC `0x00079e1e`, instruction 139,585,840, virtual time 398,187,029 ns. A snapshot immediately before the reset records BFAR `0x40008060`; its exception frame has r0 `0x40008060`, r1 zero, stacked LR `0x000cb84b`, and stacked PC `0x000cb854`. Pristine code at `0x000cb84c..0x000cb852` reads CTIMER INTEN, ORs firmware-held `r4=0x00004000`, and writes the result, so the next exact refusal is INTEN value `0x00004000`, not INTCLR. | High for offset/bit identity, write-one-to-clear behavior, channel-7 deassertion, exact-value refusal atomicity, existing-format snapshot round trip, and two-run checkpoint. The prior flash runner is bounded before timer handling at the byte-identical 40,000,000-instruction checkpoint `pc=0x000dac46`, virtual time 160,176,520 ns. No generic INTCLR mask or INTEN expansion is authorized; INTEN requires a separate ticket. |
| E-SAP-0026 | 2026-09-03; ticket 723 exact-hash production-path runs; `$FIRMWARE_ROOT/emulator/renode/mspi2/SapporoApollo4Mspi2.cs` SHA-256 `a782983bbc12a93078362a2e121d4e1d786944e16405e86c61b5c8ad0c029c2f`; pristine disassembly at `0x001026c8..0x0010274e`; debugger exception-frame and MSPI2 DMA inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic 32-MiB full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`, containing the existing synthetic manufacturing sector SHA-256 `c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53` | Firmware issues MSPI2 DMA configuration `0x17`, page-program instruction `0x12`, device address `0x008d0000`, SRAM source `0x100407f8`, and count 256. The pinned reference model programs each byte as `old & requested`; the former in-tree direct call to the strict generic storage API instead refused the first requested 0-to-1 bit, raising a precise HardFault at stacked PC `0x0010272a`. Merging the complete validated page with existing bytes before calling that unchanged storage API removes this fault. Two post-fix production logs are byte-identical (SHA-256 `21c415dee3c52ef4f77f0da42c3ea4020e0c7dc0e7092bf3bcab08d2c6661cc5`): the first reset moves to instruction 41,435,661 at 278,677,270 ns, and the run halts at PC `0x00079e1e`, instruction 139,587,697, virtual time 398,188,886 ns. Debugger inspection identifies the new independent precise fault as a 32-bit write of `0x00004000` to CTIMER address `0x40008068` by instruction `str r0,[r1]` at PC `0x000cb882` (stacked return PC `0x000cb884`, LR `0x0012334f`), after the flash DMA has completed. | High for the exact NOR merge, immutable source, write-enable lifecycle, regression, and deterministic advanced checkpoint. The focused test proves requested 0-to-1 bits complete without becoming set and that the next program without WREN refuses. The external fixture is hash-gated and opt-in; it is not physical-watch data or a profile default. CTIMER value `0x4000` is a separate fail-closed boundary and is not authorized by this entry. |
| E-SAP-0025 | 2026-09-03; ticket 722 implementation runs from the exact-hash `sapporo-2.39.20` profile; E-A4-TIMER-001 trace-derived offset contract; pristine disassembly at `0x000f7d4c..0x000f7d76`; debugger fault inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011) | Firmware PC `0x000f7d60` temporarily writes `0x00012300` to the existing CTIMER pattern-address offset `0x104`, after reading `0x00012301` and before restoring it at PC `0x000f7d74`. Implementing only that additional whole-register value and its existing-format snapshot validation removes the last observed precise fault. Two runs bounded by 200,000,000 instructions and 2,000,000,000 ns produce byte-identical one-line logs (SHA-256 `96e428de7caf01f866ed3a91193a7e45ff2c37d700a63a9deb9764d8f0506890`), contain no reset, and stop only on the configured time budget at the firmware WFI/ISB idle path: PC `0x000e955a`, instruction 84,856,118, virtual time 6,372,873,793 ns. | High for the exact value, readback, refusal atomicity, snapshot round trip, and fault-free bounded idle checkpoint. No generic mask or semantic interpretation of the reference model's pattern-address slot is authorized. Longer runtime, display, storage, and interaction behavior remain separate gates. |
| E-SAP-0024 | 2026-09-03; ticket 721 implementation runs from the exact-hash `sapporo-2.39.20` profile; pinned native full-UI trace; Apollo4 Plus PAC 1.0.0 `usb.rs` and `usb/clkctrl.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`; pristine disassembly and debugger fault inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); native trace SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e`; PAC SHA-256 values: `usb.rs` `9e201740137a7be0e1c8489562e17a51adca5e60783676263c67ac8818f675a9`, `usb/clkctrl.rs` `9657d3fe91761d71c43961ab28ea3c9575fdb6e1cad238940597a2c9448a059f` | The PAC pins USB CLKCTRL at offset `0x2000`, reset zero, with PHYREFCLKSEL in bits 24:25 and value 2 selecting the 24 MHz HFRC clock. The native trace independently records PC `0x000f8c02` reading zero from `0x400b2000`, then PC `0x000f8c08` writing `0x02000000` to the unimplemented reference peripheral. Implementing exactly that zero read and no-output write advances two byte-identical 100,000,000-instruction logs (SHA-256 `06e69fa86a9034a491bc7381a4b51b6dea538e9d326d29fb81bba10945d79374`) to first reset instruction 77,220,237 at 368,259,842 ns with zero compatibility hits; terminal budget PC is `0x000a7ab6` at 396,379,500 ns. Debugger inspection pins the next precise fault to CTIMER observed-pattern offset `0x104`; pristine PC `0x000f7d60` writes `0x00012300`, clearing bit 0 of the preceding `0x00012301` value before later restoration. | High for the exact stateless CLKCTRL boundary: unit coverage verifies zero reads before/after the accepted write, invalid-value refusal, and wrong-width refusal; the private runner enforces exact two-run equality. This authorizes no persistent USB state, snapshot change, transport, endpoint, FIFO, PHY, battery-detection, IRQ, DMA, or host-device behavior. CTIMER value `0x12300` requires a separate evidence-gated ticket. |
| E-SAP-0023 | 2026-09-03; ticket 719 implementation runs from the exact-hash `sapporo-2.39.20` profile; E-A4-TIMER-001 exact-hash 2.22 trace; Apollo4 Plus PAC 1.0.0 `timer.rs` and `timer/outcfg26.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`; pristine disassembly and debugger fault inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); 2.22 trace SHA-256 `82a435c30af1175bd6ef38d6d21663c02aa5a04f2815dc4420377a72ea23d397`; PAC SHA-256 values: `timer.rs` `5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`, `timer/outcfg26.rs` `04d85c84d589d099aab7d6e8e644d9a09658713ad9bf56325df39857bd0d64b1` | The PAC identifies CTIMER offset `0xe8` as OUTCFG26, with OUTCFG104 in bits 0:5 and `0x3f` meaning output disabled. The current firmware writes whole-register value `0x0000003f` at PC `0x000ea56a`; implementing only that additional value and its existing-format snapshot validation advances two byte-identical 30,000,000-instruction logs (SHA-256 `8e079f452fdc7f6485d6688746a1db93f0688fe517b01f1ca295ad6db5e8cb23`) to first reset instruction 24,771,518 at 30,111,413 ns with zero compatibility hits; terminal budget PC is `0x000d1634` at 35,339,895 ns. Debugger inspection pins the next precise fault to a 32-bit read at USB CLKCTRL address `0x400b2000`; pristine PC `0x000f8c02` loads the register before PC `0x000f8c08` writes `0x02000000`. | High for the exact OUTCFG26 value, readback, refusal atomicity, unchanged reset policy, and snapshot round trip. The private runner enforces exact two-run equality. No other OUTCFG value/register, routing, waveform, IRQ, or electrical behavior is authorized. USB CLKCTRL requires a separate evidence-gated ticket. |
| E-SAP-0022 | 2026-09-03; ticket 717 implementation runs from the exact-hash `sapporo-2.39.20` profile; Apollo4 Plus PAC 1.0.0 MCUCTRL sources at commit `75e44b7061b5f707907fe33688db46edeef726bb`; pinned Renode Apollo4 platform; pristine disassembly and debugger fault inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); PAC SHA-256 values: `mcuctrl.rs` `15222f850ab0df000d34834a7b67ce0d175b5038e9403bd6aaf487af81424cf5`, `chipid0.rs` `f0a049c66a6081cfd498484902c8d1ac235a18c4ca3b635b787ec34d724ca5ff`, `chipid1.rs` `e972abcc8d3316f308f8ab024933f2963730ed2b8114ad4763c7a4178130721c`; reference platform SHA-256 `e1f8560bd32f82fb2aaf7e9a8877b1b1c4aff7bd0a35821f8761a76243b74376` | The PAC pins CHIPID0/CHIPID1 at MCUCTRL offsets `0x04/0x08`, full 32-bit fields, and reset zero; the reference platform independently returns zero by silencing exactly `0x40020000..0x40020fff`. Implementing only those two read-only zero values advances two byte-identical 20,000,000-instruction logs (SHA-256 `3d182aea65869a4414579e79ce5f942610570257b606d5b99b71c3f2674a483a`) to first reset instruction 19,948,596 at 25,288,491 ns with PC `0x000d2f6e` and zero compatibility hits; terminal budget PC is `0x001b4b56` at 25,339,895 ns. Debugger inspection pins the next precise fault to a write at `0x400080e8`; pristine PC `0x000ea56a` writes CTIMER auxiliary value `0x3f`. | High for the explicit deterministic zero identity and post-717 boundary: focused tests cover both reads, write refusal, wrong-width refusal, and unchanged neighboring-offset policy; the private runner enforces exact two-run equality. This is not a broad MCUCTRL zero page and authorizes no CHIPPN, SKU, fuse, host, or random identity behavior. CTIMER value `0x3f` remains a separate gap. |
| E-SAP-0021 | 2026-09-03; ticket 716 implementation runs from the exact-hash `sapporo-2.39.20` profile; Apollo4 Plus PAC 1.0.0 `wdt/rstrt.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`; upstream Renode `AmbiqApollo4_Watchdog.cs` at commit `1f6dee7643174cd61b6b4bde884c75ff8b69b6b8`; pristine disassembly and debugger fault inspection | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); PAC RSTRT SHA-256 `39763293227a12d6d3d8d849fb9a158f496cf221638b2fbb77b5e29b066fb6db`; Renode source SHA-256 `774b72e3230bf59e9dd8ef2a576d345190a66ec583b56c7df469db3dc9035f76` | Both register sources pin watchdog RSTRT offset `0x04` and reload key `0xb2`; the PAC pins read/reset value zero. Implementing only zero readback and the exact key advances two byte-identical 20,000,000-instruction runs (SHA-256 `0a092da13d76a589b189bc43a22461bd5e280d68e791927d81dd2193f96958f6`) to a first reset at instruction 19,945,598 and 25,285,493 ns with PC `0x000d2f6e` and zero compatibility hits; terminal budget PC is `0x001b4b52` at 25,339,895 ns. Debugger inspection pins the next precise read fault to `0x40020004`; pristine PC `0x0008a0ba` loads MCUCTRL CHIPID0 and the next instruction loads CHIPID1 at `0x40020008`. | High for the stateless restart-key behavior and exact post-716 boundary: focused tests cover zero readback, exact-key acceptance, invalid key/width/offset refusal, and non-mutation; the private runner enforces exact two-run equality. No counter, timer, expiry, reset action, or other watchdog behavior is authorized. MCUCTRL chip identity requires a separate deterministic policy ticket. |
| E-SAP-0020 | 2026-09-03; ticket 714 implementation runs from the exact-hash `sapporo-2.39.20` profile; Apollo4 Plus PAC 1.0.0 `wdt.rs` and `wdt/wdtieren.rs` at commit `75e44b7061b5f707907fe33688db46edeef726bb`; pristine application disassembly; debugger inspection of the fail-closed bus-fault request | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); PAC source SHA-256 values `aa2fcbd39026abb3ada86172b263eef0973203a57fa41c26202cdefa7265553d` and `5413778245f4ca19f56f5a3ad5f42a39146337310ad6436c1bbcc26dd71aed31` | The PAC pins WDTIEREN at watchdog offset `0x200`, reset zero, WDTINT bit 0, and DSPRESETINT bit 1. Implementing only that register advances two byte-identical 20,000,000-instruction logs (SHA-256 `493e50f23db1149402e2aadb20cbe6108c7e37fec27be6700e23c8821d5c35fa`) to a first reset at 11,897,284 ns with PC `0x000d2f6e` and zero compatibility hits; terminal budget PC is `0x000d163e`. Debugger inspection pins the new precise fault address to `0x40024004`; pristine code at PC `0x000e93aa` writes restart key `0xb2` to that address after enabling the watchdog interrupt. | High for WDTIEREN and the exact post-714 boundary: focused tests cover reset, firmware value, both named bits, reserved/width/offset refusal atomicity, and snapshot validation; the private runner enforces exact two-run equality. This authorizes no interrupt assertion or other watchdog register. The restart-key write requires a separate evidence-gated ticket. |
| E-SAP-0019 | 2026-09-03; ticket 713 implementation runs from the exact-hash `sapporo-2.39.20` profile, plus pristine disassembly at `0x000e9348..0x000e9458` and the read-only reference trace pinned by E-SAP-0018 | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011) | A dedicated RSTGEN block implementing only 32-bit CFG at `0x40000000` advances two byte-identical 20,000,000-instruction runs to the next precise initialization boundary. Both logs have SHA-256 `540b62b500147fffa74f44f5ba4d0f1f02c9fa1c7713512a8834c78700fee7b5`; the first reset is at 11,897,266 ns with PC `0x000d2f6e` and zero compatibility hits, and the terminal budget PC is `0x000d163e`. Pristine control flow next loads watchdog INTEN address `0x40024200` at PC `0x000e9454`, ORs bit 0, and writes `0x1` at PC `0x000e9458`; the committed reference trace independently records that access and value. | High for the exact RSTGEN CFG contract and post-713 checkpoint: `test_apollo4_rstgen` covers reset, firmware value, refusal atomicity, and snapshot validation, while the private runner requires exact two-run equality. This authorizes only RSTGEN CFG. Watchdog INTEN remains a separate fail-closed gap requiring its own ticket; no permissive page, compatibility hook, firmware bytes, or additional RSTGEN behavior is introduced. |
| E-SAP-0001 | project target contract | Sapporo `2.22.60.3383-P` | Initial display is 240x240 RGB565 and board has three physical buttons. | Target assumption; validate in private integration tests. |
| E-SAP-0002 | known native frame | Sapporo `2.22.60.3383-P` | Normal frame is 115200 bytes, SHA-256 `8503ffbde124e35f914b09eea858ffcda2fc4bc453d3f9c7eb3e88388621d9cc`. | Golden; reproduce twice. |
| E-SAP-0003 | known native frame | Sapporo `2.22.60.3383-P` | Middle-button language frame SHA-256 is `68a4126a8f908e9dd7c5703982c6fe141e6cc89cc383ee0d9d6d502eeadcb2d9`. | Golden; reproduce through semantic input. |
| E-SAP-0004 | known native frame | Sapporo `2.22.60.3383-P` | Lower-button transition SHA-256 is `dcec235c8b450c96356b27b49306026ab9d14e7626714cdacb8bf3737a623ad3`. | Golden; reproduce through semantic input. |
| E-SAP-LIVE-0001 | native live UI navigation research trace, SHA-256 `10d64a29e0d982563c36e959d56498254cc27566a5a83d44bdcbe6792b3dd804` | Sapporo `2.22.60.3383-P` | The native active-low GPIO bridge holds each button press for at least 70 ms of guest time and keeps the released level stable for 70 ms before a subsequent press; the native live viewer uses a bounded quiet settle window when a stable-frame-only capture is requested. | Read-only native trace summary; applies to the SDL host bridge and checkpoint presentation only, without changing guest behavior. |
| E-CPU-0001 | prior Renode workaround report | ARMv7E-M firmware paths | `MOV.W r0,sp` and `STMDB` are valid instructions and must be CPU regressions, not compatibility hooks. | Validate with architectural instruction vectors. |
| E-COMPAT-0001 | recovered native record builder and research notes | Sapporo `2.22.60.3383-P` | Missing manufacturing state is represented by an opt-in, session-local `sapporo-2.22-no-device` layer with ProductionData, ACCR, ACCC, MAGN, and HLAT records. | Fixture-backed; generated from the firmware's CRC table and covered by provenance and hit-budget checks. |
| E-SAP-0005 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Resident component loads at `0x00019000`, size 71504, SHA-256 `a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522`. | Exact profile metadata; checked before mapping. |
| E-SAP-0006 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Application loads at `0x00040000`, size 1493234, SHA-256 `c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`. | Exact profile metadata; checked before mapping. |
| E-SAP-0007 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Resource component maps at `0x14000000`, size 16519168, SHA-256 `ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1`. | Exact profile metadata; checked before mapping. |
| E-SAP-0008 | bounded local authentic-firmware run | Sapporo `2.22.60.3383-P` | With `sapporo-2.22-no-device`, the headless startup test passes deterministically (wfi-deadlock at 1M instructions, 500M virtual ns). Beyond the startup gate, a bounded run now advances to the max-time budget at 12,700,000 instructions (PC=0x000be942) after BusFault exception handling was implemented. Previously stopped at STR `0x6008` (PC=0x00098ea2, 9,448,881 instructions) because unmapped/misaligned bus accesses halted the CPU; now a BusFault exception is taken (escalating to HardFault when SHCSR.BUSFAULTENA is clear, per ARM DDI 0403E.e B1.5.5/B1.5.7). Single LDR/STR no longer pre-refuse misaligned addresses — the bus handles alignment faults via BusFault (A3.2, A3-65). POP/LDM with a non-Thumb PC value no longer refuses — the PC is loaded with EPSR.T=0 (A7.7.99 POP, A7.7.10 BX), and the next instruction fetch takes an InvState UsageFault (B1.5.5) that escalates to HardFault via `armv7m_request_fault` when SHCSR.USGFAULTENA is clear. STRD/LDRD and T3 conditional branch fixes remain in effect. | Reproducible bounded run. The firmware runs to the virtual-time budget without hitting a CPU instruction gap. |
| E-SAP-0009 | validated SOF extraction manifest | Sapporo `2.33.16.17428-P` | SOF package SHA-256 `4384322af5ffedf05de1144d069b05369f14e8803b2dfa2814a83272eefd0327`, size 5262962, revision `acf9fa01`. Resident: load `0x00019000`, size 73377, SHA-256 `c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5`. Application: load `0x00040000`, size 1561397, SHA-256 `ba286a7bce55dd2f7aba81436d3c5152401c28d86bef89b2c9dbbacd04b7e9e5`. Resources: load `0x14000000`, size 16519168, SHA-256 `b06492a982faa188a14d0a458e7165f9e0b13d0917ded3c3e37077f6ef68d5c5`. Direct vector words: initial SP `0x1005ffc0`, resident reset `0x0001dfd5`, application reset `0x001bc5dd`, all Thumb and in-component. | Exact profile metadata; component hashes and vector words independently verified on 2026-09-03 with `shasum -a 256`, direct little-endian reads, and `$FIRMWARE_ROOT/tools/inspect_sapporo_boot.py` (SHA-256 `1bbf78c5006286ecbccdd7e180481a7081344003d4bc7b671ccd86261594a4f8`). This corrects stale vector values previously recorded in the contract. Contract file: `fixtures/evidence/sapporo/sapporo-2.33.contract.semu`. |
| E-SAP-0010 | validated SOF extraction manifest | Sapporo `2.35.34.18929-P` | SOF package SHA-256 `79ca59c012795e3694ca317e38ce42568b8acf425ea3a131bab09fdc0ed32a07`, size 5390294, revision `b0e8e377`. Resident: load `0x00019000`, size 73377, SHA-256 `c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5` (identical to 2.33). Application: load `0x00040000`, size 1585150, SHA-256 `36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a`. Resources: load `0x14000000`, size 16519168, SHA-256 `f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22`. Direct vector words: initial SP `0x1005ffc0`, resident reset `0x0001dfd5` (same as 2.33), application reset `0x001c2225`. | Exact profile metadata; component hashes and vector words independently verified on 2026-09-03 with direct little-endian reads and the hash-pinned firmware analyzer from E-SAP-0009. This corrects stale vector values previously recorded in the contract. Contract file: `fixtures/evidence/sapporo/sapporo-2.35.contract.semu`. |
| E-SAP-0011 | validated SOF extraction manifest | Sapporo `2.39.20.22297-P` | SOF package SHA-256 `7f275bef9eb029693a8f4517fe0c9c9a3a1aeba7224f79f06f5e860876541355`, size 5341502, revision `f06153d9`. Resident: load `0x00019000`, size 73377, SHA-256 `c81aa19dd99d75519566a4210f2ee0d74f1cd4ad721bb4d7efe5eab62bbf69c5` (identical to 2.33/2.35). Application: load `0x00040000`, size 1596337, SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`. Resources: load `0x14000000`, size 16519168, SHA-256 `49a3936f4c9d61dbceee12324f41412334c42aa98b900f6f2d3fc5633ae43aea`. Direct vector words: initial SP `0x1005ffc0`, resident reset `0x0001dfd5` (same as 2.33/2.35), application reset `0x001c4fb7`. | Exact profile metadata; component hashes and vector words independently verified on 2026-09-03 with direct little-endian reads and the hash-pinned firmware analyzer from E-SAP-0009. This corrects stale vector values previously recorded in the contract and agrees with the independent native port note `$FIRMWARE_ROOT/docs/research/sapporo-2.39-emulator-port.md`. Contract file: `fixtures/evidence/sapporo/sapporo-2.39.contract.semu`. |
| E-SAP-0012 | cross-version SOF extraction comparison | Sapporo `2.22–2.39` | Header component (type 1, enc v1, 5854 bytes) SHA-256 `5d2d0bb9f59481328453e2a64ebb8fbfb3cde2262bb9590f53e7adc7c14e5b9b` is identical across all six builds. Resident and XIP components are identical from 2.33.12 through 2.39.20 but differ from 2.22. Application size increases monotonically: 1493234 (2.22) → 1561609 (2.33.12) → 1561551 (2.33.14) → 1561397 (2.33.16) → 1585150 (2.35.34) → 1596337 (2.39.20). All versions share direct initial SP `0x1005ffc0` and Thumb reset; application reset vectors are `0x001a2431`, `0x001bc6bd`, `0x001bc67d`, `0x001bc5dd`, `0x001c2225`, and `0x001c4fb7`, respectively. | Cross-version contract corrected and independently revalidated on 2026-09-03 using direct vector reads from all six hash-pinned application components and the analyzer pinned by E-SAP-0009; no firmware bytes committed. Contract file: `fixtures/evidence/sapporo/sapporo-cross-version.contract.semu`. |
| E-SAP-0013 | bounded reset/MMIO trace attempt | Sapporo `2.33/2.35/2.39` | Historical pre-705 probe: bounded traces for later Sapporo versions were initially blocked because no version-specific profiles existed. E-SAP-0014 and E-SAP-0015 now cover the 2.33.16 profile/reset boundary; 2.35 and 2.39 still lack profiles and bounded traces. | Historical blocked boundary; current 2.33 profile and reset conclusions are superseded by E-SAP-0014/0015. Cache/logical-storage interventions for 2.35/2.39 remain unknown until their profiles and bounded authentic runs exist. |
| E-SAP-0014 | bounded Sapporo 2.33.16 profile run | Sapporo `2.33.16.17428-P` | With the `sapporo-2.33.16` profile (no compatibility layer), the emulator now runs to the max-time budget at 12,700,000 instructions (PC=0x000c97f8). Previously stopped at unsupported IT instruction `0xbfe2` (ITEAL, cond=AL, mask=2) at PC=0x0006f408 after 10,695,606 instructions; the emulator rejected condition 14 (AL) in IT instructions, but per ARM DDI 0403E.e A7.7.78, IT with cond=AL and non-zero mask is a valid (deprecated) encoding. Before that, stopped at unmapped read PC=0x00c6f5e4 (10,620,789 instructions), now handled via BusFault exception. CP11 double-precision load/store (VLDR/VSTR D, VLDM/VSTM D) and T3 B<cc>.W conditional branch remain handled. | Reproducible bounded run. The firmware runs to the virtual-time budget without hitting a CPU instruction gap. |
| E-SAP-0015 | bounded reset diagnostic run | Sapporo `2.33.16.17428-P` | Two fresh runs with the exact profile and three validated component hashes produced the identical bounded trace SHA-256 `f2ba977943301be8ffd3332a4cd188df1549160da71c24e059bd30e272de7d12`. The guest issued `SYSRESETREQ` at PC `0x000c97f2`, LR `0xffffffe9`, SP `0x1005ff08`, R0–R3 `0x05fa0004/0xe000ed0c/0x05fa0004/0x49000000`, xPSR `0x29000003`, reset count `1`, compatibility hit total `0`, and virtual time `11648405`; reset handling then continued to the 12,700,000-instruction budget at PC `0x001acff4`. | Reproducible emulator observation only. No native reset-register or post-reset transaction trace is available, so this entry authorizes diagnostics and comparison, not a controller or compatibility change. |
| E-SAP-0016 | bounded 2.22 reset/OHR diagnostic comparison | Sapporo `2.22.60.3383-P` | A fresh exact no-layer OTA run with all three validated component hashes repeatedly issued `SYSRESETREQ` at PC `0x000be93e` with `compat_hits=0`; the first reset occurred at virtual time `23119424` with LR `0xfffffffd`, SP `0x1005ff58`, and xPSR `0x21000003`. In the opt-in layer run, the bounded OHR trace accepts BSL identity/configure, transitions to MAIN, accepts result/echo exchanges, and records each ready assertion/clear without refusal. | Reproducible emulator observation only. The no-layer run stops at the existing reset loop, while the opt-in OHR fixture follows its declared startup contract; no native reset-register or post-reset transaction trace is available to justify a behavior change. |
| E-SAP-0017 | 2026-08-20; read-only later-Sapporo research audit: `$FIRMWARE_ROOT/docs/research/sapporo-2.33-mspi-power-change.md` (SHA-256 `c0c690d03f3f003d2d7290dbfcff9621fdf8e17b17d26528c39abd6e089e39a2`), `$FIRMWARE_ROOT/docs/research/sapporo-2.39-logical-storage-adapter.md` (SHA-256 `1da2362ae14f78285c463749ff418d51ce3d14b429d40fa2a99e475d14e1e0c3`), and `$FIRMWARE_ROOT/docs/research/sapporo-2.39-ui-selection-boundary.md` (SHA-256 `570afd32a90ffb43b933279742c036d6515af0a09f4dcf93bfc1bd1b3b41df84`) | Sapporo `2.33.16.17428-P` and `2.39.20.22297-P` (component hashes E-SAP-0009 and E-SAP-0011) | The 2.33 MSPI power-sequencing change is an exact static hotfix candidate, but its failing runtime state and resource `0x63` attribution are not recovered; the current E-SAP-0014 profile run reaches its bounded max-time checkpoint, so no device/storage stop is available for ticket 710. The 2.39 native probes reach the file resolver with null volume/path state and require a native `storage/` open plus later watch-face notification before any logical-storage binding; the UI boundary explicitly refuses fabricated open events. No later-Sapporo device/storage gap is implementation-eligible. | High for the cited read-only boundaries and source hashes; blocked for a 710 implementation until an exact failing transaction, native provenance, and an allowed module boundary are available. |
| E-SAP-0018 | 2026-09-03; exact-hash C-emulator diagnostic profile for Sapporo 2.39.20, two byte-identical 20,000,000-instruction baseline logs (SHA-256 `5ba6187563478deda274fad86942f937b615d5b0a7c03884c957ace80233e7ba`), stacked HardFault/CFSR/BFAR inspection, disassembly of the pristine application, and read-only comparison with Renode's upstream `AmbiqApollo4_PowerController.cs` and `AmbiqApollo4_Watchdog.cs` at submodule commit `1f6dee7643174cd61b6b4bde884c75ff8b69b6b8` (file SHA-256 values `18b1d1b872a4241700c24c1ab69301c0ce5c25c4d9ef0edff21fb6e73d3b3219` and `774b72e3230bf59e9dd8ef2a576d345190a66ec583b56c7df469db3dc9035f76`), Apollo4 Plus PAC 1.0.0 at commit `75e44b7061b5f707907fe33688db46edeef726bb` (crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`, `rstgen.rs` `2a3b575150c80b35ebac042a17b3d2017feb9bff8e90eee40c4f61366b648b66`, `rstgen/cfg.rs` `d489ece425d2daa44623162228399a9607084e8f6432d8d31e5982a8e7f07305`), and the committed 2.39 full-UI reference trace/provenance (SHA-256 `fd5b6208937d1cfe4479405ea5af5635659ecf3163c8f033da6013d3b858ef6e` / `62ef36a7498881cd86bd481111e7f47c9a0bbabc2bf656b74400136e1a5e0bb3`) | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011) | The apparent reset loop is the firmware HardFault handler requesting `SYSRESETREQ`, not the later footer validator. The first fault is precise and address-valid (`CFSR=0x00008200`, `HFSR=0x40000000`) at `BFAR=0x40021058`, with stacked PC `0x000a35c2`, LR `0x000a8d4f`, and first reset at 11,897,027 ns. `0x40021058` is `PWRCTRL.DSP0MEMPWREN`; the upstream Apollo4 register model defines enable/status/retention triplets at DSP0 offsets `0x58/0x5c/0x60` and DSP1 offsets `0x78/0x7c/0x80`. Diagnostic-only register experiments advanced the fault in that exact order (`0x58` -> `0x5c` -> `0x60` -> `0x78`); supplying both triplets advanced to a distinct precise fault at `BFAR=0x40024000`, stacked PC `0x000e9348`, first reset at 11,897,251 ns. The pristine code at `0x000e930c` builds watchdog CFG value `0x033c3d06` and writes its base at `0x40024000`; the Apollo4 Family Programmer's Guide independently identifies `0x40024000..0x400243ff` as the watchdog block. The upstream model pins CFG reset `0x00ffff00`, reserved bits 4..7 and 27..31, and valid clock selectors 0..4. A further reverted probe mapped only a zeroed watchdog window: execution advanced three instructions to a precise fault at `BFAR=0x40000000`, stacked PC `0x000e9350`, LR `0x000d1ee9`, first reset at 11,897,254 ns. The disassembly shows this is the Reset/BoD routing read/modify/write immediately after the watchdog CFG write. Apollo4 Plus PAC 1.0.0 identifies offset zero as RSTGEN CFG, reset zero, with only BODHREN bit 0 and WDREN bit 1; the committed 2.39 full-UI reference trace independently records the exact read returning zero at PC `0x000e934e` and write of `0x2` at PC `0x000e9358`. Ticket 706 installs the exact built-in profile and preserves the strict component contract. Ticket 711 implements only the two DSP register triplets: enable/status mask `0x3`, retention mask `0x1f`, zero reset, status mirroring enable, reserved/status/width refusal, and snapshot round-trip validation. Two canonical post-711 runs are byte-identical (SHA-256 `2b0592bdf0bef54da8d48d26f1137af5835395f0cf7dae1863422b1ce8788be7`), first reset PC `0x000d2f6e` at 11,897,251 ns with zero compatibility hits, terminal budget PC `0x000d1634`. Ticket 712 implements only watchdog CFG with the upstream reset/field validation and strict offset/width refusal. Two canonical post-712 logs are byte-identical (SHA-256 `c332eee489a7209230192b03e57183fbb4f0db45d3175332daeb0ad4b705ebe5`), first reset PC `0x000d2f6e` at 11,897,254 ns with zero compatibility hits, terminal budget PC `0x000d1648`. | Reproducible emulator/firmware-RE evidence that supersedes E-SAP-0017 for 2.39 implementation eligibility. The DSP memory-power and watchdog CFG gaps are implemented without compatibility hooks; Reset/BoD routing at `0x40000000` is the next separate gap. No diagnostic instrumentation, firmware bytes, or permissive fallback remains in the tree. |
| E-NEMA-TSC6A-001 | 2026-08-20; read-only `$FIRMWARE_ROOT/docs/research/native-tsc6a-transition-surface.md` (SHA-256 `d9ae4eaec1cb5c53c5322f28911dc6f4d04a7df8c8e610c5d80eb6e0aa9301dd`) and linked native live-navigation trace (SHA-256 `10d64a29e0d982563c36e959d56498254cc27566a5a83d44bdcbe6792b3dd804`) | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | The native path targets a 480x480 TSC6A surface at `0x10100d40` with format `0x17`, fstride `0x170005a0`, and size `0x2a300` ending at the proven 240x240 RGB565 surface. It first draws clipped edge-antialiased triangles and A2LE masks, then resolves the TSC6A source through the observed affine/program state into RGB565. The evidence-gated emulator keeps an uncompressed ARGB shadow, commits it transactionally, and refuses unknown formats, programs, geometry, and bounds; compressed guest bytes are never decoded or stored. | Positive/refusal coverage: `tests/unit/test_nema_tsc6a.c`; authentic SDL setup transition run has no TSC6A renderer refusal. This entry authorizes the semantic shadow/resolve boundary only; it does not establish compressed-codec or physical-panel equivalence, nor a reset workaround. |
| E-SAP-ONBOARD-001 | 2026-08-20; read-only native live UI capture and `$FIRMWARE_ROOT/emulator/display/captures/sapporo-native-onboarding-profile.json`; contract `fixtures/evidence/sapporo/sapporo-2.22-onboarding-profile.semu` | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | Selecting the observed English row from the native language menu with a middle-button event reaches a 240x240 RGB565 frame whose visible caption is `Define your profile`. The raw frame is 115200 bytes, SHA-256 `0930d2cc5fe092f0d7b5c74b1d5047860d0b2719a99a339af19162ed47e20ec7`; metadata SHA-256 is `fc99fad9a54d1e940ed5c9f38571595ddc4f9222b133c1695c8b9ad8160c420a`; PNG SHA-256 is `4577b963fb28416e7b94af1447bd462cebd0421bf0c9b2e75bd04a001d5ba8e5`. The predecessor language-menu frame is E-SAP-0003 (`68a4126a...`). | High for the captured native software-rendered transition and trigger. The source explicitly marks `physical_nemap_output=false`, `pixels_substituted=false`, and `complete_watch_ui=false`; this entry authorizes provenance/comparison only, not a screen-specific emulator golden or physical-panel claim. |
| E-SAP-ONBOARD-EMU-001 | 2026-08-20; deterministic headless frame-callback comparison using the external `$TMPDIR/suunto-middle.sems` snapshot and `$TMPDIR/suunto-setup-replay.txt` replay; contract `fixtures/evidence/sapporo/sapporo-2.22-onboarding-profile.semu` | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007; emulator binary SHA-256 `c80b8191b89c697224f009d3318d2d336d11f12a3c1fcdab4071d93bb9338935`) | Two identical `--until setup-next` probes stopped at the first visible post-input frame (generation 2, raw SHA-256 `0096f059eea6df7a2e72ba1dd1c30fcd9a6be9e253576276f30dc2010f0c145b`, CRC32 `bbf3549e`) at `pc=0x000d4b4a`, 549,900,000 instructions, and virtual time 8,802,698,080 ns. A longer diagnostic continuation reached a last distinct frame (generation 59, raw SHA-256 `688ca6657a4f4353de85cf42c22b7a350c0144776a1a22405d4eb1b95bf4219a`, CRC32 `d4ed66c7`) before its budget. Neither emulator frame equals native E-SAP-ONBOARD-001 raw SHA-256 `0930d2cc...`. | Reproducible emulator observation only: snapshot/replay hashes are pinned but no provenance sidecar exists, and the callback probe is not a normal-build checkpoint. Pixel equivalence is false; this entry authorizes keeping `setup-next` neutral, not adding a screen-specific checkpoint or golden. |
| E-SAP-ONBOARD-EMU-002 | 2026-08-20; current SDL dummy `SEMU_SDL_LIVE_TEST=setup-walk` run from `/tmp/suunto-ui-preframe.sems` with the exact external manifest and `sapporo-2.22-no-device` layer | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | The semantic TSC6A path accepted the observed onboarding target/resolve forms and settled 19 bounded middle-button transitions with no renderer refusal; the last settled frame was generation 2009, CRC32 `629da47e`, at `pc=0x0800009e`, 5,830,245,056 instructions, and virtual time 39,982,601,551 ns. The guest also requested `SYSRESETREQ` at `0x000be93e` during the run (including after reset), before the native 28-transition setup sequence and phone-pairing boundary could be reproduced. | Reproducible emulator diagnostic; this records the reset boundary and renderer success, not full setup completion. E-SAP-0016 remains authoritative for the no-reset-hook refusal, and no screen-specific golden or reset compatibility behavior is authorized. |
| E-SAP-ONBOARD-EMU-003 | 2026-08-20; exact SDL dummy setup-walk plus read-only debugger fault capture at the first unsupported CTIMER write | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | The guest executes Thumb `STR` `0x6008` at the setup path's `0x000bb112` and writes `r0=0x00010301` to `0x40008104` (Apollo4 CTIMER0 offset `0x104`). The prior timer model refused this exact, observed pattern value and raised a precise BusFault (`CFSR=0x8200`); the value is an opaque retained pattern and does not imply additional timer semantics. | Reproducible emulator observation; authorizes accepting only pattern value `0x10301` with readback and snapshot validation. The refusal case remains an unobserved pattern, and no general read-as-zero or reset compatibility behavior is authorized. |
| E-SAP-ONBOARD-EMU-004 | 2026-08-20; exact SDL dummy setup-walk plus read-only debugger fault capture after E-SAP-ONBOARD-EMU-003 | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | After the evidenced `0x00010301` pattern is accepted, the guest reaches `PC=0x000e4a22` after Thumb `STR` `0x6003` at `0x000e4a20`, writing the opaque value `0x00010300` to `0x40008104`; refusal raises the same precise BusFault (`CFSR=0x8200`). | Reproducible emulator observation; authorizes accepting only pattern value `0x10300` with readback and snapshot validation. The refusal case remains an unobserved pattern, and no general CTIMER fallback or reset compatibility behavior is authorized. |
| E-SAP-ONBOARD-EMU-005 | 2026-08-20; exact SDL dummy setup-walk plus read-only debugger fault capture after E-SAP-ONBOARD-EMU-004 | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | After the `0x00010300` pattern is accepted, the guest reaches `PC=0x000bb114` after Thumb `STR` `0x6008` at `0x000bb112`, writing the opaque value `0x00012301` to `0x40008104`; refusal raises the same precise BusFault (`CFSR=0x8200`). | Reproducible emulator observation; authorizes accepting only pattern value `0x12301` with readback and snapshot validation. The refusal case remains an unobserved pattern, and no general CTIMER fallback or reset compatibility behavior is authorized. |
| E-SAP-ONBOARD-EMU-006 | 2026-08-20; exact SDL dummy `SEMU_SDL_LIVE_TEST=setup-walk` from `/tmp/suunto-ui-preframe.sems`, plus read-only `$FIRMWARE_ROOT/docs/research/native-live-ui-navigation.md` (SHA-256 `10d64a29e0d982563c36e959d56498254cc27566a5a83d44bdcbe6792b3dd804`) and its historical spatial-label correction (firmware-repo commit `c409c2d`, later reverted) | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | Mapping semantic upper/middle/lower to GPIO59/58/57 and emitting the bounded Return-plus-middle sequence settles 12 frames and reaches the native handoff caption `Continue the setup on your phone`: generation `1295`, CRC32 `ea3bc5f8`, `pc=0x0009a3a8`, 3,844,224,576 instructions, virtual time 19,024,454,947 ns. The prior GPIO57/58/59 semantic mapping stalled before this boundary. | Medium-confidence emulator/UI checkpoint, corroborated by the native note's exact Continue-screen boundary and the historical spatial-label experiment; this is not evidence of phone pairing, watch-face assets, or physical-panel equivalence. The final frame is recorded by hash only; no pixels enter Git. |
 | E-SAP-ONBOARD-EMU-007 | 2026-08-21; SDL dummy runs from a snapshot saved at the E-SAP-ONBOARD-EMU-006 stop state (handoff frame, generation `1295`, CRC32 `ea3bc5f8`), with input replays covering upper/middle/lower presses at 300 ms guest-time holds, a 3-second lower hold, and mixed press sequences, plus a 51-second idle continuation; temporary (uncommitted) instrumentation of the machine input path, the NEMA backend submit funnel, and Apollo4 `INPUT_READ1` polls | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | On the `Continue the setup on your phone` handoff screen every replayed press is delivered to the machine (GPIO59/58/57 level toggles confirmed) and the guest's own `INPUT_READ1` polling observes the pin-57 press transition; nevertheless the guest submits zero NEMA command lists after the handoff: no backend submission, no new frame, no renderer refusal across all press patterns and 51 seconds of idle guest time. The trace shows no CPU exception, assertion, or device refusal, only the expected `gps-awake-pulse` intervention hits, whose eleven-hit budget (E-SAP-COMPAT-GPS-005) bounds any continuation at roughly 48.6 s after the handoff. The 2.22.60 resource string table contains the post-handoff onboarding strings `Skip` (`750d4b9f`), `Time/date` (`6a5c2eec`), `Time zone` (`6072e392`), and `All done!` (`2bb84502`), but the recovered resource files contain no view definitions for those screens. | Medium-confidence emulator observation only: the handoff screen's button path is inert in the standalone 2.22.60 environment even though the input reaches the guest's button polling. This entry authorizes keeping the setup-walk pinned at the handoff and does not authorize a skip transition or any post-handoff checkpoint or golden; a native 2.22.60 capture of the handoff screen's button handling (NEMA command lists plus UI dispatch on the lower/middle press, or a researcher-owned full-flash dump containing the onboarding view resources) is required before implementing anything past this boundary. |
 | E-SAP-ONBOARD-EMU-008 | 2026-08-25; two independent, byte-identical SDL `SEMU_SDL_LIVE_TEST=setup-walk` runs from the identity-pinned `/tmp/suunto-ui-preframe.sems` pre-frame snapshot, with the exact external manifest, the `sapporo-2.22-no-device` layer, and `SEMU_SDL_SETUP_WALK_POST=llll…` (twelve semantic lower presses after the phone-pair handoff); the 2.22.60 onboarding view resources recovered from the read-only `$FIRMWARE_ROOT/artifacts/analysis/sapporo-2.22.60/component-05-type-1-v3.raw` (type-1 `xz` resource) | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | The board input boundary had the semantic upper/lower pins **inverted** relative to E-SAP-BUTTONS-001 and the verified `sapporo_wiring.c` table: it emitted upper/middle/lower to GPIO59/58/57 instead of the correct GPIO57/58/59 (upper=top=GPIO57, middle=GPIO58, lower=bottom=GPIO59). With that inversion, the setup-walk's bottom-button press at the handoff drove GPIO57 (the physical "back/previous" pin) and the onboarding stalled at `Continue the setup on your phone`, which is the "inert button" symptom recorded in E-SAP-ONBOARD-EMU-007. Correcting `machine.c` to upper=GPIO57/middle=GPIO58/lower=GPIO59 makes the semantic lower press drive GPIO59 (the physical bottom/Skip pin), and the onboarding advances. From the handoff frame (generation `1295`, CRC32 `ea3bc5f8`, E-SAP-ONBOARD-EMU-006), a bounded sequence of semantic lower (GPIO59) presses settles three further distinct post-handoff frames: generation `1395` CRC32 `9b58f243`, generation `1492` CRC32 `eb868d29`, and generation `1592` CRC32 `ed7eeb7a`. Both independent runs stop byte-identically at `pc=0x0010fbde`, 5,280,223,507 instructions, virtual time 73,145,995,522 ns, `stop=compat-refused` (the eleven-hit `gps-awake-pulse` budget, E-SAP-COMPAT-GPS-005, exceeded). The recovered 2.22.60 resource DOES contain the post-handoff onboarding view definitions `w-conn-1`, `w-tida`, `w-ltim`, `w-year`, `w-mont`, `w-day`, `w-time`, and `w-done` (with the strings `Skip` `750d4b9f`, `Time/date` `6a5c2eec`, `Time zone` `6072e392`, `All done!` `2bb84502`), superseding E-SAP-ONBOARD-EMU-007's "no view definitions for those screens" conclusion. The final settled frame `ed7eeb7a` matches the `w-ltim` Time-zone / "Search for GPS" layout (top-heavy content, no bottom button); that view's `onActivate` arms GPS and a 5 s `setTimeout` that subscribes to `Dev/Time/LocalTime` and advances only when that value updates (post-2022 epoch → the UTC-offset menu; otherwise the "Set manually" fallback routing to manual `w-year` clock entry). Time-ordered frame capture shows `w-ltim` settles at ~20.76 s and then renders no new frame for ~52 s: the onboarding is genuinely stuck on `w-ltim`, not merely pacing-limited. A control run with the `gps-awake-pulse` budget raised to 500 still halts on `w-ltim` (run ends at `--max-time` 226 s with no new settled frame), proving the primary blocker is the absent clock value, not the GPS budget. | Medium-confidence emulator/UI checkpoints recorded by CRC only; no pixels enter Git. This entry supersedes E-SAP-ONBOARD-EMU-007's "inert button" and "no view definitions" conclusions as they were observed under the inverted mapping. It authorizes the corrected upper=GPIO57/lower=GPIO59 mapping and the post-handoff diagnostic continuation; it does NOT authorize raising the `gps-awake-pulse` budget or a post-handoff screen-specific golden. Completing past `w-ltim` to `w-done` ("All done!") requires a new TimeProvider-style publish of `/Dev/Time/LocalTime` (local resource ID `0x2705`, packed `0x2705001f`, per the 2.39 delivery trace `$FIRMWARE_ROOT/docs/research/sapporo-2.39-wfa-atlas-lifecycle.md`), because the `w-ltim` subscription fires only when that value is published: a post-2022 epoch routes to the UTC-offset menu (`utc-found`, `iset=true`) and one semantic lower press saves it to `w-done` → `open('main')`; a pre-2022 value routes to `utc-notfound` whose "Set manually" button opens the manual `w-year` clock entry. The no-device layer currently has no such publish path, and the NEMA backend implements only GPU draw commands, so this is a roadmap/evidence-gated compatibility change needing a 2.22.60 onboarding time-sync native trace (plus headroom in the `gps-awake-pulse` budget for the added virtual time), not a walk-side change. |
 | E-SAP-ONBOARD-EMU-009 | 2026-08-26; ten fresh-boot SDL `SEMU_SDL_LIVE_TEST=setup-walk` runs from the exact external manifest with the `sapporo-2.22-no-device` layer and `SEMU_SDL_PPM_DIR` frame capture (walks: base, m2, mid, mid2, mid3, mid4, rtc, rtc2, nortc, up, reg, rep, repU, repL; each `--until setup-next`, `--max-time 300000000000`; `reg` repeats the default repeat-off baseline, while `rep`, `repU`, and `repL` additionally enable the new opt-in `SEMU_SDL_SETUP_WALK_REPEAT` bounded same-frame re-press helper), plus two temporary, fully reverted in-tree experiments (a `SEMU_GPS_TIME_EXPERIMENT` one-shot NMEA time+fix group injected on the version-pinned `@GSR` running-status exchange in `src/compat/sapporo_222.c`, and a `SEMU_RTC_TIME_EXPERIMENT` gated value in `apollo4_rtc_read` for `0x40004820`/`0x40004800` in `src/soc/apollo4/auxiliary.c`), plus read-only RE of the 2.22.60 onboarding views in `component-05-type-1-v3.raw` (`w-ltim` @ `0x892300`, `w-tida` @ `0x8bb61d`, `w-year` @ `0x81be1d`, `w-mont` @ `0x69021d`, `w-day` @ `0x89701d`, `w-time` @ `0x127f3d`, `w-tset` @ `0x7b921d`, `w-done` @ `0x288e1d`) and the application binary timesync/RTC strings | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | (a) Corrected fresh-boot screen map with the fixed pin mapping: steps 1-11 advance on semantic MIDDLE (welcome `4979f432` -> `629da47e` -> LANGUAGE `d4ed66c7` -> `2a01c517` -> `11409fe0` -> GENDER `08f12393` -> `626b8201` -> UNITS `03f6aaf4` -> TIME FORMAT `d2d91bba` -> WEIGHT `73c1cbde` -> HEIGHT `a423440c`); step 12 settles `261712ad` "Connect with mobile" and advances only on MIDDLE to the handoff `ea3bc5f8` (step 13); three LOWER presses settle `9b58f243` (14), `eb868d29` (15), `ed7eeb7a` (16, rendered "Watch info: SUUNTO 9 PEAK PRO" with a `Later` pill); MIDDLE on `ed7eeb7a` then settles a phone-pairing recommendation sequence `74a5e6ab` ("It is highly recommended to connect with mobile...", 17), `b26dd658` (18), `0b93f6c9` ("Connect / Connect Later", 19), `a797ec30` ("software updates" + Later/Continue, 20), and MIDDLE on `a797ec30` settles `8362b9bc` "Time/date" (`w-tida`, step 21, ~23.0 s virtual). On `8362b9bc` no physical input advances the onboarding: MIDDLE, LOWER, and UPPER are each inert both singly and when repeated up to twelve times at 400 ms virtual spacing (`rep`, `repU`, `repL` settle byte-identical to the repeat-off `reg` baseline through step 21). The `w-tida` resource shows the rendered "Time/date" popup is `#vs-h` index 1 - the only non-empty, `selected` div, with indices 0 and 2 empty - whose `onTap`/pushButton `next` calls `next('#vs-h')` and whose `onIdle` acts only at `targetData==0` (close `w-tida`, open `w-conn`) or `targetData==2` (navigate back to 1, open `w-ltim` when `DEVICE_HAS_LOCATION`, else `w-year`), so a single MIDDLE should reach `targetData==2` and open `w-ltim`, which it never does. The emulator input model exposes only the three GPIO buttons (no swipe/gesture channel), so `8362b9bc` is a phone-gated handoff: the post-`w-conn-1` "Continue the setup on your phone" branch leaves time setup to the phone, and no standalone 2.22.60 clock source can advance it (NMEA time resets the guest; the RTC value is ignored; no OHR2 phone time command exists). (b) Every walk (with or without the gated RTC value) stops byte-stably at `pc=0x0010fbde`, `stop=compat-refused`, the eleven-hit `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005) at virtual time 73.08-73.15 s; the default-lower baseline instead idles at `261712ad` until the same abort. (c) Time-source RE (2.22.60 application binary + resources): `w-ltim.onActivate` puts `Navigation/State=1` and after a 5 s `setTimeout` subscribes to `Dev/Time/LocalTime`; `t >= 1646092800` (2022-03-01Z, i.e. unix seconds) routes to the UTC-offset menu then `w-done`, otherwise `utc-notfound` shows "Search for GPS" whose **down** button ("Set manually") opens `w-year`. The manual chain `w-year` -> `w-mont` -> `w-day` -> `w-time` (each an up/next/down spinner; `next` saves the component and opens the next field; `w-time` next opens `w-tset`/`w-done`) and `w-done` (auto-opens `main` after 3 s) are fully present in the resource. The authoritative application clock path is the `GpsTimeSynchronizer` worker comparing the system clock against NMEA GGA/RMC time-of-day ("Timesync: sys %lldms gps %lld", "Device time updated.", "setting new utc time %u"); no OHR2/phone time-sync command string exists in the 2.22.60 application binary. (d) Empirical boundaries: (i) injecting a valid time-bearing GGA+RMC+EPU group (2024-01-01 12:00:00 UTC, verified checksums, with or without coordinate fields) on the `@GSR` running-status exchange makes the firmware issue repeated deliberate `SYSRESETREQ` writes (`0x05fa0004` to AIRCR `0xE000ED0C` from PC `0x000be93e`; reset count climbs 1->6 across the two variants, while the identical baseline run records zero) and does not stop the GPS power-cycling; synthetic NMEA time is therefore not authorized as a `LocalTime` source. (ii) returning `1704110400` from `0x40004820` (plus time-set bit at `0x40004800`) leaves the onboarding byte-identical (same frames, same steps, same abort) - the RTC value register does not feed `Dev/Time/LocalTime` in the standalone context. | Reproducible emulator observation plus static RE; frames recorded by CRC32 only, no pixels or firmware bytes enter Git, and all experiment code is reverted (tree clean, `make check` green, 87 task contracts). This entry refines E-SAP-ONBOARD-EMU-008: the furthest reached screen is "Time/date" (`w-tida`, `8362b9bc`), one viewset-advance before `w-ltim`, and the `w-ltim` "Search for GPS" screen is not reached because the `w-tida` internal viewset cannot be driven by the one-press-per-frame walk. It authorizes no new device, NEMA, time-provider, or GPS-budget behavior: the repeated-button runs confirm the `Time/date` stop is input-side (no button, singly or repeatedly, is accepted there) rather than a walk-model limitation, so completing onboarding requires the phone (OHR2) side of the onboarding flow - an owner/roadmap decision outside this ticket - plus a native 2.22.60 onboarding time-sync capture to evidence the phone's time-delivery protocol. |
| E-SAP-ONBOARD-EMU-010 | 2026-08-26; fresh-boot SDL `SEMU_SDL_LIVE_TEST=setup-walk` runs from the exact external manifest with the `sapporo-2.22-no-device` layer and `SEMU_SDL_PPM_DIR` frame capture (completion walk: `SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm` plus the new opt-in `SEMU_SDL_SETUP_WALK_TIMELINE=30000:l` absolute-virtual-time press driver; regression: default walk `SEMU_SDL_SETUP_WALK_POST=mlllmlllmm`, no timeline; each `--until setup-next`, `--max-time 300000000000`), plus read-only RE of the 2.22.60 onboarding views in `component-05-type-1-v3.raw` (`w-ltim` @ `0x892300`, `w-year` @ `0x81be1d`, `w-mont` @ `0x69021d`, `w-day` @ `0x89701d`, `w-time` @ `0x127f3d`, `w-done` @ `0x288e1d`) | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | (a) Corrects E-SAP-ONBOARD-EMU-009(a): `8362b9bc` ("Time/date", `w-tida`) is NOT phone-gated. MIDDLE on it DOES advance the onboarding to `w-ltim`; the 009 same-frame repeat runs only settled byte-identically to the repeat-off baseline because the `w-ltim` "Searching for GPS" ring animates every ~1-4 ms of virtual time and therefore never reports a settled frame - the frame-stepped walk could no longer observe a step, which looked like inert input. (b) New opt-in `SEMU_SDL_SETUP_WALK_TIMELINE="ms:letter[,ms:letter...]"` driver fires a button press at an exact virtual time regardless of the 350 ms settle window (bounded to 64 entries, off by default, byte-identical when unset). A LOWER at virtual ms 30000 presses the `w-ltim` "SET MANUALLY" down button, opening `w-year`. (c) Deterministic completion chain to `main`, settled CRC32s (default year 2022; one LOWER on `w-year` to 2023): `5321867e` (`w-year` 2022) -> `c683e828` (`w-year` 2023) -> `cd1b0979` (`w-mont` Jan) -> `455b603a` (`w-day` 1) -> `53d3f0c1` (`w-time`, hour focus) -> `17e1772c` (`w-time`, minute focus) -> `578e2601` (`w-done` "Done") -> `1c62ab1a` (main transition) -> final main-menu frame `fb8e0155` (Navigation/Logbook/Media controls) at ~37.9 s virtual; the run then idles and stops at `stop=halt` pc=0x000727ca at ~43.8 s virtual (clean idle, not a GPS-cap abort). `w-time` "next" is `switchFocus()` (hour<->minute); the second MIDDLE hits `onIdle targetData==2`, which saves hour+minute+local via `saveTimeComponent` and opens `w-done`; `w-done` sets `/Settings/Ui/FirstUseWizardExecuted=true` and auto-opens `main` after 3 s. (d) The default walk with the driver disabled remains byte-identical to the 009 baseline: last settled frame `8362b9bc`, abort at `pc=0x0010fbde`, `stop=compat-refused`, eleven-hit `gps-awake-pulse` budget at ~71.1 s virtual. (e) The gate constant `1646092800` (2022-03-01Z) still governs `w-ltim` auto-routing and `startup`'s post-wizard branch; manual entry below the gate (e.g. 2022-01-01) still completes the wizard to `main` in-session, but a pre-gate `LocalTime` routes the next boot to `n-sync-rec` instead of `main`. | Reproducible emulator observation; the disabled-driver default walk is byte-identical to the 009 baseline (`frames.log` compared), and the parser refactor keeps both the regression and completion `frames.log` byte-identical. Frames recorded by CRC32 only; no pixels or firmware bytes enter Git. Committed regression: `tools/test_sdl_onboarding_completion.sh` (wired into `make check-sdl`; parser refusals run unconditionally, both firmware walks gate on `SEMU_FIRMWARE_MANIFEST`) re-derives the completion chain and the byte-identical disabled baseline from the manifest. This entry corrects E-SAP-ONBOARD-EMU-009(a): "Time/date" is not phone-gated and onboarding completes to `main` standalone via the firmware's own manual-entry chain. The phone-time negative RE of 009(c) stands: no OHR2/phone time-delivery command exists in 2.22.60, so a phone time source is neither needed nor buildable for this firmware. |
| E-SAP-ONBOARD-EMU-011 | 2026-09-03; two independent fresh-boot SDL dummy `SEMU_SDL_LIVE_TEST=middle-language` runs from the exact external manifest with the `sapporo-2.22-no-device` layer, no snapshot, `--until middle-language`, `--max-instructions 14000000000`, and `--max-time 22000000000`; current SDL binary SHA-256 `d31e2c72145532ab38e9b2a068ffe84214f143aed76a93362a48ff75d1aceafc` at commit `cdfc953be30cacdbf988a3b1c984a76a8d10fb6a` | Sapporo `2.22.60.3383-P` (component hashes E-SAP-0005..0007) | Both runs produced byte-identical complete logs (SHA-256 `55d96468b4b41a938f98ab9db500dabc99ad491d7cc1e5595b933119a7b1f72b`). The initial validated 240x240 frame is generation 1 CRC32 `2a01c517`; the queued Return/Enter edge settles generation 3 CRC32 `4979f432`; two successive middle-screen clicks settle generation 5 CRC32 `629da47e` and generation 63 CRC32 `d4ed66c7`. The synthetic SDL quit event stops repeatably with `stop=user`, PC `0x080000a2`, 804398304 instructions, and virtual time 9504428769 ns. | Reproducible current-emulator observation authorizing the narrow `tools/test_sdl_live_input.sh` checkpoint re-pin. It changes no renderer, input, compatibility, or guest behavior; frames remain represented by CRC32 only, and no firmware bytes or pixels enter Git. The check still fails closed on invalid live-test configuration and skips the authentic run when the private manifest is absent. |


| E-SAP-FLASH-001 | 2026-08-16; native MSPI2 boundary trace and SapporoApollo4Mspi2 model | Sapporo `2.22.60.3383-P` | The model pins the 32-MiB geometry, 20 BB 19 identity, 0x9F/0xAF ID forms, observed one-byte 0x85 zero response, 0x70 status, 0x0C 24-bit reads, write-enable 0x06, 4-KiB sector erase 0x21, and page program 0x12; the boundary trace records 31,881 MSPI2 flash reads, observed 0x12 TX DMA frames, 0x21 command completions, 5,340 RX DMA records, 5,152 TX DMA records, and 5,521 command completions. | Verified bounded flash contract; the emulator keeps the source artifact immutable, applies NOR 1-to-0 programming through the storage overlay, aligns sector erase, consumes write-enable once, and refuses unsupported shapes, missing write-enable, page crossing, and out-of-range operations. |

| E-SAP-COMPAT-RESOURCE-001 | 2026-08-16; OTA resource storage model and native-render run | Sapporo `2.22.60.3383-P` | The OTA resources component is mapped at `0x14000000`; the evidenced resource-list lookup for key `0x1d00` reaches the same list root with OTA staging data as the Renode storage model. The no-device layer translates only the version-pinned wrapper sentinel `0xcc` to the observed success status `0xc8`, once, so the production application can continue to its display service. | Synthetic compatibility boundary; the component hash pin, exact PC/register tuple, one-hit intervention budget, and refusal tests prevent this from becoming a general resource or status fallback. |
| E-SAP-COMPAT-GPS-004 | 2026-08-17; native running UI trace and GPS fixture | Sapporo `2.22.60.3383-P` | The native running UI trace reaches the later `@GSR` request and `$PSS0000` response after startup. The OTA-only compatibility layer reproduces only this exact version-pinned 10 ms exchange once, then resumes at the observed second-open UART boundary with a session-local event object. | Synthetic compatibility boundary; the trigger PC/register shape, one-hit budget, observed UART writes, and refusal tests are bounded. Navigation, satellite, and physical GNSS behavior remain unsupported. |
| E-SAP-COMPAT-GPS-005 | 2026-08-20; read-only native CXD5610 lifecycle boundary, `$FIRMWARE_ROOT/docs/research/cxd5610-gps-boundary.md`, SHA-256 `17f9f1e38ba16a38d43f945b0240b72046802fa063afcc8bed00b1ac5ea46eba` | Sapporo `2.22.60.3383-P` | An independent 60-second native capture reaches eleven successful state-12 polls at the same roughly 5.5-second interval, with no GPS recovery, assertion, or reset. The compatibility layer therefore permits exactly eleven observed GPIO24 awake-pulse interventions before refusing a twelfth. | High-confidence native lifecycle boundary; the eleven-hit budget is evidence-bounded and covered by `test_gps_awake_evidence_budget`. Physical GNSS navigation, later commands, and any twelfth pulse remain unsupported. |

## Migration Procedure

Inspect `suunto-firmware` behavior and traces without copying large Renode classes literally. Reduce each fact to a register contract, bus transcript, memory-map entry, or algorithm; cite its source file/symbol or trace hash; add focused positive and refusal tests; then update the corresponding matrix status.

Guesses remain labeled hypotheses and belong in compatibility work until authentic traffic validates them. Evidence that conflicts with an existing entry creates a new entry and explains the superseded assumption; do not silently rewrite history.

## CPU Reference Pins

These entries pin primary documents for Phase 2. The PDFs and upstream source
are not repository inputs; the URLs, document identifiers, revisions, and
source hashes below make the validation inputs reproducible without copying
reference content into the repository. Product/firmware hashes are `n/a` for
all four entries because these are architecture, ABI, and upstream-source
references rather than product inputs.

| ID | Date / source and exact revision | Stable source location and source hash | Observation and covered sections | Affected modules / validation use | Confidence / unresolved questions |
| --- | --- | --- | --- | --- | --- |
| E-CPU-0002 | 2026-08-13; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621 | [Arm documentation-service PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f); SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747` | Normative Thumb encoding and instruction pseudocode. Common locations: A3.2 “Alignment support” (A3-65), A3.3 “Endian support” (A3-68), A3.4 “Synchronization and semaphores” (A3-75), A5.1–A5.3 Thumb encoding (A5-126–A5-158), A7.7 instruction details (A7-186 onward), and D6.1.1 pseudocode (D6-805). Ticket 200 uses A5.2.1/A5.2.2 and the A7.7 arithmetic/shift entries; 205 uses A5.2.3/A5.2.5/A5.2.6 and the A7.7 control, branch, hint, extension, breakpoint, and UDF entries; 210 uses A5.2.4 and the A7.7 single-transfer, stack, and multiple-transfer entries; 220 uses A5.3.1–A5.3.4, A5.3.11–A5.3.12, and the A7.7 data-processing/branch entries; 225 uses A5.3.5/A5.3.6/A5.3.7–A5.3.10, A3.2, A3.4.1–A3.4.3, A7.7 single and multiple transfer entries, A7.7.23 CLREX, A7.7.53–A7.7.54 LDREX*, A7.7.167–A7.7.169 STREX*, A7.7.185 TBB/TBH, and A7.7.33–A7.7.37 barriers; 230 uses A5.3.12–A5.3.17 and the A7.7 multiply, divide, DSP, saturation, packing, reverse, and select entries; 240 uses A7.7.29 CPS, A7.7.82 MRS, A7.7.83 MSR, and B5.1.1/B5.2 for special-register encodings, masks, and privilege behavior. | `src/cpu/armv7m/**`; the vector contract and coverage seed in `tests/fixtures/cpu/`; exact future family vectors for tickets 200–240. Existing positive checks `test_load_store`, `test_mov_w_sp_regression`, `test_stmdb_sp_regression`, `test_orr_modified_immediate`, and `test_indexed_word_load`, plus refusal check `test_unsupported_instruction`, are the current validation boundary. | High: exact revision, document identifier, stable primary URL, and source hash captured. The manual leaves implementation-defined choices (for example implemented interrupt lines and some memory-system behavior) to later evidence; no such choice is inferred here. |
| E-CPU-0003 | 2026-08-13; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621, plus Arm, *Cortex-M4 Devices Generic User Guide*, ARM DUI 0553B, ID012616 | DDI 0403E.e: [Arm PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f), SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747`; DUI 0553B: [Arm PDF](https://documentation-service.arm.com/static/5f2ac76d60a93e65927bbdc5), SHA-256 `a388721e2c87fdf0a70c40545b6a72d390afdf87d47f654a6577e5e61cd92b30` | Normative exception, stack, return, and SCS references. DDI 0403E.e: A3.2 alignment/PC-return constraints; B1.5.6 “Exception entry behavior” (B1-531), B1.5.7 “Stack alignment on exception entry” (B1-535), B1.5.8 “Exception return behavior” (B1-539), including vector fetch, basic eight-word frames, STKALIGN/xPSR[9], accepted EXC_RETURN forms, reserved-token INVPC behavior, and derived exceptions on entry/return; B3.2 “System Control Space” (B3-595), including B3.2.2 SCB/FP register summaries (B3-596 onward), B3.3 SysTick (B3-622), and B3.4 NVIC (B3-625 onward); B5.1.1/B5.2 MRS/MSR/CPS special-register encodings and privilege rules (B5-669–B5-677). DUI 0553B: Chapter 2 §§2.1–2.5 (programmer model, memory, exception, fault, and power management) and Chapter 4 §§4.1–4.6 (NVIC, SCB, SysTick, MPU, and FPU). | `src/cpu/armv7m/**`; exception/NVIC/SysTick/FPU-context vectors for tickets 240, 245, 250, 260, and 275. Existing positive checks `test_svc_exception_return`, `test_level_irq`, and `test_wfi_deadlock` validate only the current narrow boundary; later tickets must add refusal coverage for invalid frames, priorities, SCS offsets, and fault escalation. | High for the cited architectural behavior. Apollo4-specific priority-bit count, implemented external IRQ range, and other implementation options remain unpinned and are not supplied by this entry. |
| E-CPU-0006 | 2026-08-14; deterministic synthetic Cortex-M4 profile decision grounded in E-CPU-0003 | E-CPU-0003 generic-guide SCS tables and implementation-option constraints; no firmware bytes or product claim | The standalone CPU profile pins 240 externally addressable IRQ lines (IRQ0–239), all eight priority bits (`0xff` raw mask), VTOR bits[31:7] with 128-byte minimum alignment, and no host-side SYSRESETREQ action. CPACR is a reset-zero, raw-bit register until ticket 260 owns FPU access behavior. These are explicit synthetic-profile choices, not Apollo4 observations; a later SoC evidence entry may supersede them. | `src/cpu/armv7m/{cpu.c,nvic.c,scb.c,exception.c}` and CPU SCS/NVIC/fault vectors for ticket 245 | Medium: the choices are explicit and deterministic for the synthetic Cortex-M4 profile; Apollo4-specific IRQ wiring, priority width, and reset-request behavior remain separate evidence work. |
| E-CPU-0007 | 2026-08-14; deterministic synthetic Cortex-M4 FPU profile grounded in Arm, *Cortex-M4 Processor Technical Reference Manual*, revision 100166_0001_04 | [Arm documentation-service PDF](https://documentation-service.arm.com/static/5fce431be167456a35b36ade); SHA-256 `7415b77eeff5d0bfa1f8f43eb4fe3e97b22b34fe76baed21d0b791f3a238eb94` | The profile pins FPv4-SP reset/state behavior for ticket 260: CPACR `0x00000000`, FPSCR `0x00000000`, FPCCR `0xc0000000` (ASPEN/LSPEN set), FPDSCR `0x00000000`, and FPCAR `0x00000000` as the deterministic zero choice because the TRM leaves its reset value unspecified. FPCCR writable model bits are ASPEN/LSPEN; FPCAR is word-aligned; FPDSCR uses the documented default-status fields. This is a synthetic profile decision, not an Apollo4 observation. | `src/cpu/armv7m/{armv7m_internal.h,scb.c,thumb32.c,thumb32_fpu.c,fpu_transfer.c}` and ticket-260 raw-bit/reset/refusal vectors | High for the Cortex-M4 register reset table and field locations; medium for the explicit FPCAR-zero profile choice, which remains a deterministic emulator policy until a later target-specific source supersedes it. |
| E-CPU-0004 | 2026-08-13; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621, FPv4-SP extension | [Arm documentation-service PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f), SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747`; the document identifies FPv4-SP in A2.5 and defines the extension in the same pinned revision | FPv4-SP is the single-precision VFPv4-D16-derived Armv7-M extension. Use A2.5 “The optional Floating-point Extension” (A2-38–A2-46) for binary formats, FPSCR fields, rounding, NaN/default-NaN, flush-to-zero, and exception flags; A6.1–A6.6 (A6-160 onward) for floating-point encodings and register fields; A7.7 FP instruction entries for transfer, arithmetic, compare, conversion, and immediate operations; B1.6 “Floating-point support” (B1-565) for access enable/NOCP behavior; B3.2.2 FP system-register descriptions (B3-597 onward) for CPACR/FPSCR/FP context control; and B1.5 FP stacking (B1-536 onward) for basic/extended/lazy context. | `src/cpu/armv7m/**`; raw-bit transfer/arithmetic/conversion/context vectors for tickets 260–275. Existing positive `test_fpscr_transfer` is only a narrow transfer smoke check. No arithmetic, conversion, or lazy-stacking behavior is marked implemented or verified by this entry. | High for the FPv4-SP architectural contract. Cortex-M4 implementation options and the synthetic guest’s exact context schedule remain validation work; double precision and unreferenced implementation choices are out of scope. |
| E-CPU-0008 | 2026-08-14; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621, FPv4-SP arithmetic rules | [Arm documentation-service PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f); SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747` | Ticket 265’s arithmetic contract is pinned by A2.5.4: the binary32 format and classifications (A2-39–A2-40), FPSCR DN/FZ/RMode and cumulative IDC/IXC/UFC/OFC/DZC/IOC fields (A2-37–A2-38), flush-to-zero input/result processing and signed-zero preservation (A2-43), NaN propagation, signaling-NaN quieting, Default NaN, and invalid-operation rules (A2-43–A2-44), exception default-result table (A2-45), cumulative exception combinations (A2-46), and the integer-representable pseudocode for FPUnpack, FPProcessNaN(s), FPRound, FPAbs, FPNeg, FPAdd, FPSub, FPMul, FPDiv, FPMulAdd, and FPSqrt (A2-47–A2-56). The instruction operations and scalar encodings are pinned by A7.7 VABS (A7-455), VADD (A7-456), VDIV (A7-468), VMLA/VMLS (A7-476), VMUL (A7-487), VNEG (A7-488), VNMLA/VNMLS/VNMUL (A7-489), VSQRT (A7-498), and VSUB (A7-503). A7.7.238 and A7.7.250 explicitly sequence multiply then add/negate with separate operation calls; FPv4-SP does not use a single fused multiply-add result for these instructions. FPv4-SP uses the single-precision `sz=0` forms; the double-precision, short-vector, and FPv5-only forms remain refusal cases. | `src/cpu/armv7m/{fpu_arith.c,fpu_softfloat.c,thumb32.c,thumb32_fpu.c}` and `tests/fixtures/cpu/fpu-arith/**`; exact arithmetic results and cumulative FPSCR flags for ticket 265. The manual explicitly leaves relative priority unspecified when independent exceptions in a multiply-accumulate do not depend on one another; ticket 265 records all cumulative bits and does not implement trapped FP exceptions, so that ambiguity is not guest-visible in this scope. | High for the cited architectural rules and exact revision. No Apollo4-specific arithmetic deviation is asserted; any target-specific difference requires a later evidence entry. |
| E-CPU-0005 | 2026-08-13; Arm, *Procedure Call Standard for the Arm Architecture*, AAPCS32 document IHI 0042J, ABI release 2020Q2; FreeRTOS-Kernel `V10.4.6` ARM_CM4F port at peeled commit `a4b28e35103d699edf074dfff4835921b481b301` | AAPCS32 [exact Arm ABI repository revision `ee4b3c12d57c8424ff60c2ae56e10690d0604ab6`](https://github.com/ARM-software/abi-aa/blob/ee4b3c12d57c8424ff60c2ae56e10690d0604ab6/legacy-documents/aapcs32/ihi0042_J/IHI0042J_2020Q2_aapcs32.xhtml), document `IHI0042J_2020Q2_00_en`, source SHA-256 `92d7c399a47fa31451703f6a9c07c965daf5de399282310c74d3f05ac4741a57`; FreeRTOS [`portable/GCC/ARM_CM4F/port.c`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/a4b28e35103d699edf074dfff4835921b481b301/portable/GCC/ARM_CM4F/port.c) and [`portmacro.h`](https://github.com/FreeRTOS/FreeRTOS-Kernel/blob/a4b28e35103d699edf074dfff4835921b481b301/portable/GCC/ARM_CM4F/portmacro.h). Source SHA-256: `port.c` `8aa709759655b1711b28ea943b232a8c7f3ddbbb06f6c05f4328c681b223a734`; `portmacro.h` `a0cc3996ab10e9dce31b6665502e7a1328d134383d467b16e0aab205a7b07ccb` | AAPCS32 §§5.1–5.2 (data types, endianness, alignment), §6.1 (core registers), §6.2.1 (stack and universal stack constraints), §§6.3–6.6 (calls, returns, parameter passing, interworking), and §7.1 (VFP register arguments) define the ABI assumptions. The exact FreeRTOS port is used only as a synthetic-gate shape reference: `port.c` lines 189–220 (`pxPortInitialiseStack`), 247–321 (SVC/startup), 368–390 (VFP enable and FPCCR), 439–495 (PendSV), 496–518 (SysTick), and 712–731 (VFP enable); `portmacro.h` lines 172–239 cover interrupt-state and BASEPRI helpers. | `fixtures/synthetic/rtos/**` and `tests/integration/test_cpu_rtos_guest.c` in ticket 280; ABI/context handoff to ticket 275. No FreeRTOS source, binary, or copied implementation is part of this repository. | High for the named ABI release and exact upstream revision. The port is a gate-shaping reference only, not evidence that the emulator matches FreeRTOS or a product firmware; the synthetic checkpoint/hash remains to be produced by ticket 280. |
| E-CPU-0009 | 2026-08-14; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621, FPv4-SP compare/conversion/immediate rules | [Arm documentation-service PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f); SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747` | Ticket 270 uses the exact A2.5 pseudocode and instruction entries: `FPCompare` and its N/Z/C/V table (A2-53–A2-54) produce less `1000`, equal `0110`, greater `0010`, and unordered `0011`; signaling NaNs always set IOC and `VCMPE`/quiet-NaN exception mode also sets IOC, while `VCMP` with a quiet NaN does not. `FPToFixed` and `FixedToFP` (A2-58–A2-59) define FPSCR-directed versus round-toward-zero conversion, ties-to-even, signed/unsigned `SatQ` results, IOC on saturation/NaN/infinity, and IXC only for non-overflowing inexact finite results. A7.7.226 (A7-457–A7-459) pins VCMP/VCMPE register and +0 forms; A7.7.228 (A7-460–A7-463) pins signed/unsigned 32-bit VCVT/VCVTR and the `sz=0` single-precision forms; A7.7.229 (A7-463–A7-465) pins S16/U16/S32/U32 fixed-point widths, legal fraction ranges, source low-bit extraction, sign/zero extension, round-toward-zero float-to-fixed, and round-to-nearest fixed-to-float; A6.4.1/A6-166–A6-167 pins `VFPExpandImm` for binary32 modified immediates; A7.7.239–A7.7.240 (A7-478–A7-480) pin VMOV immediate/register. The ticket implements only FPv4-SP `sz=0` forms; double-precision and FPv5-only forms are explicit refusal cases. | `src/cpu/armv7m/{fpu_convert.c,fpu_softfloat.c,fpu_softfloat_convert.c,fpu_softfloat.h,fpu_softfloat_internal.h,armv7m_internal.h,thumb32_fpu.c}` and `tests/fixtures/cpu/fpu-convert/**`; exact raw S-register/FPSCR vectors, including comparison flags, conversion saturation, NaN/invalid, rounding modes, modified-immediate classes, and reserved/double refusals. | High for the cited primary document, exact revision, pseudocode, and opcode pages. No target-specific conversion deviation is asserted; unreferenced FPv5/double forms remain outside this ticket. |
| E-CPU-0010 | 2026-08-14; Arm, *Armv7-M Architecture Reference Manual*, ARM DDI 0403E.e, ID021621, FP exception context and lazy preservation | [Arm documentation-service PDF](https://documentation-service.arm.com/static/606dc36485368c4c2b1bf62f); SHA-256 `76500176d20f897eaf05eeadb5a6202cef641e332073b107905c8898e0ee0747` | Ticket 275 uses B1.5.6–B1.5.8 and Figure B1-4 as the exact context contract. Basic frames are 0x20 bytes with R0/R1/R2/R3/R12/LR/return PC/xPSR at offsets 0x00–0x1c. Extended frames are 0x68 bytes: the same basic frame remains at 0x00–0x1c, S0–S15 are at 0x20–0x5c, FPSCR is at 0x60, and the reserved word is at 0x64. Extended entry forces 8-byte alignment and records a pre-entry 4-byte alignment in stacked xPSR[9]. If CONTROL.FPCA is 1, entry selects an extended frame; with FPCCR.LSPEN clear it eagerly writes S0–S15/FPSCR, and with LSPEN set it reserves the FP area, writes only the basic words, sets FPCAR to frame+0x20, and sets FPCCR.LSPACT. The first permitted FP instruction while LSPACT is set writes the saved S0–S15/FPSCR to FPCAR and clears LSPACT before executing the instruction. Exception entry clears CONTROL.FPCA; exception return sets it to the inverse of EXC_RETURN[4]. The FP extension defines six valid tokens: 0xffffffe1/0xffffffe9/0xffffffed for Handler/Thread-MSP/Thread-PSP Extended and 0xfffffff1/0xfffffff9/0xfffffffd for the corresponding Basic frames. S16–S31 are unchanged by hardware stacking. B3.2.21–B3.2.22 define FPCCR status/control fields and FPCAR alignment; B3.2.18 and B3.2.20 define MLSPERR/LSPERR for delayed FP preservation faults. The synthetic profile chooses zero for architecturally UNKNOWN handler FP registers and post-preservation FPCAR, while preserving all stacked raw bits for exact return. | `src/cpu/armv7m/{fpu_context.c,exception.c,scb.c,fpu_transfer.c,thumb32_fpu.c,thumb32_system.c,armv7m_internal.h}` and `tests/fixtures/cpu/fpu-context/**`; positive and refusal tests cover basic/extended/lazy frames, nested MSP/PSP returns, alignment, exact FP restoration, invalid frames/tokens, and delayed-preservation faults. | High for the exact primary document and revision. The zero choice for architectural UNKNOWN values is an explicit deterministic synthetic-profile policy; no Apollo4-specific context deviation is asserted. |
