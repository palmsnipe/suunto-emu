# Migration Evidence Ledger

## Purpose and Format

This ledger records facts migrated from `suunto-firmware`, firmware traces, documentation, and synthetic experiments. It contains summaries and hashes, never copyrighted firmware bytes. Each implementation and hardware status change cites stable evidence IDs from this file.

Each future entry must contain: ID, date, source kind and location, product/firmware hashes, observation, confidence, affected modules, validation test, and unresolved questions. Local-only source locations must be described symbolically, such as `$FIRMWARE_ROOT`, rather than with a user path.

Rows that describe an earlier blocked probe remain historical evidence. In
particular, E-SAP-0013 records the pre-705 state in which later-version
profiles were unavailable; E-SAP-0014 and E-SAP-0017 supersede its current
profile and gap conclusions without changing the original observation.

## Seed Evidence

### E-EMU-NEMA-CALLBACK-001

2026-09-08; ticket 761 callback-lifecycle audit after `cfce2a1`. Sources:
completion/scheduler ownership contracts, architecture callback lifetime rules,
E-NEMA-LISTS-001 notification ordering and a synthetic slot-reuse regression.
The scheduler removes a due event before dispatch. Completion then marks its
entry inactive before notifying CLID, INTERRUPT=1 and IRQ28. Previously all
three calls reread that entry: scheduling from the CLID callback can reuse it
and redirect the remaining calls to another recipient. Reset can clear the
function pointers; destroying the completion owner can free the entry.
The new regression fails before implementation because the original recipient
receives only one notification instead of three. No firmware behavior is
inferred; this is an emulator callback ownership defect.

Copy the accepted notification tuple before making its slot reusable. Retire
its active bookkeeping before callbacks, then use only the local copy. Reset,
cancel or destruction affects queued work but cannot revoke an in-flight
CLID/INTERRUPT/IRQ sequence. Callbacks and their contexts remain borrowed until
that sequence returns; the scheduler must also remain alive. This does not
permit freeing callback contexts or destroying the machine from a callback.
The regression additionally exercises reset, cancel and completion-owner
destruction with live external contexts, verifies pending-event cancellation,
and preserves the equal-deadline older completion before the newly scheduled
100-us completion. Confidence: high for the bounded synthetic lifecycle cases.
Snapshot layout, notification ordering and delay are unchanged. Exact results
and native checkpoints are recorded in ticket 761's handoff.

### E-EMU-NEMA-TEXTURE-MEMORY-001

2026-09-08; ticket 761's user-authorized texture-reader scope extension.
Sources: E-EMU-NEMA-MEMORY-001's external counter probe, existing byte-wide bus
read/copy and overlay contracts, architecture transaction rules, and new
synthetic texture/backend regressions. No firmware-specific behavior is added;
native application/component hashes remain the preceding entry's pinned inputs.
Five texture cases fail zero-MMIO-read assertions before implementation:
validation, RGB565 low/high bytes, A2LE and a later bilinear tap. A sixth finds
that the A2LE wrapper clears RGB output before its alpha read refuses, including
with a null error sink. The composed backend case refuses a device-backed
second texel after an earlier child draw and requires no device callbacks,
state/pixel publication or diagnostic replacement. Before implementation it
instead invokes the device callback. Existing and adjacent-ROM success controls
pass. Exact commands and logs are recorded in ticket 761.

The correction uses memory-only byte copies at the original access granularity:
a two-byte RGB565 copy would incorrectly bypass a one-byte overlay under the
existing bus rules. Both bytes must succeed before publishing a texel. A2LE
uses a local alpha until the whole output is valid; bilinear already stages
all taps. Memory errors retain their original code/text. Descriptor validation
checks arithmetic and last-byte addressability, not every byte in the range;
each actual sample is separately checked. Transactional callers stage target
pixels, so holes/overlays encountered later still refuse without publication.
Confidence is high for these bounded software contracts; no device-backed
texture support, compressed texture decode or physical GPU behavior is inferred.
Native pins and callback lifecycle acceptance require the ticket's final review.

### E-EMU-NEMA-MEMORY-001

2026-09-07; ticket 761 command-memory/lifecycle audit, continuing the
uncommitted inline-plan slice on `611d3c4`. Sources: architecture's
validation-before-mutation rule, E-EMU-NEMA-ATOMIC-001, the existing bus
read/copy implementation and synthetic counter-backed MMIO command sources.
Ordinary bus reads invoke device callbacks; `semu_bus_copy_out` only reads
mapped memory and refuses devices without invoking them. Ring, child and
backend command fetches previously used ordinary reads, leaking external
device state even if a later command refused. Four regressions fail before
the fix: device-backed ring, child, backend list, and a RAM register followed
by a device-backed value. Each requires zero device reads. A synthetic ROM
success control verifies explicit little-endian decoding and RAM retries.

Command fetches now share a memory-only four-byte reader in the framing
module, used by the parser and backend. Failure preserves the caller's output
word and the original bus error. No bus policy/API or memory mapping changes.
Confidence is high for these synthetic refusal paths and the existing mapped
RAM/ROM command contract; no MMIO command-fetch support is inferred. The
related texture-source read paths remain an integration gap: a bounded external
synthetic probe observes one MMIO callback before texture validation refuses,
two before RGB565 sampling refuses, and one before A2LE sampling refuses.
Mapped-ROM RGB565/A2LE controls pass. Probe source SHA-256 is
`9c0c29f72931d2c1888638f589786739f55786b2fa9fc87db125bd1302d089ce`;
source and output are in the external evidence directory named by ticket 761.
These readers and their focused tests are outside its Allowed Files. No
renderer-side duplicate validation or bus fallback was added to bypass that
boundary. Exact commands, results, native pins and requested integration are
in ticket 761. Completion callback lifecycle remains unaudited acceptance work.

### E-EMU-NEMA-INLINE-001

2026-09-07; ticket 761, continuing the uncommitted E-EMU-NEMA-TAIL-001 slice
on `611d3c4`. Sources: E-NEMA-RING-001 and read-only
`$FIRMWARE_ROOT/docs/research/native-nema-ring-bootstrap-decode.md`, plus
E-EMU-NEMA-TAIL-001's active-span observers on both hash-validated applications.
The bootstrap decode establishes paired inline register/value syntax, exact
CL_NOP (`0x00010000`) padding and held wrap controls. It explicitly assigns
no command meaning to the unused zero-filled gap. The later active observers
see three initialization prefixes in each version, each containing
INTERRUPT=0, IMEM slot setup and constant writes. Application pins remain
E-EMU-NEMA-CONTROL-001's exact values. Confidence: high for framing and the
existing evidenced register set; no fragment-processor ISA or IRQ-clear
semantics are inferred. INTERRUPT=0 remains a non-requesting control.

The prior scanner ignores unknown inline words, and its second marker scan
can mistake values for opcodes. A synthetic mixed inline/child transaction
accepts an unknown register after an earlier child instead of refusing. The
new regression fails before the fix. The shared parser now stages one ring
plan: ordered contiguous inline runs and child lists, plus complete marker
IDs. The same plan supplies renderer preparation and completion admission.
Inline runs use the public transaction descriptor's no-publication flag,
retaining per-child callback order and inherited state across the whole stop.
Unknown flags, inline registers/prefixes, nonexact NOPs and truncated commands
refuse. This adds no second backend API, runtime hook or persistent encoding.

Two older framing tests used an unmatched zero register and an unmatched held
graphics word as padding. Those synthetic inputs now explicitly refuse; their
corrected retries use exact NOPs. Their prior acceptance was not a native
golden. Child syntax callbacks retain their original purpose; the GPU consumes
the complete plan instead of silently dropping inline commands. Bounds remain
32 children and 64 markers, with at most 64 total child/inline spans. Complete
inline runs may wrap via separate spans; a graphics pair split across the
physical ring end remains unsupported pending evidence. Final commands,
success pins and remaining acceptance gaps are recorded in ticket 761.

### E-EMU-NEMA-TAIL-001

2026-09-06; ticket 761 follow-up on `611d3c4`. Sources: read-only
`$FIRMWARE_ROOT/docs/research/native-live-ui-navigation.md`, sections
"CMDSIZE entry-count correction" and the superseded rounded-tail theory;
E-NEMA-LISTS-001; synthetic regressions and bounded native-stream observations.
The later research explicitly corrects the older first-frame list notes:
CMDSIZE counts 32-bit entries, not bytes. The apparent unmatched tails came
from decoding one quarter of a list; 902 entries occupy 3,608 bytes and 1,954
occupy 7,816 bytes. This is not evidence for fetching an undeclared value.
Supported child syntax consists of complete register/value pairs, including
held writes. Odd entry counts must refuse, not drop or extend the last record.
Confidence: high for the supported paired-list grammar; no claim about unknown
hardware tail opcodes. Affected modules: framing and backend preparation.

Before the fix, three new synthetic regressions fail: the backend accepts a
red-color write followed by an unmatched draw register and leaks the color;
the GPU accepts a later odd child and consumes its stop/marker; framing emits
callbacks for the incomplete child. Tests exercise ordinary and held tails,
unchanged inherited color/pixels/generation, zero callbacks/completions on
refusal and successful corrected retries. Validation uses
`make test TEST_FILTER=nema_backend_atomic`, `nema_gpu_atomic`, and
`nema_refusal` (the latter contains the framing regression).

An external read-only submission observer on the unchanged baseline finds no
odd child counts during bounded 2.22 startup and 2.39 middle-button execution.
Both versions submit an inline initialization prefix; its semantics and
strict inline/padding validation remain separate unresolved work. Application
SHA-256 pins are those in E-EMU-NEMA-CONTROL-001. No firmware bytes, pixels,
compatibility rule, snapshot encoding or success pin changes are authorized.
Final verification and external observer provenance are recorded in ticket 761.

### E-EMU-NEMA-CONTROL-001

2026-09-06; ticket 761 control-framing follow-up on commit `1733bb8`.
Sources: E-NEMA-RING-001's pinned bootstrap/wrap capture and read-only
disassembly of the exact Sapporo 2.22 application, plus a first-refusal probe
of the validated Sapporo 2.39 application (manifest/component pins unchanged).
The bootstrap's held CMDADDR targets the ring base, but it is not the only
native held-control form. 2.22 builder `0x000bb4e8` writes CLID/INTERRUPT,
pads the write index to a multiple of four, and at `0x000bb5e2..0x000bb624`
writes held CMDADDR with `ring_base + 4 * (index + 4)`, held CMDSIZE and the
configured byte capacity. It then advances the index and writes CMDRINGSTOP.
Thus an inline held continuation targets the word immediately after itself.
The exact 2.39 image has the matching stores at `0x000cbc62..0x000cbca4`.
Application SHA-256 values: 2.22
`c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`,
2.39 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.

The exact 2.39 diagnostic observes base `10143678`, capacity `400`, previous
stop `10143768`, raw stop `1014378c`: a completion marker followed by a held
control at `10143778` targeting `10143788`, with held CMDSIZE and capacity
`400`. A draft validator that allowed only the bootstrap base target refused
this valid native continuation and broke both firmware gates. That draft is
not accepted; no golden is changed. Confidence is high for the two evidenced
forms (base wrap and immediate linear continuation), not arbitrary held jumps.
Affected modules: framing parser, GPU strict access checks and synthetic
framing/GPU/allocator regressions. Unknown inline syntax and unmatched tails
remain a separate audit. Validation results and hashes follow below.

Four narrow regressions fail before their fixes: incorrect non-power-of-two
wrap span, unvalidated control fields, overflowing ring ranges, and successful
unsupported GPU access widths. Final production changes check whole-ring
address arithmetic, use subtraction without unsigned-wrap dependence, validate
all held/marker fields before callbacks, preserve both evidenced held targets,
and refuse byte/halfword GPU access without changing read output or state.
A corrected same-stop retry after malformed control executes exactly once.

The integration test directly compiles the production framing,
backend-transaction and scheduler-batch translation units with only allocation
calls replaced. There is no runtime allocator hook or alternate backend API.
Three deterministic runs fail allocation of parser records, per-child frames,
and scheduler growth respectively, through the real MMIO path. Fifteen existing
events, queue bytes/IDs/sequences/time, the complete GPU codec, pixels/generation
and inherited blue color remain unchanged. Retrying two children/two markers
publishes both frames, preserves generations and completes IDs 7/8 in order;
repeated stops are no-ops. ASan/UBSan runs cover these same tests.

Exact final commands all pass, with actual test selection:

```sh
make test TEST_FILTER=nema_backend_atomic       # 7
make test TEST_FILTER=nema_gpu_atomic           # 8
make test TEST_FILTER=nema_completion_atomic    # 5
make test TEST_FILTER=scheduler_batch           # 4
make test TEST_FILTER=nema_refusal              # 5
make test TEST_FILTER=nema_framing              # 15 (includes integration cases)
make test TEST_FILTER=nema                      # 92
make test TEST_FILTER=transcript                # 91
make test TEST_FILTER=machine_snapshot          # 4
make check-task-contracts                      # 130 tickets
make check-lines                              # no hard-limit violations
make check                                    # 809 tests
make sanitize                                 # 809 tests
make sdl
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_gps_awake
```

External logs/artifacts are in `/tmp/semu-761-framing.owN7uU`. The first-refusal
observer `diagnostic.c` has SHA-256
`cf8b4f998ecf0701ea72435fa011021c8ab7749916ba2ef4deeaf00b70ecbb35`.
It instruments GPU refusal only, stops at the first failed submission and
writes no guest/firmware state. With the rejected base-only draft, compiling
and running it as below exits 3 at the held continuation described above.
The final renderer observer and included UI observer are unchanged from
E-EMU-NEMA-GPU-001, with SHA-256 respectively
`b59e73c8220beaade48cee629a1da3b5f8b93b8b47a1cc6cd0be1a2e4ee32ab7` and
`6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`.

```sh
probe_dir=/tmp/semu-761-framing.owN7uU
arm-none-eabi-objdump -D -b binary -m arm -M force-thumb --adjust-vma=0x40000 --start-address=0xbb4e8 --stop-address=0xbb678 tests/private/sapporo-2.22.60/application.raw
arm-none-eabi-objdump -D -b binary -m arm -M force-thumb --adjust-vma=0x40000 --start-address=0xcbc62 --stop-address=0xcbcf0 tests/private/sapporo-2.39.20.22297/application.raw
# The diagnostic compilation/run was performed with the rejected draft.
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices "$probe_dir/diagnostic.c" build/libsemu.a -o "$probe_dir/diagnostic"
"$probe_dir/diagnostic" cold 1 "$probe_dir/diagnostic" > "$probe_dir/diagnostic.trace" 2> "$probe_dir/diagnostic.log"
# Final implementation; all three bounded renderer runs succeed.
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-761-gpu.aQow5X/render-probe.c build/libsemu.a -o "$probe_dir/render-probe-final"
"$probe_dir/render-probe-final" cold 1 "$probe_dir/final-middle-a" > "$probe_dir/final-middle-a.trace" 2> "$probe_dir/final-middle-a.log"
"$probe_dir/render-probe-final" cold 1 "$probe_dir/final-middle-b" > "$probe_dir/final-middle-b.trace" 2> "$probe_dir/final-middle-b.log"
"$probe_dir/render-probe-final" /tmp/semu-239-ui.690H4Q/cold-a.prefix.sems 1 "$probe_dir/final-middle-resume" > "$probe_dir/final-middle-resume.trace" 2> "$probe_dir/final-middle-resume.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc /tmp/semu-761-gpu.aQow5X/atomic-probe.c build/libsemu.a -o "$probe_dir/atomic-probe"
"$probe_dir/atomic-probe" > "$probe_dir/atomic-probe.log"
"$probe_dir/atomic-probe" > "$probe_dir/atomic-probe-repeat.log"
cmp "$probe_dir/atomic-probe.log" "$probe_dir/atomic-probe-repeat.log"
cc -std=c99 -Wall -Wextra -Werror -pedantic -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude -Isrc /tmp/semu-761-gpu.aQow5X/atomic-probe.c build/sanitize/libsemu.a -o "$probe_dir/atomic-probe-san"
"$probe_dir/atomic-probe-san" > "$probe_dir/atomic-probe-san.log"
cmp "$probe_dir/atomic-probe.log" "$probe_dir/atomic-probe-san.log"
```

All private components are validated before execution; full-flash SHA-256
remains `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Both private gates retain their exact previous pins. The short 2.22 gate stops
at `user / 000bacf4 / 774081920 instructions / 6520939902 ns`; all four frame
CRCs/generations and cold-log hash in E-SAP-ONBOARD-EMU-012 pass. The 2.39 gate
retains all four native pulse/snapshot phases and fifth-hit refusal:
`compat-refused / 001291cc / 1272353867 instructions / 32770943068 ns`,
log SHA-256 `06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`,
snapshot SHA-256 `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.

The corrected middle runs retain all E-EMU-SAMPLING-CLIP-001 values:

- Both cold traces: `a65336ef681dde147b4ed5bd6d777fd353b46e86a0000361028db53ebf5c9d46`.
- All final RGB565 images: `f66dc6d3f1c20bd937c0f166b13e01450949103cb733ac50974e0adf87e69b03`.
- All final snapshots: `4cf21fba0d84778dadc8de6706bda2e57b98ada9f3cabab58845805a708723c5`.
- Last changed cold frame: generation 79, CRC `6b6aa2dc`, instruction
  1088274630 at 12335112985 ns.
- Endpoint: 1300000000 instructions, 22286110403 ns, PC `000a7abc`,
  GPS hits `2,2,3`; cold frames/changes 79/62, resume 76/60.

The original five-scenario probe remains byte-identical over two normal and
one sanitizer runs, reporting zero atomicity failures. New code/tests stay
below 300 lines; no firmware bytes, generated pixels, goldens, profiles,
registry, CPU/bus policy or persistent format changes. Remaining gaps are
strict inline/padding and unmatched-tail interpretation plus final acceptance
review. No extra integration authority is requested; ticket 761 is incomplete.

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

### E-SAP-COMPAT-GENERAL-239-001

2026-09-08; ticket 762 evidence collection on committed runtime `2d230a2`
(planning handoff `6ec6572`). The unchanged production E-SAP-UI-239-002
observer was rerun twice: both traces retain SHA-256
`c7851cdb89e7998a2e94eedd6a7e5e815d3e45f359553f8dd577dc9e5b2d6a34`,
all five saved checkpoint pairs match, and final inspection again identifies
`settings/general`, mode two, PC `0x000920b4`, LR `0x000ad079` at
1,376,488,437 instructions / 14,401,737,146 ns. Production still refuses
before the open succeeds; no production source or limit changes in this work.

An isolated external translation unit changes only its logical-file ceiling
from 76,279 to **76,791**, and aggregate from 76,282 to **76,794** (512
diagnostic operations, not a proposed production allowance). It supplies the
same descriptor symbol to the otherwise unchanged library at static link time;
there is no private-state mutation, counter replenishment or parallel API.
A forwarding wrapper records the existing file adapter's arguments, cursor,
status, return PC/value, hit ordinals and payload hashes without changing its
results. A normal-backend wrapper counts submissions/refusals unchanged.
All four layers, native input, CPU execution, file ABI/capacities and GPS
limits remain intact. Every run validates all three E-SAP-0011 components and
the E-SAP-UI-239-002 full-flash hash before loading a snapshot or executing.

The complete native suffix is **92 operations**: one mode-two open, 90 writes
totalling 1,505 bytes, then close. Handle `0x10161200` starts at offset zero;
each write returns its full requested length and advances contiguously. The
file was already 1,505 bytes and does not grow. No seek, truncate, flush or
read intervenes. The ordered write sizes are:

```text
9,11,19,19,11,17,19,15,16,21,25,16,24,24,24,22,19,20,23,17,19,13,20,15,
15,2,16,2,16,2,16,2,13,2,18,2,16,2,14,2,20,18,28,22,18,20,20,20,20,
20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,20,19,14,16,
17,18,18,22,21,16,15,14,16,16,11,14,15,19,14,14,15,17
```

The final write consumes logical ordinal 76,370; close consumes 76,371 and
returns one at 14,401,853,366 ns. Resulting file SHA-256:
`df219aee240f50708ecc9bd603ea77c965f927b45bc9a4859731cc22c7cb26d4`.
At `0x000ad086`, instruction 1,376,604,655 / 14,401,853,364 ns, the native
serializer has returned one; object `0x10035408` still has pending byte one
at `+0xf4`. At `0x000ad092`, instruction 1,376,604,659 /
14,401,853,368 ns, native code has closed the file and cleared that byte.
No successful adapter hit occurs afterward before the next stop. Native
read-only filesystem calls still pass through and are separately visible in
the observer; they are not counted as logical-file interventions.

Pristine application analysis covers save wrapper `0x000ad064..0x000ad092`,
the entire serializer `0x000d5794..0x000d5bcc`, and helpers at
`0x000af55c`, `0x000af6e0..0x000af76c`, `0x000af806`,
`0x000af8a8..0x000af914`, `0x000af9bc..0x000af9e0`,
`0x000afa30..0x000afa60`, `0x000afac8..0x000afada`, and
`0x000afb10..0x000afb26`. Scalar helpers format bounded records and compare
write return values with their requested lengths. The eight empty variable
records take the two-write header/CRLF branch at `0x000af724..0x000af748`.
The serializer checks the boolean results, including its six-by-four field
loop at `0x000d5a1e..0x000d5a5c`; its observed final return is success.
The 90 writes are dynamically measured, not inferred by counting static calls.
No ABI correction is indicated by this successful path.

Write-call LR distribution corroborates the native helper branches:
`0x000af583` x14, `0x000af733` x8, `0x000af743` x8,
`0x000af813` x5, `0x000af8f9` x26, `0x000af9d7` x25,
`0x000afa4b` x1 and `0x000afb1d` x3.

Two diagnostic runs from the production refusal and two runs from the original
700-million-instruction pre-screen prefix reach the same next refusal:
PC `0x001291cc`, **2,363,623,546 instructions / 32,619,070,564 ns**,
`2.39 GPS awake lifecycle or hit budget refused`, GPS hits `2,2,4`.
This is the existing intentional fifth-awake refusal, not a new GPS contract.
All runs have absolute bounds of five billion instructions and 35 billion
virtual ns; changed-frame trace capacity is 5,000. An earlier two-billion
instruction diagnostic ended normally at its instruction budget and is not
the acceptance endpoint.

The prefix runs reproduce E-SAP-UI-239-002's actual four input edges and
publish 676 frames / 473 consecutive CRC changes, all 676 submissions accepted.
The later-start and mid-save runs publish 528 frames / 359 changes, all 528
accepted. The last changed prefix frame is generation 673 at instruction
2,064,298,519 / 19,042,711,247 ns; the last callback is at 19,059,727,592 ns.
Final CRC `405d1af6`, RGB565 SHA-256
`6eb15b72ea2d250b1106d6a89c39ac87eb3827ebd1ce7c367bf1efcfeb2b4742`.
Visual inspection reads **Define your profile**, with a cyan person icon and
right arrow. This is a native renderer milestone, not a watch face, completed
setup, physical-panel equivalence or a new production golden. Starting with
an empty frontend surface mid-animation produces different early partial
frames, but all runs converge to these exact final pixels and machine state.

The mid-save snapshot is after logical ordinal 76,320, instruction
1,376,525,552 / 14,401,774,261 ns / PC `0x000af744`. Its resume retains
all later operation arguments/returns, exact log suffix and final pixels/state.
Pairwise SHA-256 pins:

| Artifact | SHA-256 |
| --- | --- |
| Prefix-run trace (`full-a`, `full-b`) | `a2739edaaa06b8ef81f7110c5eac1353be044886affbf442aa8ffe2e5a3d819f` |
| Refusal-start trace (`long-a`, `long-b`) | `d9ad3c2b7e55c7a2be7de058878ec9cedc00d44c3f50eeb6c30e0e7c9bd9589f` |
| Mid-save resume trace | `223a734b3f173351b9cb424d3811194a76d552210089f33d35954e8c93aa8b7a` |
| Mid-save snapshot (all captures) | `76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66` |
| Final snapshot (all five runs) | `613712b78fd1ba748517e710d70280300bee0a48ee43133c9fd9f8070f5442f8` |
| Prefix-run log | `47979b14ad148e366fce73b64a5589c7793ae58a58645f02f2bdaf761ae3458b` |
| Refusal-start log | `9559f2aec824099c30a8d399c1e943014abfc9c1218726e9a8a9d50e03ab3aa6` |
| Mid-save resume log / matching suffix | `9dc4cff27c7a2a3d8499af7460e830d2dc9cc8615c5d50290abc56422b95eb3f` |

External directory `/tmp/semu-762-save.YOUCY2/` contains all private artifacts.
Source SHA-256 pins:

| Source | SHA-256 |
| --- | --- |
| `sapporo_239_diag.c` | `548d5a7a67451033e40ba6a468d762434dac01ea4589f55814fc297195d16c87` |
| `file-observer.c` | `10c3c1684343f245bf47cc27b67f34b16199705b3ed43897d7637b3593df77af` |
| `save-probe.c` (35-second / five-billion final version) | `6bb343bb392c52a366f683f21c72f516ae13b8ecb001f9a234b344c97a2b94f7` |
| `full-probe.c` | `bd9175bcfb24ff1968d74ac9876b3ade8e3f0d5de16c2d64f624f142dc2a1d18` |
| `ui-probe-long.c` | `140c33e9d0754f58edc06aa77758bb56fe1dfb0c009bfae0f10fbcc5a188c5a4` |

The last file changes only the earlier E-SAP-UI-239-002 observer's frame trace
capacity. `file-observer.c` includes the unchanged in-tree adapter source.
The library hash remains E-SAP-UI-239-002's
`9327c56df85ace7f21814087ce634d2898edb55b2836f7524f84f02af1619776`.
Reproduction from the repository root, with the external sources available:

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc \
  -Isrc/compat -Isrc/devices /tmp/semu-762-save.YOUCY2/full-probe.c \
  /tmp/semu-762-save.YOUCY2/file-observer.c \
  /tmp/semu-762-save.YOUCY2/sapporo_239_diag.c build/libsemu.a \
  -o /tmp/semu-762-save.YOUCY2/full-probe
```

Run that executable with output prefixes `/tmp/semu-762-save.YOUCY2/full-a`
and `full-b`, redirecting stdout to each `.trace` and stderr to each `.log`.
For the refusal-start executable, replace `full-probe.c` by `save-probe.c`
and output executable by `save-probe-long`; run with prefixes `long-a` and
`long-b`. Supply `/tmp/semu-762-save.YOUCY2/long-a.mid.sems` as its second
argument for a mid-save resume. Each observer exits zero after capturing its
explicit END record; zero does not mean onboarding completed. Final snapshots
and RGB565 pixels compare byte-for-byte. Firmware disassembly uses
`arm-none-eabi-objdump -D -b binary -m arm -M force-thumb --adjust-vma=0x40000`
with the exact ranges above on the pristine application component.

Confidence is high for the 92-operation suffix and the exact proposed
**76,371 logical / 76,374 aggregate** limit. Production integration still
requires a regression and review; the diagnostic 512-operation allowance must
not enter production. Other file paths/modes/capacities, subsequent profile
choices, the fifth GPS pulse, watch-face activation and menu navigation are
not authorized or claimed by this evidence.

#### Production integration (ticket 763), 2026-09-08

The accepted 762 evidence is integrated using precisely 76,371 logical-file
hits and 76,374 aggregate hits. Runtime diff: only the two constants and
their comment in `src/compat/sapporo_239.c`. The new synthetic general-budget
regression fails at the first post-76,279 open before this change and passes
after it. It checks all 90 write sizes and synthetic payloads, mid-write file
snapshot restore, exact final close and atomic excess/unknown-mode/path refusal.
Historical activity/history sequences remain intact; only their final-limit
assertions and separate saturation steps are updated.

The private-only `sapporo_239_general_probe.c` uses the production library,
normal NEMA backend and existing machine/snapshot/input APIs. It generates the
original prefix, advances to the four evidenced actual instruction/time edges,
captures the exact mid-save image and requires the previously pinned terminal
PC/instructions/time/frame. No diagnostic descriptor, file-hook replacement,
opaque-state mutation or direct native callback is linked. The shell gate
compares two prefix continuations and a mid-save resume, 92 operations/90
writes, exact logs, snapshots and pixels, and wrong manifest/flash rejection.
Its first draft correctly rejected its malformed negative fixture's absolute
component paths, but the test expected a hash-related diagnostic. The fixture
now uses safe relative paths through a temporary read-only source-directory
symlink, and requires the exact application/profile mismatch diagnostic.
No emulator behavior or expected successful checkpoint was changed for that
test-harness correction.

Production library SHA-256
`d5be57c61d475e2b3dc10c17d4bdfc8f257dae86fbfc8895ef277a1bb7db12a9`.
The retained production observer run in `/tmp/semu-763.WR6heT/first.*` matches
the established mid/final snapshot hashes and full log
`47979b14ad148e366fce73b64a5589c7793ae58a58645f02f2bdaf761ae3458b`.
All 676 renderer frames succeed; final CRC `405d1af6` and pixel SHA-256
remain unchanged. Ticket 763 records exact verification commands and results.

### E-SAP-COMPAT-PERSONAL-239-001

2026-09-08; ticket 764, committed production baseline `3afb5ff`. Separate
planning review accepts ticket 763; no runtime source changes in this evidence
work. Two newly compiled production runs of the E-SAP-UI-PERSONAL-239-001
observer reproduce its trace, final snapshot and pixels exactly, including
the refused personal open at 2,953,605,137 instructions / 24,380,651,994 ns.
Normal renderer and all four explicit layers are retained throughout.

An external translation unit substitutes only the descriptor ceilings at link
time: **76,883 logical / 76,886 aggregate**, 512 extra diagnostic operations.
This is not a production proposal. A forwarding adapter observer records
arguments, returns, cursor and hashes without modifying their values. No
private-state mutation, counter replenishment, firmware patch or callback
invocation occurs. Every run validates all three firmware components and full
flash before execution. Bounds: five billion instructions, 35 billion virtual
ns, 5,000 changed frames. Production remains **76,371 / 76,374**.

The first personal save is **68 operations**: mode-two open, **66 successful
contiguous writes totalling 1,727 bytes**, close. Handle `0x10161300` starts
at zero; the existing file size stays 1,727. No read, seek, flush or truncate
intervenes. Ordered write sizes (metadata only):

```text
10,2,10,16,17,17,17,19,11,10,17,20,25,21,25,21,25,24,22,23,22,
32,32,32,32,28,28,28,28,28,28,28,28,34,34,34,34,30,30,30,30,
30,30,30,30,34,34,34,34,30,30,30,30,30,30,30,30,29,25,25,31,
27,27,31,27,27
```

Every write returns its requested size; final write consumes ordinal 76,438.
Close returns one at ordinal **76,439**, virtual ns 24,380,809,738.
File SHA-256 `a7bcb15b0615a8c530b22b3b735cf6bea34827a316f6a47ace2cb39896034180`.
At PC `0x000adb3c`, instruction 2,953,762,879 / 24,380,809,736 ns, the native
serializer returned one and object `0x10035500` has pending byte one at
`+0x145`. At `0x000adb48`, instruction 2,953,762,883 / 24,380,809,740 ns,
the native close and flag clear have completed. A later invocation on a
different object with its flag already zero is a no-op, not a second save.

Pristine wrapper `0x000adb1e..0x000adb48` opens only when pending, skips a
null handle, invokes `0x000d5ff8`, closes and clears the flag. The wrapper
does not test the serializer result, so flag clear alone would not prove
success. The entire serializer `0x000d5ff8..0x000d6248` independently checks
helper booleans and returns their accumulated result. Its loops serialize
three-by-three-by-four scalar fields and three-by-three boolean fields.
The initial empty variable record uses the two-write header/CRLF branch at
`0x000af724..0x000af750`. Helpers `0x000af55c..0x000af58e`,
`0x000af6e0..0x000af770`, `0x000af806..0x000af81e`,
`0x000af8a8..0x000af918`, `0x000afa30..0x000afa62`,
`0x000afac8..0x000afadc` and `0x000afb10..0x000afb28` were read before
interpreting their full-length comparisons. Observed write LR counts:
`af583` x10, `af733` x1, `af743` x1, `af813` x48, `af8f9` x3,
`afa4b` x1, `afb1d` x2. No adapter ABI correction is indicated.

Two runs from the production refusal, two from the earlier general-save
midpoint (with the six E-SAP-UI-PERSONAL-239-001 edges), and a personal
mid-save resume converge to the same next stop with no further button input:

```text
compat-refused pc=001291cc instructions=3152721353 time=32538694863 gps_hits=2,2,4
```

This is the unchanged fifth GPS-awake refusal. Earlier-start runs accept all
778 renderer submissions; refusal-start/resume runs accept six. All converge
to BIRTH YEAR, CRC `568bdc7d`, pixels SHA-256
`d2c4833a433610b5087f6e04fe16c7c4bd9d3baf6573df21cc72e0abde77b09b`.
The midpoint is after ordinal 76,405, instruction 2,953,666,398 /
24,380,713,255 ns, PC `0x000af814`, after 33 of 66 writes. Resume preserves
the remaining operations, exact log suffix, final pixels and machine image.

| Artifact | SHA-256 |
| --- | --- |
| Refusal-start trace pair | `10e01c8cd11e016e3e34c13c63f0c7c14b42dfb8837f1fa69501e7b5b651efb4` |
| Earlier-start trace pair | `37187c7cd2870379a4c97c62130e9abff4426d724653f37beb3688c89ff221c5` |
| Mid-save resume trace | `40d2cf595245b2a01bc3c9e15d5dbee5f0c76402aa6635ea480b466a0af16c2a` |
| Mid-save snapshot | `68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8` |
| Final snapshot (all five runs) | `4faf5b8934c80cbadc33a7d6a389dd8f50a26bacdf2ed7208effd7a4abadb3e3` |
| Refusal-start log pair | `62eebc2edfde72ac0537692975cc9fc9490d7507433b0464ef818782411b9782` |
| Earlier-start log pair | `509437ffa701685958420794fdf70d24ef4704b2869c0f49fb3fc09f0130dfcf` |
| Resume log / exact suffix | `f6d6c1bab95a4150129d65d917ddbeade37bd7b0647354ba6877d68ad2013bd3` |

All sources/artifacts remain in `/tmp/semu-764-personal.rFx4hQ/`. Source pins:
`sapporo_239_diag.c` SHA-256
`bb487188910069998074a583126bf6b18148580f6c0317442f2ac8d1d498ef10`,
`file-observer.c` `69c45be2dd01da1f7ec50a9584a1518f2c1319a367bde772a85872d2f2bff22b`,
`personal-probe.c` `f39a817fda5fc710811333c1c789077c04e97c0c461af3ca6a9371100a36d007`.
The latter includes the unchanged `ui-probe-long.c` pinned under the general
save evidence. Library remains
`d5be57c61d475e2b3dc10c17d4bdfc8f257dae86fbfc8895ef277a1bb7db12a9`;
full flash remains
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Exact compile/run commands and further navigation findings follow in ticket
764's handoff. No firmware bytes, settings payloads or frame pixels enter Git.

#### Native continuation to weight and the next file boundary

Three further native MIDDLE presses at requested 26, 28 and 30 seconds expose
a more useful integration endpoint before the GPS ceiling. Two complete
earlier-start runs reproduce all twelve input edges; two continuations from
the personal-save midpoint reproduce their exact remaining log and final
snapshot/pixels. Additional actual edges are:

| Edge | Instructions | Virtual ns |
| --- | ---: | ---: |
| Press 4 | 3016651598 | 26010661264 |
| Release 4 | 3023149894 | 26096218953 |
| Press 5 | 3326045273 | 28022050108 |
| Release 5 | 3332543381 | 28107577092 |
| Press 6 | 3637404457 | 30036360075 |
| Release 6 | 3640223889 | 30116594734 |

Native button events remain 2/5/1 at `0x0010ace2`. View-open tokens are
`4f56c51f`, `b3ffea5a`, `9fdbd95d`; only the final pixels are visually
identified here: **WEIGHT**, 70 kg centred in the native selector. This is a
firmware default, not a supplied user measurement. No direct view calls,
host-drawn UI, fake time or extra GPS fixture are involved.

The whole post-76,371 suffix contains exactly **228 operations**:

- First personal save: 68 operations, ordinals 76,372–76,439, detailed above.
- Second personal save: the same 68 operations/sizes, ending at 76,507;
  native serializer returns one at PC `0x000adb3c`, instruction 3,268,066,472 /
  26,342,736,425 ns; pending clears four instructions later. File hash is
  unchanged from the first personal save.
- One general save: the established 92-operation sequence from
  E-SAP-COMPAT-GENERAL-239-001, ending at 76,599. All 90 write sizes and
  final 1,505-byte hash match that evidence; close returns one.

All 222 writes return their full size and advance contiguously, 4,959 total
bytes across three saves. There are no extra admitted operations before:

```text
compat-refused pc=000920b4 lr=000acb8b instructions=3885178598 time=30368914377
r0=000ace70 r1=00000002 r2=10034da0 path=settings/time gps_hits=2,2,4
error=unknown Sapporo 2.39 writable file path
```

This is an unknown-path refusal with diagnostic budget still available, not
another hit-limit failure. Pristine wrapper `0x000acb78..0x000acba4` tests
object byte `+0x12`, opens the path in mode two, calls serializer `0x000d5084`,
closes and clears the flag. Its schema, capacity and complete operation
sequence are not measured; this observation authorizes neither a time file
nor a fabricated clock source. It does not prove that the weight selection
was accepted or that setup is complete.

Complete earlier-start runs accept 1,004 submissions, no renderer refusals;
mid-save continuations accept 232. Final frame CRC `a8c9f3d3`, SHA-256
`3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585`.
Last frame time is 30,366,784,927 ns. Repeat pins:

| Artifact | SHA-256 |
| --- | --- |
| Complete continuation trace pair | `59a9c0b4079ebd6758f66788a2d15e1e7b4dc3e11379111ebafafacc55fba87a` |
| Complete continuation log pair | `21a566167b8e34a1bf36e25feca4f1e337e8ca1f4d858822fe8e24661d3de139` |
| Mid-save continuation trace pair | `2484f1751dddfed44e45ecba3238e4a74869eb7cf2e7819e5cce80e84296e002` |
| Mid-save log pair / exact full-log suffix | `f287fc1e9276ea1540596234b8b65ae4d560fdda493ae7865bd9c3d928ae54bf` |
| Final snapshot (all four runs) | `3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6` |

Sources `total-probe.c` SHA-256
`86a11d4c0a46ba515e78d48ee3b5a7ee616fe5e9af394016c69f3349aa3a1482`
and `next-probe.c`
`6a3fc194494f0a6bd7d9ff87cc166da7e9e81b54787d3840705ebc286a7b9f65`
use the same diagnostic descriptor/adapter/library and bounds. The only
stimulus difference is six MIDDLE presses versus the first three; the latter
source starts mid-save and supplies only the remaining three presses.

The proposed separate production integration is therefore **76,599 logical /
76,602 aggregate** (228 measured additional operations), not 512 diagnostic
operations or an unbounded file budget. A 68-operation-only integration would
stop again at the next already-measured personal save. Keep the shorter idle
branch's GPS refusal and the full branch's unknown-time-file refusal as
distinct required gates. Watch-face activation, post-setup menus, later
personal values/sizes and physical-panel equivalence remain unproven.

Ticket 766 production verification (2026-09-08): the only runtime delta is
the exact 76,599 / 76,602 ceiling plus its evidence comment. The new public-API
observer uses the normal library/backend and four explicit layers, without
a diagnostic descriptor. It cold-generates the historical general midpoint
`76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66`,
then verifies repeated full/idle branches and personal-midpoint restoration.
All final logs, snapshots and pixels match the preceding evidence exactly.
The full prefix logs contain 279 file operations (51 remaining prior-general
operations plus 228 new ones), ending at ordinal 76,599. A one-instruction /
one-nanosecond retry of either refusal preserves the entire machine snapshot.
Wrong firmware metadata, full flash and starting checkpoint negatives pass;
the source flash hash remains unchanged. Production library SHA-256 is
`7a0cb81857760b224d4511ea2e1c49392c2591ee1df34166110a15ce1f4d3e05`.
All 831 normal and sanitizer cases, 46 focused Sapporo 2.39 cases, four
machine-snapshot cases, the personal/general/activity/GPS private gates and
2.22.60 SDL live-input gate pass. No later path or GPS behavior is admitted.

### E-SAP-COMPAT-PERSONAL-SUFFIX-239-001

2026-09-09; ticket 772 against accepted five-pulse integration (tickets
769/771, commits `0826523` and `29d74db`). No production source changes in
this evidence work. An external probe (`probe.c` SHA-256
`e8157f288a4654c7ea9d9b75540d155c038096a2dc7dbd8e43d068b28a1daa8b`) reuses the
committed four-layer set `sapporo-2.39-synthetic-wbsto`, `gps-startup`,
`gps-reopen` and `gps-awake-five`, the normal NEMA backend and the unchanged
file adapter, linked against a separately compiled descriptor copy
`sapporo_239_diag.c` (`31e2c1ed652e312ec11da351ce979494fa67beeef2d19672d0abf09f91074b27`)
that substitutes only the file ceilings with **77,111 logical / 77,114
aggregate** (production plus 512 diagnostic operations). This is not a
production proposal. Bounds: six billion instructions, 45 billion virtual ns,
5,000 changed frames. Every run validates the manifest, three components and
the full flash `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`
before execution and starts from the cold-regenerated five-identity prefix
`6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de`.
Production remains **76,599 / 76,602**.

Under the diagnostic ceiling the mode-two `settings/personal` open that
production refuses at 4,232,903,136 / 34,413,596,174 ns begins identically
and completes as **68 accepted operations**: mode-two open handle
`0x10161600`, **66 successful contiguous writes totalling 1,727 bytes** with
the exact ordered write-size sequence of the first birth-year save in
E-SAP-COMPAT-PERSONAL-239-001, and close returning one at ordinal 76,667.
The wbsto aggregate moves 76,602 → 76,670; GPS startup/reopen/awake hits
remain 2, 2 and 5. No read, seek, flush or truncate intervenes. One-step
observation of the pristine wrapper repeats the 764 shape: the serializer
returned one at PC `0x000adb3c`, instruction 4,233,060,376 / 34,413,753,414
ns, object `0x10035500` pending byte `+0x145` still one; close and flag
clear complete at PC `0x000adb48`, instruction 4,233,060,380 /
34,413,753,418 ns, pending byte zero. The wrapper still does not test the
serializer result, so adapter `result=` evidence remains required.

The save is UI-invisible at this boundary. The screen stays on HEIGHT; the
last accepted frame is unchanged at CRC `cd4c0a99`, SHA-256
`33339448cbcfafa47bd9d0ed4e37b61abd43062acf95f2bf7470ebefae921072`
(75 accepted submissions). Execution continues natively to the next
independent boundary, which is the sixth GPS-awake admission refusal, not a
file refusal:

```text
compat-refused pc=001291cc lr=000d3c35 instructions=4345171340 time=37899807613
r0=00000001 r1=00000005 r2=0000000c gps_hits=2,2,5
error=2.39 GPS awake lifecycle or hit budget refused
```

No file operation, save or renderer refusal occurs between the close above
and this refusal; the 444 remaining diagnostic operations were never
approached, so the file layer is not the binding limit anywhere through the
HEIGHT save. The accepted 771 idle control tuple `001291cc / 4071207676 /
37929735196` is unchanged under the diagnostic ceiling, proving no GPS
lifecycle perturbation.

Protocol: two clean repeat runs share one byte-identical trace and adapter
log; a third run with one-step save observation differs from them only by
probe output lines; all three plus both resumes share one final snapshot.
The mid-save capture is after the open and 33 of 66 writes (through ordinal 76,633),
PC `0x000af814`, instruction 4,232,964,397 / 34,413,657,435 ns, snapshot
`e343e340897a72dceb71ea9c8fbbb319e65eb7463141f3f944e6c9fc7161b7a3`. Mid-save
resume reproduces the remaining operations, both SAVE tuples, the exact
refusal tuple and the final and refused snapshots; a refusal-start repeat
re-emits the identical refusal with the snapshot unchanged (atomic: the
refused image equals the final image).

| Artifact | SHA-256 |
| --- | --- |
| Clean repeat trace pair | `76c0ffc1ff0bd31e9325e468d446f8dc8b9bd15b42de3027e4fd08f73dff38b7` |
| Clean repeat adapter log pair | `a79b6362bfb2299d23087ef60b1e9df7269e053488c427471a4faaa7f138f65b` |
| Instrumented trace (mid-save and save tuples) | `46993fb7a092767899ad85abb131357330587c7431d0b96406636e604e94a9a4` |
| Mid-save snapshot | `e343e340897a72dceb71ea9c8fbbb319e65eb7463141f3f944e6c9fc7161b7a3` |
| Mid-save resume trace | `9447f5d92169bfc5ca6038c166bada12b9ad597910afa555e4df61ed43511ac1` |
| Refusal-start trace | `6e259d035bb48e9cc472af6506e62a907e67281135f29f297a519057d8effad8` |
| Idle control trace | `76f3cbb825af28a2be93de80a32124b5ddd482b9c024e1b21b092d8df960d807` |
| Final snapshot (all five continuing runs) | `41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa` |

The unchanged adapter ABI carried the whole save; no ABI correction is
indicated. The proposed separate production integration is therefore
**76,667 logical / 76,670 aggregate** — the measured 68 additional
operations exactly — not the 512 diagnostic operations or an unbounded
budget. Applying it, naming its gates and tests belongs to a separate
integration ticket. Whether the HEIGHT selection itself was accepted, and
which input follows the completed save, remain unmeasured: the sixth-admission
refusal bounds any continuation at 37,899,807,613 ns. No firmware bytes,
settings payloads or frame pixels enter Git; all artifacts, both binaries and
the prefix snapshot remain under `/tmp/semu-772-suffix/`.

**Production integration (ticket 773, 2026-09-09).** The proposal was
applied exactly: `src/compat/sapporo_239.c` now carries 76,667 logical /
76,670 aggregate. Under the production ceilings the five-pulse MIDDLE
branch completed the HEIGHT save and refused the sixth GPS admission at the
measured tuple `001291cc / 4,345,171,340 / 37,899,807,613` ns with 75
frames, HEIGHT CRC `cd4c0a99` and unchanged final-frame SHA `33339448…`,
and its final snapshot is byte-identical to this entry's diagnostic final
image `41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`;
the repinned gate also records the name-normalized companion
`65255eb1abe56f8f3ff82e1320dfce40c7671a52e446f15b3cdced37768a83d5` (proved
equal to the four-pulse-compatibly-named image by the existing normalization
rule). The cold prefix `6e670940…`, the idle-branch pins `d5244833…` and
`127214e5…`, every four-pulse checkpoint and the 2.22.60 gate are unchanged,
because none of those runs attempts an operation beyond ordinal 76,599.

### E-SAP-GPS-CONTINUATION-239-001

2026-09-09; ticket 774 against accepted integration ticket 773. No
production source changes. This entry measures what the firmware attempts
after the completed HEIGHT personal save when the five-pulse GPS-awake
ceilings are raised in a diagnostic-only build, and what the next boundary
then is. One separately compiled copy of `src/compat/sapporo_239_gps_awake.c`
(SHA-256 `3f3377c2d93ae75f2afaa38ab377e921b12388d4b1c84a15490ef2e451153cee`)
raises exactly two constants — the five-pulse intervention maximum and the
awake-five layer aggregate, 5 to 25 — and is linked ahead of
`build/libsemu.a` (run-time SHA-256
`25f1f6d51e213ee4a5d1fd5c764e3c3c240145df815263c23a84fa22e7acda16`) so the
awake registry, application and snapshot identities stay self-consistent.
File budgets stay at production 76,667 / 76,670; nothing else differs.

Because the sixth admission hook fires at instruction start, no snapshot can
straddle it; byte-identity was therefore proven at the last preceding
instruction with time-capped control runs of identical probe sources
(`b46eccf82e3d17dcc749af63c20e67cb781f03b2324d3cccb782a27a162efd92`,
`598de23c7d24aea78a6b2d8a8e1360ce12f66dfe7e1464883ec29926d2442642`) linked
against the production library and against the diagnostic copy. Idle stops
at 4,071,207,675 instructions / 37,929,735,195 ns, PC `0x001291ca`,
snapshot `e46aa7e6c57bb90491f84f74aa9cb10f2b2f7eeb1189938351e73acc5ea5b41e`;
the accepted-post-save branch stops at 4,345,171,339 / 37,899,807,612 ns,
PC `0x001291ca`, snapshot `6c3e261bc4bb76456ecdee1f506cb42ff7d30d7425cfe5cf09a4b785ce812ad2`;
both snapshots and the whole adapter log are byte-identical between the two
link variants, so the raise provably does not disturb the cold prefix
(`6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de`,
regenerated identically), pulses one through five, the 773 refusal tuples,
or any state before the first diagnostic grant.

With the sixth admission granted, the idle control logs the ordinal-6
`gps-awake-pulse` line and then performs no further wake, save or UI change:
at PC `0x000d2f6e` / LR `0xffffffed` / r0 `0x05fa0004` / r1 `0xe000ed0c` /
r3 `0x01000000` / xPSR `0x21000003` the firmware writes SCB AIRCR
VECTKEY|SYSRESETREQ — a genuine firmware system-reset request — at
4,536,286,836 instructions / 60,828,679,535 ns (`reset_count=1`,
`compat_hits=76602`). The emulator models the reset internally per
E-CPU-0006 (no host-side action); the post-reset machine then reaches
`SEMU_STOP_HALT` at PC `0x00079e1e`, 4,592,622,963 / 61,142,551,099 ns,
final snapshot `bd5004c0cabe0ce0857bd9167a15ea5a77c10c4a545aa13ad6301dde8d96f35b`.
The post-save branch is identical in structure: after the accepted save it
performs no further writable file operation (still 68 personal-save
operations, `compat_hits=76670` at reset), emits no new frame (frames stay
75, CRC `cd4c0a99`, SHA `33339448…`), requests the same reset at
4,875,799,883 / 60,779,430,750 ns, and halts at the same post-reset PC,
4,932,136,015 / 61,093,302,319 ns, final snapshot
`411e3551a95a33aa0fb8afbc756ed9b6f27fe94adf1e7959c83d096d0916c6c8`.
Repeat runs are byte-identical (trace pair
`04723096035b034741c0e381aca28187188e185e27301ff4543d689834e5a0aa`, log pair
`ed0f31b0e5837c58f7320ba752e5bd6233f26c0364914023922fca9325a9e637`, idle
trace `ac9f7eae3bb5df5075f063ceb1dfd76990908845bd787aa3c99e152ad636430f`,
idle log `0c36f724ca391b9c0b1073498e6cd17f9e95250908592290cff754b5bcd0adac`);
resuming from the post-grant sixth-admission snapshot
`2b5c43a4808462f2a3ad94ba3ac0eaae6716b7085cf2b4f416bccd6e52b61af2`
reproduces each final snapshot byte-for-byte.

| Artifact | SHA-256 |
| --- | --- |
| Diagnostic awake copy (5→25, two constants) | `3f3377c2d93ae75f2afaa38ab377e921b12388d4b1c84a15490ef2e451153cee` |
| Pre-admission idle control pair (both link variants) | `e46aa7e6c57bb90491f84f74aa9cb10f2b2f7eeb1189938351e73acc5ea5b41e` |
| Pre-admission post-save control pair (both link variants) | `6c3e261bc4bb76456ecdee1f506cb42ff7d30d7425cfe5cf09a4b785ce812ad2` |
| Diagnostic idle final | `bd5004c0cabe0ce0857bd9167a15ea5a77c10c4a545aa13ad6301dde8d96f35b` |
| Diagnostic post-save final | `411e3551a95a33aa0fb8afbc756ed9b6f27fe94adf1e7959c83d096d0916c6c8` |
| Post-grant sixth-admission snapshot (post-save) | `2b5c43a4808462f2a3ad94ba3ac0eaae6716b7085cf2b4f416bccd6e52b61af2` |

Limitations: the sixth and later wakes are diagnostic grants, not physical
observations; E-SAP-GPS-FIFTH-239-002 remains the physical bound of five
pulses and production keeps the unchanged sixth-admission refusal. What
this entry does establish is that if wake admission ever continued, the
2.39 firmware's own next actions would be: no second save and no UI change
after the HEIGHT save, then a self-initiated SCB system reset roughly 23
seconds later, ending (post-reset, in this emulator) in a halt at
`0x00079e1e` before any frame. Further 2.39 profile continuation therefore
requires physical evidence of GPS behavior beyond the fifth pulse (or a
GSTP-time contract), not a ceiling raise. All artifacts, binaries and
snapshots remain under `/tmp/semu-774-postsave/`; no payload or firmware
byte enters Git.

### E-SAP-TIME-SCHEMA-239-001

2026-09-08; read-only static analysis of the pristine Sapporo
`2.39.20.22297-P` application, E-SAP-0011, SHA-256
`85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`.
Use `arm-none-eabi-objdump -D -b binary -m arm -M force-thumb
--adjust-vma=0x40000 --start-address=0xd5084 --stop-address=0xd51b8`
on the private application; read the little-endian literal pointers at
`0x000d51b8..0x000d5200` and their NUL-terminated key names separately.
Wrapper `0x000acb78..0x000acba4` checks object byte `+0x12`, opens
`settings/time` in mode two, invokes serializer `0x000d5084` for a nonnull
handle, closes, then clears the flag. It does not test the serializer result.

The serializer calls these 19 fields in order, checking each helper result;
it returns zero on failure and one only after the final field succeeds.
Offsets below are hexadecimal offsets into the source object, not disk offsets.

| Key | Object offset | Helper / source access |
| --- | --- | --- |
| TimeFormat | `0e` | `af8ec`, byte |
| DateFormat | `0f` | `af8ec`, byte |
| AlarmClockTime | `00` | `afb10`, 32-bit |
| AlarmClockMode | `0c` | `af8ec`, byte |
| AlarmClockSnoozeDuration | `04` | `afb10`, 32-bit |
| WeekType | `0d` | `af8ec`, byte |
| AutoTimeSync | `10` | `af55c`, boolean |
| DstChange0 | `14` | `afb10`, 32-bit |
| DstChange1 | `18` | `afb10`, 32-bit |
| DstChange2 | `1c` | `afb10`, 32-bit |
| UtcOffset | `20` | `af88c`, byte |
| DstOffset | `21` | `af88c`, byte |
| DstActive | `22` | `af88c`, byte |
| TzShort | `24` | `af5d2`, string; caller R3=4 |
| TzName | `28` | `af5d2`, string; caller R3=52 |
| LocName | `5c` | `af5d2`, string; caller R3=32 |
| DualTimeEnabled | `11` | `af55c`, boolean |
| LocalTimeOffset | `08` | `af9bc`, signed 16-bit |
| DualTimeOffset | `0a` | `af9bc`, signed 16-bit |

Read-only helper inspection at `0x000af5d2..0x000af658`, `0x000af88c`,
`0x000af904`, `0x000af9bc` and the existing scalar/boolean helpers shows
length-checked writes of formatted values. The string writer calls native
`strlen` at `0x000700a4`; the caller's R3 is not a string-length bound.
Nonempty strings take header, payload and CRLF writes, while empty/null
strings omit the payload write. Therefore 19 fields are not 19 file writes,
and the R3 literals alone cannot justify a capacity or operation ceiling.

Confidence is high for static order, object accesses and success checks.
Actual field values, emitted sizes, complete native save/close behavior,
required capacity and subsequent navigation remain unmeasured. Production
continues to refuse the path. This entry authorizes evidence collection,
not a new file slot, fabricated time value, GPS fix or runtime hook.

### E-SAP-TIME-NATIVE-239-001

2026-09-08; ticket 767, production base `bca8f7d`, exact E-SAP-0011
application SHA-256 `85dcf109cb7a39f811dafc9553ac79d3b8c40159ab007f609427267b95e21b89`
and full flash `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Every observer validates all three components, full flash and input snapshot
hash before execution. Bounds: five billion instructions, 35 billion virtual
ns, 5,000 frames. All sources/artifacts remain external at
`/tmp/semu-767-time.PaLEy5/`; production C, capacities, budgets and formats
are unchanged. Ticket 767 records exact reproduction commands/source pins.

Two production attempts at ticket 766's final checkpoint preserve snapshot
`3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`:
unknown mode-two time open, `000920b4 / 3885178598 / 30368914377`, object
R5 `0x1003538c`, pending byte `+0x12` equal to one. The known static
serializer is E-SAP-TIME-SCHEMA-239-001.

#### Measurement and native-path control

An isolated diagnostic copy appends only `settings/time` with a 4,096-byte
observation ceiling and at most 256 extra logical operations (76,855 / 76,858).
Existing file operation logic is unchanged. The diagnostic codec appends slot
13 only after creation and retains exact 11/12-slot historical encodings.
No guest object, counter, time source or file bytes are seeded or repaired.
Both repeated runs and a mid-save restore show **two 24-operation saves**:
open, 22 full contiguous writes, close, 349 bytes per save. Write lengths:

```text
15,15,25,19,35,13,16,21,21,21,14,14,14,13,2,12,2,13,2,19,22,21
```

The three strings are empty (header + CRLF, no payload). Every write returns
its full length; native serializer returns one at `0x000acb98` and pending
clears at `0x000acba2`. The two output hashes are
`496716b1f14deebf5f1201146c75d6138d3c22dc8b7ca52f641c357984047491` and
`7c471060d2e4e4c8f8c3258d328de60564a60164b96c4b96daa284e2c81b108d`.
349 is the observed size, not evidence for a general capacity or persisted
default. The 4,096-byte/256-hit ceilings are experimental, not proposed runtime
limits. The diagnostic ends at GPS refusal with logical hits 76,647.

The independent **native-path control** changes only the unknown-create guard:
exact `settings/time`, mode two, forwards the open to native instructions.
It uses the original twelve-slot file implementation and original production
descriptor, no new slot, no new counter or hook result. Native mode construction
at `0x000bdfd0..0x000be062` selects write mode and invokes `0x000cd1ae`.
Native writes pass `0x000be0c4..0x000be0e8` → `0x000cd214` → `0x000cfda4`;
the observer sees all 44 actual helper-return lengths equal their requests.
All 44 input lengths/payload hashes also match the diagnostic sequence exactly.

| Native save | Open return | Serializer returns one | Close continuation | Pending cleared |
| --- | --- | --- | --- | --- |
| First | `3885586479 / 30369322258`, handle `00000ae0` | `3885633877 / 30369369656` | `3885698502 / 30369434281` | `3885698504 / 30369434283` |
| Second | `3886287972 / 30370023751`, handle `00000af0` | `3886347995 / 30370083774` | `3886412674 / 30370148453` | `3886412676 / 30370148455` |

Table pairs are instruction count / virtual ns. The public native close wrapper
does not return the adapter's synthetic one: observed R0 is `0x10053b2c` at
`0x000acb9e`, and this caller ignores it. The native inner close result is
checked in `0x00092110..0x0009213e`; both executions continue without assertion.
Do not translate or infer a new public close ABI from this observation.

Read-only `semu_storage_read` of the flash overlay finds the second exact
349-byte output absent before execution and present once at `0x00a91a00`
afterward. Dirty-page count goes from 69 to 72. A restored native midpoint
starts with 70 dirty pages and reaches the identical retained bytes and final
snapshot. The original production loader also restores the final native
snapshot, finds the bytes at that same offset, and retries the GPS refusal
with a byte-identical snapshot. The immutable source flash hash never changes.
Thus the existing lower-level storage path suffices for this observed file;
adding session-file retention would unnecessarily replace working native code.

#### Repeat/resume pins and integration boundary

Two native refusal-start runs, two personal-midpoint starts with the six
remaining ticket-766 input edges, and a mid-native-save restore converge to
`compat-refused / 001291cc / 3960123530 / 32455738919`, GPS hits `2,2,4`,
logical hits unchanged at 76,599. The final frame remains WEIGHT, CRC
`a8c9f3d3`, SHA-256
`3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585`.
There are six post-refusal-start / 238 earlier-start frames. No renderer refusal,
reset, assertion, fabricated time value or new GPS response is involved.

| Artifact | SHA-256 |
| --- | --- |
| Native midpoint, `3885613020 / 30369348799 / 000af89c` | `b85eed95839285b520bb560cd1fff13431b837c59b29b60f5d6a12d8b86b58f2` |
| Native final, every repeat/resume plus sanitizer run | `ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d` |
| Native refusal-start trace pair | `598ec08b97e1b65ac1f58a52e7f67616fa7e88e541deba454f77e16d72c9c0eb` |
| Earlier native trace pair | `af9de2d604b53fad3ca92aef55315bf7fad0df917e8debbb2fda51ddb35d7022` |
| Earlier native compatibility log | `f287fc1e9276ea1540596234b8b65ae4d560fdda493ae7865bd9c3d928ae54bf` |
| Native resumed SAVE/WRITE_RETURN/END suffix | `6ddd5846c029f4b04ddf2e41f7526037d3d8f5351b2041b94694b3feb6a61406` |
| All 44 native/diagnostic write-size and hash records | `11bc65c3d99473602e122f09dceca585547ce0c33a96d3d87554f8d7a9195f81` |
| Diagnostic midpoint | `d96e97d12badecb2bff5ed295923fa528fed0c80ef9a6b77cbc1979571387af5` |
| Diagnostic final | `8c4461bd698930930aea0620a0a4985cfa5b1da8b01d5220045da281826314e6` |
| Diagnostic trace pair | `79663293b02c7d1900c706b247ce89fbb395420785e28fa01c6407ffd2c1a425` |
| Diagnostic log pair / resumed log | `29f0ae6558d7ccab7ecb28832f3c9b3213531c813bc0c3c70e3cdb5574dec06b` / `9664c250af9ec23b17ec1c45daef059bbf97a1d6abf793997570250f15207c3e` |

Native refusal-start and resumed compatibility logs are empty: the time saves
execute normally without consuming an intervention. The earlier log is still
ticket 766's exact remaining general/personal suffix. Diagnostic and native
final snapshots are intentionally distinct; neither replaces a release golden.

Confidence is high for these exact two native saves and retained bytes.
Propose a separate integration limited to exact mode-two `settings/time`
native routing in the existing hash-pinned opt-in layer, with unknown-create
refusal tests and repeated/resumed native storage gates. No extra compatibility
hit, file slot, capacity, snapshot version, firmware patch or public API is
needed. Do not generalize this success to other unknown create paths or the
historical general-file FAT failure. The fifth GPS-awake lifecycle remains the
next independent evidence boundary; setup and watch-face navigation are unproven.

#### Ticket 768 production verification

2026-09-08; the production hook now forwards only normalized exact mode-two
`settings/time` after existing validation. No other runtime change is made.
The new routing regression fails before this change and passes afterward;
CPU/RAM/file codec/counters/logs remain unchanged on forwarding and refusal.
Sibling paths, malformed/unterminated input, invalid modes and disabled or
wrong-hash activation retain fail-closed behavior. Table-owned files still
consume the original bounded synthetic operations.

The extended personal gate preserves general/personal midpoint and log pins,
and the complete shorter idle branch. Its full branch now verifies the accepted
native time midpoint/final hashes above, including a mid-time-save restore and
byte-identical refusal retries. Two independent production runs from the old
time-open refusal also reproduce that final hash and the one retained 349-byte
payload at `0x00a91a00`, with 69 to 72 dirty pages. The old snapshot saves back
byte-identically immediately after load; source flash retains its original hash.

External production artifacts: `/tmp/semu-768.42dyOp/`. Observer source is the
unchanged ticket-767 `probe.c`, SHA-256
`611fd0c04d823f3a76018b5cd8fa789deb96e2264cc8e4da089904eddb55acbb`,
compiled against production without an adapter override. Its included in-tree
personal probe has SHA-256
`272bca169f8d9d9628464b55c1bf9fdcc01bde3c865f7ca86a94614ba70ad4a9`;
production library SHA-256
`acc9513c4e4d2db74c2eba170eefe165152a69d11aa031982dc39945276a2547`.
Repeated production trace SHA-256
`896bb9a2ade5f6d244bcbc7f84634222faec635b3d59b740bce354d832044483`.
All 832 normal/sanitizer tests, 47 focused cases, four machine-snapshot tests,
four 2.39 private gates, SDL build and 2.22.60 live-input gate pass. Ticket 768
records the commands. No setup completion or additional GPS behavior is claimed.

### E-SAP-GPS-FIFTH-239-001

2026-09-08; read-only production inspection of the native time final checkpoint
`ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`,
with the exact component/full-flash inputs above. External observer
`/tmp/semu-768.42dyOp/gps-inspect.c`, SHA-256
`72affc4dfbe83a77eb9f3e4d957c43d5c415d4849ebdd92498a178968920cc2e`,
links the same production library. It adds only read-only CPU/bus/scheduler
inspection to the bounded observer; no hook, pulse or guest state repair.
Trace SHA-256
`8735ef6ab4923fdf325e5b6837fa9ad4631046dd314db6f19874b5c07bd8882b`.

At PC `0x001291cc`, instruction 3,960,123,530 / 32,455,738,919 ns,
the GPS counters are `2,2,4` and logical-file count is 76,599. Registers are
R0=1, R2=12, R4=`100588a2`, R5=`10036944`, R6=`100369ec`,
R7=`1003674d`, R8=`100366d8`. Driver bytes +272/+273 are 12/10;
awake/retry bytes `100588a2`/`100588a4` are 1/0; GPIO24 configuration at
`40010060` is `00000093`. There are no queued CXD-awake events.

The first check in the existing awake hook rejects four exhausted hits; the
remaining currently checked driver predicates match. A retry refuses without
advancing time/count; the inspected final snapshot remains byte-identical.
This identifies the immediate emulator stop as an exhausted compatibility
fixture, not a newly observed driver mismatch. It does not establish a physical
fifth-pulse cadence, a GPS fix/time or a receiver response. Those require a
separate evidence ticket before changing the bounded GPS behavior. The last
rendered frame remains WEIGHT (CRC `a8c9f3d3`).

### E-SAP-GPS-FIFTH-239-002

2026-09-08; ticket 769, production base `44ee4d6`, exact E-SAP-0011
components and full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`.
Every observer validates all component bytes, full flash and starting snapshot
hash before execution. Start is E-SAP-TIME-NATIVE-239-001 final
`ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`.
The production control repeats that exact snapshot/refusal without mutation.

Only an external copy of `sapporo_239_gps_awake.c` changes: its total/per-trigger
and validation/refusal bounds are five rather than four. All driver/GPIO
predicates, CPU execution, pulse scheduling, IRQ handling, file limits, layer
identity and snapshot encoding remain unchanged. No live counter or guest RAM
is repaired. The diagnostic uses the existing 100-ms delay/1-ms high fixture;
this is not a new physical-cadence claim. The source has compile-time controls
for no pulse or six-second delay; neither supplies a UART response.

The fifth admission occurs at `3960123530 / 32455738919 / 001291cc`.
The native clear at `00129250` is reached at 3,960,123,849 / 32,455,739,238 ns.
The GPIO rise invokes `00128926` at 3,962,901,511 / 32,555,739,360 ns,
with awake zero; three retired instructions later `0012892e` observes awake
one. The next successful poll is `4071207676 / 37929735196 / 001291cc`:
callback 12, pending ten, awake one, retry zero, no pulse pending, GPS `2,2,5`.
The diagnostic refuses a sixth admission atomically. No additional renderer
frame or logical-file operation occurs in this idle continuation.

Two complete runs, pending-rise/high/fallen restores and an ASan/UBSan run
produce identical final state. Repeated normal/sanitizer traces and logs match;
restored traces match their native suffix and all restored logs are empty.

| Artifact | SHA-256 |
| --- | --- |
| Pending rise, `3960123531 / 32455738920` | `3a8d9d5e24ccd74df852681b8e80097272095e2b2b55443ed543280c8a74d674` |
| High after native STRB, `3962901514 / 32555739363` | `03712d1dea6acbb028b5a3b04fd2c1b5d6f2df848bcdf96ece85a72f70f274e4` |
| Fallen, `3962902096 / 32556738919` | `f545b28f0ac7575c3ce29294487231136210512a64a3db505a77d6a85eb27452` |
| Idle final / atomic refusal retry | `127214e55e966741d3cc3acb5fd5fad50988b3cb4bdadb78788e590b91f8df28` |
| Repeated idle trace / one-hit log | `1537c34214c48cd407e3ec08e7d85cd0084c5358f99203e71271b24007a06d23` / `cce5f2765d9562c45a062ca2db03d7126657b37bfcf2f8c52d105ed0fe9fb5c5` |
| No-pulse trace | `6b023bc5ceff3ec672a52144c1f7e3e3454faea520c7b859ffc88e787814146d` |
| Six-second-late trace | `6a6492b4d8598e3dbf09a0cfae7bfb3f7011e4ce0e1b57942c643dba6c65a4e3` |

Both controls reach the same native precise fault at instruction 4,071,268,202 /
37,929,984,335 ns: PC `001c0db4`, BFAR `4001d000`, stacked PC `00171798`.
Neither invokes an awake callback. The no-pulse variant forwards the validated
poll without admission/hit; the late variant queues the pulse after the fault
deadline and has five hits. The latter retains the original static descriptor
effect text, but the compiler override is six seconds, not the text's 100 ms.
These controls distinguish the actual input deadline from mere hit accounting.
Production refuses the five-hit pending snapshot; wrong starting hash also
fails before execution. Source flash retains its initial hash.

#### Navigation boundary within the additional interval

A normal MIDDLE click requested at 34 s has actual press/release edges
`3991602893 / 34088644931` and `3995360381 / 34174452535`; the release was
requested 70 ms after actual press, with WFI overshoot retained. Two runs
match traces, logs and final snapshot
`00432bcc97bc988da8370e9e2a298a39bdfa86971e5a8000ccd36dafaaf5a286`.
There are 69 native renderer publications; final CRC `cd4c0a99`, pixel SHA-256
`33339448cbcfafa47bd9d0ed4e37b61abd43062acf95f2bf7470ebefae921072`.
Visual inspection of the live-run capture identifies HEIGHT with 170 cm
centred; that is guest state, not personal information supplied by the user.
The next operation is a refused mode-two `settings/personal` open, LR
`000adb2f`, at `000920b4 / 4232903136 / 34413596174`, logical hits 76,599.
Its retry and an independently inspected loaded image are byte-identical.
The completed additional save and its required operation count are unmeasured.

External sources/artifacts are `/tmp/semu-769.py1saC/`:
`probe.c` SHA-256 `6a188ed8f630153871df48016abea4e5072c404a4a4939caf4b17d841bf02f5f`,
`awake.c` `4e62e815ac56a533ee555386c5192288c63defc94ddf87b317d6f5f5c3b688de`,
`inspect.c` `aa5d4f8d1e023afc0668d1fe085467e12665ef62169ac390b6a96ca655e8cade`.
Included personal probe/library hashes remain those in ticket 768's entry.
Navigation trace SHA-256
`f0aa8fc8be5eb05331f01969c53d3daf474e3b8911d81a39e9a87c33a9a662d2`;
independent live-image trace
`6b23a486dff03a3458cc61c5b626d7a41ef9374f0e4d91053cf7b6ff34325349`.
The latter repeats the same final machine/image; a loaded snapshot alone
does not republish a frontend frame and was not used to identify the screen.

Confidence is high for one additional synthetic liveness interval and its
native input path, not physical timing, GPS fix/time or indefinite operation.
Production is unchanged. Any integration must preserve the four-pulse layer's
historical identity/refusals and explicitly select extended fixture behavior;
do not silently repin existing gates. The subsequent personal save is a
separate evidence task. Ticket 769 records exact commands and results.

#### Ticket 771 production integration verification

2026-09-08; evidence commit `15c2a53`. The production implementation adds
the separately selected `sapporo-2.39-gps-awake-five` alternative. It retains
the measured predicates and pulse shape, pins its bound to five, and rejects
simultaneous variants and cross-identity snapshot loads. The four-pulse layer
is unchanged. No CPU, snapshot format, file budget or receiver response changes.

The new in-tree private probe generates its starting checkpoint from cold native
execution using the previously measured input edges. Its boundary helper checks
both count and time: WFI can advance time without retiring an instruction, so
matching count alone could inject an edge too early. Bounded native dispatches
reach the measured time without mutating CPU/time or changing execution semantics.

| New-identity production artifact | SHA-256 |
| --- | --- |
| Cold prefix, `3960123530 / 32455738919` | `6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de` |
| Pending rise | `83ddd7933cc11667595277e53ba19ef14cbc84c658b246aee91d6d5066dc614f` |
| IRQ entry, `3962901511 / 32555739360` | `14f8dfd79dfcbf5ec2035025cf8e1bf02ac3ef59619b8fc1d985f309331f49f2` |
| High after native STRB | `4c59bc416db19638a06a925880b6af3dc2e470cd3dca9e898aa3589e40e3f894` |
| Fallen | `f2f88ecc28fcd7968cd94e0ec09bc87f2cdfcc6be2c6525a0a45eaf0c341a028` |
| Idle final / atomic refusal retry | `d5244833987e6801192af15f0c57077eba2d23329fbaa4e09a6ccc4ec5b455c1` |
| MIDDLE final / atomic refusal retry | `2224b55ed72f0cac548fa80117787ae5b049f5783f10a8b6730f5ea1a0936467` |
| One-hit log | `18a7d21d6d2dd42c1e7841530dd340bfdb103257310588c6195e5aba5bbea93c` |
| MIDDLE trace | `0ea2119eed8b56107a023ce2b932607af2fcf8c3758ff90229e8abe441519cb7` |

The production idle and MIDDLE finals have exactly the diagnostic state above
apart from their serialized layer ID. A test-only snapshot comparison removes
only the new ID's `-five` suffix and matches the old diagnostic hashes. That
comparison image is never loaded or executed; runtime migration remains refused.
The new private gate repeats both branches, restores pending/IRQ/high/fallen
snapshots to the exact idle final, and checks input/component/hash and duplicate
selection refusals. MIDDLE reproduces all 69 frame publications and the exact
HEIGHT pixel hash above, then stops at the unchanged personal-file budget.

All 836 normal and sanitizer tests, 138 task contracts, the new private gate,
legacy four-pulse and personal-budget private gates, SDL build and 2.22.60 live
input pass. An independent production ASan/UBSan MIDDLE continuation matches
normal trace/log/final bytes. Source flash retains its initial SHA-256.
External verification artifacts are `/tmp/semu-771.BtFo6c/`; source
`tests/integration/sapporo_239_five_probe.c` SHA-256 is
`44b91b22702a85729ab9295ead3aa40ac59b28cda6acbeaa9d7b0669d9bdb1ab`,
production awake source `f09cd4e5fdb4979d4934c2cb61b9c0820c8f1fcfeba280870d17583b9bd3279d`,
and library `dae599fb003ea6c24b0f627d600b7f7b58e6fe4757c7b1036b45aa5bdb440901`.
Ticket 771 records commands and handoff; status awaits separate review.
No sixth pulse, physical cadence, GPS fix/time, additional file allowance,
completed setup, watch face or post-setup menu is established by this integration.

### E-SAP-UI-PERSONAL-239-001

2026-09-08; bounded read-only follow-on observation using ticket 763's normal
production library, all four explicit layers and the normal NEMA backend.
Every run validates the unchanged E-SAP-0011 components and full flash before
restoring the general-save midpoint SHA-256
`76a7af5385eb2ddf5dfe94f6607db34e6e820edb06f5054a31bce7a5ef4ada66`.
There are no firmware, CPU/RAM, file ABI, descriptor or GPS changes. Native
button input is the only additional stimulus. The existing observer wrappers
forward production backend results without modification.

Two runs request three MIDDLE clicks at 20, 22 and 24 seconds, with release
requested 70 ms after each actual press. WFI overshoots yield actual edges:

| Edge | Instructions | Virtual ns |
| --- | ---: | ---: |
| Press 1 | 2103476826 | 20020743409 |
| Release 1 | 2107221220 | 20106721328 |
| Press 2 | 2397641544 | 22084812137 |
| Release 2 | 2404140067 | 22170370053 |
| Press 3 | 2701906762 | 24048201779 |
| Release 3 | 2708404518 | 24133728411 |

Each click produces native button events 2, 5, 1 at `0x0010ace2`.
View-open `0x00073898` observes tokens `0x529ec8bc`, `0xadadd14f` and
`0xe0d3258c` in sequence without invoking the opener itself. The final
visually inspected frame reads **BIRTH YEAR**, with 1990 centred in the native
selector; this is firmware state, not a supplied user birth year.
Both runs accept 772 renderer submissions with no refusal, and stop at:

```text
compat-refused pc=000920b4 lr=000adb2f instructions=2953605137 time=24380651994
r0=000ae360 r1=00000002 r2=10024e88 path=settings/personal gps_hits=2,2,3
```

The logical-file budget is exhausted at 76,371; this next open has not been
admitted. The existing file capacity is 1,727 bytes, but neither the required
operation count nor a complete personal-save execution is measured here.
Pristine disassembly `0x000adb1e..0x000adb48` tests object byte `+0x145`,
opens through `0x000adb16` in mode two, calls `0x000d5ff8` with the object
and address of the returned handle, closes it and clears the byte. This
identifies the next serializer to investigate, not permission to assume its
ABI, count, success or payload from the general-settings serializer.

Repeat SHA-256 pins:

- Trace: `4d2d046a4dbf6744d762eaf240229aa610a56a6a2fbc117fb0d668930d94c20a`.
- Final snapshot: `b94ffb8e873b141fb05defa02754311d67ce48939b537da86ea421de8ed39062`.
- Final RGB565: `d2c4833a433610b5087f6e04fe16c7c4bd9d3baf6573df21cc72e0abde77b09b`.

Private artifacts are `/tmp/semu-763.WR6heT/walk-{a,b}.*`. Observer
`walk-probe.c` SHA-256
`c04fe9705e93b0383424f8285163cec4fabc0cf09fb4f6b775b0ce2007a06b84`
includes the earlier `ui-probe-long.c` pinned above. Compile with
`cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices`
and the normal `build/libsemu.a` (no diagnostic translation units). Its two
arguments are an output prefix and the production `first.mid.sems` path;
stdout/stderr are retained as `.trace`/`.log`. Both runs exit zero after
capturing the explicit refusal, bounded by five billion instructions,
35 billion virtual ns and 5,000 changed frames. The probes do not complete
onboarding, reach a watch face, or justify extra GPS pulses/file headroom.

### E-SAP-UI-239-002

2026-09-08; read-only native continuation after ticket 761's integration
review. Runtime commit `2d230a2`, library SHA-256
`9327c56df85ace7f21814087ce634d2898edb55b2836f7524f84f02af1619776`.
The observer validates all three E-SAP-0011 component sizes/hashes and full
flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`
before execution. It uses the normal NEMA backend through forwarding
prepare/commit/abort callbacks that count results but do not alter them.
All four explicit layers remain enabled: synthetic-wbsto, gps-startup,
gps-reopen and gps-awake. No production budget, guest-state patch, direct
native callback, profile, firmware or snapshot-format change is made.

Two runs restore the E-SAP-UI-239-001 prefix at 700,000,000 instructions,
5,792,348,681 ns, PC `0x000be50a`, SHA-256
`7650d82fe72e58d544dc0043df99ab756ece41092d39e94d7c0b2b7460a904d2`.
They single-step with absolute limits of two billion instructions and
35 billion virtual ns; requested MIDDLE presses are at 12 and 14 seconds,
with release requested 70 ms after each actual press. WFI jumps produce
the following actual native-input edges (active-low):

| Edge | Instructions | Virtual ns |
| --- | ---: | ---: |
| First press | 847389018 | 12010884553 |
| First release | 850221529 | 12096961148 |
| Second press | 1132984059 | 14075897022 |
| Second release | 1136723800 | 14161873768 |

Native publication `0x0010ace2` records events 2, 5, 1 for each click.
Native view-open `0x00073898` records token `0x8a7f9b55` before input and
`0x9cdbd4e2` after the second click. Both runs accept all **148 renderer
submissions with zero refusals**, unlike the old E-SAP-UI-239-001 observation.
The observer records 148 frame callbacks and 115 consecutive CRC changes;
its final frame callback is at 14,399,427,971 ns. The final saved pixels are
diagnostic, not a settled-screen or physical-panel golden.

Both stop with `compat-refused` at 1,376,488,437 instructions,
14,401,737,146 ns, PC `0x000920b4`, LR `0x000ad079`. Snapshot inspection
finds R0 `0x000ad9ec` naming `settings/general`, R1 two, R2 one,
R3 `0x351e000f`. The diagnostic names the exhausted `logical-file` trigger;
the limits remain 76,279 logical and 76,282 aggregate. GPS hits are `2,2,1`.
Thus the old stop/time is reproduced without swallowed renderer failures;
this still does not measure any successful operation beyond that open.

Pairwise SHA-256 pins (both `nav-a` and `nav-b`):

| Artifact | SHA-256 |
| --- | --- |
| Normalized trace | `c7851cdb89e7998a2e94eedd6a7e5e815d3e45f359553f8dd577dc9e5b2d6a34` |
| Before first press snapshot | `d85840a8065640b3cfe7480ce6103a71643b47deb6bcc0bd0c807c81ea0cfd08` |
| Before first release snapshot | `24a4db0066973eaf9335acd2abfee2600f4a9ca7a54bfe9649cae29370fd0384` |
| Before second press snapshot | `e75e7d180e2a81c0e65ea10067c4f5fcbf50c007a2f2129e47f97819529376c2` |
| Before second release snapshot | `d144f21ce3051229f1d2bc762f004c547e734cd6cb334c15fb0ffd54587577bc` |
| Final snapshot | `088bb2058955ee19a7e33582c0c5a2a743408395b75219aa84eb34e6a8ca18c1` |
| Last changed RGB565 pixels | `617ebc8ee9347ab0d493276901d14df99a5ad4c2882fcbb23fd35f49de91c592` |

Read-only pristine application disassembly establishes the wrapper at
`0x000ad064..0x000ad092`: test object byte `+0xf4`; if nonzero, open
`settings/general` in mode two at `0x000ad074`; if a nonzero handle returns,
pass its stack address and the object to `0x000d5794`, close at `0x000ad088`,
and clear the flag. Calling this a pending-save wrapper is an inference from
that control flow. The serializer's initial section (`0x000d5794..0x000d58bc`)
calls `0x000af55c`, `0x000af8ec` and `0x000af806` for distinct object fields
and checks return values. Neither the complete reachable helper ABI nor the
operation count is established by this partial static analysis. The current
run stops before the open returns; no successful serialization is claimed.

External artifacts: `/tmp/semu-762-nav.TIs8nQ/`. The observer source
`nav-probe.c` SHA-256 is
`b1007fe204f66c16e44dbda932f2a071163c3a678e5c6884fd305d4c448cf39e`;
it includes `/tmp/semu-761-gpu.aQow5X/ui-probe.c`, SHA-256
`6a7131826e53fe885b50e17f9ed6a30555a8d2df98e1e8da0cc4d3695c791b75`.
It adapts the old navigation observer (SHA-256
`508c684cc26eb1781d2c5618488b4636d3343a955aa14fb6013f8d68c5cb3233`)
to the accepted backend contract and adds result counts. Native RAM reads
are observational; inputs are delivered through the normal machine API.
Reproduction from the emulator root (private artifacts must be present):

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices \
  /tmp/semu-762-nav.TIs8nQ/nav-probe.c build/libsemu.a \
  -o /tmp/semu-762-nav.TIs8nQ/nav-probe
/tmp/semu-762-nav.TIs8nQ/nav-probe /tmp/semu-762-nav.TIs8nQ/nav-a \
  > /tmp/semu-762-nav.TIs8nQ/nav-a.trace 2> /tmp/semu-762-nav.TIs8nQ/nav-a.log
/tmp/semu-762-nav.TIs8nQ/nav-probe /tmp/semu-762-nav.TIs8nQ/nav-b \
  > /tmp/semu-762-nav.TIs8nQ/nav-b.trace 2> /tmp/semu-762-nav.TIs8nQ/nav-b.log
/tmp/semu-762-nav.TIs8nQ/nav-probe /tmp/semu-762-nav.TIs8nQ/inspect \
  /tmp/semu-762-nav.TIs8nQ/nav-a.final.sems
arm-none-eabi-objdump -D -b binary -m arm -M force-thumb \
  --adjust-vma=0x40000 --start-address=0xad064 --stop-address=0xad094 \
  tests/private/sapporo-2.39.20.22297/application.raw
arm-none-eabi-objdump -D -b binary -m arm -M force-thumb \
  --adjust-vma=0x40000 --start-address=0xd5794 --stop-address=0xd58bc \
  tests/private/sapporo-2.39.20.22297/application.raw
```

All commands exit zero; probe exit zero means the observation was captured,
not that firmware completed onboarding. The explicit END/RENDER records and
hashes above define success. Full serialization, a finite added hit count,
mid-save snapshot continuation and the next independent stop are still missing.
Ticket 762 scopes this evidence work; no production increase is justified yet.

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
| E-ULS-0006 | reference-lane reset wiring RE (read-only files: `FW/emulator/devices/ulsan/ulsan-2.35.36.resc`, `ulsan-platform.repl`, `ulsan-storage.repl`; upstream `platforms/cpus/ambiq-apollo4.repl` base) | Ulsan `2.35.36.10731-R` | Boot tuple and memory map proven by the reference lane. Component loads: resident `0x00010000`, application `0x00030000`, resources raw at `0x18040000` (logical flash offset `0x00040000` + MSPI1 XIP base `0x18000000`). CPU start tuple: vector table `0x00030000`, SP `0x1005ffc0`, PC `0x001e1b4d` (application reset vector). Regions: `mcu_mram 0x00000000+0x200000`, `mcu_tcm 0x10000000+0x60000`, `shared_sram 0x10060000+0x100000` (upstream ambiq-apollo4 base); `extended_sram 0x10160000+0x60000` (Apollo4 Plus class referenced by Ulsan binaries); `working_memory 0x101C0000+0xA7000` enclosing the scatter-table zero range `[0x101c0000,0x10266dc8)`; `MSPI1 XIP window 0x18000000+0x02000000` whose first 256 KiB is device-specific, absent, and not synthesized. Observed-but-unimplemented reference facts recorded for later tickets: NVIC priorityMask `0xE0`, CLKGEN `0x40004000` size `0x800` with repeated `+0x84` traffic, MSPI1 registers `0x40061000`, display controller `0x400A0000`. | Read-only RE observation of the reference lane's wiring declarations; not Sapporo inheritance and not register-behavior proof for our SoC blocks. The resc translator-substitution sites (`0x000c2f94`, `0x000c30c4`, `0x0012cb4c`) are Renode 1.16.1 artifacts; the in-tree interpreter executes the pristine bytes. Bootrom (`0x08000000`, `0x07FFFFFC`) and all `0x400xxxxx` blocks stay unmapped in-tree until evidence-backed. |
| E-ULS-0007 | in-tree deterministic fault-capture probes on the private `ulsan-2.35.36` bundle (single-step IPSR/BFAR watch plus exception-frame PC capture), read-only reference-lane RE (upstream `platforms/cpus/ambiq-apollo4.repl` GPIO wiring `gpio: GPIOPort.AmbiqApollo4_GPIO @ sysbus 0x40010000` with `McuN0IrqBank0..3 -> nvic@56..59`, `McuN1IrqBank0..3 -> nvic@60..63`; raw lane trace run1/run2 for 2.35.36 record zero GPIO warnings through boot), and static RE of the application pad-setup shim (`0x000c22a6`/`0x000c22f4` regions, literal pool `0x000c2370..0x000c2392`) | Ulsan  `2.35.36.10731-R` | The application walks a 530-entry delta step table `0x001abb04..0x001ac34c` through runners `0x001d73a0`/`0x001d7380`/`0x001d4798` from supervisor `0x001d746c`, and re-runs the whole table every ~12,578,000 instructions while any step reports failure. One step is the Ambiq pad-setup shim: Thumb `movs r3,#0x73; str r3,[r2]` and `str.w r1,[r4,r0,lsl #2]` at `0x000c22b0..0x000c22b2` store PADKEY value `0x73` to `0x40010200` and PINCFG words to `0x40010000 + 4 * pin` (literals `0x40010000`, `0x40010200`; accessor modes use `0x40010204 + 4 * ((pin >> 5) & 3)` input banks and `0x40010214`/`0x40010224`/`0x40010234`/`0x40010244`-style bank windows). With the block unmapped the store raised a precise BusFault (`CFSR 0x00008200`, `BFAR 0x40010200`, exception-frame PC `0x000c22b2`, first at instruction 12,578,512), the shared fault handler `0x001dde09 -> 0x001ceb50` absorbed it, the step never completed, and the supervisor re-ran the table forever: the in-tree boot livelock. Attaching the existing in-tree Apollo4 GPIO controller at `0x40010000` in `semu_ulsan_board_map` clears the fault and boot advances; the only remaining refusal at the 13,000,000-instruction frontier is a 32-bit read of `0x40021008` (power controller) from PC `0x00096b66` (`lsrs r1,r1,#20; and.w r1,r1,#1`), first at instruction 12,583,862. Frontier (reproduced twice equal): budget stop at instruction 13,000,000, PC `0x001d0f24`, SP `0x1005ffa8`. | Reproducible in-tree observation plus lane silence; authorizes attaching the GPIO controller with its existing Sapporo-pinned PADKEY/PINCFG/INPUT/OUTPUT/interrupt-bank semantics and no IRQ sink (no bank IRQ is observed through boot). The reference lane resolves the same stores without any log line, matching handled-register silence. GPIO offsets observed in the shim literal pool but not yet touched during boot (`0x4001020c`/`0x40010210` input banks 2-3, the `0x40010214`-family accessor windows) remain unsupported until their access is observed. The `0x40021000` power-control block is the next observed gap; its lane model is `AmbiqApollo4`-class with per-bit field tags (first lane log line: write offset `0x24` value `0x3` bits `PWRENSSRAM`), and its register semantics are not yet pinned. |
| E-ULS-0008 | read-only reference-lane observations for the 2.35.36 lane (`trace-235-run1.log`/`trace-235-run2.log` power-controller lines, identical in both runs; upstream `platforms/cpus/ambiq-apollo4.repl` line `pwrctrl: Miscellaneous.AmbiqApollo4_PowerController @ sysbus 0x40021000`; lane probes 4/5/6: state dumps at guest PCs 0x0009661a/0x0009662c/0x00096650/0x000966a6/0x000966b2 and at shim entries 0x000965ec/0x000966f8/0x00096b5c plus after-idle final dumps; guest RAM word read at 0x1005b078) and in-tree deterministic fault/trace probes on the private bundle | Ulsan `2.35.36.10731-R` | Boot-phase behavior of the power-control block. Writes: the lane logged ten writes (0x04 values 0x8000/0x40000/0x80000/0x400000 as PWRENMSPI1/PWRENDISP/PWRENDISPPHY/PWRENUSB, 0x24 value 0x3 PWRENSSRAM, 0x58 value 0x1 PWRENDSP0RAM, 0x60 value 0x4 ICACHEPWDDSP0OFF, 0x78 value 0x1 PWRENDSP1RAM, 0x80 value 0x4 ICACHEPWDDSP1OFF, 0x100 value 0x1 SIMOBUCKEN), each naming the complete written value as unhandled bits: the lane model stored none of them. Reads: the lane returns {0x00:9, 0x04:0x00100000, 0x08:0x00100000, 0x0c:0, 0x10:0, 0x14:0x3F, 0x18:0x3F, 0x1C:8, 0x24:0, 0x28:3, 0x2c:0x3FC, 0x58:0, 0x5c:0, 0x60:0, 0x78:0, 0x7c:0, 0x80:0, 0x100:0} at every boot-time probe point; the values are unchanged across the guest read-modify-write chains at 0x000965ec and 0x000966f8 (0x14/0x18/0x1C probed before and after each store) and, except 0x04/0x08 which fall to 0 only once the lane idles, at run end. The boot sampler at 0x00096b5c (called from 0x0009d20a, ldr at 0x00096b66) keeps bit 20 of 0x08 and the lane stores the consumed result as guest RAM byte 0x1005b079 = 0x01. The in-tree device `src/devices/ulsan_pwrctrl.c` (attached by `semu_ulsan_board_map`) serves exactly these 18 read offsets and accepts exactly the lane-logged write pairs plus the two pairs the guest itself computes in-tree ((0x14,0x3F),(0x1C,8), observed by instrumented scratch run, 15 identical cycles through instruction 200,000,000 with no other block access); everything else refuses. Fault-chain record for the unmapped/strict states: BFAR 0x40021008 fpc 0x00096b68 first@12,583,862; BFAR 0x40021014 fpc 0x0009661a first@12,583,876; BFAR 0x4002101C fpc 0x000966a6 first@12,583,944; with the full table the only remaining power-block-era fault is an instruction fetch at 0x0800009c (fpc == BFAR == 0x0800009c) at instruction 12,584,023. Frontier pin (reproduced twice): budget stop at instruction 13,000,000, PC 0x001d0f22, SP 0x1005ffa8. | Reproducible lane probes plus in-tree records. Limits: the lane PowerController is a binary model, so values are pinned at probe points, not as an internal state machine; 0x04/0x08 are boot-phase constants only (they drop to 0 after idle, which no boot-phase read observes). The upstream repl declares the lane bootrom as `Memory.MappedMemory @ 0x08000000` whose code blob is explicitly unavailable (repl comment) with `BootromLogger @ 0x07FFFFFC`; reproducing the 0x0800009c call-site behavior is a separate gap and the boot-init worker still cycles at 200,000,000 instructions with zero refused power-block transactions, so the remaining boot-completion divergence is not a power-block refusal. |
| E-ULS-0009 | the 2.35.36 reference lane's platform description (`ambiq-apollo4.repl` lines 14-87: `bootrom: Memory.MappedMemory @ sysbus 0x08000000, size 0x1000` whose complete content is the published WriteWord/WriteDoubleWord init list because the real blob is declared unavailable, plus `bootrom_logger: Miscellaneous.AmbiqApollo4_BootromLogger @ sysbus 0x07FFFFFC`) and a confirmation probe on the running 2.35.36 lane (instruction hook at 0x0800009C, bus words after RunFor) | Ulsan `2.35.36.10731-R` | Bootrom stub block behavior. The strict power-block state (E-ULS-0008) left one fault per pass: an instruction fetch at 0x0800009c (fpc == BFAR, first at instruction 12,584,023). The lane declaration identifies 0x9C..0xA4 as `delay` (adds r0,#15; loop subs/cmp/bne; bx lr), 0x74 as `read_word`, 0x6C/0x200/0x220 as `program_main2`, and 0x30..0x38 as the handler that stores the caller's LR to the logger_address literal 0x07FFFFFC at 0x48 and branches there (the lane aborts on logger access). The running-lane probe confirms execution (the hook at 0x0800009C fires repeatedly during boot) and byte-exact words: 0x0800009C = 0x3801300F, 0x080000A0 = 0xD1FC2800, 0x08000074 = 0x47706800, 0x08000030 = 0x3014F8DF, 0x08000048 = 0x07FFFFFC; all other bytes of the 0x1000 block are zero-initialized lane memory. The in-tree device `src/devices/ulsan_bootrom.c` serves exactly this halfword table read-only (widths 1/2/4, width-aligned) and maps the logger word as a device whose every access refuses with the written value named in the diagnostic, the deterministic abort-equivalent. Effect: the fetch fault disappears, the boot advances 613 instructions in the pass, and the strict frontier pin moves to instruction 13,000,000, PC 0x001d0f2a, SP 0x1005ffa8, reproduced twice across a reset. | Reproducible lane declaration, confirmation probe, and in-tree records. Limits: these are the lane's declared stub bytes, not the physical bootrom image, which is unavailable even as a blob and unrecoverable without a physical capture; the lane block is writable RAM but no boot-phase write to it is observed, so all in-tree writes refuse. After this gap the next strict fault is BFAR 0x40004044 (fpc 0x00096b86, first at instruction 12,584,636): the clock controller block, still unmapped. |
| E-ULS-0010 | the reference lane's Ulsan platform description (`ulsan-platform.repl` lines 13-24: `clkgen_ulsan: Python.PythonPeripheral @ sysbus 0x40004000, size 0x800` whose script is the complete model: reads return the stored dictionary value for the offset or 0, writes store the 32-bit value under the offset; the comment names repeated +0x84 traffic during display-clock bring-up), lane probes 8/9 (bus words at instruction hooks 0x00096b86/0x00096b8c/0x00096bd4/0x00096bd6 and an idle dump), the 2.35.36 lane trace (no CLKGEN refusal or warning lines), and an in-tree scratch access trace through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | Clock-generator block behavior. Boot's only observed register is offset 0x44: read returns 0 (probe at 0x00096b86), the guest stores 0x00FC0000 at 0x00096b8a and the device holds 0x00FC0000 (probes at 0x00096b8c and 0x00096bd4, where the guest reads it back), the guest then stores 0x00FC0040 (0x00096bd4's OR 0x40), and the value survives to the idle dump. The scratch trace shows offset 0x44 is the only block access through instruction 200,000,000, and the same step also writes 19 zero words at power-control offsets 0x140..0x188, which the lane absorbed without any log line; those pairs joined the power-control accept table. The in-tree device `src/devices/ulsan_clkgen.c` reproduces the lane dictionary exactly over a 64-slot table (overflow refuses rather than guessing lane capacity) with 32-bit width-aligned accesses. The power-control block fault chain continued: BFAR 0x40004044 (fpc 0x00096b86, first at 12,584,636), then after this gap BFAR 0x48000050 (fpc 0x0009750c, first at 12,584,686) in the main I2C block, still unmapped. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8. | Reproducible lane declaration plus probes and in-tree records. Limits: the dictionary capacity is declared unbounded in the lane; the in-tree table bounds it at 64 stored offsets, which boot uses twice, and overflow refuses. Equal frontier pins across the two boot passes confirm reset rebuilds the dictionary like the lane's per-run init. Next gap: the 0x48000000 I2C block (lane pairs it with the 0x48000028-target fuel-gauge device per E-ULS-0004's queue). |
| E-ULS-0011 | the reference-lane platform description (`ambiq-apollo4.repl` lines 197-201: `cpu_complex: Python.PythonPeripheral @ sysbus 0x48000000, size 0x1000` with the complete model script `request.Value = 0x4 if request.Offset == 0x54 else 0` and the comment `DAXI Control = 0x4 - DAXIREADY bit set`; lines 203-205: `SilenceRange <0x40020000, 0x40020FFF>` MCUCTRL and `SilenceRange <0x47FF0000, 0x47FF0004>` SYNC_READ; a lane probe confirming SilenceRange reads return 0x00000000 even after a write; lane probe 10 (words at guest PC 0x00097510 and the path hook at 0x0009752e); an in-tree scratch access trace through instruction 200,000,000) | Ulsan `2.35.36.10731-R` | Cpu-complex DAXI and silenced-range behavior. Boot reads 0x48000050 (bit-2 continue test at 0x0009750c; lane 0 so the continue path runs), reads 0x48000054 and stores to 0x48000054 (scratch trace: exactly these three block accesses per pass, nothing else through 200,000,000); the script discards every write. The in-tree device `src/devices/ulsan_daxi.c` serves the script value at every offset of the block (the lane's byte truth for that range, not a fallback), maps the two declared SilenceRanges as read-zero accept-write devices with 32-bit width-aligned access, and refuses narrower accesses. Fault-chain record: with the clock generator in place the block fault was BFAR 0x48000050 (fpc 0x0009750c, first at 12,584,686); after DAXI, BFAR 0x47FF0000 (fpc 0x00097524, first at 12,584,695); after the silence ranges, BFAR 0x4002037C (fpc 0x00096bda, first at 12,594,389); after MCUCTRL, the next refusal is the lane-tagged NVM_OTP/INFO1 read at 0x42003240 (fpc 0x000c0570, first at 12,594,450), whose lane log lines record reads of 0x42003240 and 0x42003310 returning 0x00000000. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8. | Reproducible lane declarations, probes and in-tree records. Limits: the DAXI script is a fixed-value model with no stored state, so post-DAXI transfer semantics are out of scope; OTP INFO1 is a separate gap. The IOM4 block itself sits at 0x40054000 in the lane (upstream repl line 131), not 0x48000000, and boot reaches no 0x40054xxx access inside the observed cycles. |
| E-ULS-0012 | the 2.35.36 reference-lane trace, identical in both runs (`sysbus: [cpu: 0xC056C] (tag: 'NVM_OTP/INFO1') ReadDoubleWord from non existing peripheral at 0x42003240, returning 0x00000000` and the same line for 0x42003310; upstream repl tags `NVM_OTP` <0x42000000,0x4200FFFF> and `INFO1` <0x42002000,0x4200332F>) and an in-tree scratch access trace through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | NVM OTP INFO1 boot reads. The lane has no peripheral in the NVM_OTP range; its sysbus answers these two reads with 0x00000000 and the boot continues. The scratch trace shows the boot phase reads exactly 0x42003240 and 0x42003310 (twice per cycle) and never writes any NVM_OTP address through instruction 200,000,000. The in-tree device `src/devices/ulsan_otpinfo.c` serves only those two words with the logged zero and refuses all other reads and every write (a device-wide read-as-zero would be a guess, since no lane line covers other offsets). Fault chain continued: BFAR 0x400c0fe0 (fpc 0x00096a62, first at 12,594,763) in the CRYPTO block after this gap. Frontier pin (reproduced twice across a reset): budget stop at instruction 13000000, PC 0x001d0f22, SP 0x1005ffa8. | Reproducible lane log lines plus in-tree records. Limits: these are lane sysbus fallback answers for a not-modelled region, not OTP cell contents; no other INFO1/NVM_OTP offset is claimed. |
| E-ULS-0013 | the 2.35.36 reference-lane trace, identical in both runs (`sysbus: [cpu: 0x96A60] (tag: 'CRYPTO') ReadDoubleWord from non existing peripheral at 0x400C0FE0, returning 0x00000000`; upstream repl tag `CRYPTO` <0x400C0000,0x400C3FFF>), the lane PowerController write-logging convention (every nonzero-bit write to 0x40021000 is logged as unhandled bits, so a zero-value write is silent), the lane idle dump 0x40021004 = 0x00000000, and in-tree scratch access traces through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | CRYPTO bus-zero read and silent zero write. Boot reads 0x400C0FE0 once per pass (only access to the block in the tree trace, reader 0x00096a60) and the lane answers 0x00000000; `src/devices/ulsan_buszero.c` maps the declared tag range and serves only that logged offset, refusing all other reads and every write. Boot also stores 0x00000000 to power-control +0x04 at 0x00096a32 (continuation branch); the lane logs no unhandled-bits line for a valueless write and idles with 0x04 = 0, so the pair (0x04,0) joined the power-control accept table as a no-op. Fault chain: BFAR 0x400C0FE0 (fpc 0x00096a62, first at 12,594,763) -> BFAR 0x40021004 (fpc 0x00096a32, first at 12,594,776) -> BFAR 0x40008800 (fpc 0x0009bf3e, first at 12,595,495) in the TIMER block, still unmapped. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f22, SP 0x1005ffa8. | Reproducible lane lines and in-tree records. Limits: the bus-zero table holds only offsets with explicit lane log lines, not whole-range zeros; the (0x04,0) accept is inference from the lane's write-logging convention plus the idle dump, not a direct write log line. |
| E-ULS-0014 | lane probes of the upstream `AmbiqApollo4_SystemTimer` at 0x40008800 (CNT/LOAD/CTL logged at guest hooks 0x0009bf4e/0x0009bf5c as CNT=3..0xA7ED with LOAD=0x303, CTL=0, and idle dumps LOAD=0x303 CNT=0x6C9C rising 648 words over a further 0.5 s of lane time), the 2.35.36 lane write-log lines for the boot continuation (`Unhandled write to offset 0x58 ... value 0x1` PWRENDSP0RAM, `0x60 ... 0x4` ICACHEPWDDSP0OFF, `0x78 ... 0x1`, `0x80 ... 0x4`), and an in-tree scratch access trace through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | SystemTimer registers and power-control byte service. Boot RMWs LOAD +0x00 with constants 0x303 and 0x80000000 (the lane reads LOAD back as 0x303, so bit 31 of that write is not stored), clears CTL +0x100 to 0, and samples CNT +0x04 three times in a stability loop at guest helper 0x000c2bc8; CNT free-runs past LOAD (0xA7ED > 0x303) even with CTL=0. `src/devices/ulsan_stimer.c` holds LOAD/CTL with the observed mask and serves a monotonic per-read CNT tick: the lane counter follows its own virtual time, so exact CNT values are not byte-comparable across engines; monotonicity and the no-wrap fact are the transposed observations. The same continuation performs a 1-byte read at 0x4002105A: the lane register framework slices 1/2/4-byte reads from the register word (no fault, no log line), so power-control reads now serve byte lanes of the observed constants; writes keep requiring lane-recorded 32-bit pairs. Zero writes (clears of the logged bits, observed in the scratch trace) joined the accept table under the established logging convention. Fault chain: BFAR 0x40008800 (fpc 0x0009bf3e, first at 12,595,495) -> 0x40021058 (write; lane-log value 0x1) -> BFAR 0x40024000 (fpc 0x000da82c, first at 12,595,838) at the watchdog. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8. | Probes plus lane lines; CNT's absolute value and tick rate remain lane-time facts, and byte-width writes remain unrecorded. |
| E-ULS-0015 | the 2.35.36 lane trace (`wdt: Unhandled write to offset 0x0. Unhandled bits: [1] when writing value 0x33C3D06. Tags: INTEN (0x1)`; the guest writer at 0x000da7xx assembles exactly that constant; upstream repl `wdt: Timers.AmbiqApollo4_Watchdog @ sysbus 0x40024000`) and an in-tree scratch access trace through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | Watchdog control write. Boot performs exactly one WDT-window access per delta pass: the 32-bit write of 0x033C3D06 to +0x0 (the lane logs the identical value). No lane log line records any watchdog interrupt, reset, or reconfiguration after it, and no WDT read occurs inside the observed cycles, so `src/devices/ulsan_wdt.c` accepts only the logged pair (no state, no expiry semantics) and refuses reads and all other writes; the lane's later 0x200 InterruptEnable read/write lines belong to a phase the observed boot cycles do not reach. Fault chain: BFAR 0x40024000 (fpc 0x000da82c, first at 12,595,838) -> BFAR 0x40000000 (fpc 0x000da834, first at 12,595,841) at UART0, the guest logger's port. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f26, SP 0x1005ffa8. | Direct lane line for the single value; the upstream watchdog's timing side effects stay lane-internal since no expiry was ever observed. |
| E-ULS-0016 | the 2.35.36 lane trace (`sysbus: [cpu: 0xDA832] ReadDoubleWord from non existing peripheral at 0x40000000.` and `[cpu: 0xDA83C] WriteDoubleWord to non existing peripheral at 0x40000000, value 0x2.`), a lane probe at guest PC 0x000da832 pinning the read result to 0x00000000 (bus read of the same sysbus path), and an in-tree scratch access trace through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | Guest logger port behavior. Boot reads 0x40000000 and stores 0x2 to it once per delta pass (scratch trace: the only accesses anywhere near the port; widths 4). The lane has no peripheral there, answers the read with 0, and discards the write. The buszero device table gained a second row: read 0 at 0x40000000 (window size 4, the lane's sysbus fallback is per-address and a wider window would fabricate coverage) plus the logged write value 0x2 accepted and discarded. Fault chain: BFAR 0x40000000 (fpc 0x000da834, first at 12,595,841) -> BFAR 0x40024200 (fpc 0x000da93a, first at 12,595,853), the watchdog InterruptEnable register whose lane read/write lines exist (read before write, so the pre-write state 0 is byte-truth). Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f26, SP 0x1005ffa8. | Direct lane lines plus one probe; only the logged write value is accepted. |
| E-ULS-0017 | a lane probe at guest PC 0x000da892 reading register R4 = 0x33C3D04 immediately after the guest's control read (the last control store was the lane-logged 0x33C3D06), the lane write-log line 'Unhandled bits: [1] when writing value 0x33C3D06', and a clean in-tree scratch access trace of the whole 0x40024000 window through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | Watchdog register-store semantics. The control read answers the stored value with bit 1 removed (0x33C3D06 stored reads back 0x33C3D04), exactly matching the log's untagged bit; the guest then stores (read | 1) = 0x33C3D05, needing no lane log line (bit 1 clear). Boot's full per-pass WDT sequence is: store 0x33C3D06, read control, store 0x33C3D05, store reload 0xB2 at +0x4, read InterruptEnable (+0x200, 0 before its write), store 1 there. `src/devices/ulsan_wdt.c` stores control with the bit-1 mask, reload plainly (no lane read line exists, so reload reads refuse), and InterruptEnable as a store/read pair; reads elsewhere refuse. Fault chain: BFAR 0x40024200 (write path, first at 12,595,853) -> BFAR 0x40024000 read (fpc 0x000da892, first at 12,595,872) -> BFAR 0x40008858 (fpc 0x0009bf7a, first at 12,595,889) in the TIMER block's lower window, still unmapped. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f26, SP 0x1005ffa8. | Probe + lane line pin the store mask; reload reads, other WDT registers, and any expiry timing remain unobserved. |
| E-ULS-0018 | a lane probe at guest PC 0x0009bf7a (word 0 of a SystemTimer comparator access, base literal 0x40008850) showing R0 = 0x0 for the guest result and the device word 0x40008858 = 0, plus idle dumps 0x40008850/54/58/5c all 0, and a clean in-tree accept-all trace of the whole 0x40008000 window through instruction 200,000,000 | Ulsan `2.35.36.10731-R` | SystemTimer comparator word. The accept-all trace shows the boot phase reads exactly one comparator word, 0x40008858 (once per pass), and performs no comparator writes anywhere; the guest accessor bounds the index below four and reads 0x40008850 + index * 4. The lane answers with 0 at hook time and idles with all four comparator words at 0. `src/devices/ulsan_stimer.c` now serves +0x58 with that 0; +0x50/+0x54/+0x5C reads and all comparator writes still refuse (no observed traffic). Fault chain: BFAR 0x40008858 (fpc 0x0009bf7a, first at 12,595,889) -> BFAR 0x40004800 (fpc 0x0009bdec, first at 12,595,905) at the RTC block (upstream `rtc @ sysbus 0x40004800`). Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f26, SP 0x1005ffa8. | One probe-pinned word; the remaining comparator words stay unmodelled. |
| E-ULS-0019 | an in-tree scratch access trace of the whole 0x40004800 window through instruction 200,000,000 (the only RTC traffic: two reads and stores 0x0E then 0 at +0x0, two reads and a store of 0 at +0x30, a read and a store of 1 at +0x200, a store of 1 at +0x208) and the 2.35.36 lane trace (no RTC log lines at all, while unhandled RTC-register accesses in that device family do log) | Ulsan `2.35.36.10731-R` | RTC register stores. The four touched registers behave like framework-handled registers: writes store and reads answer the store; the observed read values are all 0, matching first reads from reset. `src/devices/ulsan_rtc.c` stores exactly these four registers (reset 0, cleared on machine reset) and refuses all other RTC addresses and widths. With the store semantics the boot frontier reproduces identically to the all-zeros scratch run at the same instruction (no behavioral divergence introduced). Fault chain: BFAR 0x40004800 (fpc 0x0009bdec, first at 12,595,905) -> BFAR 0x40008854 (fpc 0x0009bf7a, first at 12,596,010) at the SystemTimer comparator window's second word. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8. | The lane RTC's counter/timing behavior stays unmodelled because boot reads only these four registers; steady-state read-back values beyond the first reads follow the framework store, not lane counter state. |
| E-ULS-0020 | an in-tree accept-all trace of the SystemTimer window through instruction 200,000,000 on the post-RTC boot path (comparator traffic: store 0 to +0x54 and reads of +0x54, +0x58, +0x5c once per pass; no writes to the other comparator words) and lane probes at guest PC 0x0009bf7a pinning the comparator reads to 0 (the +0x54 probe repeated three times, matching idle dumps of all four words at 0) | Ulsan `2.35.36.10731-R` | SystemTimer comparator words. The deeper boot phase writes comparator word 1 (value 0) and reads words 1, 2, 3 through the bounded accessor (index < 4 over 0x40008850). `src/devices/ulsan_stimer.c` now keeps per-word stores: reads answer the store for +0x54/+0x58/+0x5c and a write stores into word 1 only; +0x50 reads and writes and comparator writes without lane traffic refuse. Fault chain: BFAR 0x40008854 read (fpc 0x0009bf7a, first at 12,596,010) -> write (fpc 0x0009bf68, first at 12,596,019) -> BFAR 0x40008220 (fpc 0x000daa28, first at 12,602,164) at the upstream `timer @ sysbus 0x40008000` block. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f2a, SP 0x1005ffa8. | Only the words with observed traffic answer; comparator expiry/interrupt behavior stays unmodelled. |
| E-ULS-0021 | an in-tree scratch access trace of the 0x40008000 TIMER window through instruction 200,000,000 on the post-comparator boot path (TIMER0 offsets 0x10 read/write 0x2, 0x60 read/write 0x4, 0x68 write 0x4; TIMER1 offset 0x220 five reads and stores 0x1, 0xA20, 0x2, 0; 0x228 stores 0x20 then 0; 0x22c and 0x230 stores 0; nothing else) and the 2.35.36 lane trace (no timer log lines, while unhandled registers in that device family do log) | Ulsan `2.35.36.10731-R` | TIMER block register stores. The seven touched registers behave like framework-handled registers: writes store, reads answer the store, and the observed read values are the reset-0 first reads; reads of write-only-observed registers have no lane evidence and refuse, as do all other TIMER addresses and widths. The fault at 0x40008220 came through the guest timer accessor (base + index * 0x20 + 0x200). With the stores in place the frontier reproduces identically to the all-zeros scratch run at the same instruction. Fault chain: BFAR 0x40008220 (fpc 0x000daa28, first at 12,602,164) -> BFAR 0x40021004 read (fpc 0x000cb054 after an SVC, first at 12,607,607). Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f22, SP 0x1005ffa8. | Lane counter/interrupt behavior of the TIMER block stays unmodelled because boot only reads these three registers; read-back beyond the first reads follows the framework store, not lane timer state. |
| E-ULS-0022 | the in-tree pwrctrl refusal-value trace on the post-TIMER path (one rejected 32-bit store of 0x00100020 to 0x40021004 per pass, first at 12,607,607, at guest PC 0x00096a30 - a store of the cached +0x04 word with bit 5 set) and the 2.35.36 lane trace, which logs no pwrctrl line for that value (only writes touching untagged bits log there) plus the idle and hook-time reads of +0x04 at 0 (probe at 0x000cb054, three idle time points) | Ulsan `2.35.36.10731-R` | Power-controller continuation store. The stored word equals the modelled +0x04 read value 0x00100000 with the driver's bit 5 added, so the guest round-trip is exactly the observed pair; `src/devices/ulsan_pwrctrl.c` accepts (0x04, 0x00100020) as another lane-consistent accepted-discarded write and refuses other values. Reads stay at the modelled word because the read sequence precedes this store in every observed pass; the framework clearing that leaves the word at 0 later is beyond the observed window. Fault chain: BFAR 0x40021004 (value mismatch, fpc 0x000cb054 after an SVC, first at 12,607,607) -> BFAR 0x40054104 (fpc 0x000cb054, first at 12,608,330) in the IOM4 block. Frontier pin (reproduced twice across a reset): budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8. | Only this logged-equivalent value is accepted; post-clear reads stay unobserved. |
| E-ULS-0023 | the in-tree scratch access trace of the 0x40054000 window (one-time boot burst: stores 0x1010/+0x104, 0x1D0E1301/+0x118, 0x0103F270/+0x2C0, 0/+0x11C twice; reads +0x104, +0x118, +0x11C five times, +0x200, +0x210, +0x228 twice, +0x22C, +0x234, +0x23C, +0x240, +0x244, +0x248, +0x280, +0x2C0) plus a live lane probe of all thirteen registers after the burst (0x00001010, 0x1D0E1301, 0x00000E20, 0x00000000, 0x00000002*, zeros, 0x00200000, 0x0000F270; *written later by completion-phase traffic outside the observed window) and the upstream register-collection definitions the lane executes | Ulsan `2.35.36.10731-R` | IOM4 register block. Reads answer masked stores; +0x11C keeps read-only submodule-type bits 0xE20 and clears both module enables when both are set (lane write callback); +0x280 keeps its 0x00200000 reset; fully tagged registers discard writes and read 0. The configuration store 0x0103F270 leaving 0x0000F270 proves the store mask byte-exactly. `src/devices/ulsan_iom4.c` models exactly these offsets 32-bit aligned; everything else refuses. With this block attached no strict MMIO refusal remains on the boot path: zero HardFaults to the new frontier - the machine halts (BKPT at 0x0006bdaa, taken at 12,616,290, SP 0x10029c20, reproduced twice; LR 0x0012cb76) and lane CPU hooks at 0x0006bd98/0x0006bdac never fire across a full lane run, proving the halt is a tree-side data-plane divergence (the trapping function is not lane-executed), not a device gap to paper over. | The DMA data plane and the completion-phase writes (+0x210=0x2, +0x2C0 upper bits) stay unmodelled; the halt frontier closes only via the data-plane instance. |
| E-ULS-0024 | corrected-base disassembly (image loads at 0x00030000) of the halt site, the halt-state dump (LR 0x0012cb76, panic record at 0x1005ffc0 holding magic 0xfe0e8701 and the ASCII tag "96:iom.cpp", registers r1 0xfe0e8700 r2 0x1005ffc0 matching the panic-argument pool at 0x00088838), and lane CPU hooks proving the caller at 0x0012cb76 executes in the lane while the BKPT stub at 0x0006bda8 never does | Ulsan `2.35.36.10731-R` | Frontier semantics at the machine halt. The tree-side BKPT at 0x0006bda8 is the firmware's own terminal halt after its `iom.cpp` (IOM component) assertion - the in-tree emulator BKPT=HALT stop is correct; no device register can close it. The assertion covers the IOM4 boot transaction the lane completes (its IOM master answers the command path) while the register-only tree block leaves it unfinished. The next bounded instance is the IOM4 transaction/DMA data path (command queue execution to the registered endpoint), kept separate from this ticket's register gaps per the per-instance boundary. | Endpoint registration, timing, and command-queue semantics for the specific I2C target stay unobserved for 2.35.36; the 2.44 trace recipe registers ALS on IOM3 only. |
| E-ULS-0025 | instrumented in-tree run of the attached IOM4 block (the path past the former halt is: store 0x10 to +0x11C enabling the I2C master submodule, then 51 polls of +0x248 ending in the iom.cpp assert at instruction 12,616,290), the lane probe reading +0x248 as 0x00000004 at 1 s and after completion, and the upstream register definitions the lane executes | Ulsan `2.35.36.10731-R` | IOM4 submodule-status bits. IOModuleStatus IDLEST (bit 2) reflects the idle submodule state (0x00000004 while idle - the lane state through the whole observed boot) and the tagged ERR bit 0 stores like every other tagged flag in this collection; INTCLR (+0x208, first guest write at 12,610,061) clears status bits over the all-zero observed status, so it is modelled as write-discarded/read-zero. With IDLEST answered the firmware assert is never taken: the BKPT halt at 0x0006bdaa disappears, boot runs past 12.61M, and the frontier returns to the budget stop at instruction 13,000,000, PC 0x001d0f24, SP 0x1005ffa8 (probe30 two-pass, twice). Next refusal BFAR 0x40054128 (fpc 0x000cb054, first at 12,610,074) in the transaction-configuration block. | The command queue doorbell (+0x120), the wrapper DMA lifecycle, and any status bits an executed transaction would set stay unmodelled. |
| E-ULS-0026 | refusal-log laddering of the attached IOM4 block with per-access values (ordered: W 0x218=0, W 0x21C=4, W 0x220=buffer, W 0x124=0, W 0x2C4=0x28, W 0x128=0, W 0x120=0x401), the lane wrapper source showing `PrepareDma` returning immediately when DMA is disabled and forwarding the doorbell to the upstream register collection, the upstream field definitions for each touched register, the reference-model probe reading PW +0x108 (VRSTATUS) as 0 at 1 s and after completion with no unhandled-read warning while +0x10C/+0x110 warn, and the resulting deterministic stop | Ulsan `2.35.36.10731-R` | IOM4 transaction arm and power-block VRSTATUS poll. The observed burst arms a four-byte write command (CMD=1, TSIZE=4) to I2C device address 0x28 with DMA disabled; the modelled stores (0x218 &0x303, 0x21C &0xFFF, 0x220 &0x1FFFFFFF, 0x124 DCXEN only, 0x2C4 7-bit DEVADDR, 0x128 full, 0x120 &0xFF3FFFFF with no queue-completion invention) plus the PW +0x108 read-zero pair clear every MMIO refusal through the boot window: the machine now runs to instruction 12,611,224 and stops in WFI (PC 0x000dabcc, SP 0x10029e40, twice across a reset) - the wake-source plane, not a register gap. | The I2C endpoint at address 0x28, the doorbell's queue-completion semantics (+0x12C/DmaStatus), and any interrupt the executed transaction would raise (IOM4 -> NVIC wiring) are unmodelled by design; the WFI stop is their legitimate boundary. |
| E-ULS-0027 | correctly-based disassembly of the WFI window (0x000dab98 sets a first-idle flag, clears two runtime state bits, marks a sleeping flag, then `bl 0x000975e0(1, 0)` precedes WFI at 0x000dabca; the stop PC 0x000dabcc is the ISB behind it, and 0x000975e0 spins on a RAM flag an ISR sets), lane CPU hooks with the validated single-line-quoting recipe (the WFI entry at 0x000dabca is hit 54 times during 21 lane seconds while the post-ISB continuation at 0x000dabce is never reached), and the tree stop-state NVIC dump (ISER0=0x00000406 with bit 10 - the legacy IOM4 IRQ number - ISER1=0x0F000000, ISER2=0x00000010, no pending bits, SysTick idle) | Ulsan `2.35.36.10731-R` | Frontier nature after the register ladder. The lane enters this very WFI repeatedly and always wakes with an exception vectoring before the ISB (hence the continuation hook never runs); the tree reaches the same WFI with zero pending or levelled IRQs because no Ulsan device emits interrupts yet, so the fail-closed WFI deadlock is correct. The NVIC, scheduler-wake and SysTick wake paths are all modelled in the tree core; what is missing is (a) the IOM4/I2C endpoint and queue-completion data plane that asserts a device interrupt (the NVIC-side enables are already visible in ISER) and (b) for any device to reach `semu_cpu_set_irq` - the machine's `irq_sink` exists and Sapporo SoC devices use it, but `semu_ulsan_board_map` receives only the bus, and passing the sink needs a machine.c edit outside this ticket's allowed files. That one-line pass-through plus the endpoint/queue instance is the named next stop. | Which device asserts the first wake IRQ (IOM4 completion versus the ISER1/ISER2 group at 40-43/68) stays unobserved; the hooking lane run also aborts on teardown with the WFI-entry hook installed, so wake counts are log-line counts, and the tree STIMER block control register read at 0x40003000 refuses (unobserved offsets), leaving tick timing unproven. |
| E-ULS-0028 | lane CPU hooks plus the lane platform wiring (ulsan-platform.repl `using ambiq-apollo4.repl`, the 2.35.36 resc re-registering iom4 as the Apollo4IomDma wrapper repl with `IRQ -> nvic@10`, the wrapper exposing `IRQ => inner.IRQ`), the upstream `AmbiqApollo4_IOMaster` interrupt code (INTEN/INTSTAT 15 flags, INTCLR/INTSET write-1 semantics, `UpdateIRQ` = OR(status & enable), `TryFinishTransaction` setting CommandComplete), the tree stop-state NVIC dump (ISER0=0x00000406, ISER1=0x0F000000, ISER2=0x00000010, zero pending, SysTick idle; legacy `SEMU_APOLLO4_IOM4_IRQ 10` matching the public `ambiq_apollo4p_pac::Interrupt` enum's IOMSTR4 = 10), the firmware vector table at VTOR 0x30000 (IRQ10 vector 0x0015ef26 = a dispatcher stub reading the RAM table 0x10058F78 whose live slot-4 callback is 0x0012ca28), and the lane's observed-device recorder log (73 `Apollo4 IOM I2C@0x28 write/read` messages during the boot window, the first being `write: 7F 01 D0 F0` matching the four-byte doorbell) | Ulsan `2.35.36.10731-R` | The WFI-frontier wake source is identified: the IOM4 command-complete interrupt at NVIC 10. The lane dispatcher stub executes 70 times inside 21 seconds (all other enabled handlers zero times, the post-ISB continuation zero times) - each entry vectors the exception before the ISB retires. The tree already models every register plane feeding this interrupt (INTEN mask equals the upstream bits 0-14, the +0x208 write-discard-over-zero-status matches upstream W1C while status stays unset, IDLEST answers the poll loop); the remaining gap is byte-exact completion: the doorbell must exchange data with the address-0x28 endpoint, set INTSTAT bit 0, and drive the line - which still needs the integrator-owned pass-through of the machine irq sink into `semu_ulsan_board_map` on top of the engine itself. | Whether the observed doorbell passes upstream `IsTransactionValid` (no lane ERROR lines prove it, FIFO fill state unseen) and the bytes the guest later reads from the recorder or the fuel gauge stay unobserved before the WFI; recorder reads must stay unmodelled until a lane observation pins them. |
| E-ULS-0029 | full-trace lane run (`logLevel -1`, 338k lines over 3 virtual seconds) with the wrapper `Apollo4IomDma` source read side-by-side (PrepareDma at line 286, CompleteDma at line 385, recorder class at line 656, constants at lines 1241-1263): every boot doorbell runs the DMA path - 29 `Apollo4 IOM DMA loaded 4 byte(s) from 0x10029C3C/0x10029C1C` lines at Debug level (invisible in Info-level logs, which is why the earlier ladder misread the +0x218 store as always zero; the value-0 store is the end-of-transaction DMAEN clear), each followed by the inner `Transaction received for #I2C#40; command: Write, size: 4` / `Read, size: 1, offset low=0x36|0x27|0x34|0x51...`, `Command completed: Write`, `[no-name]: Setting IRQ`, `nvic: External IRQ 26: True`, `Set pending IRQ HardwareIRQ#10`; the address-0x28 device is a 256-byte register-file recorder (`Write`: first byte selects, the rest store at selected+n; `Read`: returns registers from the selector); wrapper bounds are SRAM `0x10000000..0x10267000` plus a write-direction internal-flash allowance `0x00010000..0x00200000` with refusal otherwise (`DmaError` status + INTSTAT bit 11), command constants Write=1/Read=2, size `(v>>8)&0xFFF`, offsetLow `(v>>16)&0xFF`, offsetCount `(v>>24)&0xF`, DMA direction bit `&2`, and completions clear DMAEN and set INTSTAT DmaComplete (wrapper) with upstream CommandComplete (bit 0) | Ulsan `2.35.36.10731-R` | Doorbell data-plane byte truth. Every observed boot transaction targets device address 0x28 (`#I2C#40`) with a four-byte payload from the guest stack; reads carry offsetCount 1 selecting the recorder register (reads observed at 0x36/0x27/0x34/0x51 - unwritten registers answer zero from the recorder model). This lets the module reproduce the data plane exactly: SRAM payload into the recorder on writes, recorder bytes back into the SRAM buffer on reads, DMAEN auto-clear, internal status bits tracked without exposing them (+0x204/+0x224 stay refused - the guest never reads them in the observed window). | The exact INTEN value the guest wrote is still unobserved: register writes are not traced by default and this Renode build exposes no `python` CLI for reflection, so it is recorded as a remaining gap; it gates only the IRQ-line assertion semantics of the wake instance, not this data plane. Recorder reads for registers previously written, and fuel-gauge (address 0x36) traffic beyond the observed trace, stay unmodelled until observed. Correction (same instance, from the module's test bring-up): the doorbell fields are CMD[3:0], OFFSETEN bit 4, SIZE `(v>>8)&0xFFF`, and OFFSETLO bits 31:24 - the `(v>>16)&0xFF`/`(v>>24)&0xF` positions quoted above are mistranscribed and only the corrected layout is exercised by code and tests. |
| E-ULS-0030 | IRQ lifecycle from the full-trace lane run plus firmware disassembly of the vector-10 path: each of the 70 transaction completions logs `[no-name]: Setting IRQ` -> `nvic: External IRQ 26: True` -> `Set pending IRQ HardwareIRQ#10` -> `cpu: Waking up from deep sleep` -> `Acknowledged IRQ HardwareIRQ#10` -> dispatcher hook `0x0015EF26`, and exactly 70 `Resetting IRQ` lines land inside the handler window before `Completed IRQ active -> inactive`; the IRQ10 ISR at 0x0012ca28 indexes a module table (`add.w r4, r1, r0, lsl #4`) and calls the poll helper 0x0015e780, which loads the per-instance IOM base from the constant pool at 0x0015e9e8 (`0x4005_0000 + index << 12` - the IOM4 entry is 0x40054000), reads the word at base+0x200 (INTEN), then base+0x4 from it (+0x204, INTSTAT), and services/clears status bits during the handler; the trace also confirms the doorbell-read semantics of E-ULS-0029 - the recorder `write: 36` log line IS the OFFSETLO one-byte select write reaching the endpoint before `read: 1 byte(s)` | Ulsan `2.35.36.10731-R` | IRQ-line behaviour: the module now drives an output level `(INTSTAT & INTEN) != 0` through the legacy `semu_apollo4_irq_fn` sink at IRQ 10 (E-ULS-0028), asserting on completion and deasserting when the handler's INTCLR write empties the enabled status; +0x204 becomes a read of the recorded status (proven above), and +0x208 becomes a real write-1-clear (its lane deassert timing is proven above) | The final INTEN value stays untraced, but the seam reads the guest-programmed register so no value is invented; the machine-level pass-through of `irq_sink`/scheduler into `semu_ulsan_board_map` remains the integrator-owned one line (precedent: `map_sapporo` for the NEMA device). Until that lands the sink is NULL and boot behaviour is bit-identical (WFI frontier unchanged). |
| E-ULS-0031 | full-trace lane run (lp34b) cross-read with the lane's `UlsanApollo4Mspi1` plugin source (`emulator/renode/mspi1/SapporoApollo4Mspi1.cs` lines 228-1130, repl `ulsan_mspi1: SPI.UlsanApollo4Mspi1 @ sysbus 0x40061000` with `IRQ -> nvic@21`): the boot tail drives MSPI1 natively - JEDEC identity `C2 25 39` returned for command 0x9F into descriptor 0x10029B90 (twice), payload-free PIO commands B7/35/06 completing with PIO-complete status, zero-value status-register reads (control 0x13, count 1, descriptor SRAM words visible in the retire log), one-byte configuration write via program control 0x17, then the persistence lifecycle: erased native persistence header from the synthetic blank sector at logical flash 0x00010000, sequential `programmed native 64-byte persistence record` writes at 0x00010000/40/80/C0/100..., erased persistence-creation-header probes at the next slot before each program, 64-byte record reads through descriptor 0x10029B38 alternating all-FF and populated words (`00 10 00 00 CB 2C 0C 00...`), and `explicit erased placeholder for absent manufacturing page` (four occurrences); the plugin's own contract constants are PIO control +0x0/address +0x8/command +0xC, start controls 0xC1 (plain) and 0xE1 (addressed), queue controls 0x13 (read) and 0x17 (program), interrupts PIO-complete bit 0 and queue-complete bit 6 at IRQ 21, erased byte 0xFF, flash size 0x10267000, and a 22-stage NativeInitializationStage machine | Ulsan `2.35.36.10731-R` | Identity and post-frontier roadmap for the future `src/devices/ulsan_mspi1.c` module instance: the tree currently maps nothing at 0x40061000, so any post-wake MSPI1 access refuses (fail-closed, not guessed). This row fixes the byte truth that instance must reproduce - command set, descriptor decode (SRAM descriptor at PIO address register, retired queue word in the lane log), JEDEC bytes, zero status values, erased/program record semantics and completion IRQ bits - and its IRQ 21 output lands on the same integrator-owned irq-sink seam as E-ULS-0030 | This is a boot-window observation row, not yet an implementation contract for unobserved regions: read payloads beyond the logged descriptor heads, the OTA-read branch, the 2.44.52-only QPI/0x7A-resume paths, and the plugin's `Refusing Ulsan payload-free PIO command ... command=0xB9` boundary (which belongs to the 2.44.52 evidence set - the 2.35.36 trace is refusal-free) stay out of scope until their own observations land. |
| E-ULS-0032 | full-trace lane run plus the lane's in-wrapper `Max17050` class (`Apollo4IomDma.cs` lines 546-648 - the Ulsan fuel gauge is lane-local, not the upstream Renode part): all 19 gauge transactions in the boot trace are size-2 reads through a one-byte offset select (`#I2C#54; command: Read, size: 2, offset low=0x21 x15, low=0x19/0x09/0x06/0x00 once each`) and the class logs every completed 16-bit read, giving the exact bytes the guest saw: reg 0x00 = 0x0000 (status), 0x06 = 0x3200 (SOC 50%), 0x09 = 0xC000 (VCELL), 0x19 = 0xC000 (average VCELL), 0x21 = 0x0000 x15 (undefined register, battery-polling loop); zero gauge writes occur in the window; the class semantics are 16-bit little-endian byte-pair access with an auto-advancing register pointer at 16-bit boundaries, `FinishTransmission` re-arming the pointer expectation before each transaction | Ulsan `2.35.36.10731-R` | Second endpoint behind the IOM4 doorbell: DEVADDR 0x36 dispatches to this deterministic register model - reads of the four constant registers and the zero answer for the polled undefined register are now reproduced byte-for-byte; the endpoint logic moves to `src/devices/ulsan_iom4_endpoints.c` (module split by responsibility to stay near the size thresholds, guarded by the unchanged engine tests) | Gauge write traffic is unobserved in the boot window; the write path is mirrored from the same class source (pointer + byte-pair stores) exactly like the recorder's store path was, and stays confined to that cited semantics. Gauge state changes across resets are unobserved; reset reloads the five documented register values exactly like the class |
| E-ULS-0033 | full-trace lane run cross-read with the command-queue base class in `emulator/renode/mspi1/SapporoApollo4Mspi1.cs` (lines 15-215): the retire lines fix the register plane - queue control at +0x100 (start when value bits match 0x13; observed starts 0x13 read and 0x17 program), queue address +0x108 (SRAM command descriptor), queue device/flash address +0x10C, queue count +0x110; the interrupt plane is INTEN +0x200 (readable/writable), INTSTAT +0x204 (read-only), INTCLR +0x208 write-one-to-clear, INTSET +0x20C, line = OR(status & enable) driving IRQ 21 - the trace shows `Enabled IRQ HardwareIRQ#21 (37)` and `External IRQ 37: True` asserted at the first queue retire, with the guest INTEN bit 6 acknowledged through INTCLR in its serial-flash ISR; counts appear as 1/3/4/36/64/256 but are validated together with the device-address register (lane `IsObservedCommandQueueCount` reads +0x10C), and every response population is gated by the 22-stage NativeInitializationStage machine with descriptor contents | Ulsan `2.35.36.10731-R` | First MSPI1 slice lands as `src/devices/ulsan_mspi1.c`: queue-register stores/reads, the full interrupt plane with the shared IRQ-21 seam through `semu_ulsan_board_attach_irq_sink`, and queue starts stored but population REFUSED - exactly the lane's own unproven-payload refusal shape, so no unproven flash byte is ever observable to the guest | The response population (JEDEC/status bytes, persistence scan/program records, OTA reads, erased placeholders) is NOT yet modelled: each branch depends on stage transitions the plugin advances internally plus descriptor bytes only partially visible in the trace; those branches port as follow-up instance(s) of this same module, gated by new observations (e.g. post-wake runs with descriptor contents logged). Guest reads of queue-control +0x100 and the reset-time register-zeroing are consistent with the lane dictionary but only the zero/default path is observed |
| E-ULS-0034 | lane source `emulator/renode/mspi1/SapporoApollo4Mspi1.cs` (class `UlsanApollo4Mspi1`: TryPopulateObservedCommandQueueResponse lines 234-845, IsObservedCommandQueueCount lines 847-1240, HandleProductRegisterWrite lines 928-1075, helpers lines 1150-1294) cross-read with the full-trace 2.35.36 boot lifecycle (MSPI1 retires 17:26:10.247-13.04): 20-entry NativeInitializationStage machine (JEDEC return-word contract +0x2C=0x000F4543 in Initial / 0x000F4595 elsewhere with zero destination bytes; payload-free PIO B7/35/06 with accepted-start stage chain; one-byte 0x17 configuration write; zero status reads with the cited stage-transition table incl. BeginPersistenceProgramming clamp of the append cursor to 0x00010000..0x00030000); persistence lifecycle InitializeSyntheticPersistenceRange(FF)-blank sector -> 4-byte header probes / 64-byte record scan with read cursor + re-read contract -> erased-header ends scan -> A5A5A5 sentinel replaced by C2 25 39 -> append header -> status -> WriteStatusReady -> 06 -> 64-byte programs (control 0x17, device must equal the append cursor, cursor advances 64) with the queue-count contract as the first gate; addressed PIO erases only at IsProvenNativeEraseAddress (64K: 0x01590000/0x01600000/OTA half aligned, end<=0x02000000) / IsProvenNativeSectorEraseAddress (0x01FB0000 or the synthetic persistence range), page-program fragments only via IsProvenNativeFlashProgram (NOR may only clear bits over a proven erase); authentic OTA reads count<=0x1000 in 0x00040000..0x02000000; footer 36 bytes at 0x01FF0000 only in PersistenceWriteStatusReady; manufacturing page absent on this boot - four trace lines returned explicit erased placeholder (fixture signature ProductionData absent); completion raises INTSTAT bit 6 only when populated, PIO completion bit 0 only on accepted starts | Ulsan `2.35.36.10731-R` | Instance-29 port lands as `src/devices/ulsan_mspi1_populate.c` / `ulsan_mspi1_identity.c` / `ulsan_mspi1_pio.c` / `ulsan_mspi1_util.c` behind the +0x100 start dispatch of `src/devices/ulsan_mspi1.c`; mapping decisions, recorded: (1) the persistence range 0x00010000..0x00030000 is device-local session backing because the tree's XIP aperture below 0x18040000 is unmapped - the same region the lane itself calls missing low flash and serves from its blank-sector fixture; (2) tree xip_tail [0x18040000,0x1A000000) coincides exactly with XIP intersection [AuthenticOtaStart, AuthenticOtaEnd), so OTA/footer bytes are the bus bytes the lane image provides; (3) HasSyntheticManufacturingFixture answers absent on a bus fault at the unmapped 0x1803F000, matching the observed erased-placeholder outcomes; (4) every bulk response validates all reads and destination probes before committing (lane exceptions replaced by refusal shapes); (5) plane access is 32-bit-only and reset preserves the backing store (lane comment: emulated flash is non-volatile across the emulated reset) | Lane refusal WARNINGs are log-only and are ported as their guest-visible shape: control stored, no INTSTAT bit, no line event; tests/devices/test_ulsan_mspi1_native.c replays the traced boot sequence (identities, PIO chain, configuration write, blank-sector scan, sentinel, program + retained read-back, 4 KiB erase contract, reset non-volatility, OTA-window read outranking the stage machine) and tests/devices/test_ulsan_mspi1.c pins the refusal side; still refused: the Sapporo DIAP4 plane +0x2a0/+0x2a8 (no Ulsan-trace traffic), PIO control/address/command reads (writes are what the trace shows), and every untraced offset; the 2.44.52 descriptor-0x10029AD0 shapes are ported but dormant on this profile |
| E-ULS-0035 | lane wiring `platforms/cpus/ambiq-apollo4.repl` for `Timers.AmbiqApollo4_Timer` at `0x40008000` (`"N -> timerIRQ@N \| nvic@(67+N)"` per-comparator lines plus the CombinedInput to `nvic@14`) cross-read with the full-trace lane log (lp34b, reproducible via `emulator/devices/ulsan/run-2.35.36-smoke.sh`): the comparator line pulses 511 times over 3.3 s of virtual time with boot-epoch gaps 9.6-10.8 ms while the guest re-arms from its ISR (ICWR clears #68 at 10.1828, first ack 10.1926); steady-state register samples at 1.0 s and +3 ms (lp35/lp36/lp37 probes, run twice) show the registered interrupt window 0x60-0x6C answering 0x60=0x04 (guest store read-back), 0x64=0x00, 0x68=0x00, 0x6C=0x00 with 0x10=0x7FF, 0x220=0xA21, and adaptive 0x228 (0x397 at 1.0 s; the boot-trace stores are 0, 0x20, 0x3d), while reads at 0x40-0x5C and 0x70+ emit `Unhandled read ... (GlobalEnable+0xNN \| InterruptSet+0xN)` warnings - the framework registers only 0x60/0x64/0x68/0x6C; the tree boot records 21 stores over the eight-slot plane. Tick derivation: GSTATCLK/2 = 16.384 kHz = 61035 ns/tick, consistent with the lane's 57-64 us per-tick boot epoch. Wake frontier on the in-tree machine (reproduced twice): wake edges IRQ 14 high + IRQ 68 high at instruction 12,650,485 (44+ recorded sink edges; the IOM4 doorbell IRQ-10 edges end at 12,649,174); the guest wake path then faults precisely at pc 0x0007aa66 on RTC `0x40004820` (RTC+0x20), the BusFault escalates to HardFault (CFSR 0x8200 chain, observed with a temporary fault trace since removed), and the handler stores SYSRESETREQ 0x05FA0004 at 0xE000ED0C (pc 0x000c399e), so the machine applies its first software reset at instruction 12,715,657; a budget of exactly 12,715,657 stops SEMU_STOP_BUDGET with PC 0x001e1b4c SP 0x1005ffc0 (the reset-time vector fetch) at virtual_time_ns 16,438,461; the lane never faults or resets there because its RTC counter window is registered (live BCD hundredths counter ~100 Hz at RTC+0x20, CountersUpper +0x24 constant 0x14700101, seqlock double-read in the tree sleep path at pc 0x0007aa5c-0x0007aa6c with BCD decode helper 0x0007aa4c) | Ulsan `2.35.36.10731-R` | E-ULS-0035 lands the comparator wake in `src/devices/ulsan_timer0.c`: the 0x220 enable-bit store arms a compare event at CMP0 x 61035 ns; each event drives the combined line (IRQ 14) and comparator line (IRQ 68) high for one tick (clear scheduled at +61035 ns, matching the lane pulse shape) and re-arms for the counter wrap, so pulses repeat while enabled; clearing the enable bit cancels the next compare (success case `tests/devices/test_ulsan_timer0wake.c`, refusal cases `tests/devices/test_ulsan_timer0.c`). The integrator seam is the one machine.c `map_board` line (precedent: map_sapporo passes irq_sink): one sink+scheduler attached AFTER the board map because device maps reset and detach their wiring; `tests/devices/test_ulsan_mspi1.c` uses the same 3-argument `semu_ulsan_board_attach_irq_sink`. The IRQ 68 service routine (lane hook 0x000c30ff; the tree reaches it at pc 0x000c3240 after the wake) additionally reads TIMER 0x64; the lane answers 0 from its registered store (samples at 1.0 s and 1.003 s, run twice), so 0x64 joins the plane as a plain store like its read-observed neighbours - refusing a write there would fault the guest while the lane framework cannot fault writes to registered interrupt registers | Frontier re-pin is regression-driven: the old WFI_DEADLOCK pins at instruction 12,611,224 / PC 0x000dabcc / SP 0x10029e40 recorded the missing wake source (the E-ULS-0028 wake-source gap), which this entry closes; all twelve frontier-pinning Ulsan tests now assert SEMU_STOP_BUDGET / 12,715,657 / 0x001e1b4c / 0x1005ffc0 and reproduce identically twice (boot tests run two passes in one process). IRQ semantics recorded: guest ISER0=0x406 (IRQs 1, 2, 10) and ISER2=0x10 (IRQ 68 only); IRQ 14 pends un-enabled alongside 68 as the lane wiring implies, and the in-tree WFI release fires on any pending line, matching the lane where only 68 is vectorised. The lane boot-epoch CMP period (~168 ticks) differs from the boot-trace stores (final 0x3d) because the guest rewrites the compare adaptively; the model pulses at the guest-stored value, so first-edge timing follows the guest rather than one pinned period. The RTC +0x20/+0x24 reads remain refused (fail-closed) and are the next gap; the ~100 Hz BCD rate needs one more calibration pass before modelling. The SYSRESETREQ reset cycle at that budget is the machine-reset path working as modelled, not lane behavior (the lane does not reset there at all) |
| E-ULS-0036 | lane counter calibration probe `lp40` (volatile `/tmp/ulsan730/lp40.resc`; `sysbus ReadDoubleWord 0x40004820`/`0x40004824` sampled at 0.1 s, 0.2 s, 0.5 s, 0.9 s, seven 3 ms steps across 0.98-1.001 s, 1.05 s, 1.5 s, 2.0 s, 3.5 s, 5.0 s, 10.0 s, 11.0 s against the registered `AmbiqApollo4_RTC` window, cross-read with the full-trace lane log lp34b and the guest app image `component-04-type-4-v2.raw` at base `0x30000`) | Ulsan `2.35.36.10731-R` | RTC `+0x20` is the live seconds:centiseconds counter as packed BCD hundredths since the lane virtual-clock start: 0.1 s -> `0x00000010`, 0.25 s -> `0x25`, 0.9 s -> `0x90`, 0.983 s -> `0x98`, 0.992 s -> `0x99` (hundredths resolution at 3 ms sampling), 1.001 s -> `0x100`, 2.0 s -> `0x200`, 10.0 s -> `0x1000`, 11.0 s -> `0x1100`; `+0x24` answered `0x14700101` at every one of the eleven sample times. Neither word was ever written in the lane. The guest sleep path is a seqlock at `0x0007aa5c` (literal `0x40004820` at pool `0x0007abdc`): read `+0x20`, read `+0x24`, re-read `+0x20`, retry until stable, then decode the four `+0x20` BCD bytes through the helper at `0x0007aa4c` (bits [23:16] and [15:8] masked `0x7F`, byte fields via x10 BCD; the `+0x24` bit-28 selector adds `0xC8` or `0x64`). The tree models `+0x20` as BCD-packed `floor(semu_scheduler_now / 10 ms)` from the attached machine scheduler and `+0x24` as its store with reset `0x14700101`; writes to both counter words refuse, and `+0x20` reads refuse without the scheduler (fail-closed); the scheduler attaches after the board map per the E-ULS-0035 attach-after-map seam, device resets keep the attach and the epoch restarts with the scheduler | Confidence high for the window (deterministic lane probe, guest read site disassembled byte-exactly). The counter epoch is anchored to lane virtual time at run start (the 0.1 s sample already counts); the tree anchors it to scheduler time, which restarts at warm reset - the lane never resets there, so the post-reset epoch restart is modelled-consistent, not lane-observed. Consequence: the observed boot now passes the sleep-deepening path with no reset through 13,000,000 instructions; the first refused word is `0x40021004` at PC `0x00096a32` (one precise BusFault per epoch escalating to the same HardFault -> SYSRESETREQ chain; CFSR `0x8200`), which the lane answers from a second registered `AmbiqApollo4_PowerController` at `<0x40021000, 0x4002124F>` (lp34b sysbus registration line; the earlier `PWREN`-tagged writes at 10.2436 s belong to registered windows of this kind) - that window is the next gap. Frontier re-pin is regression-driven: the first machine reset moved from 12,715,657 to 14,769,033 instructions with the budget-stop landing unchanged at PC `0x001e1b4c`, SP `0x1005ffc0` (probe46, reproduced twice); the twelve frontier-pinning Ulsan tests re-pinned to the new number |
| E-ULS-0037 | upstream class source fetched from renode-infrastructure blob `add012af003a0f620d3da52828262676f374d121` (Renode 1.16.1 submodule; `src/Emulator/Peripherals/Peripherals/Miscellaneous/AmbiqApollo4_PowerController.cs` and `PeripheralRegister.cs` in the same tree) cross-read with lane readback probe `lp41` (volatile `/tmp/ulsan730/lp41.resc`: `sysbus ReadDoubleWord 0x40021004/0x40021024/0x40021058/0x40021078/0x40021080/0x40021100` sampled after RunFor 0.13 s, 0.23 s, 0.247 s, 3.0 s, 11.0 s), the full-trace lane log lp34b, and the guest app image `component-04-type-4-v2.raw` at base `0x30000` | Ulsan `2.35.36.10731-R` | The 2.35.36 runner registers the raw upstream class at `0x40021000` (the `UlsanApollo4PowerController` wrapper exists only in the 2.44.52 `ulsan-2.44.52-power-model.repl`), so `+0x04 DevicePowerEnable` has reset `0x00100000` with READ/WRITE storage at bits 1-4/5-8/9-12/13/20, TaggedFlag-only (warn-and-discard) bits 0, 14-19, 21-24 and reserved bits 25-31, and `+0x08 DevicePowerStatus` mirrors it on reads (the three 4-bit groups OR-reduce, the ADC and CRYPTO flags copy, everything else reads 0). The TagField warning prints the whole written value (`Unhandled write to offset ... when writing value 0x{original}`; the `originalValue` doc says "the whole value written to the register"), so the lane's whole-value `+0x04` write lines `0x8000`, `0x400000`, `0x40000`, `0x80000` prove the read-modify-write helper (load at PC `0x000969de`, `str r2,[r1]` at PC `0x00096a30`, domain mask deref `[pc,#0x480]` + `[r6,#0x20]`) observed the word as 0 - which it is: the guest's own stores at PC `0x00096a30` (`0x00100020` IOM4 continuation store then the `0x00000000` teardown, both fully handled bits, hence absent from the lane log; recorded in the in-tree instrumented trace behind the earlier pair table) clear bit 20 before the first logged write. Probe samples: `+0x04` reads `0x00100000` at 0.13 s and `0x00000000` at 0.23/0.247/3.0/11.0 s, the latter despite the logged five `PWRENDISP`/`PWRENDISPPHY` write pairs - the tag bits never reach the word. The tree had answered `+0x04`/`+0x08` reads as fixed `0x00100000`, so the helper's OR computed `0x00108000`, the pair-less store refused as a precise BusFault at instruction 14,769,033, and the epoch reset-looped there. The tree now models `+0x04` as the stored word (reset `0x00100000`; every 32-bit write replaces it with `value & 0x00103FFE`) and `+0x08` as the folded mirror (writes accepted with no state change); byte and halfword lanes slice the live words; non-32-bit writes and the other offsets keep the observed-pair refusal unchanged | Confidence high: the model is the framework's own storage semantics from the lane's exact class source, and it reproduces every lane observation (reset-era `0x00100000` reads, the 0 samples at all later times including after the DISP-phase writes, the four whole-value warning shapes). With the plane in place the in-tree epoch passes the whole enable storm fault-free: a temporary fault trace (since removed, `scb.c` diff-clean) recorded zero `armv7m_request_fault` calls through the new frontier, and the guest reaches its own system-reset helper (AIRCR `0xE000ED0C` read-modify-write with key `0x05FA0004` at PC `0x000c399c`, disassembled from the app image). Frontier re-pin is regression-driven: the first machine reset moved from 14,769,033 to 14,756,458 instructions with the budget-stop landing unchanged at PC `0x001e1b4c`, SP `0x1005ffc0` (probe47, reproduced twice); the twelve frontier-pinning Ulsan tests re-pinned and the suite reproduces identically twice. Remaining gap recorded: the guest reaches SYSRESETREQ without any refused access, so the divergence into the reset path is behavioral (the lane never requests reset during the 11 s capture); the MSPI1/DISP lifecycle era of lane time 10.18-13.04 s and the panel-transport poll values are the next frontier candidates. Correction (E-ULS-0038): this entry's fault-free claims were wrong - the temporary trace binary had silently linked a stale static library. Rebuilt traces show epoch 1 took a precise BusFault at instruction 14,756,378 on `0x40061090` (MSPI1 register window), and each later reset was the same BusFault -> HardFault -> SYSRESETREQ chain at successively later unmodelled windows; the reset at 14,756,458 was fault-driven, not behavioral |
| E-ULS-0038 | lane class sources read side-by-side: `emulator/renode/mspi1/SapporoApollo4Mspi1.cs` (`UlsanApollo4Mspi1 : Apollo4MspiCommand` `ReadDoubleWord`/`WriteDoubleWord` default cases), `emulator/renode/peripherals/SapporoApollo4Extensions.cs` (`Apollo4RetainedSystemTimer` wrapper, lines 73-140, its `Reset()` deliberately keeps the NVRAM words), and engine `SystemBus.cs` fetched from renode-infrastructure blob `add012af003a0f620d3da52828262676f374d121` (`ReportNonExistingRead`: untagged fallback returns `default(ulong)` = 0; `ReportNonExistingWrite`: logs and discards), cross-read with the full-trace lane log lp34b (every `non existing peripheral` line for the 0x400B0000/0x400B2000/0x400B2024 blocks; identical in both reference runs), the guest app image disassembly (RMW helpers at `0x0012a1ce`, `0x0009bf7a`, `0x000db168`-`0x000db19c`, `0x000f3818`-`0x000f3854`, pool literals pinned with Thumb Align(PC,4)+imm), and in-tree temporary fault traces (since removed, `scb.c`/`exception.c` diff-clean) | Ulsan `2.35.36.10731-R` | Three unmodelled windows each ended epoch 1 in the same BusFault(PRECISERR|BFARVALID) -> HardFault -> `SYSRESETREQ` chain (reset applied ~80 instructions after the fault): (1) `0x40061090` read at PC `0x0012a204`, instruction 14,756,378 - `0x40061000` is the lane's `UlsanApollo4Mspi1 @ <0x40061000, 0x40061FFF>` (lane Added line), whose class answers unknown-offset reads from its registers dictionary (missing entry -> 0) and stores every unknown-offset write before the product hook; the guest does a read-modify-write on MSPI1+`0x90` (ldr/bic/str at `0x0012a1ce`), so the tree now carries a 1024-word store-through dictionary shadow (named semantics win, `INTCLR`/`INTSET` stay unstored, `+0x204` direct writes shadow without changing live status, reset clears it like the lane dictionary Clear); (2) `0x40008850` read at PC `0x0009bf7a`, instruction 22,678,975 - `0x40008800` is the lane's `Apollo4RetainedSystemTimer @ <0x40008800, 0x4000890F>` whose wrapper intercepts `+0x50..+0x5C` as four NVRAM words on reads and writes and keeps them across Reset (source comment: the four STIMER NVRAM words survive a software reset and carry the next startup mode across AIRCR); the tree had mislabelled them COMP0-3, refused `+0x50` reads and cleared them on reset - now a four-word retained NVRAM plane; (3) the unregistered 0x400Bxxxx accesses: lane fallback lines pin `ReadDoubleWord 0x400B0000/0x04/0x08/0x0C(x10)/0x10(x5)/0x14(x5)/0x18(x5)`, `ReadWord 0x400B0002`, `ReadByte 0x400B0001(x3)/0x04/0x0A/0x0B`, `ReadDoubleWord 0x400B2000` (PC `0xF3846`, guest ldr r4,[r2] + bfi r4,r3,#0x18,#2 with r3=2), `WriteDoubleWord 0x400B2000 value 0x2000000` (PC `0xF384C`), `WriteDoubleWord 0x400B2024 value 0x80000000` (PC `0xDB18E`, pool literal `0x400B2024` verified at `0x000DB280`), `WriteByte 0x400B0001 value 0x0(x2)/0x40`, `WriteByte 0x400B000B value 0x4`, `WriteDoubleWord 0x400B000C value 0x0(x5)/0x10000/0x20000/0x30000/0x40000/0x50000`; the untagged fallback returns 0 by engine source, and no line was ever logged for `0x400B2024` reads or any untabled tuple, so the buszero device became a per-(offset,width[,value]) access table (bytes/halfwords included - the engine reports those widths itself) and the whole-window read-as-zero shape stayed refused | Confidence high: every served value and accepted write is one reproduced lane log line, refusals match the absence of lane lines, and the retained-NVRAM semantics are the lane wrapper's own. With the three planes in-tree epoch 1 runs fault-free past 30M instructions and ends at the next real gap: precise BusFault on `0x400a8074` (instruction 37,491,594, PC `0x00101ef0`) - inside the lane-registered `Apollo4DisplayController @ <0x400A0000, 0x400A8FFF>`, i.e. the queued DISP-era work - HardFault entry at the same instruction (stacked lr `0x000c05e1`), SYSRESETREQ store at PC `0x000c399e`, machine reset applied at instruction 37,491,674 (virtual time 234,445,962 ns, r3=0x41000000). Frontier re-pin is regression-driven and reproduced twice: that budget stops pass zero at the unchanged landing PC `0x001e1b4c`, SP `0x1005ffc0`; pass one - the retained-NVRAM second boot - no longer faults, runs its budget inside the running image and stops at PC `0x000c2588`, SP `0x1005ff70`, LR `0x000c25b7`, XPSR `0x21000000`, matching the lane observation that epoch two never resets in the 11 s capture. Twelve frontier tests re-pinned (pass-dependent expectations); `make test TEST_FILTER=ulsan` 55 PASS reproduced identically twice, `make check`, `make check-lines`, `make sanitize` clean. Remaining gap recorded: model the `Apollo4DisplayController` register the guest first reads at `0x400a8074` (lane class source plus lp34b DISP-era lines are the evidence path) |
| E-ULS-0039 | DISP identity-window plane carries the boot to the assert frontier | lane | `Apollo4DisplayController.cs` (lane-local class: store-through sparse dictionary; `ReadDoubleWord` answers 0xF4=0x87452365 (expected hardware id), 0xEC=0x77 (panel ready), else dictionary-or-zero; `WriteDoubleWord` stores then, for offset 0 (PLAY), ORs bit 4 into offset 0xF8 and raises the line, and clears the line when 0xF8 is written without bit 4; Size 0x9000 covers the +0x8000 DSI-PHY window including 0x400A8074; `[AllowedTranslations(ByteToDoubleWord, WordToDoubleWord)]`; `apollo4-display-controller-ulsan.repl`: `IRQ -> nvic@29`; 2.35.36 profile runs it with TraceWrites=false) | `src/devices/ulsan_disp.{c,h}` (store-through 0x2400-word plane at 0x400A0000..0x400A9000 mirroring the class, merged byte/halfword stores, PLAY/STOP IRQ-29 edge on level change, board-mapped with sink rewiring preserved across machine resets) | exc45 corroboration: lp34b shows exactly nine `External IRQ 45` True/False toggle pairs (12.6683-12.7416 s) and the tree shows exactly nine DISP-sourced exception-45 entries in each epoch (8x EXC_RETURN ffffffed at SP 0x1005ff58 plus one nested-return ffffffd), so both epochs match the lane toggle census. Frontier move: with the window modelled epoch one never faults again (zero BusFault, zero SYSRESETREQ): it runs to the AM_DEBUG_LOG_ERROR(0xB9) path in the SFL-fail window - the logger control struct at 0x10058168 has word +0xc = 0 for every step of both epochs (tree watchpoint: value never changes), the three-retry log wrapper at 0x105554 therefore returns 3, the caller asserts and the stub at 0x0006bdf6 traps BKPT #0 at 0x0006bda8 - machine stops SEMU_STOP_HALT at PC 0x0006bdaa SP 0x1002a7c0 LR 0x000f4881 XPSR 0xa1000000 after 59,251,747 instructions; pass one reaches the identical halt (same PC/SP/LR/XPSR) after 53,420,644 instructions (probe54 budget 200,000,000/4e9 ns, reproduced twice on the uninstrumented library). Lane divergence (next gap, machine-evidenced): the lane never halts in this era - lp34b keeps acknowledging interrupts (194 further Acknowledged IRQ lines, deep-sleep/wake steady era) until capture end at 13.4826 - and a direct Renode 1.16.1 probe (`/tmp/ulsan730/bkptprobe.resc`: BKPT #0 stub in the working aperture, PC set, RunFor) shows the engine executes BKPT #0 with zero log lines at logLevel NOISY and silently continues into the following BX LR, so tree BKPT-halts are engine semantics the lane does not share; the lane-side logger-era values that keep the same assert path deterministic are unlocated - candidate landmarks are the 2016 persistence-scan block rows (last 0x0002FFC0 at 12.4273) and the 2163 retirement rows ending `control=0x17 ... 53 46 4C 20 66 61 6C` (SFL fail) at 13.0408. Twelve frontier tests re-pinned from the 37,491,674-reset stop to this halt frontier (regression, evidence-justified). |
| E-ULS-0040 | assert-era continuation measured under no-op BKPT semantics; adoption recorded as a cross-era integration request, no core change | lane + tree experiment | direct Renode 1.16.1 probe (`/tmp/ulsan730/bkptprobe.resc`: BKPT #0 + BX LR stub with NOP sled written into the working aperture at 0x10266C00, `cpu PC` set, RunFor; full log retained - the engine logs nothing at logLevel NOISY for the BKPT itself and the CPU advances into the sled and slides to the aperture end at 0x10267000 where the core aborts, proving BKPT #0 executes as a silent continuation, matching the ARMv7-M rule that BKPT is a NOP while no debug session asserts DBGEN); lp34b era survival (after the SFL-fail assert era at 13.0408 the lane guest keeps working: 194 further `Acknowledged` lines, 49 `Entering deep sleep`/49 `Waking up` pairs, 141 `External IRQ 30`/141 `External IRQ 84` toggle pairs and 2 further IRQ18 in the final 440 ms, with zero further IRQ26/37/45, until the capture ends at 13.4826); and the deterministic logger-struct evidence of E-ULS-0039 (word +0xc of the control struct at 0x10058168 is 0 from boot in both epochs, so the AM_DEBUG_LOG_ERROR path returns 3 on the lane image too and the lane guest necessarily reached the same BKPT #0 at 0x0006bda8) | experiment-only temporary tree state (BKPT arm of `thumb16_control.c` replaced by `return SEMU_OK`, since reverted, git-clean): two-pass probe at budget 200,000,000/4e9 ns - epoch one crosses its assert and keeps running fault-free, reset-free and refusal-free for 140,748,253 further instructions, stopping at the instruction budget with virtual time 1,013,278,059 ns at PC 0x000b359c SP 0x1002a7ec LR 0x0009c65b; pass one crosses its own assert and stops at the same budget cap with virtual time 887,974,203 ns at PC 0x000b3598 (same SP/LR), both passes landing in the same scheduler/table era (callee lr 0x0009c65b); the run reproduced byte-identically twice | Confidence high on the machine facts, no adoption. The continuation shape matches the lane steady era (deep-sleep/TIMER1-wake cycle era with IRQ30/84 pairs and no further IOM4/MSPI1/DISP interrupts), which makes the tree halt on BKPT the first and only engine-semantics divergence between the tree and the current lane environment. Adoption is deliberately NOT taken here: BKPT halting is depended upon across the repository - 40 `SEMU_STOP_HALT` assertions in 24 test files (25 of them produced by BKPT sentinel instructions inside unit/integration snippet programs, 12 of them the Ulsan frontier boot tests in this ticket's shape, the remaining 3 being two synthetic fault-report struct tests unaffected by CPU semantics and one RTOS guest-record expectation), the `thumb16.bkpt-halt` vector row of `tests/fixtures/cpu/thumb16-control/vectors.tsv` (expected `SEMU_STOP_HALT`), and 8 `tests/integration/*.sh` era runner scripts pinning Sapporo tails literally as `stop=halt pc=0x00079e1e instructions=... virtual_time_ns=...` (E-SAP checkpoints of tickets 727/744/749/750/751/754) whose recorded log-SHA/snapshot prefix chains would cascade-repin from Sapporo-era evidence that ticket 730 does not own. The smallest integration change is therefore a core-semantics decision (execute Thumb BKPT as a no-op per the ARMv7-M no-debug rule, the Thumb32 BKPT.A32 arm staying fail-closed) with a separate era-ticket pass to re-pin the Sapporo era tails against their lane captures; until the integrator decides, the Ulsan frontier stays at the E-ULS-0039 halt (re-verified on the final clean library before this row: pass 0 stop=HALT at 59,251,747 instructions and pass 1 at 53,420,644 instructions, PC 0x0006bdaa SP 0x1002a7c0 LR 0x000f4881 XPSR 0xa1000000, reproduced twice). |
| E-ULS-0041 | adoption: Thumb BKPT executes as a no-op without a debug session; cascade migration of halt-era expectations | lane (E-ULS-0040) + ARMv7-M | The human authorized adopting the E-ULS-0040 lane evidence ("do what is necessary to move forward"): the Thumb `BKPT #imm` arm of `thumb16_control.c` (mask `0xff00` == `0xbe00`) now returns `SEMU_OK` - the dispatcher advances PC and the machine keeps running - matching the ARMv7-M rule that BKPT is a NOP while no debug session asserts DBGEN, which this emulator never presents; the Thumb32 BKPT.A32 arm is untouched and still refuses fail-closed through `armv7m_unsupported` (no lane observation covers it); `SEMU_STOP_HALT` stays in the report/state enum for format stability but is no longer produced by any CPU instruction | full-cascade reproduction on this commit: unit/integration/era suites each reproduced twice identically | Cascade census: `tests/fixtures/cpu/thumb16-control/vectors.tsv` (`thumb16.bkpt-halt` -> `thumb16.bkpt-nop-no-debug`, expected `none`) and `coverage.tsv`; `tests/fixtures/cpu/sleep/vectors.tsv` three rows (`sleep.wfe-pre-signaled`, `sleep.scheduled-wake`, `sleep.svc-pendsv-order` stop `halt`->`none`, the third also final_pc 0x102->0x104; refusal and WFI_DEADLOCK rows unchanged); `tests/fixtures/cpu/systick/vectors.tsv` two stop rows; handwritten C: `test_cpu_thumb16_control`, `test_cpu`, `test_cpu_contract`, `test_cpu_thumb16_memory`, `test_cpu_sleep`, `test_cpu_pendsv`, `test_machine` (run1/run2 now BUDGET; the requested-reset snippet now runs past its sentinel and stops UNSUPPORTED_INSTRUCTION at PC 0x40 instruction 19 - a strictly stronger pin; button-polarity WFI_DEADLOCK PC 0x32 unchanged), `test_machine_snapshot`, `test_cpu_phase2`, `test_nema_refusal`; `tests/support/cpu_guest.{h,c}` gained a `stop_pc` guest-run sentinel (break at PC, not at engine halt); the RTOS guest golden state SHA re-pinned `e4497c731b9ad944edefa89b36e75bf8ab9d6c25601ad27361dbc2cf7da9289e` -> `ae17ae3e6715a583032dbe654ae5e78d0a3965cb8251ef8e7c9abdc059e1bf09` with instruction count 268 and virtual time 344 ns unchanged (the guest now exits at the sentinel PC 0x14a6 instead of halting); the twelve Ulsan frontier boot tests re-pinned from HALT-at-assert to BUDGET: limits {200000000, 4e9 ns}, both passes stop `budget` with pass-0 PC 0x000b359c / pass-1 PC 0x000b3598, SP 0x1002a7ec, LR 0x0009c65b, XPSR 0x61000000 - identical to the E-ULS-0040 experiment - with 58 frontier PASS result lines in two consecutive suite runs; Sapporo era scripts (see drift note): `activity_budget.sh` halt run re-pinned to `stop=budget pc=0x00079e1e instructions=932397950 virtual_time_ns=11388431927` exit 3 with all logo/log/sems hashes and the 21-trigger census preserved (PASS twice); `ctimer_combined_inten.sh` layer-off run capped at the sentinel instruction count 72774982 so the checkpoint keeps pinning the exact BKPT retire boundary, now `stop=budget pc=0x00079e1e instructions=72774982 virtual_time_ns=521257564` exit 3 (PASS twice); `logical_files.sh` and `wbsto_cache.sh` layer-off checkpoints re-pinned the same way (status 3 + `stop=budget` sentinel tail; these two scripts still fail downstream at their September-3 layer-ON artifact-hash pins - pre-existing, see drift note); regression-first: the updated BKPT test fails 10/1 against the halt decoder and passes 10/0 against the adopted one | Pre-existing era-gate drift finding, reported and NOT re-pinned here: six era scripts (`file_seek.sh`, `file_size.sh`, `ohr2_command2.sh`, `ctimer13_inten.sh`, `logical_files.sh`, `wbsto_cache.sh`) fail their cold-run log/snapshot artifact-hash pins identically at current HEAD on BOTH the old halt-semantics binary and the adopted binary (engine-independent, adoption-changes-nothing: their cold runs stop before any BKPT boundary), and the probe cold run reproduces the identical drifted artifacts (`stop=budget pc=0x000a7dda instructions=416256851 virtual_time_ns=1955213393`, log SHA `c5376d7e0012d7638f7f5610b44a7292340b40f959011060ec19e5977dd951e7`) at commits 963bdee, c7800be and 59d6168 - the drift came from accepted Sapporo integrations outside `make check` coverage (era scripts are opt-in behind TEST_PROFILE plus private evidence): the probe cold run byte-reproduces its own pin at cd6b67d (`stop=budget pc=0x00079e1c instructions=416256851`, the instruction immediately before the BKPT sentinel at 0x00079e1c), a second artifact `710f82a81fefa185d4bc578bf9a90ddcab65675450bd8defb9ddf60e47599f3c` is stable across d311da0/e09be61/719d1b7/6555d38, and the current artifact appears at 963bdee and stays through HEAD; `file_size.sh` broke first: at 9f149f4 the same probe cold run still hits its pinned sentinel (`stop=halt pc=0x00079e1e instructions=405895302`), but cd6b67d (the logical-seek success-return fix, 9 minutes later) moved the guest BKPT sentinel from instruction 405895302 to 416256852 - a legitimate behavior change - and re-pinned only `file_seek.sh`, so `file_size.sh` cold/`--halt`-run pins have been structurally stale since 2026-09-05; trigger censuses and the instruction/virtual-time budget lines still match the pins; the four earliest-touched scripts were left byte-identical to HEAD in this commit and the two late-touched scripts contain only the adoption-side layer-off re-pins (their layer-off halves pass on the adopted binary, the failure is at the layer-ON hash stage). Re-derivation of these six gates against Sapporo lane evidence belongs to the era ticket that owns them. Lane-fidelity caveat unchanged from E-ULS-0040: no physical device, the lane engine remains the only machine oracle. |
| E-ULS-0001 | validated SOF extraction manifest, ticket 720 | Ulsan `2.35.36.10731-R` | SOF package SHA-256 `52cea276bd7982aafb7c6052b94135a6ee6d0bf8541382e97cc993f636b592ff`, size 11025350, revision `133145ce`. Header: type 1 v1, load `0xffffffff`, size 5854, SHA-256 `5d2d0bb9f59481328453e2a64ebb8fbfb3cde2262bb9590f53e7adc7c14e5b9b`. Code3: type 3 v2, load `0x00040000`, size 131725, SHA-256 `ed24b1ca93d0bbb518f4e720cceabd685570c708284a49260dca839c2fffa4d3`, leading pair `0x1005ffc0/0x00060149`. Resident: type 5 v2, load `0x00010000`, size 124123, SHA-256 `8de61914e7de7b73e7a8b5a35adfd53332548a3b5611b4667f0fe41f7158d419`, vector `0x1005ffc0/0x000191d1`. Application: type 4 v2, load `0x00030000`, size 1779802, SHA-256 `5e229bb3893ee0ac0748d701bcd339aedb6dfd928250e69504e7d089d6cb5bc3`, vector `0x1005ffc0/0x001e1b4d`. Resources: type 1 v3, load `0x00040000`, size 33292288, SHA-256 `ef2a1358fff0c9d2eb1df31b65296b6e99eea729dc3d4921d0f7901e247ef906` (leading bytes are data, not vectors). Contract: `fixtures/evidence/ulsan/ulsan-2.35.36.contract.semu`. | Extraction identity independently hashed; local manifest strictly parsed and every component byte-verified with the in-tree parser and SHA-256 implementation. Load/address-space semantics deferred to ticket 725. eMMC CID/CSD, ProductionData, and the 256 KiB external-flash prefix permanently unavailable and recorded as unknown; no Ulsan profile exists. |
| E-ULS-0002 | bounded reset-trace pair on the read-only reference lane, ticket 720 | Ulsan `2.35.36.10731-R` | Two fresh `emulator/devices/ulsan/run-2.35.36-smoke.sh` runs (`ulsan-2.35.36-boundary.resc`) produced byte-identical normalized keep-set traces, SHA-256 `f1bf0a8e58037c78cd2f890fed989540d5881fdbbe44ce894d18807d081e20a8` (I2C@0x28 transaction stream, retired-command exclusions, and 12 terminal registers). Terminal: PC `0xdabcc`, SP `0x10029e40`, LR `0x9760b`, CFSR/HFSR `0/0`, IDREG `0x87452365`, display-control `0x400a00f8`=`0x00000000`, MSPI1 address `0x10033fb0` count `1`. The reference wrapper's own assertions are stale against its current model (expects display-control `0x10` and a retired-command log line no longer emitted), so the script exits 1 while reaching the deterministic pause above; the observed raw logs differed only in banner lines and one excluded MAX17050 diagnostic line. | Reproducible reference-model observation only, recorded in `fixtures/evidence/ulsan/ulsan-2.35.36.contract.semu`. Authorizes comparison and ticket 725 profile work, not a controller or compatibility change; no in-tree Ulsan runtime existed for these runs. |
| E-ULS-0003 | validated SOF extraction manifest, ticket 720 | Ulsan `2.44.52.20521-R` | SOF package SHA-256 `276ca7e6bbec83f2e08945e89b67859b090027810e2d314237e0abc25fb15a25`, size 11028954, revision `11ece069`. Header component byte-identical to 2.35.36 and to the Sapporo cross-version header (`5d2d0bb9…`, recorded as observed hash equality with no inheritance claim). Code3 121081 bytes `ac0147b6110c5ac524e70f99dae0aab1883c95afe120f4c7c6283a4e19b8238e` (leading `0x1005ffc0/0x0005d799`). Resident 114669 bytes `3ba7a5b312a2fd4fdb4c327270ec66888f3fc03940bf8b5184233dad221369d9`, vector `0x1005ffc0/0x000191d1` (equal to 2.35.36 despite different bytes). Application 1828547 bytes `64827b1c4653464e5d15de9b3efd01243193604b24891b651f28f5c6da6d34ef`, vector `0x1005ffc0/0x001da733`. Resources 33292288 bytes `81f861d4541d5c9f0e6b0cb73c2b3e44197d5abc6f147e1de8005da9af87f524`. The reference lane's eight translator-signature patch sites are recorded in the contract as reference workarounds, not guest behavior. Contract: `fixtures/evidence/ulsan/ulsan-2.44.52.contract.semu`. | Extraction identity independently hashed; local manifest strictly parsed and every component byte-verified. Reference-lane translator patches must never be treated as firmware facts. Personalized bytes permanently unknown. |
| E-ULS-0004 | bounded boundary run (single) on the read-only reference lane, ticket 720 | Ulsan `2.44.52.20521-R` | One `run-2.44.52-smoke.sh` run (`ulsan-2.44.52-boundary.resc`, `RunFor 1.000`, exit 0) reached terminal PC `0xd47f0`, SP `0x10029d70`, LR `0x779ab`, CFSR/HFSR `0/0`, IDREG `0x87452365`, MSPI1 address `0x10034360` count `1`, NVIC reads `0x28200406/0x0f000000/0x00004004/0x0/0x00012000`; MSPI1 wrote native 64-byte persistence records at `0x00010380/0x000103c0/0x00010400`, each preceded by an erased creation-header read. Normalized trace SHA-256 `756fa2c54a655842a955a9122142ea54a88a7df116469de6178f92fb81a3b32f`. NVM_OTP/INFO1 (`0x42003240`, `0x42003310`) and CRYPTO (`0x400c0fe0`) reads hit absent model peripherals and returned 0. | Single-run reference observation; repeat-pair determinism not yet demonstrated. OTP/CRYPTO zero returns are reference-model absence, never personalization values. Authorizes comparison and 725 work, not a compatibility change. |
| E-ULS-0005 | cross-version Ulsan extraction comparison, ticket 720 | Ulsan `2.35.36.10731-R`, `2.44.52.20521-R` | Header component identical across both builds (and equal to the Sapporo cross-version header byte-for-byte). Resident and application load addresses (`0x00010000`, `0x00030000`) and initial SP (`0x1005ffc0`) identical; resident reset vector identical (`0x000191d1`) while resident bytes differ and shrank (124123→114669); application reset vector differs (`0x001e1b4d` vs `0x001da733`) and size grew (1779802→1828547); resources size exactly equal (33292288) with different bytes; code3 shrank (131725→121081). Ulsan loads differ from Sapporo (`0x00019000/0x00040000/0x14000000`) while initial SP (`0x1005ffc0`) matches. Contract: `fixtures/evidence/ulsan/ulsan-cross-version.contract.semu`. | Exact byte/field comparisons only. No family inheritance is inferred from shared Apollo4 identity or the shared header hash; differing loads are recorded to prevent such inference. |
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
