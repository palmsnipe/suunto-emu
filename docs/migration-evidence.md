# Migration Evidence Ledger

## Purpose and Format

This ledger records facts migrated from `suunto-firmware`, firmware traces, documentation, and synthetic experiments. It contains summaries and hashes, never copyrighted firmware bytes. Each implementation and hardware status change cites stable evidence IDs from this file.

Each future entry must contain: ID, date, source kind and location, product/firmware hashes, observation, confidence, affected modules, validation test, and unresolved questions. Local-only source locations must be described symbolically, such as `$FIRMWARE_ROOT`, rather than with a user path.

Rows that describe an earlier blocked probe remain historical evidence. In
particular, E-SAP-0013 records the pre-705 state in which later-version
profiles were unavailable; E-SAP-0014 and E-SAP-0017 supersede its current
profile and gap conclusions without changing the original observation.

## Seed Evidence

### E-EMU-SAP235-DMA-001

2026-09-19; bounded maintenance against `5980046`. Sources: the complete-range
DMA rule in `docs/execution-model.md`, the byte-wide memory/overlay contracts
in `include/semu/bus.h`, E-SAP-0036's existing IOM4 law, and synthetic cases in
`tests/devices/test_sapporo_iom4_dma.c`. No new lane behavior is inferred.
The implementation previously read memory-to-peripheral DMA incrementally,
mutating FIFO state before discovering a missing tail. Peripheral-to-memory
DMA ignored bus write errors and still set completion after partial writes.
Wide source copies also bypassed one-byte device overlays and incorrectly
rejected transfers spanning adjacent memory regions.

All four added cases fail before correction (`make test
TEST_FILTER=sapporo_iom4_dma`, exit 2; external log
`/tmp/suunto-iom4-dma-before.log`, SHA-256
`275017025a5572643d87338a621e6e9d4bb45d98645700838a17514608a77312`).
The fix admits every byte before changing command/FIFO/gauge/IRQ/memory,
stages up to 4095 source bytes, and propagates the original mapping error.
It preserves the lane-observed invalid-address-window status/IRQ path.
The tests cover missing tails in both directions, a ROM destination,
one-byte device overlays with zero callbacks, unchanged complete controller
state/RAM on refusal, successful retry after mapping the missing tail, and
adjacent-ROM source bytes delivered intact to the gauge.

`make test TEST_FILTER=sapporo_iom4` passes all 13 cases. Two direct runs of
`build/tests/test_sapporo_iom4_dma` give four passes and identical logs
(`/tmp/suunto-iom4-dma-pass-{1,2}.log`), SHA-256
`2d8532b2ee606c721cf28e0366827cb1b5fa5542e13f132e11ca89b226b4dd5c`.
Confidence: high for these synthetic memory-admission properties; this does
not add endpoints, DMA modes, scheduling laws, or firmware compatibility.

The exact private manifest `tests/private/sapporo-2.35.34.18929/firmware.semu`
is validated by the CLI before execution (all three profile-pinned components).
One pre-fix and two post-fix runs of
`build/suunto-emu run --profile sapporo-2.35.34 --firmware
tests/private/sapporo-2.35.34.18929/firmware.semu --max-instructions 1000000000
--max-time 300000000000 --trace /tmp/suunto-235-PASS.trace` reproduce
E-SAP-0036 exactly: `stop=budget pc=0x000e1862 instructions=122878688
virtual_time_ns=300000000000`, exit 3. External logs
`/tmp/suunto-235-before.log`, `/tmp/suunto-235-after-{1,2}.log` all hash to
`3f6e0818a4ba15a39c56299f6da22b30e49ed4ee1e56905a4d6c4b9d8771c938`;
their corresponding traces are empty, SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
No UI progress beyond that cold-run boundary is claimed.

### E-EMU-SAP235-SNAPSHOT-001

2026-09-19; bounded snapshot-safety maintenance. E-SAP-0036 already records
that the live 2.35 RTC and IOM4 are absent from vmstate. The public machine
snapshot contract requires all mutable state. A new synthetic regression in
`tests/devices/test_apollo4_snapshot.c` demonstrates that the old SoC reader
nevertheless accepts a legacy image after selecting the live profile.
`make test TEST_FILTER=apollo4_snapshot` fails that case before the guard
(two existing cases pass; external log `/tmp/suunto-235-snapshot-before.log`,
SHA-256 `df7a6252e68cbbdf109e14035d7cefea8a45ede24743fc073f1120763c9e58b5`).

The SoC writer now refuses whenever either live module is selected; the
reader uses that same writer to build rollback state before consuming bytes,
so both directions refuse without mutation. The test proves unchanged reader
offset, retained FIFO word, empty writer, and an explicit RTC/IOM4 diagnostic.
Two direct runs of `build/tests/test_apollo4_snapshot` pass all three cases
with identical logs (`/tmp/suunto-235-snapshot-pass-{1,2}.log`), SHA-256
`6ff902a02aad1d54639841274b2a12b63e572aba62fc2df37804dbb53cce0712`.
A validated CLI cold run capped at 1000 instructions/1000000 ns with
`--snapshot-save /tmp/suunto-review-unsupported-235.sems` exits 2, reports
`Sapporo 2.35 RTC/IOM4 snapshots are not supported`, and creates no image.
Confidence: high for refusal safety. Full 2.35 snapshot support still requires
codecs for both live modules, scheduler event identity/link validation, and
cold-versus-resumed firmware equivalence. RTC ownership was addressed by the
following maintenance entry. No format change or migration is introduced by
this guard.

### E-EMU-SAP235-RTC-OWNER-001

2026-09-19; bounded ownership correction, preserving the E-SAP-0035 register
and timing law. Source contracts: `docs/architecture.md` ownership/lifetimes,
`include/semu/scheduler.h` owner-checked cancellation, and E-SAP-0035/0036.
The old RTC lived in a process-global `rtc_instance`; mapping any Apollo4
SoC detached it, and every Sapporo machine replaced its scheduler/IRQ binding.
The synthetic public-API probe `/tmp/suunto-review-rtc-isolation.c`
(SHA-256 `48559dbf42ed006813159dfb20ce5608b77193e29079357330f3b201dfda0baa`)
sets machine A's RTC CTRL to `0x10`, creates a distinct 2.22 SoC B, and reads
A's CTRL again. Before correction it changes to zero: two runs exit 1 and
produce identical logs `/tmp/suunto-review-rtc-isolation-{1,2}.log`, SHA-256
`1659c674ef9da202f68d48b43f4f83f9eac97c6004188911ec4dd16e22a97498`.

Each live SoC now owns an allocated RTC, passes it as the existing bus callback
context, and binds its existing scheduler/IRQ dispatcher at profile selection.
Machine creation no longer attaches or clears global state. Reset preserves
instance bindings; reset, rescheduling and destruction cancel only events
whose ID, callback and context still belong to that RTC, protecting IDs reused
after a scheduler reset. Destruction removes pending callbacks before freeing
the instance; the scheduler must outlive the RTC. No public header, register
law, guest cadence, snapshot bytes, or profile selection default changes.

`make test TEST_FILTER=sapporo_rtc_ownership` fails all three new cases before
the correction; log `/tmp/suunto-rtc-ownership-before.log`, SHA-256
`6c0831790e175a264f85654cc8b79d0351ec8cd0199c22f24300e21f0946e605`.
They now pass: creation/reset/destruction of an unrelated profile preserves
the first clock; two live RTCs keep independent clocks, pending alarms and IRQ
sinks; destruction cancels only owned work, including the reused-event-ID
negative control. Two direct binary runs produce identical three-pass logs
`/tmp/suunto-rtc-ownership-pass-{1,2}.log`, SHA-256
`80c39b6caf10dc82867932686aad619494633f2a37ddb99cd4f34af3b6bb0182`.
The original eight RTC cases retain their register, IRQ and three-pass firmware
checkpoint expectations; their fixture now destroys its SoC and filters the
shared SoC IRQ sink to RTC line 2 instead of receiving the old private sink.

The same external isolation probe linked against the correction preserves
`0x10`, exits zero twice, and produces logs
`/tmp/suunto-review-rtc-isolation-fixed-{1,2}.log`, SHA-256
`af78457b0526eb7be626555e8b7a205e20136628b94f7b4f35607e2380d068d9`.
Two final 300-second cold runs use the command in E-EMU-SAP235-DMA-001:
`/tmp/suunto-235-owned-{1,2}.log` still hash to
`3f6e0818a4ba15a39c56299f6da22b30e49ed4ee1e56905a4d6c4b9d8771c938`,
with empty traces of SHA-256
`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`.
Confidence: high for the tested ownership/lifetime correction. The live-module
snapshot refusal remains necessary. This does not establish whole-machine
concurrency for unrelated devices, nor add any new lane or UI behavior.

### E-EMU-SAPPORO-AUDIT-001

2026-09-19; emulator review and bounded maintenance validation against
`5980046`. Sources: exact profile-validated private firmware manifests,
existing firmware gates and their retained baseline census. This entry records
emulator behavior and static diagnosis, not new lane-observed hardware laws.
No firmware bytes, pixels, raw logs, or relaxed goldens are committed.

Final `make check` passes 939 cases, including five SDL cases (exit 0;
`/tmp/suunto-review-check-owned.log`, SHA-256
`8daea1b0699bbb42d65a201543efe636d4c8941100400111c141db876f43dd6b`).
Final `make sanitize` passes 934 cases (exit 0;
`/tmp/suunto-review-sanitize-owned.log`, SHA-256
`636ec7d81cbd4ae4753939be33b0b6f6812ecec57c5304397f55e1eaea86c2f5`).
`make all sdl`, `make check-lines`, `make check-task-contracts` (143 tickets)
and `git diff --check` pass. File-size warnings are now advisory as requested;
the 511-line RTC and an isolated synthetic 501-line file both warn without
failure. These checks do not replace the full private firmware gates below.

The baseline `make check-sdl` exits 2: its short input walk passes, and the
long walk reaches step 30, generation 4403, CRC `fb8e0155`, at 38231199108 ns.
The final tuple is `stop=budget pc=0x0005a8f8 instructions=40000000000
virtual_time_ns=69612174532`, instead of the historical
`stop=halt pc=0x000727ca instructions=14178200857
virtual_time_ns=43790375389`. Its disabled-case branch is not reached.
External log `/tmp/suunto-review-sdl.log`, SHA-256
`e191edfb248a019ea8ab70e471ddb5158d2edeae5241ecd84743fe32a55f9ef3`.
After all runtime fixes, `sh tools/test_sdl_live_input.sh` passes again with
`stop=user pc=0x000bacf4 instructions=774081920 virtual_time_ns=6520939902`;
log `/tmp/suunto-review-sdl-live-final.log`, SHA-256
`228575cb7c370d51649adcd05eb6ae3bc055c22bcd177c7ba3c9381300d5d263`.

Static Capstone 5.0.7 inspection of the 2.22 application SHA-256
`c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`
confirms the budget PC branches to itself; `000727c8` is a breakpoint and
`000727ca` returns through LR. External probe source
`/tmp/suunto-review-222-stop.py`, SHA-256
`cd042bea98c457ebfe07935f3b0761e041dffc5d57ec9bafa86120a490f4d4c4`;
two identical derived logs `/tmp/suunto-review-222-stop-{1,2}.log`, SHA-256
`b6f29f252c03009d0f792baa84fa891052cd18326dceecad6b2de2e72dd5686b`.
Derived census: one terminal self-branch, one historical breakpoint, one
historical return. This identifies the terminal instruction only; no call path
or correction to the existing E-ULS-0041 CPU semantics is inferred.

A single validated 2.33 CLI audit uses `build/suunto-emu run --profile
sapporo-2.33.16 --firmware tests/private/sapporo-2.33.16/firmware.semu
--max-instructions 100000000 --max-time 3000000000 --trace
/tmp/suunto-review-233.trace`. It exits 3 at
`000a4c78 / 100000000 / 372326454`, with two reset requests at `000c97f2`
(times 175577735 and 347614991 ns), and zero compatibility hits. Log
`/tmp/suunto-review-233.log`, SHA-256
`a099676c6537e375c55c2aa6766cceb6b2d24443f71c16559d6a47fa44588213`;
trace SHA-256 `704dfae2d63a06aa106664a54ea6ee76518fe3ed44776ce196215372f939adb7`.
This single audit is not implementation evidence or a working UI claim.

`SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make check-era`
uses full-flash SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`
and the validated 2.39 manifest. It exits 2: 27 of 43 scripts pass; 16 fail.
The derived failure list below uses the suffix after
`tests/integration/test_firmware_sapporo_239_`, before `.sh`:

```text
gpio_wt1 haptic haptic_calibration history_budget lps22
ohr2_boot_mode ohr2_bsl_identity ohr2_echo ohr2_main_identity
ohr2_result_13 ohr2_result_14 ongoing preload1 quiet_read widgets zip_read
```

The sorted names exactly match the retained E-SAP-ERA-GATES-239-001 baseline list
`/tmp/sap235/era_0037_set.txt`, SHA-256
`19e7541c00cdba4bcb1de2bb2e500b328fec8c9748f81275d1de141ca38c4314`.
Current log `/tmp/suunto-review-era.log`, SHA-256
`2c863b372a98b55336672f8b53205df3eb23df5487a91496f985e8ec38aa19a6`.
This is one sweep spanning the maintenance session; binaries were rebuilt
while it ran. The corrected live RTC/IOM4 paths are selected only for 2.35.
The census establishes no additional failing script, not byte-identical
whole-suite behavior or complete 2.39 compatibility. No pin is changed.
Full census repair exceeds ticket 777's current six allowed scripts and needs
an integrator scope update. The long 2.22 endpoint, later-version UI progress,
2.35 snapshot codecs and previously deferred GPS behavior remain open.

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
| E-ULS-0042 | steady-era tree-vs-lane interrupt census after the E-ULS-0041 adoption; divergences localised, no core change, no checkpoint re-pinned | tree experiment + lane (lp34b) | probe `probe70` (volatile `/tmp/ulsan730/probe70.c`, linked against the adopted clean library) runs the two Ulsan 2.35.36 epochs exactly like the frontier harness (pass 0 cold, pass 1 after the harness reset with retained STIMER NVRAM and MSPI1 planes) with limits 4,000,000,000 instructions / 13,500,000,000 ns and logs every board IRQ edge with virtual time and instruction instant; both epochs reach the instruction budget with zero refusals, resets and deadlocks (pass 0 stop=budget pc=0x000b359c vt=4813278059, pass 1 pc=0x000b3598 vt=4687974203, both SP 0x1002a7ec LR 0x0009c65b XPSR 0x61000000 - the identical prefix of the E-ULS-0041 frontier pins at 200,000,000 instructions); the two retained logs (`/tmp/ulsan730/census70_a.log`, `census70_b.log`) are byte-identical (SHA-256 `829347e8cbd668d7e873584bfebddd854decdc0c504fa7cf886989cc165ae8e4`, third run also rc=0). The lane oracle is `lp34b` (`RunFor 1.000` + `RunFor 2.000` = 3.0 s of virtual time; the 13.4826 in E-ULS-0040 is a host timestamp); IRQ number mapping: tree sink line N = lane `HardwareIRQ#N` = lane `External IRQ N+16` (lane acks print both). Comparing the tree pass 0 at virtual time <= 3.0 s with the full lane capture: IRQ26 (line 10) 140 edges vs 140 exact; IRQ45 (line 29) 18 vs 18 exact; IRQ37 (line 21) 4388 vs 4368 (+10 toggle pairs); IRQ30 (line 14) and IRQ84 (line 68) 2302 vs 1022 each (+640 pairs, tree fires 2.25x the lane); lane IRQ18 (line 2) shows 5 edges (True at vt about 0.109, 0.280 and 0.443 s; 2 acks) and the tree shows none. The tree TIMER1 comparator period is exactly the guest-programmed value (median/p10/p90 all 1.953 ms = compare 0x20 x 61035 ns tick), so the IRQ30/84 excess is duty/era not tick rate: the lane period histogram is bimodal (about 0.5-2.5 ms busy band plus a 7-9.5 ms band, 9 gaps over 20 ms, 113 `Entering deep sleep`/114 `Waking up` in 3.0 s), indicating the lane guest or sleep gating stops the comparator line during eras where the tree keeps pulsing; attribution needs a fresh lane probe sampling 0x40008220/0x40008224/0x40008228 across the pulse-train boundaries, which lp34b does not dump. Next gaps in priority order: (1) IRQ30/84 comparator duty mismatch, (2) unmodelled IRQ18/line-2 source (three early-era pulses), (3) IRQ37 +10-pair tail. |
| E-ULS-0043 | attribution census: the Ulsan lane guest reprograms TIMER1 compare0 to a steady 0x397 tickless cadence while the tree guest never reprograms; the E-ULS-0042 duty-gating hypothesis is disproved | ticket 730, lane probe lp43q run twice, tree probe71 and census70 | Lane probe `/tmp/ulsan730/lp43q.resc` (sha256 `5508dd830ab230585ea3d2df071007891db5ac2151d3092bde752965ca1b2a9c`; includes `emulator/devices/ulsan/ulsan-2.35.36.resc`; 112 `RunFor` chunks totalling 24.6 s vt, sampling `0x40008220`/`0x40008228` after each) run twice from the lane repository root: `/tmp/ulsan730/lp43q1.out` (sha256 `1b1223ad93646900012ee12a526993bfa6c3c886b5e5ac1f5017f791fc64ed23`) and `/tmp/ulsan730/lp43q2.out` (sha256 `768549f4ee581e8397b1be7cfe56222631a83d1e3401107b8f7884e45547e799`); the register-pair stream is identical for the first 92 samples (vt <= 11.6 s); Renode aborted host-side near the tail of both runs (exit 134) at differing points, so only the common prefix is cited. Census: TIMER1 control stays 0x00000A21 (enabled, no gating) from 0.14 s to 14.6 s; compare0 timeline 0xFFFFFFFF pre-init, 0x64 at 0.14 s, 0x20 at 0.16 s, churn 0x107/0x103/0xA8/0x107/0x103/0xA8/0x108/0x102/0x11A over 0.20-0.36 s, 0x20 at 0.38 s, 0x352 at 0.45 s, 0x20 at 0.50 s, 0x388 at 0.60 s, then 0x397 = 919 ticks (56.09 ms at the 61035 ns tick) from 0.65 s with micro re-anchors 0x394 at 1.05 s, 0x386 at 1.60 s, 0x396 at 1.70/2.10/2.70 s, 0x391 at 3.30/3.90/6.35 s each returning to 0x397 within one second, and a deep-era re-anchor to 0x20 at 11.1 s. Tree side: probe71 (probe70 plus every-2000th-edge bus samples, source sha256 `3a141a44b9893c436d0ac898b861f8ac7feaf44e54ea349cb2a1c76d71a18bd0`, logs probe71a.log and probe71b.log byte-identical sha256 `259eb6a421fcf2c1866e4e820367d1904edf74165b728cb466995a1dcf0f1858`) shows the guest-visible control read 0xA21 (same enable regime) and the read-refusal sentinel at 0x228 (the model refuses compare reads by design per E-ULS-0021, a readability observation about the tree, not guest behaviour); the E-ULS-0042 census70_a.log (sha256 `829347e8cbd668d7e873584bfebddd854decdc0c504fa7cf886989cc165ae8e4`) pass-0 per-second TIMER pulse census is 127/512/512/512/416 pulses with median gap 1.953 ms, i.e. constant compare 32 through the full 4.81 s run, so the tree guest never executes the lane's compare-reprogram sequence. Static scan of the private 2.35.36 `application.raw` found exactly two literal sites referencing the 0x40008000 timer base (0xdab2c, 0x10a718, both inside a timer-driver register-helper cluster), consistent with reprogramming flowing through one driver path. Attribution: the E-ULS-0042 IRQ14/IRQ68 +640-pair delta is a cadence-regime difference, not gating or tick rate: the tree guest keeps the boot-era 32-tick cadence, and the E-ULS-0035 re-arm model (control store re-arms at the current compare value) is byte-unchanged and would have followed any new compare value; the divergence lives in guest state gating the reprogram path. Next gap: read-census of the lane wake/re-anchor path inputs versus the tree answers for the same registers (candidate first: the 0x64 INTEN read the tree answers 0), no engine change is justified by this entry. Tooling notes: the lane `@emulator` include resolves against CWD so probes must run from the lane repository root; Renode 1.16.1 exposes no `WatchMemory`; the deep-vt DebugMonitor pending storm floods global NOISY logs, so register-sampling probes should stay quiet.
| E-ULS-0044 | INTSTAT read-census: the Ulsan guest never branches on a `0x40008064` read value (both reader sites discard it), so the tree's constant-zero INTSTAT answer is not an engine gap; the lane-vs-tree compare-reprogram divergence localises to the STIMER time-delta feeding the reprogram computation, and the E-ULS-0043 "never reprograms" summary is corrected to "reprograms with values the wake tail rewrites to 0x20" | ticket 730, lane probe lp46 twice, tree probe72v2 twice plus control probe72nh, capstone static RE | Lane probe `/tmp/ulsan730/lp46.resc` (sha256 `9c9a24e58f00ef3ee2f4474b16a5f5166a11962ad075eeb72f27d1031213e5f8`; includes `emulator/devices/ulsan/ulsan-2.35.36.resc`; 92 sample groups each `emulation RunFor` then `sysbus ReadDoubleWord` of `0x40008060`/`0x40008064`/`0x40008220`/`0x40008228`; 11.600 s total virtual time) run twice from the lane repository root through `/tmp/ulsan730/run_lane.sh` (sha256 `43494037ac9b931da9a02db9c95cdaffb36a965453aba0df9923e3fa8da400c7`; Renode 1.16.1 with `--config /tmp/ulsan730/renode.cfg` and `DOTNET_BUNDLE_EXTRACT_BASE_DIR` redirected to /tmp): raw logs `/tmp/ulsan730/lp46a.out` sha256 `83fb6287b94a81b4d73e641f5638c6eb878f2c885ccac82a2f8875bc4942714d`, `/tmp/ulsan730/lp46b.out` sha256 `6c47b650e464d818f07cc6932b8ec0786ca095d8bbf266c7f8fe88819113ec89` (timestamps only differ), extracted 368-sample value stream byte-identical both runs sha256 `f54d28a2456cea12554ae7c9eebc9a0f16524f41417d8986b77738bfc14294bc`. Lane census: `0x40008060` answers 0x0 at the 0.02 s sample then 0x4 from 0.14 s on; `0x40008220` 0x0 then steady 0xA21 from 0.14 s (confirms the E-ULS-0043 control plane); `0x40008228` reproduces the E-ULS-0043 compare0 timeline (0xFFFFFFFF@0.02, 0x64@0.14, 0x20@0.16, 0x107/0x103/0xA8 churn 0.20-0.30, 0x108/0x102/0x11A 0.32-0.36, 0x20@0.38, 0x352@0.45, 0x20@0.50, 0x388@0.60, 0x397@0.65 steady, micro re-anchors 0x394@1.05/0x386@1.60/0x396@1.70-2.70/0x391@3.30-6.35, deep 0x20@11.1); `0x40008064` answers 0x00000000 at 90 of 92 sample boundaries, the only nonzeros being 0x00000004 at 11.10 s and 11.60 s (the deep-wake pending window), so even the lane answers the live event bit only transiently at chunk boundaries. Tree probe `/tmp/ulsan730/probe72.c` (sha256 `59265e1d836e327f4a5bc3c9ef64e182ccf13b44649f58534c53c1c003f2f15a`) with companion instrumented copy `/tmp/ulsan730/ulsan_timer0_p72.c` (sha256 `a0bb81100f6cd7cbb210f436b7d011f35106a37a8244d32ad32ebcc49a60900a`, object sha256 `ee6257db8acc82c1742bf9ad4f2dd7d9e1f8267bbdb25b7106e079bc25fec88e`, linked ahead of `build/libsemu.a`; adds only a NULL-initialised observation callback invoked after the model commits a legal access value) and board IRQ sink injecting `semu_cpu_set_irq` exactly like probe71; binary `/tmp/ulsan730/probe72v2` sha256 `c3cfc27817d42b22adeb23544fbe9a784e08bb5ca41f0d10c624768239b21f96`; two full two-pass runs byte-identical, log sha256 `32bb13afcb61c558c40a1d3549e5ffcaa062f0c0d03bc90bc7c842ce5e3745de`, PASS pins equal the E-ULS-0042/43 frontier pins exactly (pass0 inst 4000000000 vt 4813278059 ns pc 000b359c sp 1002a7ec lr 0009c65b xpsr 61000000; pass1 vt 4687974203 ns pc 000b3598; both stop=budget, zero refusals); control `probe72nh` (same link, callback never armed) reaches identical pins, proving observation neutrality. An early variant that omitted `semu_cpu_set_irq` diverged (inst frozen at 12609348, budget stop pc 000dabcc WFI, IRQ10 line stuck high); its two runs are byte-identical, log sha256 `12f39ccb3a082bc07415bf8f20cfc3e69d8240d9f31097816e1680f45627f6e8`, and the callback-never-armed control `probe72nh.log` (same link) reproduces the same frozen pins, log sha256 `2b827b8b28a420910053c1972a4d169a1cdd128e42b160c7e889c6787e44bf51` - the pair is recorded as an invalid harness (a guest whose NVIC never sees the board lines cannot service anything), not as evidence, and the identical frozen pins double as proof that the observation callback is neutral. Tree census (pass0/pass1): reads at `0x64` 61151/54374, every one answered 0x00000000 (PCs: wake tail 0xc3240 n=40/37, IRQ68 service 0xc310a n=61111/54337); stores at `0x68` = 0x00000004 n=61152/54375, all from helper store PC 0xdab24 (helper 0xdab20, literal 0x40008068 at 0xdab30); read at `0x60` n=1/1 answered 0x0 immediately before the single 0x60<-0x4 store (PCs 0xdab1a/0xdab1e, helper 0xdab0e, literal 0x40008060 at 0xdab38); stores at `0x228` n=82/76 (init store 0 from 0xdaa36; 81/75 from 0xdaafe = helper 0xdaaf2 CMP0 store, base literal 0x40008000 at 0xdab2c) with values boot 0x20@inst12601538, 0x3d@12636773, wake 0x20@16360129, then 0x69@18413047, 0x20@24821946, ..., pass0 tail 0x36e@791214237, 0x20@844803193, 0x1a3@844871240, 0x20@870445128; pass1 0x2f4@676986717, 0x20@723129400, 0x129@723197528, 0x20@741325146 - so the tree guest does execute the reprogram path in both epochs (E-ULS-0043's "never reprograms" was a snapshot artifact: its every-2000th-edge and per-second censuses sampled pulse-time compare values dominated by the wake tail's unconditional 0x20 rewrite, and the median-gap 1.953 ms hides the occasional long periods); pass0 IRQ14/IRQ68 edge totals 4158/4158 are consistent with the census70 steady ~512/s. Callback PCs are logged +2..+4 past the access instruction. Static RE (capstone 5.0.7 over the manifest-pinned `application.raw`, Thumb): the guest readers of 0x40008064 are exactly two - the wake tail 0xc323a-0xc323e (literal pool 0xc3264=0x40008064 read into r1, r1 immediately clobbered by the tail branch target 0xc30ae `movs r1,#0x20; movs r0,#1; b.w 0xdaaf2` = unconditional CMP0<-0x20 store) and the IRQ68 service 0xc3106-0xc3108 (`ldr r0,[r1]` r1=0x40008064, r0 immediately clobbered by `bl 0xc2b76` -> 0x9bf48 -> 0xc2bc8, the STIMER-backed 0x40008804 RAM-cache tick getter); no branch consumes the loaded value on any path, so the tree constant-zero INTSTAT answer cannot steer guest control flow: E-ULS-0044 is not an engine gap and no core change is made. The reprogram computation is 0xc315a(r4 = requested OS ticks): gate `bl 0x9c81e` reads only a RAM control block (literal-relative fields 0x00/0x38/0x4c/0x60; returns 0 only when both 0x00 and 0x60 read zero - no MMIO input), skip test 0xc3190-0xc3196 compares against r7 = now-stamp (stamp stored by 0xc30b8 at 0x1000b488 from the same STIMER tick getter), computation CMP0 = ((ticks-1)<<5) - elapsed stored via 0xc31a2 -> 0xdaaf2, restart via 0xc31a8 -> 0xdaa4e (control |= 1 re-arm); the second literal site 0x10a6f2-0x10a706 is a frequency helper using literal 0x40008000 (pool at 0x10a718) for indexed CTIMER-page CFG(+0x8) reads with /100 scaling, executed in neither epoch (0x40008008 absent from the tree access census). Era note for ticket 777: in the current 2.35.36 era the boot INTCLR/INTEN dance runs through the 0xc30b8/0xdaaXX driver cluster at inst ~12.60 M; the 0x4629x-era INTEN/INTCLR/INTSTAT touches listed in E-ULS-0021 did not reappear in the two frontier epochs and await the ticket-777 era-script re-derivation. Next localisation candidate for the lane-versus-tree reprogram-value divergence (lane steady 0x397 vs tree full-period values): the STIMER time window (0x40008804 via 0xc2bc8) now-stamp advance across sleep, recorded as the E-ULS-0045 follow-up candidate. |
| E-ULS-0045 | STIMER COUNT clock law: the lane answers a virtual-time-driven 32000 Hz free-running counter (frozen at 0 until boot writes LOAD; the guest's three `cpsid i`-scoped reads inside one tick-getter call always read equal), while the tree advances COUNT +1 per read so `now - stamp` counts getter calls instead of ticks - the confirmed engine divergence behind the E-ULS-0043/E-ULS-0044 compare-reprogram-value divergence; engine change deferred as an era-adoption step, not taken silently mid-instance | ticket 730, lane probes lp47+lp48 twice each, tree probe73 twice, capstone static RE | Lane probe `/tmp/ulsan730/lp47.resc` (sha256 `095997122a17dcb3ea93561a38c2e9498b504fc22791a6e107c569f243211639`; includes `emulator/devices/ulsan/ulsan-2.35.36.resc`; 300 groups: `emulation RunFor 0.005` then `sysbus ReadDoubleWord` of `0x40008800`/`0x40008804`/`0x40008850`/`0x40008900`; 1.500 s vt) and `/tmp/ulsan730/lp48.resc` (sha256 `99f0b6ae1001ac9e14bc7a200e65f092fbbc72afbe5bdd041e7d36f7c30db5b0`; same four reads at vt=0 pre-RunFor and at 0.001 s) each run twice through `/tmp/ulsan730/run_lane.sh` (sha256 `43494037ac9b931da9a02db9c95cdaffb36a965453aba0df9923e3fa8da400c7`); lp47 raw logs `/tmp/ulsan730/lp47a.out` sha256 `8bc84c31399aa4d67fc640aa19c25a5a3fc7ff8c8b952dab43fe0f8b1ff56345`, `/tmp/ulsan730/lp47b.out` sha256 `dd4a7d9daf2da9cd479114a1ff5423fb876e7eb04aa6058ea97e6800571de753`, extracted 1200-sample value stream byte-identical sha256 `9684dd90f0570badab09c896c655962e6d982c7e0dfd874946d68df9129a7797`; lp48 raw logs sha256 `5613dbb304d8a1de79aad0d130c4a0d6aecbc54209abf4d1257e5f97c9a24729` / `2baf1fc5bdf739d87e19a730ad2faa2795494f30e53e8725fd8bc2b9609bf0a1`, streams identical. Lane census: at vt=0 (pre-RunFor) LOAD(+0x00)=0x80000000, COUNT(+0x04)=0, NVRAM0(+0x50)=0, CONTROL(+0x100)=0; during execution LOAD reads 0x80000000 through 0.13 s and 0x303 from the 0.135 s sample on (absolute store, not RMW - the read-back stays a clean 0x303 although the earlier read saw 0x80000000); NVRAM0 and CONTROL answer 0 at all 300 samples; COUNT answers 0 at samples 0.005-0.12, then 0x7C at the 0.135 s sample, and exactly +0xA0 (160 ticks) per 5 ms for 273 consecutive samples (delta histogram {0x0 x25, 0x7C x1, 0xA0 x273}), final 0xAB1C at 1.500 s - i.e. 32000.0 ticks/s, a 31.25 us tick, starting 0.1311 s (0.135 - 0x7C/32000); monitor sampling cannot perturb this law (read-driven would have to out-pace the guest's own ~2073 getter reads per 5 ms chunk with only 1 monitor read). Tree probe `/tmp/ulsan730/probe73.c` (sha256 `001271ed131d086a3dd4d1b9dc9a3c032e6d09b5a7d0c6853fdb4104ad3e8366`) with instrumented copy `/tmp/ulsan730/ulsan_stimer_p73.c` (sha256 `69f7da1928952fbc87c4bd3f602becc32de45e459e9e57308236fa09c4ac4fad`, object sha256 `d3a86d557289f5a11251b405c9a1a1123fe736e30d2fbc3de538885e2f0aeb0c`; sole delta a NULL-guarded post-commit observation callback; diff-verified) and NVIC-injecting sink; binary `/tmp/ulsan730/probe73` sha256 `0b94abb32f0a26225c9276f66e148fb41efaaa45d481783cb868bac36e24024b`; two full two-pass runs byte-identical, log sha256 `60f9e15ac1205ce6fb63e89fbe9d9674ab7fd3a8fac2c20a256c138d2464106a`, PASS pins byte-equal to the E-ULS-0042/43/44 frontier pins in both passes (pass0 inst 4000000000 vt 4813278059 pc 000b359c sp 1002a7ec lr 0009c65b xpsr 61000000; pass1 vt 4687974203 pc 000b3598), proving observation neutrality for a device the earlier probes had not touched. Tree census (identical structure both passes): COUNT reads = 3 x 221037 (pass0) / 3 x 202649 (pass1) at PCs 0xc2bd2/0xc2bd4/0xc2bd6, answered as consecutive integers starting 0x00000000 (vt 12594863) and ending 0x000A1E46 = 663110 = total pass0 COUNT reads - 1, i.e. the answered value IS the read index; LOAD read x2 answered 0 then absolute writes 0x80000000 (vt 12594818) and 0x00000303 (vt 12594831) at PCs 0x9bf3e/0x9bf40; CONTROL read once v=0 @0x9bf8a (vt 12594846) + write 0 @0x9bf90; NVRAM reads +0x58/+0x54 x1 v=0 @0x9bf7a (vt 12595211/12595332), write +0x54 v=0 @0x9bf68 (vt 12595341), +0x5c x3 v=0 @0x9d832 (pass0 vt 12595724/29084268/32810123), +0x50 x1 v=0 @0x9bf7a (pass0 vt 32810123, pass1 vt 28903894); no other window offset is touched in either epoch and no refusal occurs. Static RE (capstone 5.0.7, Thumb, `application.raw` @0x30000): helper 0xc2bc8 = push {r1,r4}; cpsid i (0xc2bce); `ldr r1,[r0]` 0xc2bd0, `ldr r2,[r0]` 0xc2bd2, `ldr r3,[r0]` 0xc2bd4 with r0 = 0x40008804 (literal pool 0x9bfac = 0x40008804 loaded at 0x9bf4c); pop; triple stored to the caller buffer at +0/+4/+8; bx lr 0xc2be2. Tick getter 0x9bf48: v1=[sp+4], v0=[sp], cmp, `it ne; ldrne r0,[sp+8]` - it returns the middle read when the first two agree, else the third: a 3-sample stability read. Consumers per E-ULS-0044: 0xc30b8 stamps the value to RAM 0x1000b488; 0xc315a computes r7 = now - stamp feeding the wake reprogram test 0xc3190-0xc3196 and computation 0xc31a2. Divergence mechanism, fully determined: on the tree the triple is always (n, n+1, n+2) so v0 != v1 always takes the unstable branch and `now` advances exactly +3 per getter call (clock quantum = getter-call count, 221037 calls in pass0); on the lane virtual time cannot advance inside the cpsid-scoped instruction sequence, so all three reads answer the same 32000 Hz tick, the stable branch is taken, and `now` advances by real elapsed ticks; cross-correlation of the two independent censuses closes the loop: between the TIMER init store (inst 12601493, probe72v2) and the boot computed compare store 0x3D (inst 12636773) the tree log shows exactly 6 COUNT reads = 2 getter calls, and 0x3D = (3-1)<<5 - 3 is exactly the computation with elapsed = one getter call's worth of tree ticks. Attribution: the engine gap is STIMER COUNT clock semantics only - tree COUNT is a read-advance counter (the E-ULS-0014 law, pinned from the single-sample lp40-era probe, now superseded by this census), lane COUNT is virtual-time-derived at 32000 Hz from the first LOAD write; CONTROL/NVRAM store-back already matches the lane in this epoch, and the LOAD reset-value difference (lane 0x80000000 vs tree 0, observed by lp48 at vt=0 and read by the guest at 0x9bf3e before its absolute 0x80000000 write) is value-discarded in both epochs and steers nothing here, but should be matched for fidelity in any engine change. Proposed engine change, deferred as its own adoption step rather than taken silently mid-instance: COUNT = 0 before the first LOAD write, else floor((vt - enable_vt)/31250), LOAD reset value 0x80000000, enable_vt latched from the first LOAD write; this is guest-visible timing and WILL move the budget-stop frontier census PC and the 2.35.36-era script pins, so it requires the E-ULS-0041-style era adoption with human authorization plus era-script re-derivation and `make sanitize`, tracked as the E-ULS-0046 candidate; silent re-pinning is prohibited by AGENTS.md. |
| E-ULS-0046 | STTMR engine-law adoption (the E-ULS-0045 deferred change, taken as an explicit non-silent era adoption with human authorization, E-ULS-0041-style): CONFIG(+0x00) is a plain store with reset 0x80000000 (E-ULS-0014 bit-31 mask retired), STTMR(+0x04) is a read-only live virtual-time counter derived from the scheduler clock at the lane CLKSEL table rate (CLKSEL 3 = exactly 32000 Hz, not silicon 32768), held while gated, base captured only on CONFIG gate transitions; the first adoption fold re-anchored per read and FROZE the counter at 184 under the wake loop's ~170 ns getter cadence - found by the in-era instrumented probe and fixed to a read-pure floor((now-anchor)*hz/1e9) law, after which the frontier re-parks via a wake-overflow TIMER0 re-arm wrap and the wake-reprogram overflow becomes the next ticket-730 instance | ticket 730 (human authorization: "I go with your recommendations, continue" approving the E-ULS-0045-proposed adoption), lane-class + firmware-contract RE reports, tree probes pindump46b/probe72x/probe73c each twice byte-identically, make sanitize | Law (lane class audit `/tmp/ulsan-stimer-window-report.md` sha256 `5bbc5c0845d8c2e1a4ccabdce3267c072f7be25c261e60b7970afe993d0abe0e`: window answered by Timers.Apollo4RetainedSystemTimer (lane SapporoApollo4Extensions.cs:73) delegating to upstream Timers.AmbiqApollo4_SystemTimer; CONFIG plain store, upstream reset 0x80000000; STTMR is a LimitTimer over machine.ClockSource, Enabled = !FREEZE && !CLEAR && Frequency != 1, reads 0-while-gated held never cleared, STTMR writes dropped by the lane; CLKSEL 1=6 MHz, 2=375 kHz, 3=exactly 32000 Hz (lane models XTAL_32KHZ as 32000; the lane is the sole oracle), 4=16 kHz, 5/6=1 kHz, invalid indices not enabled; NVRAM 0x50-0x5c retained across reset; unregistered in-window offsets answered warn+0 upstream and keep refusing in the tree since no era epoch touches them; +0x108 INTCLR is written only by the IRQ40 vector handler 0xc2bc0, which no tree IRQ line ever raises, so it stays refused). Firmware contract (static RE report `/tmp/ulsan_stimer_contract.md` sha256 `ad04c7b9f7b5f81771c0fbcc0154c624bd26fe2c6b2f26e78860c67d926d42ae`, capstone 5.0.7 Thumb over the manifest-pinned application.raw @0x30000, MRS/MSR hand-decoded around 0xc2bc8): boot stores CONFIG 0x80000000 then (old & 0x7FFF00F0)|0x303 via 0x9bf38 callers at 0xc2b52/0xc2b60 - with the 0x80000000 reset the RMW still lands exactly 0x303, matching the lp47 read-back; the 3-sample getter 0xc2b76->0x9bf48->0xc2bc8 returns the middle of three cpsid-scoped [0x40008804] reads when the first two agree; wake math stamps the tick at RAM 0x1000b488 (0xc30b8/0xc30c2), r7 = now-stamp at 0xc315a, CMP0 = ((ticks-1)<<5) - elapsed stored via 0xc31a2 -> 0xdaaf2; the BASEPRI ladder gated on elapsed>>5 at 0xc3114/0xc321e was literally dead under the old +1/read law (elapsed counted getter calls, always 0 after the first call within one tick). Model: src/devices/ulsan_stimer.c (sha256 c735cd38a9266e3c57053acd04a626d7318f240f2db35481a58f854512f878ce) + ulsan_stimer.h (sha256 7fc6c419971a33d3cc02541f9cb55d30929034d03c9e6ae24861d5b91fea1589) + board attach call in ulsan_board.c attach_irq_sink (sha256 d3f2fc72239d5b5052bac79f1df6e500b629fa1c1233570148add13ef1aa6337); without an attached scheduler the virtual-time registers refuse (fail-closed). Unit coverage tests/devices/test_ulsan_stimer.c (sha256 e3285e7d326ecd6317afa3c370b6976f85b3a97ad00bbe33196788d030d023e8): CONFIG reset read 0x80000000, boot RMW result 0x303, triple equality, exactly 1 tick per 31250 ns, exactly 32000 ticks per 1e9 ns (kills the silicon-32768 law), FREEZE holds across +1e6 ns, re-enable resumes (32002 after +62500), freeze regression of 400 reads spaced 100 ns apart crossing exactly one tick, NVRAM survives semu_bus_reset while CONFIG reverts to FREEZE, full refusal matrix (no-scheduler CONFIG write/COUNT read; 0x08/0x0C/0x104/0x108/0x1F4 both directions; STTMR write; sub-word NVRAM). Interim adoption record: the first fold passed unit tests but in-era instrumented probe `/tmp/ulsan730/ulsan_stimer_p73b.c` runs `/tmp/ulsan730/p73ba.log`=`/tmp/ulsan730/p73bb.log` (sha256 85fc8c093188a15d4907d1e49176851e513494697774c1aa0f935271f433a1e2, 148M access lines) showed getter triples (n,n,n) - the stable branch - pinned at v=0xb8=184 = (18344831-12594831)/31250 while virtual time ran to 4003844874 ns: each ~170 ns-spaced read truncated its sub-tick remainder at anchor=now; the interim TIMER0 census `/tmp/ulsan730/p72wa.log`=`/tmp/ulsan730/p72wb.log` (sha256 aec7fb4854650a539598053b878d1d3a14bf2253466f261c7aa734a0b2fe0549; probe72.c sha256 59265e1d836e327f4a5bc3c9ef64e182ccf13b44649f58534c53c1c003f2f15a + instrumented ulsan_timer0_p72.o sha256 ee6257db8acc82c1742bf9ad4f2dd7d9e1f8267bbdb25b7106e079bc25fec88e) measured that interim engine's 4G frontier (pc 0009c824 sp 10029e60 lr 000c318d xpsr 21000000 vt 4003844874 both passes; 2042 wake IRQ14/68 and 21 IRQ10 pairs per pass; 6 computed 0x228 re-arms 0x3f/0x20 + wake-tail rewrites) and the interim 200M frontier dump (sha256 ae279d6361c523e68d4c9e6f665e223ee352d18e7920ebe3b3d16f1991bc9a26: pc 0009c3ae sp 10029e78 lr 0009c7f3 xpsr 61000000 vt 203844874 at inst 200000000); both interim frontier sets are superseded by the fix and recorded here (and in the re-pinned test comments) rather than discarded. Final-law verification: instrumented copy `/tmp/ulsan730/ulsan_stimer_p73c.c` (sha256 8bf6cea0e328815fcac5b78b67c1d5c44abd0148102755573898e30cfdc63d11), binary `/tmp/ulsan730/probe73c` (sha256 8371059b97a908b3c7d091ae74328ef0c20fc47bcf505f0e084500c7fefa39e4), log `/tmp/ulsan730/p73ca.log`=`/tmp/ulsan730/p73cb.log` (sha256 9ddc2d8299e08cd48cfcfee9508f498196c42f8b75305f3bcfadac192ffb08d9): all 15603 getter triples per pass have three equal answers and every one equals floor((vt-12594831)/31250 exactly (machine-checked 0 unstable, 0 law violations over 46809 answered COUNT reads per pass); boot CONFIG reads answer 0x80000000 at vt 12594817/12594830 before/after the 0x80000000 store at 12594818 (PCs 0x9bf3e/0x9bf40) then 0x303 stored at 12594831; CONTROL read/write once v=0 at 0x9bf8a/0x9bf90; NVRAM r+0x58 @12595211, r+0x54 @12595332, w+0x54 v=0 @12595341, r+0x5c @12595724 (all v=0) - the window's legal access structure is byte-identical to the E-ULS-0045 census shape, zero refusals (PASS err empty both passes); final answered tick 0x1c2=450 at vt 26676314 = floor((26676314-12594831)/31250) exact; instrumented PASS pins equal the uninstrumented ones exactly (observation neutrality). Final frontier (`/tmp/ulsan730/pindump46b.txt`=`pindump46b2.txt` sha256 c5a90475530e4890693ce7bf89871208ef9e2f0c664b4fd379c23130867648b3, harness /tmp/ulsan730/pindump46.c sha256 987a20553a392e17b7ec921fc1575bd32415aa7a842bfff9170f9ae24008f912, limits {200000000 inst, 4e9 ns}): stop=budget at the vt cap in both passes with inst 16667327 pc 000dabcc sp 10029e40 lr 0009760b xpsr 61000000 vt 262143351559124 (= about 2^32 timer ticks plus wake offset), zero refusals; the same pins reproduce under probe72x and probe73c. Final-law TIMER0 census (`/tmp/ulsan730/probe72x` sha256 6ab2c4c27aa7a8585a814735fc9ab4630f6fa2eea0b8bb21d6ff334c6edeb0bb, log `/tmp/ulsan730/p72xa.log`=`/tmp/ulsan730/p72xb.log` sha256 d20a30d9db64eb1055d4f7639ab67e9575735dbf7d64305afd9e4389725907ee): per pass 21 IRQ10 pairs, 5 wake IRQ14/IRQ68 level-ups, 0x228 stores 0(init@12601493) 0x20@12601538 0x3f@12636773 0x20@16482208 0x65@20449974 0x20@26615010 then 0xffffffbe@26676042 (PC 0xdaafe) - the final re-arm is the wake computation wrapping negative (0xffffffbe = 0 - 66) exactly 66 ns after the IRQ14 at vt 26614541, arming a one-shot ~2^32 ticks out; the guest then WFI-parks to that event (frontier pc 0x000dabcc is the re-arm WFI, matching the E-ULS-0002 lane terminal PC 0xdabcc) - the wake reprogram overflow under real ticks is the next ticket-730 instance (candidate: TIMER0 wake-event delivery vs the lane cadence per E-ULS-0043/0044); IRQ21/IRQ29 did not appear in either epoch under this frontier. Adoption impact: 12 device-test boot frontier blocks (test_ulsan_{stimer,bootrom,buszero,clkgen,daxi,gpio,iom4,otpinfo,pwrctrl,rtc,timer0,wdt}.c) re-pinned to the new dump with old pins (E-ULS-0041 pc 0x000b359c/0x000b3598 era, and the interim frozen-fold dump ae279d63) cited in the comments; no ULSAN-profile era scripts exist (all tests/integration era scripts are sapporo profiles and sapporo never mounts ulsan_stimer), so era-script drift surface for this change is zero and ticket 777's stale Sapporo 2.39 pins are untouched; the E-ULS-0042-era steady-era IRQ census claims measured the pre-adoption engine and are flagged for re-run in current-status. Validation: make test TEST_FILTER=ulsan_stimer (3/3 incl. freeze regression), make check rc=0 (903 PASS lines, 0 FAIL, 2 firmware-gated SKIP), make check-lines rc=0, make sanitize rc=0 (device change). Era note: pre-adoption claims of E-ULS-0014 (read-advance COUNT, bit-31 mask) and E-ULS-0042 (4G-instruction frontier pins) stand as history of the old engine; the boot-era wake IRQ totals 4158/4084 and the interim 2042 are superseded by the 5-wake final census. |
| E-ULS-0047 | ticket 730 census instance (wake-PARK/wake-reprogram overflow census named by E-ULS-0046); lane probe `/tmp/ulsan730/lp49.resc` sha256 `473fae0ec97c08f69642066c688bc7019cba7cd49ed04aea0ca0221ab18e270f` run twice through the E-ULS-0044-pinned `/tmp/ulsan730/run_lane.sh` harness (raw logs `/tmp/ulsan730/lp49a.out` sha256 `29c7193071362ddf323f91c4d1e6feb2cf2ac689e20166c782c24d11684656fc`, `/tmp/ulsan730/lp49b.out` sha256 `aa22d52ccf4e29e82ad0ad0ef7dc52e2c05d097897dfec92c0547730015f808e`, timestamps only differ; extracted 800-sample value stream byte-identical sha256 `5c17b4a036cfd4f786150fbd7d12d1e3859c57c40dfb7a07c5950157d285b155`); tree probes each twice byte-identically: `/tmp/ulsan730/probe74.c` sha256 `0b5cfada23d53c3d240f8f4a68c878eeee1207c768f797b867179b49e3bf68eb` binary sha256 `274841fde8da757698dcf68414d2fe67d865ba82206545690d1bc0e7dceca905` (probe72 harness, limits 200,000,000 instructions and 3e14 ns elapsed), logs `p74a.log`=`p74b.log` sha256 `ac4595ed9ad6e7a54f93d4f458486d9a79460e3a08c491c143b08abe56529889`; `/tmp/ulsan730/probe75.c` sha256 `db353a92440e7d74be5fed62726301cb0a4cd9769d76459da2b85f76ed55498a` binary sha256 `48d4ce82d8bfa832dc91cf7aaad14c05cf1b98d6dd76f8aa72d24c5eb94644d0` (1M-instruction step-walk to 300M), logs `p75a.log`=`p75b.log` sha256 `5f126c51dd96d59713c380991d6f6496560ff9ab4fc99c459a493f99724808c0`; `/tmp/ulsan730/probe76.c` sha256 `e8c08567f278a505577b8bdda462d30b8937a45f711cbab82a78caff0cbfe112` binary sha256 `1f6247454e8ab6efb7239c184ddf6bfb0b02da4facb02176434d9935fbac1a62` (walk to 700M, prints r0/r1), logs `p76a.log`=`p76b.log` sha256 `5e9a377748a4f24060a976ff19763f8fb443a25325bfc9934e59ccf4c5b8571a`; all tree probes link `ulsan_timer0_p72.o` source sha256 `a0bb81100f6cd7cbb210f436b7d011f35106a37a8244d32ad32ebcc49a60900a` ahead of `build/libsemu.a` (observation-neutral per the E-ULS-0044 control argument); capstone 5.0.7 Thumb RE over the manifest-pinned `tests/private/ulsan-2.35.36/application.raw` sha256 `5e229bb3893ee0ac0748d701bcd339aedb6dfd928250e69504e7d089d6cb5bc3` @0x30000; probe sources and logs live outside the repository tree | Ulsan `2.35.36.10731-R` on the E-ULS-0046 post-adoption engine (profile `profiles/ulsan/2.35.36`, manifest per E-ULS-0001; the STTMR law of E-ULS-0046 is the engine under census) | Lane boot-epoch CMP0 census at 0.5 ms granularity: `0xFFFFFFFF` through 131.0 ms, then the arm sequence 0x20@131.5, 0x34@132.0, 0x20@133.5, 0x64@137.5, 0x20@140.5 ms and the wake-cycle onset 0x107@198.5 ms; NO sample reaches 0x80000000 or above in 0.5..200 ms - a 2^32-scale arm would persist through hundreds of consecutive 0.5 ms samples, so the lane never arms a 32-bit-negative compare in the boot epoch. Tree same-epoch `0x228` stores (per pass, byte-identical): 0@12601493 (init), 0x20@12601538, 0x3f@12636773, 0x20@16482208, 0x65@20449974, 0x20@26615010, then `0xffffffbe`@26676042 (PC 0xdaafe) - the tree boot sequence matches the lane arm-for-arm through the fifth store (0x20/0x3f-vs-0x34/0x20/0x65-vs-0x64/0x20) and then diverges into the negative arm exactly where the wake-path elapsed subtraction (units of 1/32 tick) first exceeds the requested ticks. Boot epoch per pass: 4 wake IRQ14/IRQ68 rise-pairs (first 0x68 access exactly +454 instructions after each rise), 21 IRQ10 pairs, zero refusals. Post-wrap census (probe74, limits {200M inst, 3e14 ns}): the `0xffffffbe` arm fires one FULL 2^32-count cycle later at vt 262,143,351,559,124 ns (262,143.18 s at the calibrated 61,035 ns TIMER0 tick), where the E-ULS-0046 4e9-ns-elapsed-budget run had stopped exactly at that event; the wake service consumes exactly 61,035 instructions = 61,035 ns (CPU-bound accounting), drops both lines, and the guest then enters the BASEPRI-ladder loop at 0x000c320c-0x000c3228 (`bl 0xc2b76` tick getter; r1 := now; r0 := (now - stamp) with stamp at [r5,#4]; stamp := now rounded down by 32; r0 >>= 5; then `adds r1,#1; cmp r1,r0; blo` spin) - with the 32-bit-wrapped elapsed this is 127,926,510 iterations = 383,777,306 ns of pure CPU catch-up work. probe76 (walk to 700M with r0/r1): the ladder exits at inst 400,444,548 (vt 262,143,735,336,345; r0=0x079ffbee r1=0x0002d1ee at seg=400), the wake tail rewrites CMP0 0x20@inst 400444563, the 0xc315a recompute stores `0xffffcfa9` (-12,407 - negative again because the ladder duration itself is the new elapsed) at inst 400445067, and that arm fires at vt 524,286,308,940,131 ns (inst 400,445,430) - again one full 2^32-count cycle later - raising wake pair #6; the second ladder is in flight when the 700M walk ends (seg=699 pc 0x000c3228, r1=0x05f39cd9 still counting to 0x079ffbee). Per-pass totals: 6 wake rise-pairs, 21 IRQ10 pairs, 9 `0x228` stores, zero refusals in both passes of all three probes. Attribution: TIMER0 matches its class contract on every byte-exact observation (unsigned 32-bit compare semantics - both negative arms fire one full 2^32-tick count cycle later, at 61,035 ns/tick; IRQ14/IRQ68 double rise; wake-tail and service loop shapes unchanged from E-ULS-0043/44); the lane-versus-tree boot divergence is NOT a controller gap: the tree charges virtual time at exactly 1 ns per executing instruction (`update_run_accounting` in `src/boards/machine_run.c`: time_delta = instruction_delta while not waiting), so guest-internal wake-path latency is 1 ns/instruction, while the lane advances virtual time in CPU-bound code by Renode real-time throughput (a host-speed artifact, not a device fact under the no-device constraint) - the lane small wake-path elapsed is therefore not even a stable reference point, and no lane observation exists that pins a different TIMER0, STTMR, or wake-delivery behavior to implement | Determinism: every tree log byte-identical in its two runs; lane value stream byte-identical; raw lane logs differ only in timestamps. The census closes the E-ULS-0046 next-instance candidate: the wake-PARK and wake-reprogram overflow are BOUNDED (one ladder per cycle) and self-sustaining (each cycle: wake at a full-count-cycle park, ~384 ms ladder, wake-tail 0x20, one negative re-arm), so the tree steady state after overflow is a deterministic ~262,143 s virtual-time wake cadence, not a livelock and not a refused stop; next stop recorded as the in-flight second ladder at the 700M walk bound. NO ticket-730 controller implementation is authorized by this census: TIMER0 already implements all observed byte-exact behavior and the residual divergence lives in the core CPU cadence law (1 ns per instruction), whose adoption would move every era pin of every product and is core-scope integrator decision territory (adopt, document as accepted divergence, or re-scope); changing any src/devices or src/soc module here would violate the 730 evidence rule (implement only what the evidence names). Remaining ticket-730 census queue from prior instances is unchanged: IRQ18/line-2, IRQ37 +10 pairs, panel-era census, and the post-adoption steady-era IRQ census re-run flagged in current-status; this entry supersedes no prior row - it completes the E-ULS-0046 prediction. No firmware bytes committed; no physical device, so device truth remains lane-limited and the lane is real-time-bound inside CPU-bound code |
| E-ULS-0048 | IRQ18/line-2 source census AND observed-only implementation (ticket 730 queue item named by the E-ULS-0042-era claims and the E-ULS-0047 queue): the lane's three IRQ18 pulses are the RTC one-second alarm (`rtc: IRQ set` -> `External IRQ 18: True` -> `Set pending IRQ HardwareIRQ#2 (18)` -> `Waking up from deep sleep`), armed once at boot by the observed stores `0x200=1`+`0x208=1` (`First alarm set to: 1970-01-01T00:00:01, alarm repeat interval: Second`) and repeated by the model itself (`Alarm occurred at: t; next alarm: t+1 s`), the line drops at the NVIC acknowledge (`Acknowledged IRQ HardwareIRQ#2` -> `rtc: IRQ reset` -> False) with NO RTC MMIO in between - the guest never reads any status register; the tree's silent line 2 is now a scheduler-backed observed alarm pulse in `src/devices/ulsan_rtc.c`, the E-ULS-0036 write-refusal on the two counter words is superseded by observed service stores, and the boot frontier re-parks from one alarm to the next (four 1-second IRQ2 wakes inside the 4e9-ns budget, matching the lane's 1-second wake cadence shape) | ticket 730; lane full-trace capture lp34b re-derived twice as lp50 through the pinned `/tmp/ulsan730/run_lane.sh` harness (three-way byte-identical filtered IRQ18 stream); tree scratch probe77 v1 (log-explosion snapshot) and v2 (twice x two passes byte-identical); committed-engine census probe79 twice byte-identically; frontier dump pindump48 twice byte-identically; capstone 5.0.7 facts reused from E-ULS-0044/45/47 REs; make sanitize | **Lane facts**: retained full-trace log `/tmp/ulsan730/lp34b.log` sha256 `9499492370b8df3e9acb9de3e477ceae7c3540907a479ca8aba91e34bacd4f43` (logLevel -1) re-run as `/tmp/ulsan730/lp50.resc` sha256 `44fa5a1ea81febedbf1a56f94c5851b49eb1ef82c56d85c6aabf380caadb64e5` via `/tmp/ulsan730/run_lane.sh`, raw logs lp50a.out `0d537088df456e9ff8a5fd54ae3e68da33fa4928e42fb25e5ec8f89d1c2df8d8` / lp50b.out `864f31b1e00d19675bf969bd227fcd245f298fa3bc78b1c7539c1a95ad1c6465` (timestamps and host-timing lines only); the timestamp-stripped filter `rtc: \|External IRQ 18\|HardwareIRQ#2 (18)\|Waking up from deep sleep\|Priority .* interrupt 18` gives 140-line stream sha256 `b59d51db6d2db96b80dbcd9d044a986caaeb0a7462f9fffc5f3c6b3718add164` IDENTICAL across lp34b and both lp50 runs; category census of that stream: 1 `New date time set: 1970-01-01T00:00:00.00` (once), 1 `First alarm set to: 1970-01-01T00:00:01.0000000, alarm repeat interval: Second` (once, right after 4 `Priority` writes for interrupt 18 and the `Enabled IRQ HardwareIRQ#2 (18)` line), 3 `Alarm occurred at 00:00:0N; next alarm 00:00:0N+1`, 3 `rtc: IRQ set`/True/pend triples, 2 `Acknowledged IRQ HardwareIRQ#2` + 2 `rtc: IRQ reset` + 2 False + 2 `nvic: Completed IRQ HardwareIRQ#2 active -> inactive` (2nd/3rd ack outside the 3.0 s window), 114 `Waking up from deep sleep`; between True and False the guest touched NO RTC register (the first drop straddles a RunFor pause/resume, the second is same-instant). **Wiring**: bundled lane platform `/Users/cyril/projects/suunto-firmware/.tools/renode/Renode.app/Contents/MacOS/platforms/cpus/ambiq-apollo4.repl` sha256 `e1f8560bd32f82fb2aaf7e9a8877b1b1c4aff7bd0a35821f8761a76243b74376` lines 142-143: `rtc: Timers.AmbiqApollo4_RTC @ sysbus 0x40004800` -> `nvic@2`; the class is Suunto-fork-only (upstream renode tag v1.16.1 -> submodule sha `add012af003a0f620d3da52828262676f374d121` has no Ambiq peripherals dir - GitHub API check), so the format strings above were extracted from the AOT binary and behavior comes from lane logs + tree probes, not from unavailable class source. **Tree scratch probe77**: harness `/tmp/ulsan730/probe77.c` sha256 `e4e2267fefdc90921ae3837e445ca1f3b8bd1aa92c685f13e566a47e835128d0` (probe72 mechanics, limits 400M inst / 2.2e9 ns, A/E/R access lines); v1 instrumented object (store-answer plane, line never falls) exposed the guest IRQ18 service in full at the first occurrence (vt 1,012,595,271): TIMER wake tail first (`W 0x68=0x4` at pc 0x000dab24, `R 0x64=0` at pc 0x000c3240, `W 0x228=0x20` at pc 0x000daafe - the E-ULS-0044/47 wake tail), then RTC service stores `+0x208=1` (pc 0x0009bf1e, every service), clock dance `R +0x0=0x0E`, `W +0x0=0x0F` (pc 0x0009be3e), `W +0x20=0x100` (pc 0x0009be70), `W +0x24=20230101` (pc 0x0009beae), `R +0x0=0x0F`, `W +0x0=0x0E` (pc 0x0009beb6), then the E-ULS-0036 seqlock triple `R +0x20/+0x24/+0x20` (PCs 0x0007aa66/0x0007aa68/0x0007aa6a); the guest NEVER reads +0x1c or any other status word (no unobserved-offset access occurred in 1.85M+ service iterations before the v1 log was truncated at ~2 GB - head snapshot `/tmp/ulsan730/p77a_head.log` sha256 `ca05035dd5b9997918df8faeb20e55afc79dfbc71fbedca72ff733d6f21d1eee`, E-line extract `/tmp/ulsan730/p77a_E.log` sha256 `f559eaf52e03e282c9eed37e9290cfdf24f815d39c22adc3232e540a86b50781`): with the line held high the guest re-services RTC on every sleep attempt (1,851,834 `+0x208` stores counted) but never livelocks - the lane's acknowledge-side drop therefore has no guest-MMIO clear mechanism to model, matching the log's Acknowledged -> IRQ reset ordering. **v2 faithful mix** (`/tmp/ulsan730/ulsan_scratch_p77.c` sha256 `55d10e6983dd3dd52470352ebfd22611e6f1a1ac6ef034f59328ce49d3c141fd`, binary probe77v2 sha256 `649a6781f26bf8d648920fc2cd3777fbc211e9a4fb6b3ff4542ca4c83618d9ca`, log `/tmp/ulsan730/p78a.log`=`p78b.log` sha256 `c61bd78520094b940b5296f1184c05ca90fccb46bfb2dc5e3d6f9837b46f944a`): live-counter reads + accepted `+0x20/+0x24` stores + momentary 61,035 ns line pulse + occurrence-scheduled repeat; 4/4 passes identical (stop=budget inst 16791415 vt 3012717065 pc 0x000dabcc), zero refusals, three alarms; confirmed the `+0x20/+0x24` stores must be accepted (the guest writes them at every service) and that a store-cancel-reschedule re-arm adds ~61 us/period phase drift the lane stamps do not show (v2 measured 1,000,060,876 ns intervals) - hence the committed module keeps the occurrence-scheduled repeat and makes the observed service rewrite a no-op while an alarm is pending. **Committed model** (`src/devices/ulsan_rtc.c` sha256 `a33e4b2de6eb48100288117b3ae3c21e202967f4a511b1e377111a69fd50eebd`, header `0c6a6b48a4c421b986e4d44f5030d2324c20cceaab098c28accb9cac03726e8a`, board attach `282e97f952368c28c45af256f2f82a5478c5775f84974f9d33fcdccdfcfc3a7f`): stores 0x200=1 AND 0x208=1 arm line-2 IRQ one second ahead (either store order, arm once, no reschedule while pending); occurrence raises IRQ 2 through the machine sink, schedules the drop 61,035 ns later (E-ULS-0035 momentary-pulse convention - the lane True->False span is a host-timing fact across the ack, unpin-able in vt), schedules the next occurrence +1e9 ns (lane `next alarm` stamp); +0x20/+0x24 stores accepted (+0x20 store kept unobserved by the live-counter read, +0x24 answers its store); alarm values other than the observed 1/1 pair store without arming (unobserved); everything else still refuses; reset cancels events and drops the line. **Committed-engine census** (probe79 = probe74 harness with elapsed 4.3e9, `/tmp/ulsan730/probe79.c` sha256 `a26a699438ada747243a468c4f64e1a4d6c7ded6fca8f326ff015d5a9db347de`, binary `c25e343de8c18f72021eb0e40af82ed7421d6e908e8de1c0d4994fd3d3295aa7`, log `/tmp/ulsan730/p79a.log`=`p79b.log` sha256 `83b9c39611663864b835da91f2550b4cc8cc0d4cbf9a4bdc927ef45dd37477ff`): boot era through the 4th wake pair (vt 26,675,576) is byte-identical to the E-ULS-0046/47 census (all 7 pre-alarm `0x228` stores at the same instants, 42 boot line-pulses, 4 wake pairs, `0xffffffbe` arm at inst 16666964); the first alarm fires at inst 16667327 - exactly the old E-ULS-0046 frontier instruction (the old engine had WFI-parked to the 2.6e14 overflow there; the alarm is now the earlier event); 5 IRQ2 rises at anchor+1e9*N (anchor 12,595,271 = the boot pair-store instant, exact 1-second cadence, no drift), 4 falls at +61,035 ns each, each service consumes exactly 61,035 instructions (= 61,035 ns under the E-ULS-0047 wake-service invariant) so the drop lands at service end - the lane's across-acknowledge shape; after each service the wake-tail `0x228<-0x20` + recompute `0xffffffbf` park persists (15 stores total, the E-ULS-0047 overflow conclusion unchanged: the RTC alarm gives the guest its per-second wakes but the TIMER0 cadence divergence is untouched - it lives in the core CPU cadence law per E-ULS-0047); zero refusals, PASS pins equal both passes (inst 16915545 vt 5012595271 pc 0x000dabcc sp 10029e40 lr 0x0009760b xpsr 61000000 at the 4.3e9 budget). **Adoption impact**: frontier dump `/tmp/ulsan730/pindump48a.txt`=`pindump48b.txt` sha256 `56f4b2b8f752ce555af266d9d816948c95a30fcb0dc0d7442cc765e41ddc892e` (harness pindump46.c `987a20553a392e17b7ec921fc1575bd32415aa7a842bfff9170f9ae24008f912`, limits {200M inst, 4e9 ns}): stop=budget inst 16853480 pc 0x000dabcc sp 0x10029e40 lr 0x0009760b xpsr 0x61000000 vt 4012595271 = anchor + 4x1e9 (the 4e9-ns budget run parks by WFI-jump overshoot at the 4th alarm occurrence); the 12 device-test boot frontier blocks re-pinned with the old pins cited in comments (this replaces the E-ULS-0046 pins inst 16667327 vt 262143351559124); no ULSAN-profile era scripts exist (all tests/integration era scripts are sapporo profiles, and sapporo never mounts the ulsan board), so era-script drift surface is zero and ticket 777's stale pins are untouched. | Validation: `make test TEST_FILTER=ulsan_rtc` 6/6 (success: store plane, live counter, alarm pulse/repeat/rewrite-no-op, unobserved-value no-arm; refusal: unobserved offsets/widths and the manifest boot frontier), `make check` rc=0 (904 PASS, 0 FAIL, 2 firmware-gated SKIP), `make check-lines` rc=0, `make sanitize` rc=0; file budget kept (ulsan_rtc.c 287 lines). Unresolved: the RTC model's epoch anchor is the pair-store instant (the lane anchors at its clock-write; same boot sequence, difference invisible at 1-second period); STAT-class register semantics stay unmodelled because the guest never touches them here; the lane line-drop timing is pinned only as momentary (host-side 0-1.6 ms across the ack cannot be vt-pinned without a device). |
| E-ULS-0049 | ticket 730 post-adoption steady-era IRQ census re-run, closing the E-ULS-0042 `IRQ37 +10 pairs` item and the E-ULS-0047/0048 `steady-era re-run` item: the lane IRQ37 mass (2184 ack pairs on tree line 21 = `UlsanApollo4Mspi1 IRQ -> nvic@21`, `emulator/renode/mspi1/apollo4-mspi-ulsan.repl`) is not a steady cadence at all but three discrete persistence-flush bursts (2159 pairs in the wake1-to-wake2 interval, 20 in interval 21, 5 in interval 65), and under the post-E-ULS-0046/48 engine the tree reaches none of them because the guest's per-second alarm wake services 61,035 instructions and re-parks TIMER0 negative before the MSPI1 driver - the E-ULS-0042 +10-pair micro-divergence was a property of the retired read-advance engine era and the whole IRQ37 delta now shares the E-ULS-0047 cadence-law root; zero engine-seam gaps, no ticket-730 implementation authorized | ticket 730 census re-run over previously pinned twice-byte-identical artifacts: lane `/tmp/ulsan730/lp50a.out` sha256 `0d537088df456e9ff8a5fd54ae3e68da33fa4928e42fb25e5ec8f89d1c2df8d8` / `lp50b.out` sha256 `864f31b1e00d19675bf969bd227fcd245f298fa3bc78b1c7539c1a95ad1c6465` filtered per-IRQ twice (`/tmp/ulsan730/lane_census50a.txt` = `lane_census50b.txt` sha256 `2abca39ce14ba205952d97b54629860c70dc2af91bd9fc508f99fbb527f622f5`; filter = strip ANSI + timestamps, keep `External IRQ (18\|26\|30\|37\|45\|84): (True\|False)`, `Completed IRQ HardwareIRQ#(2\|10\|14\|21\|29\|68)`, `Waking up`); tree side = committed-engine census `/tmp/ulsan730/p79a.log` = `p79b.log` sha256 `83b9c39611663864b835da91f2550b4cc8cc0d4cbf9a4bdc927ef45dd37477ff` (E-ULS-0048 harness); analysis script `/tmp/ulsan730/irq_census80.py` | **Lane census (3.0 s capture, both runs byte-identical)**: 114 wakes; IRQ18 3 True/2 False/2 `Completed HardwareIRQ#2` (closed by E-ULS-0048); IRQ26 70/70 pairs all acked; IRQ30 511/511 pairs, 0 acks (NVIC-masked comparator line, E-ULS-0035 era fact); IRQ37 2184/2184 pairs ALL acked at `HardwareIRQ#21`; IRQ45 9/9 pairs; IRQ84 511/511 pairs with 399 `HardwareIRQ#68` completions. **Per-wake duty profile** (edges bucketed between consecutive wakes): interval 0 {26:1}; interval 1 {26:47, 30:66, 37:2159, 84:66}; every other steady interval {30:3, 84:3} plus single {26:1} in 22 of 113 intervals; burst intervals 21 (with 37:20 and 26:4) and 22 {26:4}; IRQ45 exclusively intervals 28-30 (5/3/1); IRQ37 only intervals 1, 21, 65 (2159/20/5); IRQ18 in 3 intervals. The IRQ37 census therefore measures the E-ULS-0031 persistence lifecycle as three flush bursts (98.9 % right after the first wake), the IRQ26 steady stream is one doorbell in 22 of 113 intervals plus a 4-pair double at 21-22, and the IRQ45 cluster is panel-era content belonging to the separate panel-era queue item. **Tree committed engine (p79, both runs byte-identical, budget 4.3e9 ns)**: virtual time <=3.0 s edge census = line 10 (IRQ26) 21 pairs, line 14/68 (IRQ30/84) 4 wake pairs, line 2 (IRQ18) 2 pairs, lines 21/29 zero edges - over the whole 5.0126e9 ns run line 21 stays at zero while zero refusals prove the guest never reaches the MSPI1 driver (any reached command retires with an IRQ21 edge or refuses; both outcomes absent across 16.9M instructions and 4 alarm wakes). **Lane-vs-tree table (<=3.0 s)**: IRQ18 3T/2F vs 2 pairs (closed); IRQ26 70 vs 21 pairs; IRQ30/84 511 vs 4 pairs; IRQ37 2184 vs 0; IRQ45 9 vs 0. **Attribution**: the E-ULS-0042 comparison (140=140 exact, +10 pairs, +640 pairs) measured the E-ULS-0041 engine, whose read-advance STTMR getter let the guest run ~2.5G instructions inside 3.0 s of virtual time; E-ULS-0046 replaced that getter (read-pure virtual-time counter), moving the boot-era park to inst 16667327 / vt 26675576, and E-ULS-0048 gave the guest its 1 s wake cadence - under the current engine every lane edge stream beyond the boot 4th wake pair (the IRQ37 flushes, the interval-1 IRQ26/30 excess, the IRQ45 cluster) sits behind the E-ULS-0047 TIMER0 negative-compare park: the tree guest's alarm wake re-arms 0x228 with 0x20 then recomputes 0xffffffbf and WFI-s to the next alarm without entering the flush/IOM4/display work, exactly as E-ULS-0047 measured. No new device or controller behavior is named by any of these numbers (the MSPI1 module's retire/IRQ contract is edge-validated for the eras the guest reaches per E-ULS-0031/0032/0033/0042, and the +10-pair tail died with the engine era that produced it), so per the ticket-730 evidence rule no src change is authorized here. | Validation: census derived twice byte-identically from the two independent lane runs; tree side from the twice-identical p79 pair; era scripts untouched (no ULSAN-profile era scripts exist); documentation-only instance validated with `make check-task-contracts`. Confidence: the lane window is the retained 3.0 s RunFor capture (flush intervals 21/65 are host-timestamp-order facts, not vt-pinnable per E-ULS-0040); the IRQ45 intervals 28-30 cluster stays open as panel-era census scope; the root cause of every remaining delta is the E-ULS-0047 core cadence-law divergence, whose disposition is explicitly integrator territory |
| E-ULS-0050 | ticket 730 panel-era census, closing the last item of the census queue: the lane's panel era is nine display-PLAY frame launches (IRQ45 = the lane's `Apollo4DisplayController` at `0x400A0000`, repl `IRQ -> nvic@29`) concentrated in wake intervals 28-30 (5/3/1 pairs, all strictly True-then-False with a same-instant guest acknowledge), the class law is fully source-documented (unlike the RTC class) and byte-matched by the tree's `src/devices/ulsan_disp.c` (E-ULS-0039), edge parity was measured 18-vs-18 exact on the pre-adoption engine (E-ULS-0042), and the era is now unreachable only behind the E-ULS-0047 TIMER0 park (two eras downstream of the persistence flush at wake 21-22); MSPI2 is not loaded in the Ulsan 2.35.36 lane profile at all (its repl is sapporo-only, nvic@22, External IRQ 38 count zero in the capture) and the loaded SDIO endpoint (0x40070000) has no IRQ wiring and zero activity in the retained window, so both tree fail-closed holes are consistent with the lane; the census names no engine-seam gap and no ticket-730 src change is authorized | ticket 730 census over the retained lp50 lane artifacts (`lp50a.out` `0d537088df456e9ff8a5fd54ae3e68da33fa4928e42fb25e5ec8f89d1c2df8d8` / `lp50b.out` `864f31b1e00d19675bf969bd227fcd245f298fa3bc78b1c7539c1a95ad1c6465`): per-IRQ45 stream filtered twice byte-identically (`/tmp/ulsan730/disp_census50a.txt` = `disp_census50b.txt` sha256 `9e8365376540744ac438e4e0f720ea17031e077a19f6dee3dd5aa94394eec146`, analysis `/tmp/ulsan730/disp_census81.py` sha256 `6dc63b7244d9e1e2f95292a2d94c4ef7d44309ab475a1d2a8e1b1296e1a27992`, parent per-IRQ census `/tmp/ulsan730/irq_census80.py` sha256 `46fbbf8f1b55b805d1efe9d82ce27c0c2e0569b6c111691e4bf362aeb24fdd4d`); lane sources `emulator/renode/display/apollo4-display-controller-ulsan.repl` sha256 `4807857066c2bc5725755947c5ecad269325b6dd71515b71f4c44fab0163b70c`, `emulator/renode/display/Apollo4DisplayController.cs` sha256 `fa6e67f38230899d0ae76b455e060b3e3af586f53dcace1cfa1b4191df487532` (118 lines, read in full), `emulator/renode/peripherals/apollo4-sdio.repl` sha256 `83cf2175d71023d9b31f843b6430c064f48c7f439c7759c84b321b398df6b477`, `emulator/renode/mspi2/sapporo-mspi2.repl` sha256 `4b04fc624dd3c6cfa28c8f704dcf2d16d828d83bc2c283244eb5733300250912`, machine file `emulator/devices/ulsan/ulsan-2.35.36.resc` sha256 `cb593a0867a4285a4234c568e4b6e92d28ffae2fcd2f8bab3f408b814cb3443f`; tree side = p79 pair (`83b9c39611663864b835da91f2550b4cc8cc0d4cbf9a4bdc927ef45dd37477ff`) plus `src/devices/ulsan_disp.c` sha256 `7d707c70ce51a7debdf9896d107e430ce438f0230c36283d3a2e5077830fcd1a` | **Lane class law (source, cross-checked against the trace)**: `Miscellaneous.Apollo4DisplayController @ sysbus 0x400A0000`, Size 0x9000 (DSI PHY window +0x8000 included); reads answer +0xF4 = 0x87452365 (`UlsanExpectedHardwareId`) and +0xEC = 0x77 (`UlsanPanelReadyStatus` - the guest's native panel-init path refuses queued drawable layers until it sees 0x77), all other reads return the stored dictionary value or 0; any +0x00 (PLAY) write sets bit 4 (0x10, `VsyncInterruptBit`) of the +0xF8 word and raises the line; a +0xF8 write with bit 4 clear lowers it (guest-MMIO clear - unlike the E-ULS-0048 RTC line, the panel line HAS an observed clear path); `TraceWrites` diagnostics are off in the 2.35.36 profile (zero `ULSAN_DISPLAY` lines in both lp50 logs). **IRQ45 census (filtered twice byte-identically, sha 9e836537...)**: 9 True / 9 False / 9 `Completed IRQ HardwareIRQ#29`, strictly interleaved True-False per frame (launch, same-instant service + MMIO clear), wake intervals 28 (5 pairs), 29 (3), 30 (1); 5 `Priority 0x80 set for interrupt 45` + 5 `Enabled IRQ HardwareIRQ#29 (45)` arm lines starting at interval 28; host-relative launch gaps (0.1 ms units, run a) 0, 41, 43, 120, 365, 486, 639, 670, 740 (run b within +26 on the later launches - host-side stamps, order/counts are the deterministic facts). The panel era is therefore the guest's display bring-up: arm, then nine PLAY frame launches spread over three wakes near the end of the 3.0 s capture, all services clearing the line inside the same wake. **Tree parity**: `src/devices/ulsan_disp.c` implements exactly this law (E-ULS-0039 row, which also recorded the same nine toggle pairs from the boot window); the E-ULS-0042 census measured 18 vs 18 edges exact on the pre-adoption engine - the module is edge-validated; p79 shows zero line-29 edges under the committed engine because the guest parks TIMER0 negative at every alarm wake, the persistence flush era (wake 21) is already unreached (E-ULS-0049), and the panel era sits two further eras downstream at wake 28-30. **Adjacent peripheral census**: MSPI2 (`SPI.SapporoApollo4Mspi2 @ 0x40062000 -> nvic@22`) is NOT loaded by `ulsan-2.35.36.resc` (which loads only the display and SDIO repls, lines 31-32), `External IRQ 38` occurs 0 times in the capture, and the tree leaves 0x40062000 unmapped - consistent fail-closed; SDIO (`Miscellaneous.Apollo4Sdio @ 0x40070000`, 0x1000) is loaded, has no `nvic@` wiring, and its only lines in both captures are the two `Added Apollo4Sdio @ <0x40070000, 0x40070FFF>` registration lines - zero guest activity, tree unmapped - consistent fail-closed. **Panel pixel/DSI/framebuffer scope**: explicitly absent from the class (`This intentionally models no composition, DSI transfer, or framebuffer behavior`), so the retained lane has no panel-content oracle; frame-content fidelity is unobservable under the lane constraint. | Validation: documentation-only instance, census derived twice byte-identically from the two independent lane runs, tree side from the twice-identical p79 pair, no era scripts touched, `make check-task-contracts` rc=0. Confidence / remaining scope: this closes the ticket-730 census queue in full (wake-overflow E-ULS-0047, IRQ18 E-ULS-0048, IRQ37 + steady-era E-ULS-0049, panel era E-ULS-0050); the only path to executing the flush and panel eras in-tree remains the E-ULS-0047 core cadence-law disposition (integrator-owned); the stale `ulsan_board.c` header-comment bullet that still lists the display controller and MSPI1 as unmapped is a comment-only defect noted for the integrator (src untouched in census instances); a wider lane capture (beyond the retained 3.0 s RunFor) could pin whether further IRQ45 clusters exist after interval 30 |
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
| E-SAP-ERA-GATES-239-001 | 2026-09-16; ticket 777 era-gate re-derivation on HEAD 69b1b35 (engine byte-identical to the E-ULS-0041 post-drift state): per-script two byte-identical cold era runs, snapshot-resumed budget-cap point probes (all `--layer sapporo-2.39-synthetic-wbsto`; probes without the layer set fail closed rc=2 `snapshot layer set differs`), a stride-2 budget-cap sentinel scan of +/-8,000-instruction windows around the three drifted BKPT-era caps from region snapshots, a drift-independence control on a `6be5cf1` worktree binary, and a full era-set census through the new in-tree `make check-era` target (Makefile target + `.PHONY`, `docs/execution-model.md` Era Gates section, and the six re-pinned `tests/integration/test_firmware_sapporo_239_{file_seek,file_size,ohr2_command2,ctimer13_inten,logical_files,wbsto_cache}.sh` scripts); acceptance commands ran twice: each script under the exact ticket command (exit 0 twice), `make check-era` skip-clean twice per absent-fixture variant, two fixture-present runs, `make check-lines && make check` pass | Sapporo `2.39.20.22297-P` (`tests/private/sapporo-2.39.20.22297/firmware.semu`), exact-hash full flash `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`, layer `sapporo-2.39-synthetic-wbsto` | Re-pinned the six era scripts E-ULS-0041 proved drifted; every new pin was observed twice byte-identically; no engine source changed. Instruction/virtual-time/transcript anchors held exactly in all six (the count-preserving drift signature E-ULS-0041 describes); what moved were cap PCs, artifact hashes, two mid-era ordinal censuses, and four halt-era resumes. Per script, old -> new: `file_seek.sh` cold log `4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e` -> `c5376d7e0012d7638f7f5610b44a7292340b40f959011060ec19e5977dd951e7` (byte-reproduces the E-ULS-0041 drift-probe artifact, closing that record), snapshot `92e7f053...` -> `1b97eece8ca03173bfcc4de1485ca9fa84c06acc5904d7852cb1863fb9c6b68a`, cap pc `0x00079e1c` -> `0x000a7dda` at the unchanged `instructions=416256851 virtual_time_ns=1955213393`, and the exit-0 `stop=halt pc=0x00079e1e` one-instruction resume -> exit-3 `stop=budget pc=0x000a7a02 instructions=416256852 virtual_time_ns=1955213394`; seek `result=32` x1, read `result=56` x42, and the 595-ordinal census unchanged. `file_size.sh`: log `8139068b...` -> `c84aee5768414f622b20620a5068d5add63ae07402ea8b6e44499b9acd16538d`, snapshot `0fa411dd...` -> `d3a9e0fbc854e5b80d7a71ca2778692d7c893ee6b2423734db67bb2a8b8f0a17`, cap pc -> `0x001be364` at the unchanged `405895301/1927243545`, resume -> exit-3 budget-cap `pc=0x001be368` at `405895302/1927243546`; both file-size transcript anchors unchanged; the intervention census at this cap moved 512 -> 595 because the drifted era completes the scan earlier in instruction order (595 was already the census pinned one window later by `file_seek.sh`, so the total is unchanged). `ohr2_command2.sh`: log `9161895c...` -> `8041f273595fe444382f4e708265844c35aeb7bb4669b95e4d2d64c445e98ab7`, snapshot `c3651228...` -> `0077e33ff8c8be5ba99e19c994f0bbbbd0c8fd3d1b02444b63c5d769a04ba7d3`, cap pc -> `0x000a7efe` at the unchanged `393235868/1914584112`, resume -> exit-3 budget-cap `pc=0x000a7f00` at `393235869/1914584113`; command-2 request/response anchors unchanged; ordinal census 449 -> 452 (same earlier-completion redistribution). `ctimer13_inten.sh`: cold log `2223de22...` -> `372fdabe855d55b25e5129eb0b2587e27f1f709e40a67b40011ca04ea8b1f365`, snapshot `74e45df9...` -> `7df7e4064dff38c1bfd6918efc01d8329036694d16ed3d074973601ccd6cb54e`, cap pc -> `0x00093bdc` at the unchanged `608140266/2147096849`; the 76258-ordinal census, no-refusal guard, and resumed-equals-first snapshot equality all re-verified unchanged; the ticket-751 prefix run at `607105617` log `740750cb...` -> `aca8415854f8b93f71bdfd58f265ce171a742741696c84e36e9fb3abbccf67c5` and snapshot `15b5f207...` -> `b17a3b485f89779a3dc8191f1417c6d225a65fdcc41f0681d1cb068c1ad24d90`; the next-instruction run moved from exit-0 `stop=halt pc=0x00079e1e` to exit-3 `stop=budget pc=0x00093bde instructions=608140267 virtual_time_ns=2147096850`. `logical_files.sh`: artifact hashes only - log `f61318b9...` -> `4b96c1ba019787054179ee691e5a2ac2535f6e18111111f432113f20d0338591`, snapshot `3d94e351...` -> `c1ea5c288fa6be6f6e1adbb60376fb7a74cbf7f4bc2db50a795edd04363c26ba`; the boundary line `stop=budget pc=0x000be522 instructions=78868137 virtual_time_ns=520697206`, the resume `pc=0x000be524` at `78868138/520697207`, the 118-intervention census, trigger and retained-file counts, the 0x0f676e34-absent assertion, and the E-ULS-0041 layer-off sentinel all held unchanged. `wbsto_cache.sh`: log `b1156669...` -> `6682af6ef5457a2155866229c29e25dda161cbb787e4c140619d79358d379d2d`; the advanced checkpoint re-anchored because ticket 729's accepted semantics (E-SAP-COMPAT-FILES-239-001, this gate's dependency) removed the 0x0f676e34 FAT underflow: the old exit-3 `stop=unmapped-access pc=0x0007038c instructions=78496951 virtual_time_ns=526979533 detail=unmapped write at 0x0f676e34` line is unobservable and 0x0f676e34 has zero hits in the current 500,000,000-instruction era run, so the checkpoint is now the era's cap terminator `stop=budget pc=0x00070f3e instructions=500000000 virtual_time_ns=2038956542` (exit 3) plus an explicit assert that 0x0f676e34 is absent; trigger-once-per-signal, no-reset, and the unchanged layer-off half all held. BKPT-retire observability at HEAD: the stride-2 scan (region snapshots `SK@416248851` cap line pc `0x000f74f0`, `OH@393227868` pc `0x000a7f06`, `CT@608132266` pc `0x001b7d1c`; ~12,000 probes) found no pc `0x00079e1c`/`0x00079e1e` within +/-8,000 instructions of any cap, so the four halt-era resume assertions are pinned as exact next-instruction budget-cap continuations per the E-ULS-0041 precedent (`activity_budget.sh`, `ctimer_combined_inten.sh`); no assertion was deleted or loosened. NEW GAP FINDING beyond the six: the full 43-script era census under the exact fixtures is NOT green - 21 of the 37 out-of-scope scripts pass and 16 are additionally drift-red in the same artifact-hash/boundary class (`gpio_wt1`, `haptic`, `haptic_calibration`, `history_budget`, `lps22`, `ohr2_boot_mode`, `ohr2_bsl_identity`, `ohr2_echo`, `ohr2_main_identity`, `ohr2_result_13`, `ohr2_result_14`, `ongoing`, `preload1`, `quiet_read`, `widgets`, `zip_read`); `lps22`, `haptic`, `haptic_calibration`, and `gpio_wt1` reproduce identical failures on the `6be5cf1` worktree binary, proving pre-existing drift, not behavior from the E-ULS-0045/0046 window; E-ULS-0041's six-script census was undercounted because the 239-era scripts sit behind `TEST_PROFILE` and ran in no automated suite - the defect class `make check-era` now makes visible. Two fixture-present `make check-era` runs (1,095 s full set, single machine) reported the byte-identical census `16 of 43 Sapporo 2.39 era scripts FAILED` with the same 27 green PASS lines including all six re-pinned scripts; these 16 re-derivations are outside ticket 777's allowed files and were NOT silently re-pinned - they need their own era-re-derivation ticket | High for every re-pin (each byte-identical twice, plus twice through `make check-era`); the halt-to-budget-cap migration is authorized precedent (E-ULS-0041), not a weakened golden; the era remains tree-observed behavior, not lane-verified device behavior (unchanged Sapporo lane caveat); no engine change was made to satisfy any pin (engine sources byte-identical before/after). Unresolved: 16 further drifted era scripts await their own re-derivation; the true BKPT retire boundaries for the four migrated scripts remain unlocated beyond +/-8k of the caps; the 0x0f676e34 sentinel removal is attributed to the accepted ticket-729 change; no physical device, so device truth for the era remains out of reach |
| E-SAP-0029 | 2026-09-03; ticket 727 exact-hash production-path runs; Apollo4 Plus PAC 1.0.0 at crate SHA-256 `1820cb448e123a06aa3cabff2427b71138931dfa8d67b86af2c53dd2be02b9e2`; `timer.rs` SHA-256 `5027c11d25536ffe30caae4991460351f45f3e3a70e635c3df56618521bc1393`; `timer/inten.rs` SHA-256 `52bd21a8c63b7b638032953f471000c7d1ca1bb76ed57b5c8e39c6fd658d3747`; pristine disassembly at `0x000f7d4c..0x000f7d84` and `0x00124870..0x001248d4`; pre-halt snapshot SHA-256 `552c0ef371008e455199e1c13dcb82ee69efcaf727d2641009c7fc53f3fc758f` | Sapporo `2.39.20.22297-P` (component hashes E-SAP-0011); explicit synthetic full-flash fixture SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` | The PAC identifies INTEN bits zero and 14 as Timer0 CMP0 and Timer7 CMP0. Accepting and retaining only the observed combined whole-register value `0x00004001` removes the E-SAP-0028 precise fault and all reset events from the bounded run. Two complete one-line logs are byte-identical (SHA-256 `db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`) and stop at `stop=halt`, PC `0x00079e1e`, instruction 72,774,982, virtual time 521,257,564 ns. The preceding instruction at `0x00079e1c` is `BKPT #0`; a snapshot one instruction earlier records LR `0x001248b9`. Pristine code at `0x001248b0..0x001248b4` passes line 67 and the string `StartupClient.cpp` to the fatal path at `0x00079e56`, which disables interrupts and reaches the breakpoint. | High for exact INTEN readback/refusal atomicity, existing-format snapshot round trip, immutable source, two-run no-reset checkpoint, and static identity of the terminal fatal call. Ticket 726's runner is bounded before the combined write at instruction 49,456,350, PC `0x000f7afc`, virtual time 441,085,024 ns. The halt is firmware-owned rather than an unsupported instruction or device transaction; its triggering StartupClient state requires separate reverse engineering before any new behavior is authorized. |
| E-SAP-0030 | 2026-09-16; ticket 710 first-instance offline-RE derivation on the read-only lane: `emulator/renode/run-sapporo-2.35-smoke.sh` twice (raw logs `7f00f4c9372129ecf271c637640c4ff5dafbc752548fa8ab37c12deef82e8edf` / `b255da13abbfafaef38e67385542815eb77c52db8b388a18ff1085313907ccd6`, identical apart from Renode timestamp prefixes, the include-path line, and real-time warning-coalescing counts); scratchpad bounded probes re-including the lane `emulator/renode/sapporo-2.35.resc` machine (10-second probes twice, logs `b58e219e57fa065e3ff6bd34bf0e34f07cc7276c8d46ff9b56a725481e8b3b29` / `d67bb1dfa7b9f44b955acaa1b70187c97c03042912b620df3861ea0fe1e4ba47`, identical terminal block; NVIC probe log `e3528f0a753aefbc4f1916965620502c4f9128a2d4ed03bd41f8b01d60a855ce`; stepped boot probe log `49ca54ae36dfa0829f28681ee6bd8a64c07665856dda78c421f403eb7350192d`); capstone disassembly of the hash-pinned pristine application `component-04-type-4-v2.raw` per E-SAP-0010; in-tree device-coverage cross-checks; probe scripts and logs live outside the repository tree | Sapporo `2.35.34.18929-P` (component hashes E-SAP-0010; private bundle `tests/private/sapporo-2.35.34.18929` validates byte-consistently with that record; no in-tree profile exists). The lane profile loads resident at `0x00019000` and application at `0x00040000` only - its resources component is never mapped - and its four `MOV.W r0,sp` translator patches are recorded reference workarounds, not guest behavior | Every bounded run terminated identically: PC `0x000e1862`, SP `0x100256c8`, LR `0x000a3d93`, AIRCR `0x00010000`, CFSR 0, HFSR 0, and the PC is stable across 1 ms, 1 s, 9 s, and 10 s bounds. `0x000e1862` is the instruction after a `WFI` at `0x000e1860` in a startup idle loop (helper call at `0x000e185c` into `0x000a3d1e`); NVIC `ISER0..ISER2` = `0x08400406`, `0x0F000001`, `0x00010000` (IRQ 1, 2, 10, 22, 27, 32, 52..55, 80 enabled) with `ISPR0..ISPR1` zero, so the park waits for an event the reference model never asserts. Stepped determinism: PC `0x001b2bb6` at 10 ms, a checksum-shaped loop at `0x000cc40c..0x000cc418` through ~0.6 s, park by 1.0 s. Exact guest transactions recorded during boot: PWRCTRL DSP0/DSP1 RAM triplets at `+0x58/+0x60/+0x78/+0x80`; watchdog CFG write `0x033C3D06` plus INTEN(`+0x200`) write 1, both already implemented in tree per E-SAP-0018 and the era watchdog gates; RSTGEN@`0x40000000` read then write `0x2` at PCs `0x000e1656`/`0x000e1660` (the same Reset/BoD pattern E-SAP-0018 records for 2.39 at `0x000e934e`/`0x000e9358`; in-tree RSTGEN is mounted by the apollo4 composite); PWRCTRL`+0x4` writes `0x8000` (PWRENMSPI1) and `0x10000` (PWRENMSPI2); reads of the `0x400B0000` block (I2C0 per the E-ULS-0038 map note) at offsets 0x02/0x04/0x0A/0x10/0x14/0x18 from PCs `0x000f57c2`, `0x000f57c8`, `0x000f57ce`, `0x000f5ef8`, `0x000f5e74`, `0x000f5ea0`; a byte-walk of the XIP resource tail at `0x14FC0000`+ from PC `0x000ff702` (unmapped in the lane because resources are never loaded); Timer13 edge-mode and Timer9 PWM unsupported errors; one NVIC priority write for IRQ16 of `0x8080FF`. The lane model itself lacks I2C0, the resource map, PWRCTRL PWREN tag handling, RSTGEN, and Timer edge/PWM modes, and its NVIC model aborts on a monitor read of `0xE000E608` - so the park is a reference-model artifact, not an established device boundary | High for determinism of every register, PC, and recorded transaction (each observed at least twice byte-identically, and the lane machine itself was never written to); NOT implementation-eligible as a ticket 710 gap: every recorded guest transaction maps to behavior the tree already implements or to unmapped-region probes that cannot be exercised until a 2.35.34 profile exists. This entry authorizes the next-instance order: first a 705-style `2.35.34.18929` profile instance reaching its bounded native reset stop (materials: E-SAP-0010 bundle plus this entry), then the first in-tree stop - predicted to be the `0x400B0000` (I2C0) probe at PC `0x000f57c2` or the resource-tail walk, which needs a version-specific full-flash fixture - identifies the byte-exact failing transaction for a 710 implementation instance. Reference translator patches and lane model gaps are workarounds, not firmware facts; no compatibility layer, device behavior, or golden change is authorized by this entry; no physical device, so device truth remains lane-limited |
| E-SAP-0031 | Sapporo 2.35.34 profile dispatch reaches its bounded native stop: the in-tree guest parks in the startup idle loop at the reference lane's park PC | Sapporo `2.35.34.18929-P`; ticket 705 2.35 dispatch authorized by the E-SAP-0030 next-instance order; profile `profiles/sapporo/2.35.34/profile.semu` (grammar and components exactly as E-SAP-0010 and the validated private bundle `tests/private/sapporo-2.35.34.18929`, whose `firmware.semu` equals this row's hashes); CLI `validate` rc=0 (`valid profile=sapporo-2.35.34 product=Sapporo version=2.35.34.18929-P components=3`); bounded runs executed from the bundle directory (read-only symlinks into `../suunto-firmware/artifacts/analysis/sapporo-2.35.34.18929`, never written), raw logs `/tmp/sap235/final_a.log` = `final_b.log` = `run_a.log` = `run_b.log` sha256 `87ea1ca817cb3375ab35463cfe45c266a89b96a8ea4d82ac2bf25efb1dc07600` (the pair re-verified after the `cli_profiles.c` split, proving the split behavior-unchanged) with `--trace` pair `/tmp/sap235/trace_a.semu` = `trace_b.semu` sha256 `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | `suunto-emu run --profile .../2.35.34/profile.semu --firmware firmware.semu --headless --max-instructions 100000000 --max-time 30000000000` terminated identically on two cold starts with `stop=budget pc=0x000e1862 instructions=89405875 virtual_time_ns=36373760383` (rc 3). `0x000e1862` is exactly the instruction after the `WFI` at `0x000e1860` in the startup idle loop that E-SAP-0030 recorded as the lane park, so the in-tree guest and the reference-lane guest park at the same PC through different model internals. Budget-minus-one probes: 89405874 stops at `pc=0x000e1860` (the `WFI` itself) at `virtual_time_ns=26374950913` and 89405873 at `pc=0x000a3d94` (return leg of the helper called at `0x000e185c`), confirming the park spin and that each WFI iteration advances virtual time by exactly 1 us (10,594,125 spin instructions = 10.594125 s of the recorded virtual time). The `--trace` pair is empty twice: no SYSRESETREQ, fault, refusal, or other recordable event occurred in the whole boot - unlike Sapporo 2.33, which issues a SYSRESETREQ during startup (E-SAP-0015), 2.35.34 reaches the park without any reset cycle. The tree executed the boot past every component the lane model lacks (RSTGEN, PWRCTRL PWREN bits, resources mapped at `0x14000000`) without a single refusal; the E-SAP-0030-predicted `0x400B0000` probe region produced no failure in-tree. No in-tree wake source has been identified: the park waits for an event nothing asserts yet, exactly as on the lane. | Validation: profile unit test `tests/unit/test_sapporo_profile_235.c` (6 cases: load, components, valid manifest, wrong hash, wrong version, real-profile wrong hash) green twice via `make test TEST_FILTER=sapporo_profile_235`; stop record byte-identical on two independent cold starts; contract fixture `fixtures/evidence/sapporo/sapporo-2.35.contract.semu` `[bounded_traces]` moved from blocked to observed with these hashes. Registration surface: `src/boards/machine.c` known-profile list, `src/frontends/cli.c` profile-path/list handling moved to the included `src/frontends/cli_profiles.c` translation unit (cli.c would otherwise pass the 500-line hard limit; precedent: `cli_debug.c`), `src/devices/sapporo_devices.c` accepts `sapporo-2.35.34` in the non-2.39 device group (same device snapshot as 2.33 per E-SAP-0018/E-SAP-0030 evidence; no new device behavior). Existing profile code paths are untouched, so no era pin can drift. Confidence / remaining scope: high for every recorded number (two byte-identical runs plus two budget-probe runs); the park's wake source is the next observation - identifying it (RTC/TIMER/BLE-host or a refused probe behind the park) is the first candidate gap for a ticket 710 implementation instance against this profile; device truth remains lane-limited. |
| E-SAP-0032 | Sapporo 2.35.34 park wake source identified: the guest arms the Apollo4 one-second RTC alarm before parking, and both the lane `AmbiqApollo4_RTC` class and the in-tree register stub leave it inert | Sapporo `2.35.34.18929-P`; ticket 710 instance-2 census authorized by the E-SAP-0031 "first candidate gap" order; lane scratchpad probe `/tmp/sap235/rtcprobe.resc` (sha256 `2a97a06e136b553290ff93918c5f8ed2ef1c571ed60af1b6eebb3ae564f70166`) re-including the lane `emulator/renode/sapporo-2.35.resc` (sha256 `7314f30f2341ffa743926190ce9e1c347605579ea0b5691fd1f5c55f6ed1f75c`) after `sysbus Unregister rtc`, replacing the RTC block with the access-logging dictionary peripheral `/tmp/sap235/rtcprobe.repl` (sha256 `2efa1a98cb02fe96e5a0f0d77f6b40772a04e0a007e0a2c49395e26d35f2cce4`), `emulation RunFor "10.000"`; raw logs `/tmp/sap235/rp_b.log` (sha256 `a3692755d239f1dcb1f14c04c90193c68656b86bffb9637914cbb00f731d54de`) and `/tmp/sap235/rp_c.log` (sha256 `8b7061b951b273fb66aa9f8a525f9ae0983940400ff92139677db58b287f6d87`), timestamp-stripped RTC access streams `/tmp/sap235/rp_b_rtc.txt` = `/tmp/sap235/rp_c_rtc.txt` (sha256 `4bbd99e9f3c874cda42cb9b888c3204af9d073470192413cb1fce72045625a06`); static lane sources: upstream platform repl `.tools/renode/Renode.app/Contents/MacOS/platforms/cpus/ambiq-apollo4.repl` (sha256 `e1f8560bd32f82fb2aaf7e9a8877b1b1c4aff7bd0a35821f8761a76243b74376`) lines 143-144 `rtc: Timers.AmbiqApollo4_RTC @ sysbus 0x40004800` -> `nvic@2`, loaded by `suunto-sapporo.repl` via `using "platforms/cpus/ambiq-apollo4.repl"`; offline disassembly probe `/tmp/sap235/dis_sap.py` (sha256 `ed69cb9a2c054e6f6210ad4770c7173b2a8db9f07ae807ebfea73c9d8c4bd672`) of the hash-pinned 2.35.34 application; E-SAP-0030 lane NVIC facts; in-tree stub `src/soc/apollo4/auxiliary.c` (sha256 `6f92caf14bc4420ecfdf423f469e59e4c7944666f0ed895011519da197281461`) | During each 10 s lane run the guest issued exactly two byte-identical 13-transaction RTC initialization blocks plus a final lone read, all at offsets of `0x40004800`: `R+0x00`->0, `W+0x00`<-0, `R+0x30`->0, `R+0x00`->0, `W+0x00`<-0xE, `R+0x30`->0, `W+0x30`<-0, `W+0x208`<-1, `R+0x200`->0, `W+0x200`<-1, `R+0x20`->0, `R+0x24`->0, `R+0x20`->0, ... repeat ..., trailing `R+0x30`->0. The `0x200=1 && 0x208=1` store pair is exactly the alarm-load trigger E-ULS-0048 established as the Ulsan one-second RTC alarm arming law, and the `+0x20`/`+0x24` reads are the counter window that law services. IRQ 2 (lane `nvic@2`, the block's alarm line) is enabled per the E-SAP-0030 ISER dump with ISPR zero at the park, and the lane's `AmbiqApollo4_RTC` class - the same upstream class family E-ULS-0048 had to fork past on Ulsan - never schedules an alarm, so the lane park at `0x000e1862` waits on an RTC IRQ2 that no model asserts. Disassembly confirms the wait shape: `0x000e1858` sets `r0=2, r1=0`, calls the wake-source helper at `0x000a3d1e`, then `WFI` at `0x000e1860` and `ISB` at `0x000e1862`. The in-tree stub accepts exactly the observed offsets (0x00, 0x20, 0x24, 0x30, 0x200, 0x208; other offsets/widths refuse) and answers every read with 0, so the tree swallows the same arm stores and parks at the same PC (E-SAP-0031) with zero refusals - the acceptance set matching the census offsets proves the stub was pinned against this same transaction family in the 2.22/2.33 eras. | Validation: two byte-identical filtered RTC streams plus two byte-identical unpatched park runs (E-SAP-0030/E-SAP-0031 evidence re-cited, not re-run); confidence high for every observed store/read, order, and repeat count. Scope: the *firing* law (arm one second ahead of the live counter, pulse IRQ line 2 momentarily, re-arm on a 1 s scheduled cadence) is lane-established only for Ulsan via the forked class and the Apollo4 RTC architectural reference (E-ULS-0048); no Sapporo lane run ever fired it, so adopting it for Sapporo is a same-SoC-block carry that must be stated as such. No source change was made here: ticket 710's Allowed Files (one new version-specific device module + its tests + the reserved integration attachment) do not reach `src/soc/apollo4/auxiliary.c`/`apollo4.c`, where the profile-gated RTC replacement belongs, and the bus exposes no device unmap for a post-create replacement (only RAM overlays); per the contract's stop rule the smallest required integration change is reported instead of worked around: expose a profile-selection seam on `semu_apollo4` (e.g. propagate the profile id through `semu_apollo4_init` or a dedicated select call that `src/boards/machine.c` map_sapporo already invokes for the device layer) and let `auxiliary.c` map the RTC block with the E-ULS-0048 observed law (live `+0x20`/`+0x24` counter, `0x200/0x208` arm, IRQ2 pulse) only for `sapporo-2.35.34`, keeping the 2.22/2.33/2.39 stubs byte-for-byte. A follow-up 710 instance then attaches the behavior and records the new post-wake bounded stop twice; device truth remains lane-limited. |
| E-SAP-0033 | Sapporo 2.35.34 RTC wake attached: the profile-gated live alarm replaces the inert stub for that profile only, the in-tree guest wakes from the startup park, issues its footer-validation AIRCR reset, and reaches a new recorded stop | Sapporo `2.35.34.18929-P`; ticket 710 instance-3 with the E-SAP-0032 identification and the integrator-authorized seam; new module `src/devices/sapporo_rtc.c`/`.h` (law source E-ULS-0048 as stated in E-SAP-0032: the *arm* stores are Sapporo-lane-observed, the *firing* cadence is the same-SoC-block carry established on the Ulsan lane); seam: `semu_apollo4_select_profile` in `include/semu/apollo4.h`/`src/soc/apollo4/apollo4.c` (only `sapporo-2.35.34` enables the live block), dispatch in `src/soc/apollo4/auxiliary.c` rtc ops plus a machine-create detach, wiring in `src/boards/machine.c` map_sapporo; focused tests `tests/devices/test_sapporo_rtc.c`; post-wake runs executed from the bundle directory, raw logs `/tmp/sap235/wake_a.log` = `/tmp/sap235/wake_b.log` sha256 `d8029b6b1fa61cead6fe37cccff254c836e4418103e088b99a62d7f56d2dc03a`, `--trace` pair `/tmp/sap235/wtrace_a.semu` = `/tmp/sap235/wtrace_b.semu` sha256 `0628133afeb6512b7bc77eab490789baf116bbc07c524f43c1755bab573ea523` | `suunto-emu run --profile profiles/sapporo/2.35.34/profile.semu --firmware firmware.semu --headless --max-instructions 100000000 --max-time 30000000000` terminated identically on two cold starts with `stop=budget pc=0x000ccb1a instructions=100000000 virtual_time_ns=1033322780` (rc 3): the guest no longer parks - it wakes at the alarm occurrence, continues the boot, and is deep in a second boot when the instruction budget closes. The trace pair (previously empty twice in E-SAP-0031) now carries the same record twice: `time_ns=1011860573 subsystem=cpu event=machine-reset-request pc=0x000cdf5a lr=0xfffffff1 sp=0x1005feb8 r0=0x05fa0004 r1=0xe000ed0c` - an authentic SCB AIRCR write (`0x05FA0004` = VECTKEY\|SYSRESETREQ to `0xE000ED0C`) 11.9 ms after the alarm occurrence at `virtual_time_ns=1011860573`; this is the native boundary the lane 2.35 profile explicitly registers a reset macro for ("the firmware can request an AIRCR reset while validating the external flash footer", `emulator/renode/sapporo-2.35.resc`), and it is the Sapporo-2.35 analogue of the 2.33 startup SYSRESETREQ (E-SAP-0015). Budget probes: 89405875 ends at `pc=0x000cc418` and 89405876 at `pc=0x000cc400` at `virtual_time_ns~1017388762` - deep past the old park PC, confirming the same instruction counts now execute post-wake boot instead of the park spin. The 13-transaction arm census of E-SAP-0032 is what feeds the law: stores +0x208=1 and +0x200=1 arm the line-2 alarm 1 s ahead; the occurrence wakes the WFI park at `0x000e1862`, and the machine's own reset cycle (bus reset dispatches the module reset) clears stores and cancels pending alarms before the boot re-arms. | Validation: `tests/devices/test_sapporo_rtc.c` 7/7 green twice via `make test TEST_FILTER=sapporo_rtc` (stub-profile inertness with a foreign profile selected, store/BCD-counter semantics, refusal set = census offsets plus never-observed +0x20/+0x24 writes, pulse/repeat cadence law, unobserved values do not arm, reset cancels, boot pin); post-wake stop byte-identical on two CLI cold starts plus two more via the boot case; `make check-lines`/`make check`/`check-era`/`sanitize` results recorded in the instance handoff; contract fixture `[bounded_traces]` re-pinned to this row (E-SAP-0031's park stop remains on record there and above unchanged; the re-pin is evidence-driven per the ticket's acceptance wording, nothing parses the fixture). Non-regression: the live block dispatches only when `semu_apollo4_select_profile` selected `sapporo-2.35.34`; 2.22/2.33/2.39 and all Ulsan paths keep the byte-for-byte stub, era scripts re-run under check-era. Confidence / remaining scope: the wake occurrence, reset request, and new stop are twice-recorded in-tree; the firing cadence is lane-established only on Ulsan (E-ULS-0048) - a Sapporo-device confirmation would require the lane to run a firing RTC class, which it does not; the next distinct stop beyond the AIRCR boot cycle is the next instance's observation. Device truth remains lane-limited. |
| E-SAP-0034 | Sapporo 2.35.34 boot-cycle census: the software-reset scheduler flush silently killed the attached RTC alarm; the module now detects the flush and re-arms, turning the dead second park into a stable boot-wake-reset cycle | Sapporo `2.35.34.18929-P`; ticket 710 instance-4 continuing E-SAP-0033; engine semantics already on record in `src/boards/machine_run.c` `reset_after_request` (software reset retains SRAM: `semu_scheduler_reset` zeroes the scheduler - and with it every pending event - while the bus device reset is skipped); discovery: segmented machine probe `/tmp/sap235/cycle_probe.c` sha256 `62cb30d56f6faed65c3c230a67a48e48e28845431a18c6fde159c1e61b06793c` with a read-only probe getter in `src/devices/sapporo_rtc.c` (kept), pre-fix segment census `/tmp/sap235/cp_a.log` = `cp_b.log` sha256 `2b548115636094cd7e8f3357ee4c4b0e358d49782bdee32bf64d6d265855ced9`; temporary diagnostic counters used only to locate the mechanism were reverted and are not cited as evidence; pre-fix dead-park run pairs `/tmp/sap235/cyc250_a.log` = `cyc250_b.log` sha256 `7dce22c499110b16b13f4e491f3cfdac424b4f538ba101ed7c694fc1c11d8974` and `cyc500_a.log` = `cyc500_b.log` sha256 `93a50c5afb6d9b1aef3a51d710bd859916b6003ad012729c68c49900fddc5c3a`; post-fix pairs `/tmp/sap235/fix500_a.log` = `fix500_b.log` sha256 `0a893e46d5261cf6c5387de82b270b5e59f2ec8d7350cce5c9fc0cf9d4b7fb0a` and `fix1b_a.log` = `fix1b_b.log` sha256 `3909ae53ea9026e10101a18b08286f7e4ff69d20d9878b8e8a4480a19364e338` | Pre-fix, extending the E-SAP-0033 budget showed the boot cycle terminating dead: at budgets 250M and 500M (identical outcomes) the guest reached `stop=budget pc=0x000e1862 instructions=169620897 virtual_time_ns=37382080363` - a second, permanent WFI park with exactly one `machine-reset-request` in the log. The segmented probe (twice byte-identical) captured why: after the single AIRCR self-reset the module still held `regs=15,0,1,1 armed=1 high=1` with `fires=1 clears=1 arms=0 resets=1` - `semu_scheduler_reset` on the SRAM-retaining software-reset path had dropped the repeat event and zeroed the scheduler clock without any device bus reset, while the guest's new boot re-issued the arm pair and the sticky pending flag vetoed re-arming; the line stayed high and IRQ 2 never came again. The module fix uses only the public scheduler API: it records the high-water mark of the scheduler clock, and an arm-time clock regression proves the flush occurred (the scheduler clock is otherwise monotonic), so the stale ids are dropped, the possibly stuck line falls, and the re-arm takes. Post-fix the same budgets pass the old dead stop: 500M ends `stop=budget pc=0x000a6c58 instructions=500000000 virtual_time_ns=5461709873` with eight reset requests and no refusals, 1000M ends `pc=0x000a7036 instructions=1000000000 virtual_time_ns=10646636623` with eighteen reset requests and no refusals (pairs twice byte-identical each). The cycle census from the 500M log: reset occurrences at `virtual_time_ns` 1011860573, 2023721105, 3035581637, 3056809442, 4068669974, 4089946299, 5101806831, 5123124962 - a ~1.01186 s boot-to-reset cadence (the alarm occurrence drives the wake each boot) with ~21.3 ms second resets in alternate cycles, all at `pc=0x000cdf5a`; the E-SAP-0033 first-boot record (`pc=0x000cdf5a instructions=83877686 virtual_time_ns=1011860573`) and the 100M-budget stop (`pc=0x000ccb1a instructions=100000000 virtual_time_ns=1033322780`, log sha256 `d8029b6b...` unchanged) are preserved byte-for-byte, the fix being invisible below the second boot's arm. | Validation: new unit case `test_software_reset_flush_rearms` encodes the pre-fix bug (scheduler flush with a pending pulse must not sticky-veto the re-arm; rise, flush-fall, and new-occurrence rise in order) - `make test TEST_FILTER=sapporo_rtc` 8/8 green twice; the boot-case pins of instance-3 unchanged and green; `make check-lines`/`make check`/`make sanitize` and the full-flash era gate results recorded in the instance handoff; contract fixture `[bounded_traces]` extended with the cycle fields. Non-regression: dispatch is still gated to `sapporo-2.35.34`, the flush-detection lives entirely in the live module, and 2.22/2.33/2.39 and Ulsan stub paths are untouched. Confidence / remaining scope: the cycle census is twice-recorded in-tree; the guest has not escaped the boot cycle within 1000M instructions / 10.6 s virtual time, and cycle escape (if the footer validation ever passes without a full-flash preload, or by another wake source) is the next instance's observation; device truth remains lane-limited and the firing cadence carries the same E-ULS-0048 provenance caveat as E-SAP-0033. |
| E-SAP-0035 | Sapporo 2.35.34 live RTC rewritten to the full lane register law (`Timers.AmbiqApollo4_RTC`): the refused `W +0x20` counter store is gone and the boot escapes its HardFault reset cycle | Sapporo `2.35.34.18929-P`; ticket 710 instance-5 continuing E-SAP-0034. Law source: the lane registers the block as `Timers.AmbiqApollo4_RTC` at `sysbus 0x40004800 -> nvic 2` (upstream `platforms/cpus/ambiq-apollo4.repl`, included by the lane 2.35 resc); full upstream C# source fetched to `/tmp/sap235/AmbiqApollo4_RTC.master.cs` sha256 `dc22ac4df09190011dd70cce0e03b2eabace64246c4118d58e681a26d8623dc9` (renode-infrastructure master; tag 1.16.1 404s) and cross-checked by probe pairs: cold-sweep/write matrix `/tmp/sap235/rb3_a.clean` = `rb3_b.clean` sha256 `86f3382807e8066080229a574b6c34bf9da3116e2fc1f3e0c07ca7d284bcafec` and guest-mirror/extra-value matrix `/tmp/sap235/law_a.clean` = `law_b.clean` sha256 `2836d456d31f97237c1ada90104e059c9afe7c4ec6ab7c7ceb565fe19601ab9a` (each 540/25 probe reads, command-to-value pairing resolved from the resc order). In-tree guest access census `/tmp/sap235/acc_a.txt` = `acc_b.txt` sha256 `f9b97818fb5f22ac1462947178e6fe93d0af31ed1216c6e76a97779ea4e0b921`: 30 accesses before the first reset (`R0 W0=0 R30 R0 W0=0xE R30 W30=0 W208=1 R200 W200=1 R20 R24 R20 W208=1 R0 W0=0xF W20=0x100 R0 W0=0xF R30 R0 W0=0xF R30 W30=0 W208=1 R200 W200=1 R20 R24 R20`), the only refusing access being the counter restore `W20=0x100`, which faulted (`BusFault cfsr=0x8200 addr=0x40004820 pc=0xa6604`) into the E-SAP-0034 reset cycle | The census proved the boot cycle was fault-driven, not guest-policy-driven: the old module refused the post-wake `W +0x20=0x100` counter restore. The rewritten module mirrors the lane law: Control stores bits 4:0 only (write `0xFFFFFFFF` reads `0x1F`); `WRTC`(bit0) gates whole counter writes (gated writes drop without `WriteBusy`); `WriteBusy` is set by any ungated CNTL write and cleared by CNTU; the CNTL/CNTU pair commits the epoch clock (`CalculateYear` CB mapping, invalid dates do not commit, `CTERR` latches when CNTU sees the tick the last CNTL read touched and clears on the next CNTL read); counter/alarm fields store raw validated hex - BCD validation is a hex compare (`0x3F <= 0x59` accepted) so `W +0x30=0xa5a5a5a5` reads back `0x00252500` and `0x3f3f3f3f` round-trips - exactly as probed; cold counters read `+0x20=0`, `+0x24=0x14700101` (1970-01-01, Thursday, CB); `InterruptEnable` keeps bit0, `InterruptStatus` is read-only, `InterruptClear`/`InterruptSet` are write-only bit0 and read 0; IRQ line = Enable && Status until cleared (the guest `0x200/0x208` pair is IRQ setup, not alarm arming); alarm cadence comes from RPT with the lane's true-unit repeats (second, minute, hour, day, 7-day week, 31-day month, 365-day year - source quirks mirrored), first-occurrence search with strict sub-tick `firstAlarm < now` skip and `Limit == Value` firing immediately at or past the current tick; unmodelled in-window offsets read 0 and writes drop (never faulting the guest), the window ends at `IKnownSize 0x210` (map widened from `0x20c`), non-4 widths refuse. The window values above are all probe-pinned; the escape census: 100M/30s ends `stop=budget pc=0x000e1862 instructions=89441522 virtual_time_ns=30000000000` (WFI park; pair sha256 `8d4f5694161ea5bd77423226310d84cf58cccf8e6f9c6872d139697219965179`), 100M/300s ends `pc=0x000a7022 instructions=100000000 virtual_time_ns=85159975078` (pair sha256 `9100c14b13c4d6d571b59c1f6512e8f82064a70a24bf3d7cc740137c7a908b00`), and 1B/300s shows the guest alive for two ~120.148 s intervals - `machine-reset-request` at `t=120150672935` and `t=240298801495` (`pc=0x000cdf5a lr=0xffffffed`, exactly `120148128560 ns` apart), ending `stop=budget pc=0x000e1862 instructions=299057602 virtual_time_ns=300310661417` (pair sha256 `932fecab40e6b780a6309ecb33b2d526a471f2ded2526bb6a72d6d970ab8396f`). | Validation: `tests/devices/test_sapporo_rtc.c` rewritten to eight cases against the new law - stub-mode inert, lane write-law matrix (rb3/law values incl. `0x14700101`, `0x00252500`, `0x003F3F3F`, `0x1F` mask, WRTC gating), width/window refusals, CNTL/CNTU commit with `WriteBusy`/`CTERR` ordering, the RPT=Second IRQ law with exact 1.000000000 s occurrences, reset re-init, scheduler-flush re-arm with module-clock continuation, and the re-pinned boot stops (passes 89441522/85778809/85778817, deterministic twice); success and refusal cases both present. `make test TEST_FILTER=sapporo_rtc` 8/8 twice; `make check` exit 0 (918 passes); `make sanitize` exit 0; `make check-lines` clean (module at exactly the 500-line cap after splitting `src/devices/sapporo_rtc_time.c/.h`); era gate and red-set comparison in the instance handoff. Law corrections adopted as evidence-driven re-pins, documented not silent: the 61035 ns pulse convention, the `+1e9 ns` fixed-delay arming (first wake is exactly 1.000000000 s), the refuse-unobserved rule inside the window, and the constant-0 `+0x24` stub answer are retired; the E-SAP-0034 fixture line `boot_cycle_500m_stop` sha also had a rendering typo (`f2cc8d` for `f2ec8d`) superseded by the re-pin. Non-regression: dispatch stays gated to `sapporo-2.35.34`; 2.22/2.33/2.39 stub tables and Ulsan paths untouched. Confidence / remaining gaps: CTERR ordering, RPT=Week/Month/Year fire paths, and the exact busy-flag timing were implemented from source but not lane-probed (guest never exercises them); Month/Year alarm invalid dates return no-alarm where the lane source throws; the new 120.148 s-cadence reset at the guest AIRCR helper is a non-RTC guest-level blocker (next census territory); device truth remains lane-limited |
| E-SAP-0037 | input-driven phase records of the 2.35 boot: the 4096-instruction input poll of `src/boards/machine_run.c` drives a pinned eight-press button timeline through the public machine API, slice-driven at 200000 instructions and 100000000 ns per call; the census names the scheduler-drain code paths a press serves after the E-SAP-0036 WFI park | Sapporo `2.35.34.18929-P`; ticket 710 instance-7 continuing E-SAP-0036. Probe harness `/tmp/sap235/pressus.c`; run pairs byte-identical each: control `48717a1c9f489f0d130bb7b875386353b72f5830efeb9e22cf747b236de849db`, 400M-cap `cae4cba29c446e548fd860214d3308038a9c544c310bddd5de547f8cc500786a`, 1B-cap `60830b859f871a084402b6ae740ede1d834cb365b71247e8a5b13f8a608f5c20`; in-tree module pair `49c834d7bd5fff2b5b5532a4a932db1b0856ca373a46e1f289558fe28e7563e8`; capstone Thumb disassembly of the manifest-pinned `application.raw` at base `0x00040000` via `/tmp/sap235/dis_probe.py`; logger capture at `SEMU_LOG_TRACE` in both runs: zero records (`/tmp/sap235/logrec_control.txt`, `/tmp/sap235/logrec_press.txt`) | The control census (no presses) reaches `stop=budget pc=0x000e1862 instructions=122878688 virtual_time_ns=300000000000` over 927 slices - byte-identical to the E-SAP-0036 1B/300s record, so the slice-driven observer is neutral. Under the timeline (lower, middle, upper at 20/21/22 s, 30/31/32 s, lower and middle at 60/61 s, 300 ms holds, sixteen queue entries) at the 400M/300s caps: 2023 slices, eight entries delivered (through the 30300 ms release), `stop=budget pc=0x000bdc10 instructions=400000000 virtual_time_ns=30891199362`; the park share falls from 336/927 to 25/2023, the busy regime runs at 1 ns per instruction while the alarm-driven park steps the clock in 1e9-ns one-second occurrences, and the 211 distinct counters include eighteen the control never reaches, which the disassembly names: the drain at 0xa6bea serves the one-deep depth counter at struct-offset `+0x74` (incremented by the push helper at 0xa6bb6, decremented per pop at 0xa6c04, loop runs while non-zero); the event bitmap at `+0x54` takes the OR of `1 << index` at 0xa6c2c-0xa6c30; the service callback table is indexed `0x14` bytes per entry at 0xa6c34; the queue-emptiness predicate at 0xa7042 answers through the control word `+0x00`, the count `+0x60`, and the head/capacity pair `+0x38`/`+0x4c`; the 64-bit now-stamp is committed to `+0x6c`/`+0x70` with carry at 0xa754e; an exclusive LDREX/STREX add runs at 0xa7b32; a seqlock critical section (`cpsid i`, triple read of `[r0]`) at 0xccb1a-0xccb2e; the wake path at 0xe1826 tests SCR bit 4 and the low-power timer bit 20 before parking again; the cold frame-decode site 0x197a70 takes one hit, and the bootrom-vector fetch alias counters `0x0800009e/0x080000a0/0x080000a2` take one each. At the 1B/300s caps the same timeline gives 5023 slices, twelve entries delivered (through the 32300 ms release), `stop=budget pc=0x000a72cc instructions=1000000000 virtual_time_ns=49836877598`. The `--input-replay` command-line record keeps its own 100000-instruction chunk quantization: `stop=budget pc=0x000e1862 instructions=135757218 virtual_time_ns=300000000000` (+12878530 over control), re-verified on the final tree with rc=3; the 4096-instruction per-call poll boundary makes the two harness quantizations numerically different, and both are deterministic per configuration | Validation: new module `tests/devices/test_sapporo_235_input.c` (four cases: grammar acceptance and refusals of the timeline parser - the missing separator, the unknown button letter, the empty value, the junk number, the wrong separator, the tolerated trailing comma - plus the three machine records re-taken through the public API) gives 4 tests, 0 failed twice byte-identically (pair above); `make check`, `make sanitize`, `make check-lines` green; the era gate with the canonical fixture stays at the 16-name drift baseline (no engine edits this instance). Remaining gaps: the 60/61 s press pair lies beyond the 1B-cap frontier (the twelve-entry census ends at 49.84 s), the `0x0800009e`-style fetch-alias counters await naming, and device truth remains lane-limited |
| E-SAP-0036 | Sapporo 2.35.34 IOM4 lane mirror at `0x40054000`: the second fault driver of the boot is removed - the BusFault on the first IOM4 command write is served by the profile-gated mirror and the `~120.148 s` AIRCR reset cycle disappears | Sapporo `2.35.34.18929-P`; ticket 710 instance-6 continuing E-SAP-0035. Law sources: the read-only 2.35 lane `$FIRMWARE_ROOT/emulator/renode/sapporo-2.35.resc` SHA-256 `7314f30f2341ffa743926190ce9e1c347605579ea0b5691fd1f5c55f6ed1f75c`; its wrapper `$FIRMWARE_ROOT/emulator/renode/iom4/SapporoApollo4Iom4.cs` SHA-256 `b1d1dc8ee41ed84a52d22e3dd510ffb88950619638c93fc57cb43facfe8f1be7` (31544 bytes, registers the block as `Miscellaneous.SapporoApollo4Iom4` on `sysbus 0x40054000` with irq 10 and carries the wrapper constants reused below); and the fetched upstream engine `Timers.AmbiqApollo4_IOMaster.master`, `/tmp/sap235/AmbiqApollo4_IOMaster.master.cs` SHA-256 `164bf8a85ca189d2a64a71723564f0e5ab122be995a6924819e9ee1d9fb0bfad` (renode-infrastructure master, tag 1.16.1 404s). Cross-checked against six lane probe scripts, each executed at least twice byte-identically from the read-only lane (`--console --disable-xwt`, `include @emulator/renode/sapporo-2.35.resc`, `logLevel 3`, `start`, `sleep 2`, `machine Pause`) and cleaned of timestamps and colours: `iom4law.resc` `d4a6b5196f9bdc703abca646a57f29f9414b1d7daf0d3c239de5b65b527c4ad5` with run pair `5a2f3974bed4fa7c13f0f3bf458bb97d1730da2fa1c97a473a95b44b667f1f23`; `iom4law2.resc` `d2c8188c81239b0b7320d10da211d256710c3d9d3cd7ca4a99866f0096b086b1` with pair `43a639e308b1b11c4c30200e9bb36babef9ac4340f3f2f647c8d7bb77ab25f44`; `iom4law3.resc` `d6161997edcafd89040e294e512b5ab584eea406668df6fbfa510da88b723ae8` with pair `02c21cc9c7fb14ddf75806d10b34018f5b7f4f5e91667e6b870d6400a1ea9761`; `iom4law4.resc` `0c7f2942a915a71cd928fccc8c56c6e53cee9f744eb4910dcce6073853c20d48` with four runs of which two are byte-identical (`1522dcb793829fd3332d8b9d1837be8884a1dac4271fc3f41bb14ca0bfe61b16`) - the only run-to-run variation there is outside the IOM4 window, in the lane's own `[ERROR] Timer9: PWM function mode is not supported (n)` line and in two canary readouts of the non-IOM4 address `0xE000E200` (`0x00000400` or `0x00200400`), which are therefore excluded from the claim and stay refusals in-tree; `iom4law5.resc` `22743c4723887cbc38cd66d3e7603ecff4d9a987d7894bc009117cc7fbcbe735` with pair `308354e55818c7e98ec461675b3ec75d55212c5120456faa89b0f632db6bf423`; `iom4law6.resc` `9a3e3e6a45b3fe803ab2d4063af3a9f3c223c0d249826921183d6bb01ae2d9c4` with parsed readout census `ea5feadd2042fe3884a7240eaba28e6f0ec9436049d461376eb480a09530dcee` twice. | The census of the eighteen readouts, decoded: the block owns the FIFOs at `0x100`-`0x114` (two 8-word rings, `0x100` reconstructing `out*4 \| (32-out*4)<<8 \| in*4<<16 \| (32-in*4)<<24` with `0x108` popping and `0x10c` pushing, the direct `0x00`-`0x3c` word ports unguarded), the command at `0x120` with its raw type at `0x128` and its status word at `0x12c` as `(active & 0x1f) \| (cmdstat << 5) \| (size_left << 8)`, the interrupts at `0x200`-`0x20c` (`0x204` read-only status, `0x208` clearing, `0x20c` read 0, `INTEN` masked by `0x7fff`), and the DMA engine at `0x210`-`0x248` with `0x248` reading Idle `4` and Active `2`. Overflow sets bit 3 and discards the pushed word, underflow returns zero and sets bit 2, and both return before the threshold is evaluated, so `update_threshold` runs only on a `0x104` write, after a push, and after a pop; an invalid DMA target raises `DMA_ERR\|CMD` (`0x204=0x801`) with `0x224` staying `0x4` because completion bails out while the transfer is not loaded; an unregistered device raises `CMD` alone (`0x204=0x40`, `0x12c=0x20`) and records `Error << 5` while the command still completes; the threshold registers are split `thr_read = value & 0x3f` and `thr_write = (value >> 8) & 0x3f`; the device-config gate admits only the observed `0x28` device and the `0x36` fuel gauge, whose probe-pinned register map (readouts `0x06=0x3200`, `0x08=0x1900`, `0x09=0xC000`, `0x19=0xC000`, folding the expecting and swapping the byte pairs to little endian) delivers the received gauge word `0xc000` into the RX ring; a busy controller drops a new command without clearing its status word (`0x12c` keeps `0x4c2`, raw `0x120` keeps `0x101`). In the guest this is what removed the second blocker: the first IOM4 command write `0x38000212` (read, device `0x28`, offset 3, size 2) had faulted at the unmirrored window and drove the E-SAP-0034/0035 `~120.148 s` AIRCR cycle. Boot after the mirror, all pairs re-tested on the final tree: 100M/30s `stop=budget pc=0x000e1862 instructions=89441684 virtual_time_ns=30000000000` (pair `98e1a162bb2b33348bf86b17d1dd0fb7fffad076dac5dbbc52376c643161361b`), 100M/300s `stop=budget pc=0x000a6c06 instructions=100000000 virtual_time_ns=85159974844` (pair `dc35696dd307c7b3c7c326ff8fa1296bce8839c5f19d15ec29d9efe5da362fe5`), 1B/125s `stop=budget pc=0x000e1862 instructions=104283521 virtual_time_ns=125000000000` (pair `1c9e9ce0668c5951f1291941d08d67646fb388d6ec054d046a438eb07a936d97`, the whole former window now running to the time budget without a single machine reset), and 1B/300s `stop=budget pc=0x000e1862 instructions=122878688 virtual_time_ns=300000000000` (pair `3f6e0818a4ba15a39c56299f6da22b30e49ed4ee1e56905a4d6c4b9d8771c938`) with empty `--trace` logs in both runs (`e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855`, so the boot provokes no refusal and no warning anywhere), and the machine-API three-pass reset record `89441684`, `85778971`, `85778979` - uniformly `+162` over the retired E-SAP-0035 numbers because the command write no longer takes the fault vector - all `stop=budget pc=0xe1862 virtual_time_ns=30000000000` (pair `2cae3ad5eae6a38736edcae1f2149b7533478b3201b64315f5b52c9033d0f5d0`). | Validation: new engine `src/devices/sapporo_iom4.c` (364 lines) with its register access handlers in `src/devices/sapporo_iom4_regs.c` (171 lines), the gauge device in `src/devices/sapporo_iom4_gauge.c` (62 lines), and the declarations in `src/devices/sapporo_iom4.h` (42 lines) and `src/devices/sapporo_iom4_internal.h` (68 lines), wired through the new seam `src/soc/apollo4/iom_live235.c` (51 lines) so that `src/soc/apollo4/iom.c` stays within the 500-line limit at 497 lines and keeps the shared E-A4-IOM-001 law byte for byte while `live235` is NULL. Suite `tests/devices/test_sapporo_iom4.c` (499 lines) with nine cases, four successful (`test_cold_silent_command`, `test_lane_write_laws`, `test_gauge_read_transaction`, `test_observed_device_read`) and five covering the error and refusal paths (`test_m2p_write_readback`, `test_fifo_write_readback`, `test_direct_access_ports`, `test_refusals_and_errors`, `test_reset_restores`); `make test TEST_FILTER=sapporo_iom4` gives 9 tests, 0 failed twice byte-identically (run pair `a3472edfc4db54e1fb65d2fe3c87c57b61256560f66b939e571e1db0cfbb819e`). The re-pinned `tests/devices/test_sapporo_rtc.c` (test_boot_recorded_stop) gives 8 tests, 0 failed. `make check-lines` is clean with no handwritten file above 500; `make check`, `make sanitize`, and the era gate `SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin` (SHA-256 `37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`) against the 16-name drift baseline are recorded in the instance handoff. Non-regression: the mirror is created only behind the `strcmp(profile_id, "sapporo-2.35.34") == 0` gate in `src/soc/apollo4/apollo4.c`, so 2.22/2.33/2.39 and Ulsan keep the shared IOM law, and the fixture `[bounded_traces]` retires the E-SAP-0035 pins with the reason recorded. Confidence and remaining gaps: the window is mirrored only where the lane was observed - `0x50`, `0x115`-`0x119`, `0x230`, `0x238`, the `POPWR`/`POPT` controls, and non-4 widths stay fail-closed refusals that the 300 s guest boot provokes none of; `0x110` and `0x114` take the raw-store and reconstructed defaults chosen here; the live IOM4 and RTC module state is not carried in vmstate, so a snapshot resume reads it as zero; the readouts taken outside the IOM4 window (the `0xE000E200` canary and the lane Timer9 PWM error line) are lane host noise and were excluded; what the guest does beyond the WFI park at `0x000e1862` (122878688 instructions in 300 s of virtual time) is next-census territory; device truth remains lane-limited |
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

### E-SAP-0038 — 2.35 missing manufacturing records select limited boot mode

Ticket 710 instance 8, 2026-09-19. Dependency 705 is done. The selected
change is one explicitly enabled synthetic-state layer for the exact 2.35
profile; its module, internal header, focused tests and the existing board
layer attachment are in scope. No sensor behavior, CPU instruction, profile
hash, public interface or release pin is changed.

The read-only `sapporo-2.35.resc` lane, with the matching OTA resources in an
external 32-MiB XIP memory fragment, was run for exactly 2 virtual seconds.
Observer hooks only read registers. Control and production runs were each
reproduced twice. Each four-line `SAP235_MODE` census is byte-identical after
removing host timestamps; raw logs remain outside Git:

| External artifact | SHA-256 |
| --- | --- |
| `/tmp/suunto-235-lane-control.resc` | `8e573c509466d17e166b2c0e250b87c0168e9713f18ae87774b2f0ba655e64b9` |
| `/tmp/suunto-235-lane-production.resc` | `b2d5cf5b7023206f32928569d6700c878b5f7b32f2e2e5eec397725c2e10c766` |
| `/tmp/suunto-235-lane-xip.repl` | `5baccffc4e00dcf9a509d08c77aa9ed0dcaf8b0c6a8880d8bddcddb92b5a2ca1` |
| `/tmp/suunto-235-lane-control-1.log` | `ff04b5b0653954b237471b4d5167f8f59c8e8202b831f79735606780a921df22` |
| `/tmp/suunto-235-lane-control-2.log` | `8c5551f1aa6d94d650983f0897fb93298e299989917671b7eb15b6a70cad1f03` |
| `/tmp/suunto-235-lane-production-3.log` | `b522b889e10f97e884701e1c1609ba0f58011444c9cfd6a545c6c1adeb3ad2f4` |
| `/tmp/suunto-235-lane-production-4.log` | `07ff5abe1f8eaf2a5bbcf9573aa4b6bd6cfc142e3c5c4385f65b09871030d2fe` |
| control census pair | `2d8d5e2317ce47f7453af01dcf91b39f240e8db6f72f9cd01d2ef33164afb145` |
| production census pair | `3748fbbf1d0f862acc71380a840e65db54a6e846a36464606a50340c7ed95413` |
| `/tmp/suunto-235-production.bin` | `c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53` |

Derived census (hexadecimal registers; each row occurs once per run):

| PC | Control r0/r4/r5 | Synthetic-record r0/r4/r5 |
| --- | --- | --- |
| `000a839e` | `fffffffd/1000a91c/00000005` | `00000000/1000a91c/00000005` |
| `000a83e8` | `00000016/1000a91c/00000005` | `000fbc3e/1000a91c/00000005` |
| `000a8018` | `7317f137/00000000/00000002` | `80000000/000fbc3e/00000005` |
| `000e245c` | `00000002/10025544/10056a84` | `00000005/10025544/10056a84` |

The fixture was generated outside the repository with the sibling's existing
`tools/build_production_data_fixture.py`, using `--crc-table-address 0x1b1644`
and the pinned 2.35 application. Its record container matches
E-SAP-COMPAT-PROD-001: ProductionData plus ACCR/ACCC/MAGN/HLAT in a 4096-byte
sector at `14fff000`, 256-byte records, CRC over the first 252 bytes, active
byte and version. All identity and calibration payloads are synthetic; their
acceptance does not establish physical calibration. Record CRCs are
`a2c492a6/7310559d/717aee21/b7717e8b/9f42edf7` in that order.

Static inspection of the pinned application identifies the checksum table at
`001b1644` and production reader `000ce3d8`; return -3 is the failed checksum
path. The lane observation, not disassembly alone, establishes the accepted
record effect. Lane setup still emits its pre-existing RTC SilenceRange
conflict warning; the hooks and both bounded runs complete. The external XIP
fragment avoids re-registering the lane's existing MSPI1 controller.

In-tree observational probes independently reproduce mode 2 without the
record and mode 5 with it. The next refusal is an IOM2 read at
`pc=0014abaa`, address `5c`, command `0f000112`, count 1, target `1002f830`:
`I2C address 0x5c has no verified device`. It triggers the firmware fault/reset
path. This is a separate gap; this entry does not authorize a sensor fallback.
Probe `/tmp/suunto-235-production-probe.c` SHA-256
`965c513edbe215862312438140c55c971796315f5edd0d9c26a67d6f8e4039de`,
observer `/tmp/suunto-235-mode-cpu.c`
`954347ff24d2a222ead15f3180779efd67dee6aa75eb71992316b64557c64995`,
and diagnostic log `/tmp/suunto-235-production-diag2-1.trace`
`bbaf45e3c4ff89b92672f07e2ad38dbedb003e9e2f818bb0d7386116b0a1140a`
record that next boundary (80M-instruction/3-second limits).

Layer declaration: `sapporo-2.35-production-data`, synthetic state, exact three
component hashes from the 2.35 profile, cold/reset installation into an empty
(all-zero or all-FF) sector or an exact retained copy of this synthetic fixture,
one hit per machine reset, logged
`layer-hit` with `trigger=production-data`, ordinal and E-SAP-0038 provenance.
An unrelated populated sector, disabled/wrong layer, exhausted budget, missing mapping or
missing logger refuses before mutation. The immutable single-trigger descriptor
keeps its counter in the machine's layer state. Manufacturing bytes cannot be
recovered from OTA components; the lower-level representation is this explicit
synthetic storage record, not a firmware hook. No authentic identity is claimed.

Implementation validation for E-SAP-0038:

- Source lane `../suunto-firmware/emulator/renode/sapporo-2.35.resc` SHA-256
  `7314f30f2341ffa743926190ce9e1c347605579ea0b5691fd1f5c55f6ed1f75c`;
  fixture generator `../suunto-firmware/tools/build_production_data_fixture.py`
  `58c90b029e6901a266704afb2a9f20e8bcd3812b32361d13156d4db215ef3f4c`.
  Lane command, from the sibling root: `.tools/renode/Renode.app/Contents/MacOS/renode
  --console --disable-xwt /tmp/suunto-235-lane-production.resc` (or control).
- `make test TEST_FILTER=sapporo_235_production`: five synthetic tests pass:
  record layout plus independent bitwise CRC, atomic refusal, short mapping,
  all three hash mismatches/wrong profile/missing hash, and reset/independent
  ownership. Blank all-FF storage succeeds too. An exact copy of this fixture
  is accepted after a firmware reset preserves XIP memory; unrelated populated
  data still refuses. This qualifies the empty-sector trigger above.
- The integrated emitter's sector is byte-identical to the lane fixture
  (`c0881606…619a53`). Observer `/tmp/suunto-235-integrated-probe.c` SHA-256
  `dbfe9512c559f6e661fa86a83693d1cfa8824e80699ba85995977778524f90ad`
  records all four successful reader returns and mode 5 at instruction
  22889731, PC `e245c`. Trace `/tmp/suunto-235-integrated-mode.trace` SHA-256
  `14011b2a011569134adb166368f7826a700fb71d2803231ec0f78e6d058e455e`.
- `build/suunto-emu run --profile sapporo-2.35.34 --firmware
  tests/private/sapporo-2.35.34.18929/firmware.semu --layer
  sapporo-2.35-production-data --max-instructions 30000000 --max-time 1000000000`
  twice exits 3 at `000932a8 / 30000000 / 35339893`, one layer hit, no reset.
  `/tmp/suunto-235-integrated-{1,2}.log` both SHA-256
  `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba`.
- Same command with `--max-instructions 80000000 --max-time 400000000`
  twice exits 3 at `000cc418 / 77309520 / 400000000`. Exactly one firmware
  reset is requested at `cdf5a / 73523963 / 396214443`, exception 3, LR
  `ffffffed`. The layer logs once before that reset and once after it, with
  ordinal 1 in each reset epoch. `/tmp/suunto-235-next-{1,2}.log` both SHA-256
  `a5629499c8bad5b8ca5e3897103c2f4a7e32a602ab133116fbe47f7c30ca46bc`.
- `make test-firmware TEST_PROFILE=sapporo-2.35.34
  TEST_FILTER=sapporo_235_production
  SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`
  validates all components and passes the new bounded paired regression.
  `/tmp/suunto-235-production-firmware.log` SHA-256
  `0a9ad72ef1dca6e71fb58be93d168ec3b32ef945a4b942fea5956e4bcfb34643`.

This is a completed bounded record intervention, not a completed Sapporo
release gate or a status change to ticket 710. No snapshots are claimed for
2.35. Prior 2.22 onboarding and 2.39 era failures remain open.

### E-EMU-SAP235-INPUT-HARNESS-001 — bounded input test refusals

Maintenance, 2026-09-19; no guest behavior or checkpoint changes. The private
input test accepted 32 presses into a 32-edge array, ignored integer overflow,
spun indefinitely on a refused machine run, and reported malformed supplied
firmware as an optional skip. The repair limits the array to 16 presses,
checks millisecond conversion plus release-time addition, returns on refusal
or no progress, and distinguishes absent optional fixtures from invalid or
explicitly missing inputs.

`make test TEST_FILTER=sapporo_235_input` adds three synthetic regressions.
Capacity and time-overflow checks fail before the parser fix; the old sliced
runner, called with a null machine from an external wrapper, exceeds a
2-second host timeout. The corrected wrapper immediately preserves
`SEMU_ERR_ARGUMENT`. Malformed, wrong-version and explicitly missing manifest
runs exit 1; an absent conventional fixture still skips and exits 0. None of
the three authentic input checkpoint expectations changed.

| External log | SHA-256 |
| --- | --- |
| `/tmp/suunto-235-harness-before.log` | `ada409e853ecc933b21d6b364d039ebbcf5692f6b8a54d2f8e15f39a2fef0227` |
| `/tmp/suunto-235-run-refusal-before.log` | `8ee51708abad88dc19be18624ee1ad47d8f7d3460bc7282283f2d71d3229068c` |
| `/tmp/suunto-235-run-refusal-after.log` | `5e86ea95d93d1a344be4adb4309122f7c75552e4f60f3242cb4c3ef5092a7041` |
| `/tmp/suunto-235-invalid-before.log` (false pass) | `c2628f336a153d1fdfbdbcdf2dd757a4efe0105ad768a6b67cb8018e00d3ccb2` |
| `/tmp/suunto-235-invalid-after.log` | `9af8c51988c36a458ff0f9bbff83d6a2a43f07331361e9836520b9d7da4f907b` |
| `/tmp/suunto-235-mismatch-after.log` | `a0dd56ba7c0c39157af8ee7cdbc5ad8bc3d7a6d77af528bdd15cba6111a5b52a` |
| `/tmp/suunto-235-missing-after.log` | `aff3d9de8cbc22c76aba172e1eb49457cece7eb0f3e6b1b1338cbd8daf0c912e` |
| `/tmp/suunto-235-absent-after.log` | `d3cea952961bb03287b6aacbdb7e0d413eaaed956ac62f217e967c4c56a0f078` |

Commands for supplied-fixture refusals are
`SEMU_SAPPORO_235_FIRMWARE_MANIFEST=PATH build/tests/test_sapporo_235_input`,
with PATH respectively `/tmp/suunto-235-invalid.semu`, the 2.33 manifest,
and `/tmp/suunto-no-such-fixture.semu`; the absent test runs the same binary
by absolute path from `/tmp` without the variable. The two before-parser
failures used `make test TEST_FILTER=sapporo_235_input`.

The E-SAP-0037 timeline was also extended to 2B instructions/120 seconds
in `/tmp/suunto-235-frontier.c` (SHA-256
`825353ed56f839af1ff2619a553a49302497a24014c2070d98387bc8a52ecb6d`),
with 200000-instruction/100000000-ns slices and the same eight presses.
Both `/tmp/suunto-235-frontier-{1,2}.log` have SHA-256
`c0bb7a6e06c64a5112e07cb0d4306bb020e41ad345c15904fc85b02d1ebde8cf`:
all 16 edges delivered, zero frames, 10023 slices, final
`000cd0c8 / 2000000000 / 81142979747`. Merely extending the input budget
therefore does not establish a working UI in limited boot mode.

### E-EMU-SAP222-PANIC-001 — script allocation failure behind historical halt

Read-only diagnostic, 2026-09-19; no instruction, allocation policy, firmware
or expected stop was modified. Two replays of an in-tree checkpoint identify
the failure behind E-EMU-SAPPORO-AUDIT-001. This is in-tree diagnostic evidence,
not new lane authority for guest behavior.

Capture command (the fast build uses the same interpreter):

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
SEMU_SDL_SETUP_WALK_TIMELINE='30000:l' \
build-fast/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --max-instructions 14000000000 --max-time 300000000000 \
  --snapshot-save /tmp/suunto-222-beforepanic.sems
```

It exits 3 at `6e78e / 14000000000 / 43612174532`; the main-menu frame remains
CRC `fb8e0155`, generation 4403. Snapshot SHA-256
`88d89cc8bc06084912eaf61126cf9548b04d9712c74e695cb7831136c1868925`,
capture log `/tmp/suunto-222-beforepanic.log`
`10f0a6b4d73f2a83e105d472d326cd58fafbf2ecaadc4ff21381c6f1e7128c58`.

External observer `/tmp/suunto-222-alloc-probe.c` SHA-256
`e028099554e5811230f4c3e3375d5bf31667fd5704ef3281947ea0fa04d35685`
loads that checkpoint and executes at most 180201000 single-instruction
machine calls (each capped at 1 ms), reporting selected PCs/registers without
mutation. Both `/tmp/suunto-222-alloc-early-{1,2}.log` are byte-identical,
SHA-256 `9b72e5027de8546b4c59f867e440fcd62387541565e5d4f5cdc499c3a7fe7db0`.

Derived final failure chain:

- The pool allocator's failure return `6a75c` reports request size `r6=50`
  (80 bytes) eleven times: initial attempt at instruction 14156108860,
  then ten GC/retry attempts ending at 14168086274. All return zero to the
  script allocator. The final retry index is 9 at `5f326`.
- Property-table growth at `600f6` receives zero at instruction 14168086282;
  the error path `57b7c` follows at 14168086305. Later error-object allocations
  succeed; this is exhaustion of a suitable pool, not a claim that all memory
  is unavailable.
- At 14168090283, `5ec6e` finds no protected error handler. At 14168090291,
  callback `79f60` receives the exact bounded ASCII message `uncaught error`.
  The guest assertion identifies `peScriptEngine.cpp:46`.
- `727c8 / 14178200856 / 43790375388` precedes BKPT. Stepping retires it to
  `727ca / 14178200857 / 43790375389` (the historical halt tuple), then returns
  to `5a8f8 / 14178200858 / 43790375390`, the fatal handler's self-loop.
  The 40B budget failure is downstream of this same panic.

The last step pair uses `/tmp/suunto-222-panic-probe.c` SHA-256
`48a07599eedaab04f5f7e74ebdfd7ea181cfe0bc1b5da3d48c1f57c8a5974e6a`;
`/tmp/suunto-222-panic-probe-{1,2}.log` both SHA-256
`ccd2c780653772975d63b6c75ebb758803892c819deb1a373560b084cf622092`.
Why this pool exhausts, and whether the allocation history diverges from the
lane, remains unresolved. Increasing heap sizes or bypassing the assertion
is not authorized by this observation.

Final tree verification after the input maintenance and E-SAP-0038:
`make check` exits 0, 947 PASS records including five SDL cases;
`make sanitize` exits 0, 942 PASS records; `make check-lines` exits 0 with
advisory warnings; `make check-task-contracts` exits 0 with 143 indexed tickets;
`git diff --check` exits 0. Log SHA-256 values:

| External log | SHA-256 |
| --- | --- |
| `/tmp/suunto-production-check.log` | `3d5e099af43fd6e103cefcb04606796ca27360e718523b8756dec14375b0f23a` |
| `/tmp/suunto-production-sanitize.log` | `57a1994689ce2d8c38ec9126329381b9549f9f66d2c26beb83be43a4a2f4e2d9` |
| `/tmp/suunto-production-lines.log` | `238d2e497bd8e5b69b87c19203a9f2532a3c507b71697d761ad64fae84e65075` |
| `/tmp/suunto-production-contracts.log` | `9f13a69216cc5312ec45f909882c084d77c00db1db803bd4023cd265cd1dbfca` |

The affected profile's new opt-in era runner passes; neither the established
2.22 long-walk failure nor the 2.39 16-script failure set was silently re-pinned.

The ticket-form command also passes: `make test-firmware
FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34`.
The external root contains only a symlink to the original private bundle;
all three components validate, the 2.35 runner passes, and other-profile
runners explicitly skip. `/tmp/suunto-235-ticket-firmware.log` SHA-256
`058c6e698cfafb2a2bfbe943ef2b4d1760239243491c9f6785fca6721ed298c0`.

### E-SAP-0039 — 2.35 negative pressure identity probes

Ticket 710 instance-9 derivation; implementation scope is integration ticket
778. On 2026-09-19, the unchanged 2.35 lane with E-SAP-0038 synthetic records
was run for two virtual seconds, twice. Hooks at `14aba6` (before command)
and `14abaa` (after command/interrupt service) only read registers. The lane
has no LPS22 at either 0x5c or 0x5d: this is an absent-sensor probe, not
permission to attach the 2.39 sensor.

`/tmp/suunto-235-pressure-law.resc` SHA-256
`5a6c6714b61b20b227c800836ca9f373bad0678fd1cf11df5370e8d29f896fac`
includes the pinned 2.35 lane, external XIP fragment and production fixture
from E-SAP-0038. The read-only wrapper
`../suunto-firmware/emulator/renode/iom4/SapporoApollo4Iom4.cs` SHA-256 is
`b1d1dc8ee41ed84a52d22e3dd510ffb88950619638c93fc57cb43facfe8f1be7`.
Run from the sibling root with `.tools/renode/Renode.app/Contents/MacOS/renode
--console --disable-xwt /tmp/suunto-235-pressure-law.resc`.

Raw logs `/tmp/suunto-235-pressure-law-{1,2}.log` have SHA-256
`e1ae6896a1ce4fbd4ccfc3a34da3f56af3d831586605416898c7820fa7754da8`
and `edfb91d9ad4f9c43c6abb3071c462e805d88df0201e5444d4743c134b00a6268`.
Removing ANSI/host timestamps and retaining the 22 `SAP235_PRESSURE` hook
records plus twelve halted-controller readouts produces identical censuses,
SHA-256 `e9eccd183c457ef1bdc164992c7d9c45a0c164b2d738c48a0c5946832faa4190`.

Derived native census: six transactions at 0x48 (identity read selector 00
returns 49, read selector 1c returns e0, four two-byte writes), one identity
read each at 0x5c and 0x5d, and three transactions at 0x35 (two two-byte writes,
one 23-byte read). The absent-sensor commands are exactly `0f000112`, count
1, target `1002f830`. The target starts at zero and remains zero, DMA status
is 2; after native ISR service the interrupt status is zero. Both absent
probes are followed by magnetometer and OHR startup rather than a fault reset.

After the two-second run, CPU execution is paused. Each absent address is
probed separately with submodule 10, interrupts disabled/cleared, target
`10010000` initialized to `000000a5`, count 1, config `101`, command
`0f000112`. The exact six readouts, identical for 0x5c and 0x5d, are:

| Readout | Value |
| --- | --- |
| target word | `00000000` |
| INTSTAT | `00000442` (DMA complete, RX FIFO underflow, threshold) |
| DMASTAT | `00000002` |
| DMACFG | `00000100` |
| DMATRIGSTAT | `00000004` |
| CMDSTAT | `00000020` |

The wrapper drains an empty delegated FIFO after the absent-address error,
then completes its DMA. This observation authorizes only the exact one-byte
negative identity probe and its controller status, with checked RAM admission.
It does not authorize arbitrary absent devices, register values or lengths.
CMDSTAT is recorded as lane context, not a new generic register implementation.
Controller config bit 8 is retained for this profile; its codec is explicitly
unsupported rather than changing the shared snapshot format.

E-SAP-0039 implementation and validation (ticket 778):

`iom_sapporo235.c` implements the exact negative probe at the controller
boundary. The SoC selects it only on the 2.35 IOM2; its enable flag is owned
by that controller. It requires command `0f000112`, count 1, config `101`,
I2C submodule 10 and an attached endpoint. It checks a one-byte RAM write,
then commits zero data, DMASTAT 2, DMATRIGSTAT bit 2, clears DMA enable, and
sets INTSTAT `442` through the ordinary IRQ path. Configuration bit 8 is
retained only for this controller/profile. Other addresses retain the generic
strict path. There is no fake LPS22 identity or new compatibility layer.
Direct IOM2 snapshot operations refuse before consuming/writing codec bytes.

The four initial regression cases all fail before implementation
(`/tmp/suunto-235-pressure-before.log`, SHA-256
`3920f163e6f12017866b83c90779e1a4b8255a11b8f456955f557aedd15948a6`).
After implementation, `make test TEST_FILTER=sapporo_235_pressure` passes five
cases: both exact positive tuples and IRQ rises across reset; seven atomic
refusal variants (wrong selector/count/direction/enable/submodule, unmapped
RAM and ROM); independent profile ownership plus unknown-address refusal;
empty codec refusal; and a one-byte MMIO overlay with zero callbacks. The
MMIO fixture uses the existing explicit overlay API; ordinary device mapping
correctly rejected its overlapping setup. Final focused log
`/tmp/suunto-235-pressure-tests-final.log` SHA-256
`6069f6901b5918d47d2ab8135b78cf17746dacc1a1b7e50d47f3d8934327c8a7`.

Paired authentic commands use `build/suunto-emu run --profile sapporo-2.35.34
--firmware tests/private/sapporo-2.35.34.18929/firmware.semu --layer
sapporo-2.35-production-data`, with these explicit bounds:

| Bounds | Result (PC / instructions / ns) | Both log SHA-256 values |
| --- | --- | --- |
| 80M instructions, 400M ns | `000e1862 / 73528280 / 494546055` | `0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81` |
| 150M instructions, 3B ns | `000e1862 / 122589556 / 3000000000` | `4b6f2edb030d2eb32f4ca526768458e5ebe5500b1a09454116d451d7193ade1a` |

Logs are `/tmp/suunto-235-pressure-next-{1,2}.log` and
`/tmp/suunto-235-pressure-deep-{1,2}.log`; all exit 3 (`budget`), have exactly
one production-layer hit and no reset. The 400-ms request stops after a
sleeping CPU advances to its next scheduled event; the existing run loop's
granularity accounts for the 494546055-ns result. The unchanged 30M prefix
continues to match E-SAP-0038.

The previous production runner's fault suffix (`a5629499…ca46bc`) is retired
under ticket 778's explicit checkpoint scope, replaced with the exact new
suffix plus a no-reset assertion. Its prefix is unchanged. The new pressure
runner pins the three-second pair. `make test-firmware
FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34` validates
all components and passes both scripts; other profiles explicitly skip.
Log `/tmp/suunto-pressure-firmware.log` SHA-256
`52a250611e0f517adff083156db21b8db7a489b63d6eaa0a273c9b3a596d4d92`.

A longer diagnostic pair uses `/tmp/suunto-235-pressure-census` with
`300000000 30000000000 200000 100000000` (instructions, ns, instruction slice,
time slice). Observer `/tmp/suunto-235-iom-census.c` SHA-256
`f0311e9afbec5f5c6511b721294a1d5b0e1b94019175f6196595d4522d245cf6`
adds only command diagnostics to the current controller implementation;
the machine driver and CPU observer are the E-SAP-0038 external sources.
`/tmp/suunto-235-pressure-census-{1,2}.log` both have SHA-256
`326492cbf082b987da778159ddf3f82c52285b3ce8013c6de75368853c4d09bb`;
corresponding `.trace` files both have SHA-256
`94e6b43c9097e6a62ec5ddece422c6eee6781ce447e30312e559b0ac6266971b`.
The derived command census is eleven IOM2 commands: the same six at 48,
one each at 5c/5d, and three at 35 as in the lane. It reaches
`000e1862 / 203436260 / 30000000000`, 1075 slices, zero frames, zero input
edges and no reset/refusal. The lane's later OHR commands do not occur in this
in-tree run. That startup wait, not the pressure probe, is the next gap;
this does not prove a functional 2.35 UI.

Final ticket 778 gates: `make check` exits 0 with 952 PASS records;
`make sanitize` exits 0 with 947, including the corrected overlay test;
`make check-lines` exits 0 with advisory warnings;
`make check-task-contracts` exits 0 with 144 indexed tickets;
`git diff --check` exits 0. No 2.22/2.39 release expectation was edited.

| External final log | SHA-256 |
| --- | --- |
| `/tmp/suunto-pressure-check-final.log` | `665ef0cb3ec5dea428dceaf9f42a0e10087e39edba8b05861b5f17e07fb8b885` |
| `/tmp/suunto-pressure-sanitize.log` | `71f90b6a0dce8438c4d0e85252141490b02180e85b0d3c61be472e2a2dd0fd31` |
| `/tmp/suunto-pressure-lines.log` | `0e7ca399690904259d9c704eaff6f458280aa29609b21005431c7d01032e5070` |
| `/tmp/suunto-pressure-contracts.log` | `ce05d381247f1e5f6a31d415290cd3d1af3b0ac6b4eec5654a93a55740c38dff` |

### E-SAP-0040 — 2.35 IOM4 haptic startup

Ticket 710 instance-10 derivation, integration ticket 779, 2026-09-19.
The E-SAP-0038 production fixture and unchanged 2.35 lane run for two virtual
seconds twice. `/tmp/suunto-235-haptic-law.resc` SHA-256
`61d4237d048781cb84f1c4979c2cbb4ca6a5ce7ff547348f1ba79c940e89e6ac`
hooks before/after command PCs 14aba6/14abaa, then pauses for synthetic probes.
Command from the read-only sibling root: `.tools/renode/Renode.app/Contents/MacOS/renode
--console --disable-xwt /tmp/suunto-235-haptic-law.resc`.
Lane endpoint `emulator/renode/haptic/SapporoHapticPmic.cs` SHA-256
`fc7533bbcca2d6cb15d22edf0b40bb884c7df702d58f9f2bac0bbd76b291e968`;
controller/wrapper identity remains E-SAP-0039.

Raw logs `/tmp/suunto-235-haptic-law-{1,2}.log` SHA-256 respectively
`5d943ad36846a22a136f56fd432a69347d39ed0333d2a8a13bdbf1beeb36253f`,
`39fe8643dd83c3359d9c41b1a7c28dbd851034ddcce928af21abcc430da5ab96`.
Keep HAPTIC records without host timestamps and bare hex readouts; both
234-line derived censuses are byte-identical, SHA-256
`112e5ce4888497c684036a9130eaacafa5376774ce5a6b9dc0994482a52e4121`.
Probe sources, logs and full firmware-derived payload census remain external.

Derived census: 105 transactions at IOM4 address 50: 36 two-byte writes
(command 00000201), 64 five-byte writes (00000501), five one-byte reads
(selector in command bits 31:24, command suffix 000112). Every DMA count equals
command size; transmit config 103 becomes 102, receive 101 becomes 100;
DMASTAT is 2 after completion. Native ISR service leaves INTSTAT 2 at each
post-command hook. Initial configuration writes target 0d,11,12,13,1d,1e;
autotune writes 22=01. Read 22 returns 00 before autotune, then 03 twice;
reads 23 and 24 return 00. There are 29 wave-selection writes at 09 and
64 waveform writes beginning 40, covering 32 indices twice each. Startup
continues to OHR transactions in the lane.

Halted synthetic probe, address 50, submodule 10, interrupts disabled, FIFO
threshold 800, DMA RAM 10010000, clear IRQ before each command:

| Operation | RAM word | INTSTAT | DMASTAT | DMACFG | TRIG | CMDSTAT |
| --- | --- | --- | --- | --- | --- | --- |
| write five bytes 40 11 22 33 dd | 33221140 | 401 | 2 | 102 | 4 | 80 |
| read four bytes from 40 | 00332211 | 403 | 2 | 100 | 4 | 80 |
| write 22 01 | 00000122 | 403 | 2 | 102 | 4 | 80 |
| read one byte from 22 | 00000003 | 403 | 2 | 100 | 4 | 80 |

The five-byte write is delivered as chunks of four and one: 40..42 retain
11,22,33; the final dd selects a pointer rather than writing register 43.
The pointer persists across FinishTransmission; subsequent explicit selectors
override it. Autotune completes synchronously by OR-ing bit 1 after bit 0
is written. This authorizes scoped configuration retention, waveform chunking
and the observed read selectors, not arbitrary registers or physical vibration.
Whole-command DMA admission and unsupported-shape refusal are emulator safety
contracts, not claims that the permissive lane itself refuses those shapes.

E-SAP-0040 implementation/validation (ticket 779):

`src/devices/sapporo_iom4_haptic.c` admits only the observed complete DMA
shapes and selectors; the existing IOM4 owns the endpoint state and preserves
four-byte chunk delivery. `sapporo_iom4.c`, `sapporo_iom4_internal.h`,
`sapporo_iom4.h` integrate it, and `sapporo_iom4_regs.c` rejects a mid-command
endpoint change that could bypass admission. No shared-profile behavior changes.
`tests/devices/test_sapporo_iom4_haptic.c` covers autotune/reset, waveform
chunking, twelve atomic refusal shapes and active endpoint-switch refusal.
The original three cases fail with the old unregistered-address behavior.

Exact commands/results (all final commands exit zero):

- `make test TEST_FILTER=sapporo_iom4`: 17 cases pass, including four new cases.
- `make check`: 956 PASS lines, no failures (includes five SDL cases).
- `make sanitize`: 951 PASS lines, no failures.
- `make check-lines`: advisory only, success.
- `make check-task-contracts`: 145 indexed tickets validate.
- `make test-firmware FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34`:
  both 2.35 scripts pass, other-profile scripts explicitly skip.
- `git diff --check`: success.

The firmware runner preserves E-SAP-0038's 30M prefix and E-SAP-0039's 80M
production prefix. Ticket 779 explicitly retires the pressure runner suffix
`e1862 / 122589556 / 3000000000` (hash
`4b6f2edb030d2eb32f4ca526768458e5ebe5500b1a09454116d451d7193ade1a`).
Haptic completion now advances the same 150M/3s bounded invocation to
`cd0c8 / 150000000 / 1258826798`, with no reset. Both new logs have SHA-256
`c8b5c60d805fe012671575b9f850d4c42a94420d448e4bbdd0c3759fc7127f07`.
The strict hash, budget reason and no-reset assertion remain in the runner.

Two CLI runs with `--profile sapporo-2.35.34 --firmware
tests/private/sapporo-2.35.34.18929/firmware.semu --layer
sapporo-2.35-production-data --max-instructions 280000000 --max-time 3000000000`
produce identical logs, SHA-256
`b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879`.
They reach the next gap: OHR command 0010/sequence 0 in BSL is refused at
1567178557 ns; firmware resets at instruction 261789155 / 1567178637 ns,
PC cdf5a. Final tuple is budget / a6bc8 / 280000000 / 1590729375. This is an
explicit remaining failure, not a passing full-startup gate. No frame or UI
completion is claimed, no OHR implementation is added here, and 2.35 snapshots
remain unsupported. Ticket status remains ready for integrator review.

Retained external verification logs (SHA-256):

- `/tmp/suunto-235-haptic-before.log`: `58f2acdafb520d38d1ee44dab8daff93595accb613b0ce7d0c68dfbdb3620bff`.
- `/tmp/suunto-235-haptic-tests-final.log`: `d21b3372bdccd4c9b57fd551d839dfffd71fd1409bbe6630c147c34f7f02fba0`.
- `/tmp/suunto-235-haptic-check.log`: `69297c71018bf8ef9f790ecaec53cb3c08b7a221bcdb064e4492a1af12a7d8ba`.
- `/tmp/suunto-235-haptic-sanitize.log`: `f7274dcf9efa98af94bd60f6865558243ffbdbee08b9e13c8352f90e5c464613`.
- `/tmp/suunto-235-haptic-lines.log`: `264445172332e1449653ed44c53217badc8282d9c230f72c4ca3e7e02f0f1d84`.
- `/tmp/suunto-235-haptic-contracts.log`: `1fb2b0475368a44b47feb2c2505961dfd0f9e492d220dae85b79e924602bc7ec`.
- `/tmp/suunto-235-haptic-firmware.log`: `bb39fa67ef429b1f9707c9f9d8ee8fc9a79894d2adca92bfe81aa127f512faac`.

### E-SAP-0041 — 2.35 synthetic OHR startup fixture

Ticket 710 instance-11; integration ticket 781, 2026-09-19. The unchanged
2.35 lane with E-SAP-0038 production records runs for two virtual seconds,
then pauses for two synthetic echo requests. Script
`/tmp/suunto-235-ohr-startup.resc` SHA-256
`0f759f688abd77a6208c4214db7d760d6a5a405d8ba7742668a647452cb1dcf0`.
Read-only lane endpoint `emulator/renode/ohr/SapporoOhr2Transport.cs` SHA-256
`f35e6a69c86fba8142d94c835db4958d238b7cc042f58e67a0606d5a0d2b96f0`.
Run from the sibling root with `.tools/renode/Renode.app/Contents/MacOS/renode
--console --disable-xwt /tmp/suunto-235-ohr-startup.resc`.

Raw `/tmp/suunto-235-ohr-startup-{1,2}.log` SHA-256 respectively
`2a8949410ee6ba0621fe45893dfdd49a0540df4bdc4d9ecf7e425647073c90cc`,
`e5042861b62db45871c7f86ba11e83775d99ab6708444f0494bfabf797e82d02`.
Retaining OHR2 TX/RX rows without host timestamps produces identical 21-row
censuses, SHA-256
`7a0c58edee98bc0a96e5cdd41e3deabc5dfd3eb168ea8120dfaa8e6ba81e8cbc`.
Raw packets and probe sources remain outside Git.

Derived startup order (nine requests, eight responses):

| Command | Sequence | State | Response body after header |
| --- | --- | --- | --- |
| 0010 boot mode | 0 | BSL | zero |
| 0000 identity | 1 | BSL | BSL at body offset 9, otherwise zero |
| 0003 reboot | 2 | BSL -> MAIN | no response |
| 0010 boot mode | 2 | MAIN | zero |
| 0000 identity | 3 | MAIN | MAIN at body offset 9, otherwise zero |
| 000d result | 4 | MAIN | zero |
| 000e result | 5 | MAIN | zero |
| 0006 echo | 6 | MAIN | request body bytes 4..53 echoed |
| 0002 result | 7 | MAIN | zero |

IOM2 address 10 uses 59-byte TX, command 00003b01/config103; reads select 3c
with command 3c003a12/config101 and DMA count 58. Request byte 0 is zero;
54-byte bodies begin little-endian command/sequence and end with a CRC32.
Boot-mode request body byte 4 is 1, bytes 5..53 are ff. Identity, reboot and
result requests have ff in bytes 4..53. Echo has ten caller-supplied bytes
at 4..13 and ff padding at 14..53. Response CRC uses the existing reflected
IEEE CRC32 transport. Ready GPIO62 rises for each queued reply and falls when
consumed; reboot has no reply and switches identity to MAIN (lane source law,
existing E-SAP-OHR2-001 transport).

Paused synthetic echo sequences 42/43 replace those ten bytes with ascending
00..09 and f0..f9. Both replies preserve all ten bytes and the ff tail with
valid CRCs, proving echo is data transport rather than a pinned host timestamp.
The lane's optional I2C analyzer warns about dropping overlapping trace entries
during the paused synthetic operations; the wrapper's direct OHR TX/RX path
still completes and reports the exact replies. These probes establish only
payload echo, not new controller timing.

The lane source explicitly calls identity strings and zero response data a
synthetic fixture, not physical measurements. Implement as a disabled-by-default
`--layer sapporo-2.35-ohr-startup`, pinned to E-SAP-0038's three component hashes,
with eight ordered response hits per reset. The real OHR firmware/state is not
available for lower-level execution. Require exact command/sequence/state and
padding; retain normal CRC/ready/reboot framing. Unknown, reordered, malformed
or exhausted requests refuse; do not fabricate ongoing sensor data. This
fixture must log every hit and propagate a fixture refusal to a machine
compatibility stop before guest retry/reset can renew the hit budget.

E-SAP-0041 implementation and verification (ticket 781):

The new `src/compat/sapporo_235_ohr.c/.h` layer owns its eight-response budget
through `semu_layer_state`, with immutable global metadata and per-device
context. The existing device body-provider checks profile 2.35 and explicit
binding. A refusal latches a diagnostic; the existing machine compatibility
check propagates it to `compat-refused` before any following guest instruction.
This adds no instruction hook, register patch or synthetic sensor measurement.

Integration files: `src/devices/sapporo_devices.c`, `sapporo_devices.h`,
`sapporo_devices_internal.h`, `sapporo_device_compat.c`,
`src/boards/machine.c`, `machine_run.c`. Tests are
`tests/devices/test_sapporo_235_ohr.c` and the new
`tests/integration/test_firmware_sapporo_235_ohr.sh`. README and current status
record the opt-in and remaining limit. Ticket 781/index are planning scope;
status remains ready for integrator review.

Exact verification, all exit zero:

- `make test TEST_FILTER=sapporo_235_ohr`: four cases, complete transport
  lifecycle/CRC/ready/echo, eight atomic refusal variants, identity pins and
  independent counters, device binding/reset and machine compatibility stop
  with zero guest instructions or resets.
- `make check`: 960 PASS records, including five SDL cases.
- `make sanitize`: 955 PASS records, no sanitizer failures.
- `make check-lines`: advisory warnings only.
- `make check-task-contracts`: 146 indexed contracts validate.
- `make test-firmware FIRMWARE_ROOT=/tmp/suunto-235-fwroot TEST_PROFILE=sapporo-2.35.34`:
  all three 2.35 runners pass; other profiles explicitly skip.
- `git diff --check`: success.

Two enabled CLI runs with the pinned private 2.35 manifest, both
`--layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup`,
`--max-instructions 500000000 --max-time 5000000000`, agree byte-for-byte:
`budget / bdcfc / 500000000 / 1805389482`, log SHA-256
`bc24a57b3c533ec37703b3da80ba8d8dda3e4ae40ecb7bc62fbe96c1247ac16e`.
There are eight layer-hit events and eight successful responses, no reset.
The disabled 280M control remains `a6bc8 / 280000000 / 1590729375`, one reset,
SHA-256 `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879`.
Production/pressure runner hashes are unchanged. The enabled suffix is NOT a
UI completion: subsequent diagnostic probes identify a logbook-file assertion.

A one-billion-instruction observer with the real NEMA backend sees zero
frames and reaches `bdcfc / 1000000000 / 2305389482`; instruction sampling
shows the sorted-list insertion loop at bdcf8..bdd00. This is a diagnostic
observation, not a success golden. The optional 30-second lane expansion was
explicitly terminated (exit 143) after remaining at its BKPT frontier; it does
not provide completed 30-second coverage. The paired two-second startup and
halt-on-first-breakpoint probes provide the actual lane evidence.

External verification log SHA-256:

- `/tmp/suunto-235-ohr-tests.log`: `d56ba367de93213757b6056fa781a97a8004ca7d9aaf456b1e72e2b76a6856c3`.
- `/tmp/suunto-235-ohr-check.log`: `7cb39346fca8d7bd13c3cb0805e015569fd41c90685e72618eb2b4073d29238a`.
- `/tmp/suunto-235-ohr-sanitize.log`: `9645149251cb414a0c818259a26acdb516a6ad8321dda9eb416e29de775e74f6`.
- `/tmp/suunto-235-ohr-lines.log`: `c07f20b33335f07a6128ef83794c019449d5dacbde9e1d160d20ae00dd32475c`.
- `/tmp/suunto-235-ohr-contracts.log`: `21139cd9c9d94da679a797c43eabd4cc21d33f3f67d2d0d7787b2454c493b8be`.
- `/tmp/suunto-235-ohr-firmware.log`: `ae79c0bf32e36dc719941ae5a0540df9cc49b81ac38d0146c247d05b85b5eebc`.

Observer provenance for the preceding 1B diagnostic:
`/tmp/suunto-235-ohr-probe.c` SHA-256
`b0cc728d028d2056134c1cd4cda712d35a948cc66855f26ea5172ba101e334ba`;
`/tmp/suunto-235-ohr-probe-1.log` SHA-256
`caf5426a114d26923e726fdcff9c2d3ff23013ef623832642a4ca95482b6ef6b`.
Command: `/tmp/suunto-235-ohr-probe 1000000000 10000000000 200000 100000000`.
The observer enables both named layers and attaches the actual NEMA backend.

### E-SAP-0042 — 2.35 post-OHR logbook-open assertion

Read-only next-gap derivation following ticket 781. No storage behavior is
implemented by this observation. Two native lane probes and two interpreter
probes agree at the first assertion after the eight OHR responses.

Lane script `/tmp/suunto-235-post-ohr-break.resc` SHA-256
`040f7d735703f422c38c9098706a3aaca82db162d52d31865ec9e50ddbc9e2e1`
uses the unchanged E-SAP-0038 lane/production setup. Hooks at 79424/79428 halt
the CPU before the first breakpoint and read all registers plus 16 stack words;
the emulation run is bounded at two virtual seconds. Raw logs
`/tmp/suunto-235-post-ohr-break-{1,2}.log` SHA-256 respectively
`1598ced0d9ad136157075032748ebf0a30e3e3fa11987f5d639d01fb49925b67`,
`eb504c1973ba0040b5c6f1d50efc50b7f96d14a9be81b0bbe6d42e13c2f5377e`.
The single normalized SAP235_ASSERT record matches byte-for-byte, SHA-256
`02c82636fb4ec42d9dfd33651bf03a0a29c0c08c6cd139dca577091d81ef857a`.
Run with the same Renode console command as E-SAP-0041, replacing script path.

Interpreter observer `/tmp/suunto-235-post-ohr-cpu.c` SHA-256
`b97a34b105f6cbdc2e68d24f66d95049e1f5f17ee91caa5c80a109e3c93c253b`
wraps an otherwise unchanged in-tree CPU step, reading only selected boundaries.
It links with the E-SAP-0041 observer driver and current `build/libsemu.a`:
`cc -O2 -std=c99 -Iinclude -I. -Isrc/devices -Isrc/cpu/armv7m
/tmp/suunto-235-ohr-probe.c /tmp/suunto-235-post-ohr-cpu.c build/libsemu.a
-o /tmp/suunto-235-post-ohr-probe`. Run twice with
`500000000 5000000000 200000 100000000`. Logs
`/tmp/suunto-235-post-ohr-{1,2}.log` are identical, SHA-256
`44931ed81936f7d125692cb9a51e711686f111aa2a095c2a60427c74ebec574b`.

Derived boundary census:

- At instruction 280206405, PC d47f8 enters the logbook database open path.
  The helper d4850 loads the filename pointer d4ec4 (`logs/entries.bin`)
  and branches to file-open wrapper 91824, with mode argument 1.
- At instruction 280326724, PC d4800 has returned handle zero. The conditional
  path calls assertion helper d49bc with line 0x35 (53), filename at d4ed8
  (`LogbookEntryDb.cpp`). At instruction 280326729, PC 7945e sees exactly
  those file/line arguments.
- The first BKPT boundary is instruction 280326990, PC 79424, LR d4809,
  SP 10025218. All sixteen CPU registers and all sixteen stack words match
  the lane record exactly. Both execute the same failed logbook-open path.
- Continuing the in-tree interpreter reaches the next `file.cpp:167`
  assertion at instruction 280327008, then eventually loops in sorted-list
  insertion bdcf8..bdd00. The list loop is downstream of a failed file open;
  it is not evidence for changing list/CPU behavior or increasing a heap.

The required next observation is why the lane's open returns zero and what
valid persistent storage/file state this firmware accepts. This entry supplies
neither a successful file fixture nor permission to bypass the assertion,
return a fake handle, or reuse the 2.39 logical-file layer without separate
2.35 evidence and integration scope. Raw logs/firmware-derived probes remain
outside Git; only derived boundaries are retained here.

### E-SAP-0043 — 2.35 missing 64 KiB erase and controlled storage recovery

2026-09-22, ticket 710 instance-12 / integration 782. The E-SAP-0038 three
component hashes were revalidated in both private bundle and read-only lane.
The synthetic production sector retains SHA-256
`c08816067aed620fb8c3a074f5f0e3a8ceb398416d6f9c33d1f6c13df5619a53`.
No physical flash or watch observation is claimed.

Unmodified lane source `emulator/renode/mspi2/SapporoApollo4Mspi2.cs`, SHA-256
`a782983bbc12a93078362a2e121d4e1d786944e16405e86c61b5c8ad0c029c2f`,
implements only 21 erase. DC completes without erasing. The interpreter's
MSPI2 command path also silently completes unknown opcodes. Native firmware
helper f66ac receives address r1 and length r2: f66b6 selects DC at f66d4 for
length 10000; f66ee selects 21 at f6710 for length 1000. These are observed
requests, not proof that the lane already implements DC.

Probe directory `/tmp/sap235-storage/` remains outside Git. Scripts include
the unchanged 2.35 lane, map XIP, load the three exact components and synthetic
production data. Hooks read file open/close, erase arguments/results and halt
at the first assertion. Run from the sibling root:
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt
/tmp/sap235-storage/erase-native.resc` (two virtual seconds), or replace the
script with `erase-synthetic.resc` (five virtual seconds).

| External script | SHA-256 |
| --- | --- |
| erase-native.resc | `aad5ba2fb801d023968accda6d7401b4ae328ea27730155827072420d6d715b0` |
| erase-synthetic.resc | `5223a3b4faea151d402a943f25d6f25096ca9b48ebe969303c103f51ba206ee4` |
| setup.py | `505ec5e72840ced1d686ad9b51fd1b2457b70c8aca8cafe607192b69826cc124` |
| xip.repl | `5baccffc4e00dcf9a509d08c77aa9ed0dcaf8b0c6a8880d8bddcddb92b5a2ca1` |
| census.py | `3244176290c58def32de30a8734efd2e62c45f4d5de81197181815a1785a00c3` |

Raw native logs `erase-native-{1,2}.log` SHA-256 respectively
`29de59403341dc58b14410763165ec109f81dc6e0243e02ca76b6dc9b3d0d12b`,
`22bc018912778567645f70b2bc88ca6cdc6c7e65c68c1da1722e115ec1e6588d`.
All 63 normalized hook records match byte-identically, SHA-256
`7e9e0372386490cf30dd1f55dd2d16e319674f70fb1489889b94ab61f3d211a7`.
There are 939 WREN completions, one 35 setup and ten DC completions, zero
actual erases. Ordered DC addresses (all length 10000, return zero) are
b00000, b40000, 8e0000, ae0000, b50000, a50000, a90000, b60000, b10000,
9a0000. Existing CMGRPLST and three storage JSON opens return real handles.
`logs/entries.bin` read-open returns zero, create returns 80, close executes,
then read-open returns zero and assertion 79424 / LR d4809 / SP 10025218
matches E-SAP-0042. This rules out a wholly absent filesystem.

The **controlled, synthetic** experiment does not alter lane source or
firmware instructions. At f66dc, after each successful aligned 64 KiB
request, it issues sixteen WREN/21 subsector erases through the existing lane
MMIO. It restores command data/address, interrupt enable and pending status;
CPU registers, storage file handles and virtual time are not patched. This
experiment is an explicit hypothesis test of the missing erase, not a claim
that unmodified Renode models DC or physical erase latency.

Raw controlled logs `erase-synthetic-{1,2}.log` SHA-256 respectively
`f2947a9326de25272393ba608f5785bb8e54c0cb567b6c832f91983b9eda08fe`,
`a37619dc6e6bedd6019fbfee07dad90f6aff767f3bcc9a1eafe409b9b9ce95ef`.
All 207 normalized records match byte-identically, SHA-256
`9994369bb5e34b14ee6520d3b44bcbd546690e9ff4537b99e234f6248cf7b0d0`.
There are 22 DC requests, 352 injected 21 erases, 4666 total WREN completions,
and one 35 setup. After the ten addresses above, the remaining ordered DC
addresses are 8b0000, 990000, 740000, 880000, a80000, 970000, b40000,
b70000, a70000, b20000, ad0000, 6f0000. Every request is aligned, length
10000, return zero. Logbook create returns 80, next read-open returns 90,
and update-open returns a0. Further startup creates/reads activity files.
Neither run reaches the former assertion; both end at PC e1862 at five
seconds. This proves bounded file recovery, not UI or physical-device success.

Architecture corroboration: [Micron N25Q256A Rev. X, June 2018](https://www.mouser.com/datasheet/2/671/n25q_256mb_3v-1283553.pdf),
command definitions and ERASE Operations p60, defines DC as four-byte sector
erase, 64 KiB aligned granularity, FF erased bytes and WREN consumption.
[STMicroelectronics' N25Q256A driver header](https://github.com/STMicroelectronics/stm32-n25q256a/blob/main/n25q256a.h)
also specifies sector size 10000 and opcode DC. These references corroborate
the native helper's exact size/opcode pairing; they do not identify the actual
watch's package. Integration 782 uses the documented opcode/geometry and the
lane's existing erase-byte/enable law. Unknown opcodes must refuse instead of
being promoted to successful no-ops. Existing stricter missing-WREN refusals
and synchronous completion remain unchanged; protection, suspend/resume and
physical timing are outside this scope.

The strict controller allowlist also preserves the explicitly probed B9/AB
power-command completion boundary. External `power-native.resc` SHA-256
`7b6f672c50dd2c1dedeedf0b8175032ae97f7fccf83227165af72ccd08b95c80`
runs the same lane without the synthetic production sector for two virtual
seconds, pauses, clears INTSTAT through INTCLR +208, then issues B9 and AB
separately with control C1. Each reports INTSTAT 1 and command readback C1.
Raw `power-native-{1,2}.log` hashes are
`7f3dbb6f4cc4ec49b922d2c2b4416a876c3b043a409574b6ed54d97642668280`,
`d9f1dc1fbaf4d1071db134ef9bd6f9e233d07869c443e4f4479b7c35c5fb7734`.
The six normalized command/readback rows match, SHA-256
`23b91685a6cbfa44ce643eab3b7cb3432113284a111ff80f3c6b8123e1dfbee5`.
Lane source confirms neither command reaches the flash overlay or consumes
WREN. This is completion-only coverage, like existing 35 setup, not a new
physical power-state model. Refusing B9 broke the pre-existing no-fixture
input/idle paths; those checkpoints must be preserved, not re-pinned to the
resulting faults. Other unrecognized commands continue to refuse.

E-SAP-0043 implementation and verification (ticket 782):

`src/devices/sapporo_flash.c` dispatches DC to a checked, aligned 64 KiB
storage erase and consumes WREN only after success. `src/soc/apollo4/mspi.c`
preserves the full device address and uses an explicit command allowlist.
`src/core/storage.c` stages all missing pages for an aligned multi-page erase
before mutation. Public contracts/formats and source images are unchanged.
The new direct/controller test file covers lower/upper/final blocks, every
erased byte, neighbors, immutable base hash, missing/reset enable, malformed
frames, out-of-range requests, unknown-command atomic refusal and B9/AB.
The allocation regression injects failure at each of fifteen missing pages,
proving existing bytes, page ownership and count survive each failure.

Before implementation, all three initial block/controller cases failed;
`/tmp/sap235-storage/block-before.log` SHA-256
`f3af861ebf3fca63e387df30692f81b7706808dbe3e5ac7558a2283c7b0b1150`.
The allocation regression also failed, `atomic-before.log` SHA-256
`a8326270c1ecf3cb83acc89bad49a88fba75bc52911edf8514bfdb0a3a08cef8`.
After correction, four block/controller cases and the allocation case pass.

Private interpreter observer source `file_observed_cpu.c` in the same external
directory, SHA-256
`bda333d6d3ce3a8f267b2325d31a4963b77d198eaa0aff29c63a3ee95344cf8c`,
wraps unchanged CPU execution with reads at selected file/erase/assert PCs.
Link with `cc -O2 -std=c99 -Iinclude -Isrc/cpu/armv7m
/tmp/sap235-storage/file_observed_cpu.c build/obj/src/frontends/cli.o
build/obj/src/frontends/main_headless.o build/libsemu.a -o /tmp/sap235-storage/observer`.
Run the ordinary 2.35 CLI with both layers, instruction cap 1000000000 and
time cap 10000000000. `observer-{1,2}.log` match exactly, SHA-256
`a3b2157c6f2566a0fa0223cd600e62735cce3b9dc0906fb08b5e49b1a02e3936`.
At return PC 91842, logbook create is handle 80 at instruction 277549494;
read-open is handle 90 at 280328556; update-open is handle a0 at 280554620.
No old logbook assertion occurs. Final tuple is
`e1862 / 995880966 / 10000000000`, budget stop.

The regular CLI's repeated first-visible-frame runs use both layers,
`--until normal-frame --max-instructions 1000000000 --max-time 10000000000`.
They return zero at `bdd2a / 813500000 / 3733351422`, `stop=user`, complete
log SHA-256 `2813f2dfdad153de7f2f00250911d86cced75ec15ce01c73d86c822b4470a528`.
The new block-erase shell gate pins this boundary. The existing OHR enabled
500M suffix is explicitly re-derived to
`bdc36 / 500000000 / 2719206417`, log SHA-256
`b6c35ad981bdceef823937e68fb57bdf101853e62c0ae52caf63fea586aabeb0`.
The 30M, 80M, 150M and disabled 280M prefixes retain their exact earlier
hashes. No production/pressure script or no-fixture input golden was changed.

The external frame observer `frame_timed_main.c`, SHA-256
`1f02bdcf38b8a4413478b5d3cd6c26bfa94cfc6a3e41718a4c14ad468529697d`,
links with the ordinary CLI object/library and registers frame and read-only
input-poll callbacks. Repeated 1B/10-second cold logs `frame-timed-{1,2}.log`
match, SHA-256
`6d91002f6ec71769d12c6a896c2ab97dac972f35962f76c374533be453afc1ef`.
Six frames are published; distinct CRC/generation/time records are:
`2a01c517/1/3732832521`, `4979f432/2/3733315957`,
`bff092e4/4/6843088226`, `f5b34cc5/5/6843610795`,
`3bd12ac8/6/6847050830`. External pixel inspection identifies the last as
“Select language”. No frame pixels or firmware bytes enter Git.

Exact verification commands/results (logs in `/tmp/sap235-storage/`):

| Command | Result | Log SHA-256 |
| --- | --- | --- |
| `make test TEST_FILTER=block_erase` | 4 pass | `647f22e303d78aa867f7962b2e0ea874b38b91d3a4e396e715d032eaa6bedac8` |
| `make test TEST_FILTER=storage_erase_atomic` | 1 pass | `3a6a04e8ddd1099a02a4515a9698578a582e43f82981bd12443fece1f2a78df9` |
| `make test TEST_FILTER=apollo4_mspi` | 15 pass | `516d4a67c5dc697ece276578cfb46f5d629d02d55343f60c35674d76583af495` |
| `make test TEST_FILTER=sapporo_flash` | 13 pass | `c21b460ddda2648ff81870f944eb242e3fc7a89cdd67ee359cc054b412db0cb3` |
| `make test TEST_FILTER=storage` | 21 pass | `4ac66317da3716cb0327daa7e86dae1775deb152621d581341dcca34fd3f4db6` |
| `make test TEST_FILTER=sapporo_235_input` | 7 pass, original pins | `4d5a8144b0d40e8c4c24357dfa73b5cc25ea23ced7befa0707fbf50f2168940d` |
| `make check` | 965 pass, exit 0 | `2fc35ea487594ac573d60c7c6e372ad87b55d33834d390dfe699f19cee972258` |
| `make sanitize` | 960 pass, exit 0 | `3748264cff1e12a58480fb5a478e2e3e3d4d31880a10a4ba5d9372bcdd283cc1` |
| `make check-lines` | advisory, exit 0 | `45565c05d5d37c57429d9f5c5f661d9e0617ac3b97068fa1942c32dae849dddf` |
| `make check-task-contracts` | 147 before follow-up 783, exit 0 | `fd0a8463acec53a8e23556a209c7174f31f4a78e979f58cd1d2b1d9d12ff0c47` |
| `sh tools/test_sdl_live_input.sh` | exact 2.22 checkpoint/hash, exit 0 | `228575cb7c370d51649adcd05eb6ae3bc055c22bcd177c7ba3c9381300d5d263` |

`make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
TEST_PROFILE=sapporo-2.35.34` passes all four selected 2.35 scripts; other
profiles skip. Complete log SHA-256
`ab9806bb54bce3744f67e7e7ed42ab155087a3da14462fa0ff9198d583fe299e`.
`git diff --check` passes. Ticket 783 separately tracks shared 2.39 era drift;
no 2.39 golden is re-pinned here.

### E-SAP-0044 — 2.35 post-display GPS assertion and control limit

Next-gap observation only; no GPS response/pulse or button behavior is added.
The E-SAP-0043 controlled storage lane, with unchanged synthetic production
and native OHR fixture, was extended to a 15-second budget. It halts at the
first BKPT. Script `/tmp/sap235-storage/gps-boundary.resc` SHA-256
`287143c90f64742a0783bb1f0d7f65a51f82c39128fb1c66d22db3774b4d3b9f`.
Raw `gps-boundary-{1,2}.log` SHA-256 respectively
`2caa5b2b58674c369b81f04d9229b1774a1bfe0313ea4822d46e382849c4ae03`,
`73eaab9d1485a388fed8498fc41b035cc15018664d1b1650ecc54ea44186706c`.
The two assertion records reproduce identically, SHA-256
`92ba31004ec5562d40993558034d310757fbb75307f01496ae7634595753f8a7`:
file `CXD5610GF-driver.cpp`, line 894, LR 12572d; BKPT PC 79424,
LR 12572d, SP 1002f7d0. The lane's GPS transport is present but this probe
does not schedule any synthetic awake pulse or patch CPU control flow.

Interpreter source `frontier_cpu.c` SHA-256
`52bc6128706aa7e0994be6b060c16d7c234888a8837dbbd55e41954226c5cc19`
adds assertion-site diagnostics to the prior file observer. The external
MSPI observer at that run had SHA-256
`4fff541f2fb8873d9e16fbc533ff66df04362ff25d8fbc840c631d062d956b34`;
it only reports controller refusals and none occurs on this prefix.
`frontier-1.log` SHA-256
`363a1ba1aab9edcb3f8d6c252edcd3abc5f8c09f973deee9f9b6f14dbff1949d`
records helper 7945e at instruction 1022014915, identical file/line/LR,
then BKPT 79424 at instruction 1022015210. This 1.5B/15-second bounded
observer continues to `bdd00 / 1500000000 / 11525639699` after the assertion.
Static read-only disassembly of the pinned firmware identifies state 5 at
125708, increment of the byte retry counter at 12570c..125710, comparison
with three at 125714, and assertion call at 125728. The missing successful
GPS-start transition needs its own observed exchange; these facts do not
justify bypassing the assertion or adding an unbounded awake heartbeat.

Input remains a separate open acceptance condition. A single external
`frame-timed` run with a middle press at 8 seconds and release at 8.3 seconds,
1.02B/11-second bounds, publishes 22 frames but only the same five distinct
startup CRCs; final CRC remains `3bd12ac8`. Its `control-timed.log` SHA-256
is `699daf5aa81dfaa1ba0064c1229290632e28c6008f6d56d9a9e4707cae226475`;
final tuple `a703c / 1020000000 / 8374206023`. A dummy SDL run using
`SEMU_SDL_LIVE_TEST=middle-language`, both layers, `--until middle-language`,
1.1B/11-second limits reaches settled step 2, generation 6, CRC `3bd12ac8`,
then spends its instruction budget after the third injected button.
`sdl-235-control-1.log` SHA-256
`f1529a9a96da6a5189813f772d137ea6702b1db0af0a681ee834efd19c58d9a6`,
final tuple `a72d2 / 1100000000 / 7479768413`. These exploratory controls
prove delivery/redraw only, not successful language-menu navigation.

E-SAP-0044 control follow-up: the short input budgets above ended during the
transition. Extending the same 8-second middle press / 8.3-second release to
3B instructions and 10 virtual seconds opens the native language menu with
English selected. `control-long.log` and `control-long-2.log` are identical,
SHA-256 `4904ba7c49f50079264ef16505919bc92412dfd30c0c0df5f4013010aaf5fc99`.
The replay file SHA-256 is
`28946c08f887db73c98a0da41d4e917394fd7076c7547152250e83a0ec6a38a2`.
There are 79 frames, 62 content changes, last CRC `405422e1` at generation 79,
virtual time 8562766996; final tuple
`e1862 / 1244168424 / 10000000000`, budget stop. Pixels remain external.
This establishes one menu transition, not completed onboarding.

With the existing SDL live-test driver, 2B/11-second limits complete all three
steps: generation/CRC 3/4979f432, 6/3bd12ac8, 79/405422e1. The final tuple is
`0800009e / 1202094208 / 7971442704`, user stop, exit 0. The new optional
`sh tools/test_sdl_sapporo_235.sh` repeats the full run, compares logs and pins
SHA-256 `523bbceff0a44a5e5eb64ba19e8bce2ca86bc10b2aa0d8ad7bbc50e157a4bdac`.
It uses the existing SDL keys/clicks, renderer and semantic input mapping.
An explicitly missing manifest returns 2; a 2.22 manifest returns 1 after
CLI identity refusal, before guest execution. Negative log hashes are
`1ade8122cd3303c2f16db626156f267895b6632d02e2d21ce048a467d142b38a` and
`9da8df3de2928809b3df04297199f7fac47c8f12003de22d5cc6225c0c74c570`.

### E-SAP-0045 — shared erase impact on 2.39 era gates

After E-SAP-0043, run
`SEMU_SAPPORO_239_FULL_FLASH=/tmp/sap235-storage/sapporo-239-full-flash.bin
make check-era`. All 43 scripts execute; 13 pass and 30 fail, make exit 2.
Raw external log `/tmp/sap235-storage/era-239.log` SHA-256
`960b9c19a5fbee0491f642aa819cff0e52a6523b8b2176bf95ef1796a336e441`.
The synthetic full-flash fixture retains SHA-256
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`
before and after; the exact three 2.39 components validate.

All sixteen earlier E-SAP-ERA-GATES-239-001 failures remain. Fourteen additional
failing runner suffixes are `activity_budget`, `ctimer13_inten`,
`ctimer_combined_inten`, `file_seek`, `file_size`, `general_budget`, `gps_awake`,
`gps_five`, `gps_reopen`, `gps_startup`, `logical_files`, `ohr2_command2`,
`personal_budget`, and `wbsto_cache`. The prior sixteen are `gpio_wt1`,
`haptic`, `haptic_calibration`, `history_budget`, `lps22`, `ohr2_boot_mode`,
`ohr2_bsl_identity`, `ohr2_echo`, `ohr2_main_identity`, `ohr2_result_13`,
`ohr2_result_14`, `ongoing`, `preload1`, `quiet_read`, `widgets`, `zip_read`.

Failures include changed checkpoints/artifacts and real logical-file path
refusals; none is assumed harmless. For example the old layer-off boundary
changes to `70378 / 72774982 / 521257564`, and the general-settings gate
reports an unknown writable-file path after its POI prefix. Correct erases
change native filesystem state on the shared flash endpoint; existing 2.39
adapters must be audited against that state. Ticket 783 records the separate
integration audit, blocked on review of 782. No 2.39 expected hash, stop,
compatibility hit budget or unknown-file refusal is changed in this instance.

The final post-documentation `make check` still passes 965 cases, exit 0;
`/tmp/sap235-storage/check-handoff.log` SHA-256
`18452667833edda846d0295e1c013a0c10139f3833a0cb4529db970896525376`.
`make check-task-contracts` validates 148 tickets, exit 0;
`contracts-handoff.log` SHA-256
`29c4d06e1fcd5da143a5570c335f0a3c1225a506be980525e30793a345d56513`.
The passing 2.22 short SDL gate and failing 2.39 sweep are both part of the
handoff; this is not a passing all-profile release gate.

Final optional UI gate: `sh tools/test_sdl_sapporo_235.sh` exits 0 after two
byte-identical runs. Wrapper output `sdl-235-gate.log` SHA-256
`529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9`.
The subsequent `make check` including the final script/documentation state
again passes 965 cases, exit 0, with the same complete-log hash
`18452667833edda846d0295e1c013a0c10139f3833a0cb4529db970896525376`.
`sh -n tools/test_sdl_sapporo_235.sh
tests/integration/test_firmware_sapporo_235_block_erase.sh` and
`git diff --check` both exit 0. Tickets 782/783 remain ready/blocked for
integrator review; the overall fully functional Sapporo goal remains open.

### E-SAP-0046 — 2.35 initial GPS status/version lifecycle

2026-09-22, ticket 710 instance-13 / integration 784. Exact E-SAP-0038
firmware; E-SAP-0043 controlled storage recovery remains explicit in the
read-only lane probes. Source firmware and lane files are unchanged. This
is a synthetic receiver fixture experiment, not physical receiver evidence.

Pristine startup helper 125462 opens UART via 125388, installs callback
1250e7, stores pending two at driver+273, and arms state four for 3000 ms
via 126ef0. Post-arm 1254ec precedes MOVS R0,1. In both lane runs driver
100364ac, R5 1003671c, R6 100367e4, UART 100456a0, callback 1250e7,
state/pending 4/2. The native parser at 12687a..1268cc recognizes `$PSS`
and schedules the pending state after 10 ms. The retry counter at 10058a00
comes from the literal 100589fe at 125d9c plus two in the state-five branch.

External `/tmp/sap235-gps/startup.resc` supplies exactly ten synthetic bytes
`$PSS0000\r\n` after 10 ms once, at 1254ec, through the lane transport's
`InjectHexAfter`. It installs one exact `@VER\r\n` response with the same
line/delay. Native state sequence is 2,14,15; UART closes at state 14;
15 schedules 18. Later reopen arms pending seven/state four for 10000 ms
at LR125665, with no fixture. It times out to states four/five by the 15 s
cap. No assertion occurs in this positive window; only six TX bytes occur,
`405645520d0a`, and twenty RX bytes. No GPIO24 pulse or extra reply is sent.

The negative `/tmp/sap235-gps/wrong-prefix.resc` replaces only the initial
prefix with `$BAD0000\r\n`. No TX occurs. Native states four/five repeat
three times and assert `CXD5610GF-driver.cpp:894`, LR12572d, BKPT79424,
SP1002f7d0. Both negative runs stop at that assertion within 15 virtual s.

Run each script twice from the read-only sibling working directory:
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt SCRIPT`.
All scripts/logs/probe sources remain outside Git. Raw hashes:

| Artifact under `/tmp/sap235-gps` | SHA-256 |
| --- | --- |
| startup.resc | b77c328ae1fab4c74abbe0047f78ec10ee97c4b4c43d12900c87c5c1827dec2d |
| startup-1.log | 6ba4a13b3bc88ffba56cadd4db3e2d1567d2c45dba8712f40f044cdff2dd42d3 |
| startup-2.log | 4c2fa0415af96eb31745927ba0422df9c2350207f36e33623dc775e4cccf837b |
| wrong-prefix.resc | 814b737bb0b92ac99ecdfad28db0827be527d29b51efe2058adc6159711c4e55 |
| wrong-prefix-1.log | 81a8a33155ae1b064a5c0146999d15e0c68867eb6baf056c3cbb543321c3a85b |
| wrong-prefix-2.log | 6da191bde2dcbea2f2698e61d9ca84823c6d6391fd06ae71e013fac40263a445 |

Normalized positive census: 42 rows, SHA-256
`607671b75125dd2a1836671adab26e2df83cf9ce2b8278c4aae33252077310ab`,
byte-identical; negative: 31 rows,
`a767ffebd20f1eb33aacc40ed9b0450c159f8fd3abdbcaca7859865754774011`,
byte-identical. Census retains ordered arm/state/schedule/assert tuples and
UART directions/bytes; expands Renode's repeated-line `(3)` count (four
zero characters per line), excludes host timestamps. Derived counts and
boundaries above remain authoritative if volatile logs disappear.

The in-tree read-only observer independently sees the same initial driver
and UART values at instruction 251711528 / time1555697547, retry zero and
UART user-data zero. It halts only at the diagnostic BKPT at
1022015210 / 11047654909, retry three, matching E-SAP-0044. No initial
response is present in the production baseline. Integration must separately
opt in and hash-pin this fixture; this evidence authorizes no reopen,
periodic awake, time/fix or physical receiver behavior.

E-SAP-0046 implementation: `sapporo-2.35-gps-startup` is a separate opt-in,
three-hash-pinned device fixture. It checks the complete driver/UART spans,
R5/R6 offsets, callback/pending values, callback identity and zero retry before
scheduling initial RX. Ordered aggregate hits permit one initial status and
one exact version reply. Each is logged; all other lifecycle/commands refuse.
Only the diagnostic latch may change on refusal; device/RX, hits and logs do
not. The machine observes the latch before executing another instruction.
Reset cancels pending RX and clears the binding; explicit machine reset
reinitializes the layer's budget. No valid CPU instruction is intercepted or
changed; the PC observation only triggers a normal delayed UART response.
A general physical receiver model is unavailable, so this synthetic fixture
cannot be enabled automatically or used as physical GPS evidence.
The lane has no observed power/reset-to-status timing law. The post-arm PC
is the proven admission point after the native callback is installed;
inventing an automatic GPIO/power response would extend beyond this evidence.

New private regression failed before implementation with `unknown layer`
(`/tmp/sap235-gps/before.log`, SHA-256
`c9727972bb0e044536e2ed6b9b582568db063c8d4baa12b2c5eccc8436ef38bf`).
The missing-response baseline was separately observed by the external CPU
wrapper `observed_cpu.c`, SHA-256
`55fcdce6d331635904fd2d02b5f9a6039df179a5f16be03d62690b1bcbd71f34`;
baseline log SHA-256
`c213534492b1358bece668640d84488084c5cec6f70955f483740803b95b7c51`.
It logs selected driver PCs and stops at BKPT only for diagnosis; it does not
supply responses or alter instructions. Census source SHA-256
`f8654d534383c65bc09b0f281ad896e126398fe4ed0246e04404855f52becd02`.

Paired enabled interpreter observations, `enabled-1.log` / `enabled-2.log`,
byte-identical SHA-256
`d778b66024588674f258f16317b67d7b25530180a96134c42eab1f96c4c170d6`:

| Native boundary | Instructions | Virtual ns | State |
| --- | ---: | ---: | --- |
| initial arm 1254ec | 251711528 | 1555697547 | 4 / pending 2 / retry 0 |
| state 14 entry 125694 | 281973256 | 2184057416 | 14 / pending 14 / retry 0 |
| state 15 entry 125694 | 289445162 | 2421599960 | UART closed / retry 0 |
| reopen timeout arm 126ef0, LR125665 | 816474243 | 4655188295 | pending 7 / retry 0 |
| unsupported retry 1254ec | 996415389 | 14881889213 | compat-refused, hits 2 |

Observer command: compile with `cc -O2 -std=c99 -Iinclude -Isrc/cpu/armv7m
/tmp/sap235-gps/observed_cpu.c build/obj/src/frontends/cli.o
build/obj/src/frontends/main_headless.o build/libsemu.a -o /tmp/sap235-gps/observer`;
run with exact private 2.35 manifest, all three named 2.35 layers,
`--max-instructions 1500000000 --max-time 30000000000`. Unmodified CLI
reaches the same final tuple without observer rows; full-log SHA-256
`94acff58c93ab408b34b4a5233670740d93464cbab739f02717f35eb5f62d878`.
The earlier 500M/5s CLI checkpoint is `budget / 000ee120 / 500000000 /
3343660033`, full log
`f40ab5506257140b44724ad8522a515ffdc4a3bd98cf2af5d21dbe7b47f22284`.
Both are exact pins in the new private runner; no existing pin was changed.

New-layer SDL command: `SDL_VIDEODRIVER=dummy
SEMU_SDL_LIVE_TEST=middle-language build/suunto-emu-sdl run` with the same
manifest/three layers, `--until middle-language --max-instructions 2000000000
--max-time 15000000000`. It reaches generation 79 / CRC405422e1 and exits
`user / 000cd0ba / 1149073728 / 8679404651`. Exact full transcript SHA-256
`5b000056a5cb5f20e54f7f405fae2e8856cb0c1e4aa9da9c2ee9ff4b16682629`.
GPS changes timing and execution, but not the language-menu pixels. Existing
two-layer SDL transcript and checkpoint remain unchanged.

### E-SAP-0047 — 2.35 later GPS reopen census and bounded integration

The next lifecycle is distinct from initial startup. Extend E-SAP-0046's lane
probe with exactly one post-reopen injection at 125664 and a second ordered
exact exchange, `@GSR\r\n` -> synthetic `$PSS0000\r\n`, each delayed ten ms.
Keep the controlled flash and original startup interventions explicit; no
GPIO24 pulse, register/CPU edit or location/time sentence is added. Both runs
have a 20-second virtual cap. No response is supplied to later commands.

At 125664: driver100364ac, R5=10036520, R6=1003671c, UART10045798;
bytes at +74/+7b/+7f/+272/+273 are 15/0/2/4/7. Retry10058a00=0 and
startup-seen100589ff=1. Native scheduling/dispatch advances through
7 -> 8 -> 9 -> 10 -> 12, zero retry. State10 schedules state12 after5500ms;
first state12 schedules another5500ms; second state12 transmits `@GSTP\r\n`.
There is no `@GUSE 0` TX in this exact branch. Total UART census is nineteen
TX bytes (`@VER`, `@GSR`, `@GSTP`, each CRLF), forty RX bytes (four synthetic
status lines), zero awake pulses, no assertion at the cap, final PCe1862.
The lane transport accepts arbitrary TX into its transcript, so its silence
after GSTP is not authorization for an in-tree completion fallback. The
existing in-tree fail-closed transport must still refuse unmodeled commands.

External `/tmp/sap235-gps/reopen.resc` SHA-256
`db734102890790cc1a1afeddcb6ad231b42fdd99417c404a26d6a6095bc7ac79`;
run with the same Renode command as E-SAP-0046. Raw logs `reopen-1.log`,
`reopen-2.log` SHA-256 respectively
`8f7cf9b004a2ca1e125df5689a8800b52c228edda03254629050358dc9c11da3`,
`635ebf6f7d028ea5875c16acbd1d930651feeb72b62afb095e7f1357b8131a8a`.
Their normalized 86-row censuses compare byte-identical, SHA-256
`2662cde0495d66e952075baec01c1489f73d9f1568763146bb0e6b13d3db667f`.
Census source `/tmp/sap235-gps/reopen-census.py`, SHA-256
`3338ac9b11a87e706d2938a1e9588cc499757831f48815986cd00eb42e0db290`,
uses the same timestamp removal and explicit repeated-line expansion, plus
post-reopen register/flag rows. Derived counts/tuples above are retained here;
no firmware-derived log, probe or pixels enter Git.

This evidence supports designing a separate bounded reopen integration.
It proves no GSTP response semantics, physical GPS behavior, ongoing cadence,
fix/time data or full onboarding. Ticket784 remains initial startup only.

E-SAP-0046 final verification (2026-09-22, integration784):

| Command/result | Log SHA-256 under `/tmp/sap235-gps` |
| --- | --- |
| `make test TEST_FILTER=sapporo_235_gps`: 4 cases, 31 refusal variants, exit0 | `2e82cdbdbdeb6cc091441afd86ecb1f640d17e0059d0946140494a003085f20d` |
| `make test TEST_FILTER=sapporo_cxd5610`: 13 cases, exit0 | `1ab0320c9d1fa894fb2ced9ba709e44762b6c845561c80eba5436b05a6b3186f` |
| `make check`: 969 cases, exit0 | `acf1939ec592e3fc5455cdb32d5b30b0e9ae4a5d1c42841cdba6725a2c63b95e` |
| `make sanitize`: 964 cases, exit0 | `f39ac1752f31a507358f29f8bff863f65df7f1bcbb912208db9529899dd29fa9` |
| `make check-lines`: advisory, exit0 | `3d52a1ca9b27d1bf5918ec3d1d691131feed47690c30d673eb0076d2dbfc3b5e` |
| `make check-task-contracts`: 149, exit0 | `68d28216f14e55d58de882f351b4b1656e946a9383bd98af059fc2dbfecd02fc` |
| `make test-firmware TEST_PROFILE=sapporo-2.35.34 SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`: five 2.35 scripts pass | `43165cbf1f4083a7dd2167a606a1527f7fea32325f9463f0d880de7fdf4d4fb0` |
| Final GPS script, including added paired refusal suffix, exit0 | `ac0aa8652fd5935ecb22d46d76e43422de9bbfa9f95565449385596eacda9a48` |
| `sh tools/test_sdl_sapporo_235.sh`: unchanged old two-layer transcript, exit0 | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |

The final GPS script was rerun directly after adding the refusal suffix:
`SEMU_EMULATOR=build/suunto-emu SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
TEST_PROFILE=sapporo-2.35.34 sh tests/integration/test_firmware_sapporo_235_gps_startup.sh`.
New-layer SDL runs `sdl-new-layer-1.log` / `sdl-new-layer-2.log` compare
byte-identical with the full transcript hash recorded above. `sh -n` on the
new script and `git diff --check` pass. Tests use bounded virtual/instruction
limits; no firmware or proprietary pixels enter Git.

Changed files: `src/compat/sapporo_235_gps.c/.h`,
`src/devices/sapporo_devices.c`, `sapporo_devices_internal.h`,
`sapporo_device_compat.c`, `src/boards/machine.c`, `machine_run.c`,
`tests/unit/test_sapporo_235_gps.c`,
`tests/integration/test_firmware_sapporo_235_gps_startup.sh`, README,
current-status, this ledger, `plans/index.tsv` and ticket784.
No public header, generic UART/scheduler/CPU behavior, profile metadata,
snapshot format or existing golden changes. Other-profile compatibility
paths are unchanged; the 2.39 era sweep was not repeated for this explicitly
2.35-only layer, and its prior 30/43 failures remain unresolved. Requested
integrator action: review 784 separately from implementation and assign the
E-SAP-0047 reopen attachment before adopting further responses. Ticket784
status remains ready; the full functional emulator goal remains incomplete.


E-SAP-0047 integration785 and verification (2026-09-22):

The separate `sapporo-2.35-gps-reopen` layer implements only the two observed
synthetic lines, each delayed10000000ns. It requires the completed initial
fixture (exact descriptor, two hits, enabled, same logger, no refusal), pins
the three E-SAP-0038 hashes, validates the register/driver tuple above plus
UART callback1250e7/userdata0, and owns two logged hits per machine. Native
instructions and parsing remain intact. Unknown commands, repeat/reordered
exchanges and invalid state refuse before RX/counter/log mutation; scheduler
failure latches its diagnostic. Machine dispatch propagates the latch before
another instruction. Reset cancels RX, clears bindings and resets hit counters.
Either CLI ordering of the two GPS layers is accepted after full binding
preflight. Initial-only attachment and old checkpoints are unchanged.

The new private regression was run before implementation and failed because
the new layer was unknown: `/tmp/sap235-reopen/before.log`, SHA-256
`9a492f6df9fae3f29885eec7e9d6d2f1bc789cb6a6977a0648cec4b92e9720cc`.
The final runner repeats the four-layer cold run, caps2B instructions/30s,
requires exit3 and exact refusal tuple, and checks two initial plus two reopen
hits. Full CLI transcript SHA-256
`690422bf19ff32bb5de8da3db63a825ba11102a326c112dcbf30ab52e5e7c9ca`.
Reversing the GPS layer option order gives that same transcript.

Paired interpreter observations in `/tmp/sap235-reopen/observed-1.log` and
`observed-2.log` are byte-identical, SHA-256
`900940065b735c11ddef1f74f05e2d30a29536ead1bf5f421a22becb4bb5f93b`.
Compile E-SAP-0046's unchanged `observed_cpu.c` against the current library
with the command recorded there, output `/tmp/sap235-reopen/observer`, then
run the four layers with `--max-instructions 2000000000 --max-time 30000000000`.
The observer records native register/state reads without changing instructions.

| Boundary | Instructions | Virtual ns | Retry |
| --- | ---: | ---: | ---: |
| reopen arm126ef0, LR125665 | 816474243 | 4655188295 | 0 |
| state7 at125694 | 821817559 | 4703807842 | 0 |
| state8 | 823662105 | 4714227482 | 0 |
| state9 | 826441348 | 4723567659 | 0 |
| state10 | 835669541 | 5331872613 | 0 |
| first state12 | 935908161 | 10809078124 | 0 |
| second state12 | 1019618827 | 16287278768 | 0 |
| next request refused, PC001be85a | 1020576082 | 16288236023 | 0 |

At reopen the interpreter UART is10045d60, callback1250e7/userdata0;
all other driver/register fields match the lane tuple. A second read-only
observer additionally records native TX helper12540c: `@VER` at
271863148/1576689357, `@GSR` at826441384/4723567695 and `@GSTP` at
1020558113/16288218054. Source `/tmp/sap235-reopen/tx_observer.c`, SHA-256
`7efd774605f96ca35eb6578ffc1a4a5085ee8092f04fd6bde28b0c9cebd69df3`;
compile/run identically with that source. Both `tx-observed-1.log` and
`tx-observed-2.log` match, SHA-256
`411ee2d41966ba1f77cf501de2d73d0a60a9380ca03293ccd30b8385fc768873`.
The pristine state12 branch tests awake flag100589fe; without an observed
awake transition it eventually takes native recovery and sends GSTP. This
integration supplies no awake pulse or GSTP completion semantics.

Negative lane control: `/tmp/sap235-reopen/wrong-prefix.resc` changes only
the post-reopen unsolicited line to `$BAD0000\r\n`. Its SHA-256 is
`1c4bcecbc00f3fd3d16ce0c7c7baf2a3904e22ab68fb68c0dc83f93e21b65026`.
Run twice from the read-only sibling with
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt
/tmp/sap235-reopen/wrong-prefix.resc`; each has a20s virtual cap. Raw
`wrong-prefix-1.log` and `wrong-prefix-2.log` hashes respectively:
`d16adcc60162dff7641c1d4ec99dfb3f8615db253faa1077a86f94448c9e6097`,
`820426c173dfa9979e6d7b7ea0e078ca48f5916f482249313701069892d81bb6`.
The normalized62-row censuses match, SHA-256
`3d0a63f8cb2fdb875f19977236ded6e134ce54cca4f363d546fa93dc0e39df82`.
Normalizer `/tmp/sap235-reopen/wrong-prefix-census.py`, SHA-256
`9c21ba595151628d8c50572b41e6f8ca8db102f221ecb73fdc0d311bd4e48da2`,
uses the same explicit repeated-byte expansion as the positive census.
Derived result: six TX bytes (VER only), thirty RX bytes (two initial valid
statuses and one BAD line); initial states2/14/15 pass, reopen pending7
expires in state4 and cycles5/startup, no state7/8/9/10/12, no GSR/GSTP,
no assertion at20s, final PCe1862. The native parser distinguishes the prefix;
a merely received line does not satisfy the reopen.

Paired SDL command (same private manifest and all four named 2.35 layers):
`SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk
build/suunto-emu-sdl run --profile sapporo-2.35.34 --firmware
tests/private/sapporo-2.35.34.18929/firmware.semu
--layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup
--layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen
--until setup-next --max-instructions 4000000000 --max-time 30000000000`.
The second run also sets `SEMU_SDL_PPM_DIR=/tmp/sap235-reopen/frames` for
visual inspection; no frame pixels enter Git. `sdl-walk-1.log` and
`sdl-walk-2.log` compare byte-identical, SHA-256
`dee38d7d1eb4682871ea573db86e3b4f5669b7b6fdf2e30c76e9cbce7b634399`.
Observed native screens: Welcome gen894/CRCa40f952b/time11430442553;
Birth year1990 gen1197/CRC2ce89ebe/time14511149843;
Unit system/Metric gen1274/CRC3e13459a/time15281284186.
After advancing step10 the walk stops compat-refused, PC001be85a,
3544601348 instructions,15965172775ns. It does not finish onboarding.

| Exact command/result | Log SHA-256 under `/tmp/sap235-reopen` |
| --- | --- |
| `make test TEST_FILTER=sapporo_235_gps`: 8 cases, 4 new/43 refusal variants, exit0 | `c5ce8ef2fdddcd0d5cd79b9c99f4a34af0f261ec59eee8d82b170f60329b38cb` |
| `make test TEST_FILTER=sapporo_cxd5610`: 13 cases, exit0 | `1ab0320c9d1fa894fb2ced9ba709e44762b6c845561c80eba5436b05a6b3186f` |
| `make check`: 973 cases, exit0 | `15d0a6f62a1edb55337de42953a44d97172cbebee47986009f37e6137c47c0d8` |
| `make sanitize`: 968 cases, exit0 | `43e95bf6c2f5506b0bc7ed6c95e212c05e2df20c8168c2bb94c38902e133297f` |
| `make check-lines`: advisory, exit0 | `d77b418dd4008e9e73e437593160f34495fd20e2464a1202ffdb66da5f953607` |
| `make check-task-contracts`: 150, exit0 | `3c1f31342b3cb66c63a145a8e56f5664a3a1665836a9721b816f60bd93e5ddd6` |
| `make test-firmware TEST_PROFILE=sapporo-2.35.34 SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`: six scripts pass, other profiles skip | `144ca2d0f4cbe40fa0353cc05266fc5cc3c33c22832508e7c021e311824339a5` |
| `sh tools/test_sdl_sapporo_235.sh`: old two-layer transcript unchanged, exit0 | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |

Changed files: new `src/compat/sapporo_235_gps_reopen.c/.h`,
`tests/unit/test_sapporo_235_gps_reopen.c`,
`tests/integration/test_firmware_sapporo_235_gps_reopen.sh`;
`src/devices/sapporo_devices.c`, `sapporo_devices_internal.h`,
`sapporo_device_compat.c`, `src/boards/machine.c`, `machine_run.c`;
README, current-status, this evidence ledger, ticket785 and index row.
No generic CPU/scheduler/UART MMIO behavior, public include API, profile,
snapshot format or existing golden changed. The profile-specific fixture
cannot activate on other versions; the 2.39 era sweep was not repeated and
its prior30/43 failures remain unresolved. Requested integrator action:
review785 without adopting ongoing GPS/awake behavior. Ticket status remains
ready. Full onboarding, 2.35 snapshots and other-version gaps remain open.

### E-SAP-0048 — 2.35 bounded synthetic GPS awake sequence

2026-09-22, ticket786. The read-only lane's normal GPIO24 transport tests the
native awake predicate without fabricating GSTP or location/time data. Both
probes extend E-SAP-0047's exact four status responses and controlled flash
experiment, retain assertion halt diagnostics, and have a60s virtual cap.
At1259fe (after native state12 rearm, before clearing awake), the positive
probe admits at most eight pulses, each delayed100ms and high for1ms. The
negative probe logs the same boundary but admits zero pulses. No firmware
instruction/register/state write or source-lane edit is added.

Run each twice from `../suunto-firmware` with
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt
/tmp/sap235-awake/pulse-N.resc`, N=0 or8. External probe SHA-256:

- pulse-0.resc: `ae40e6e60500f1a3c3f50f79397a8a2d921dd09da8d481e6c3452fba9dedaac6`
- pulse-8.resc: `de5933eb8416624ffbbd1855cb1d52bf354b1bd96d288e7e7568291283e36436`

Raw logs in `/tmp/sap235-awake`, SHA-256:

- pulse-0-1.log: `df02e3ce3da3457e35eb66c934778f02180e01bfeee2984fdad817b4802a7b95`
- pulse-0-2.log: `de09a498904b8a54950e050e034ff447f975ed5635578e033f8a31d4a0f6d91a`
- pulse-8-1.log: `d110d7b39e0ec0ca7c68dfa54be9a1bee1e828ec4fb44cb3b5237dc018716b3b`
- pulse-8-2.log: `c56e10602ad08213594f4a408cbe121eeb282cf9651ea472080345e58cc84c26`

Normalizer `/tmp/sap235-awake/census.py`, SHA-256
`8595aa4b1f29197b6fdf5af860469f1ec4fd4121f0ff7bc03ec12b19362aa013`,
extends E-SAP-0047's timestamp removal/repeated-byte expansion with awake
poll/injection/pulse rows. Paired negative116-row census SHA-256
`1db096be04838d19a8045e2a23e32923713bac3140f6a4752ae50a65e2465a8d`;
paired positive135-row census SHA-256
`2c161983514f7c3477625f29a05acdd3f08501d7d991f2c5dca0bae9b0a479a2`.
Both pairs compare byte-identically.

Derived census: positive has nine successful poll boundaries, eight injected
pulses/eight highs/eight lows, twelve TX bytes (VER+GSR), forty RX bytes
(four status lines), no GSTP/assertion, final PCe1862 at60s. Every poll has
R8=100364ac, R4=100589fe, R5=10036718 (driver+26c), R6=100367c0 (+314),
R7=10036521 (+75), state12/pending10, flags[100589fe..58a00]=1,0,0,
GPIO24 config[40010060]=93. R0/R2 vary because the scheduler call returned;
they are not trigger predicates. Native state10 bootstraps awake=1 and
schedules the first5500ms poll. Subsequent GPIO edges restore the flag after
each native clear. The ninth observed poll has no pulse admitted.

The zero-pulse control transmits25bytes (VER,GSR,GSTP,GSR), receives the
same40bytes, then cycles retry startup and asserts at
CXD5610GF-driver.cpp:894, LR12572d, SP1002f7d0, diagnostic halt79424.
No response to GSTP or the repeated GSR is configured in either probe.
This supports only eight opt-in synthetic pulse admissions at the exact
native boundary. It proves no physical cadence or unbounded receiver model.


E-SAP-0048 integration786: `sapporo-2.35-gps-awake` requires the completed
initial/reopen layers and admits exactly eight pulses through the existing
CXD transport and Apollo GPIO/IRQ route. Exact descriptors, per-machine
contexts/counters, dependency completion/refusals, trigger registers/state and
GPIO configuration are checked before scheduling. Invalid or ninth admissions
latch compatibility refusal; scheduling failures retain their original error.
Reset cancels queued rise/fall events and clears fixture bindings. The lower
transport owns timing and pulse delivery; only the evidenced native trigger
and finite synthetic admission count belong to the compatibility layer.
No instruction, native flag, unknown command or old layer budget is changed.

Before implementation, the new private runner fails with unknown-layer error;
`/tmp/sap235-awake/before.log` SHA-256
`4221f7b92fa25649720db09edf36ba021938d3f1a72c2f78e603a7d18225b1f0`.
The cold run, with all five layers and3B/70s caps, admits eight pulses and
stops before the ninth at1259fe /1579930110 instructions /54642979249ns,
`compat-refused`. Full CLI log SHA-256
`0fc177712c29f58a0b51a306db09538497bc0dd899164968f5019de6f96eb9fa`.
Reversing startup/reopen/awake CLI option order reproduces the same transcript.
The private runner pins this full transcript, refusal tuple and eight hits.

Read-only interpreter observer source `/tmp/sap235-awake/observer.c`, SHA-256
`61b6e51ccf207206dffc8a3e43c07ed8c0ef575a61b091043c6e4797689b6aca`,
extends E-SAP-0047's diagnostic with the awake flag at1259fe. Compile with
`cc -O2 -std=c99 -Iinclude -Isrc/cpu/armv7m /tmp/sap235-awake/observer.c
build/obj/src/frontends/cli.o build/obj/src/frontends/main_headless.o
build/libsemu.a -o /tmp/sap235-awake/observer`; run the exact private manifest,
all five named 2.35 layers, `--max-instructions 3000000000 --max-time 70000000000`.
The fixture validates1/0/0 flags at every admission; native IRQ handling
restores awake=1 after the preceding native clear. No flag is written by the
observer or fixture.

| Admission | Instructions | Virtual ns |
| --- | ---: | ---: |
| 1 | 935908455 | 10809078418 |
| 2 | 1019620206 | 16287096013 |
| 3 | 1098618077 | 21766683180 |
| 4 | 1180362196 | 27245613949 |
| 5 | 1260275306 | 32724937142 |
| 6 | 1339272963 | 38204336102 |
| 7 | 1419186066 | 43683657280 |
| 8 | 1499096453 | 49163930732 |
| 9 refused before instruction | 1579930110 | 54642979249 |

The unmodified SDL default setup walk is a navigation stress case, not proof
of onboarding completion: its post-phase12 LOWER presses adjust HEIGHT
repeatedly. It exits voluntarily after31 steps at generation2644/CRCf963cf9f,
PC080000a2 /8101149152 instructions /28562549498ns. Exploratory
`/tmp/sap235-awake/sdl-1.log` SHA-256
`5019316ee39179853f0e910c397a5ddaafa0890f0a31d9ce59e7337ffb9dc11b`.
Only four awake pulses are needed through this navigation window. Further
setup checks must specify button intent rather than infer completion from
the frontend's generic “completed setup-navigation” message.

Final paired interpreter observer logs `observed-1.log` / `observed-2.log`
match byte-identically, SHA-256
`615a3a60d1d6217f4ca4e2a0beb17629058756832ec6ca671e236780a87c61bb`.
Both observe the eight flag=1 admissions above; the ninth stops before CPU
observer entry. TX remains exactly VER/GSR, with no GSTP fallback.

Paired SDL confirmation walks use:
`SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk
SEMU_SDL_SETUP_WALK_POST=mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm
build/suunto-emu-sdl run --profile sapporo-2.35.34
--firmware tests/private/sapporo-2.35.34.18929/firmware.semu
--layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup
--layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen
--layer sapporo-2.35-gps-awake --until setup-next
--max-instructions 10000000000 --max-time 70000000000`.
The first additionally sets `SEMU_SDL_PPM_DIR=/tmp/sap235-awake/setup-frames`
(existing directory) for visual inspection. Both complete transcripts
`setup-1.log` / `setup-2.log` match, SHA-256
`5ba851f3775d88d5979c4501060bd93c58e54f874a09a9e2d844ed666960303a`.
They stop compat-refused at1259fe /5182618500 instructions /54208499271ns
on the ninth awake admission. Native screens verified from captured pixels:

| Step/screen | Generation | CRC32 | Virtual ns |
| --- | ---: | --- | ---: |
| 10, time format24-hour | 1351 | 4fc8fd25 | 16051416224 |
| 11, weight70kg | 1429 | 4ec75f52 | 16819674459 |
| 12, height170cm | 1504 | ac9055d3 | 17590133343 |
| 13, Connect with Suunto app | 1579 | 65575836 | 18360276118 |
| 14, phone pairing instructions | 1642 | a077755a | 19129590298 |

The later middle press does not complete pairing. No phone, GPS fix or
onboarding completion is inferred from this run. All frame pixels stay in
volatile workspaces outside Git.

Verification (2026-09-22), logs under `/tmp/sap235-awake`:

| Exact command/result | SHA-256 |
| --- | --- |
| `make test TEST_FILTER=sapporo_235_gps`: 12 cases, four new/36 refusal variants, exit0 | `45f4fd879e9d6cee8e01297e7ffb70607bb50a393bb6ce114286a80aa3eb43da` |
| `make test TEST_FILTER=sapporo_cxd5610`: 13 cases, exit0 | `005463aaa4f26637158da30a72b13dda31805e74276d12e95e2d0f550c56dc37` |
| `make check`: 977 cases, exit0 | `c2a5221c37a9755f27e69647ada996f84ca6c173b11a3ac8d944d24f4a6ba737` |
| `make sanitize`: 972 cases, exit0 | `25d3e3bba81a2ffb8dfcb0299da283271144bb53c6e38264c8c8c42d0ce36089` |
| `make check-lines`: advisory, exit0 | `9f0cdaa283042855d201f9c8dd2d5f80b8b945447e4ff899f816ee3820fcb247` |
| `make check-task-contracts`: 151, exit0 | `3af3870f12b9343e05276db271fbe055ea5eb3503bb3a63715bdb62b3c17c30d` |
| `sh tools/test_sdl_sapporo_235.sh`: original two-layer transcript unchanged, exit0 | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |

The new binding tests exercise all six GPS option permutations, invalid
sets before valid binding, low/high reset cancellation and sticky machine
refusal before guest execution. Pulse tests verify100ms rise/1ms high,
eight successes and ninth refusal, instance isolation, immutable CPU state,
log identity, and failure preservation of transport snapshot, scheduler
queue/IDs/sequence/time, signals and hit counts. The scheduling refusal tests
cover busy pulse, deadline overflow, ID exhaustion and sequence exhaustion.


`make test-firmware TEST_PROFILE=sapporo-2.35.34
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`
passes all seven2.35 scripts, exit0; other profiles explicitly skip.
`firmware.log` SHA-256
`e40c65ec10a96789749203be0b00ed8a1bc9a80ff5a7fabcdb5d852aacb38f47`.
The resident/app/resource bytes in the read-only lane root were re-hashed and
match the three pinned E-SAP-0038 component identities. `sh -n` on the new
runner and `git diff --check` pass.

Changed files for786: `src/compat/sapporo_235_gps_awake.c/.h`,
`src/devices/sapporo_devices_internal.h`, `sapporo_devices.c`,
`sapporo_device_compat.c`, `src/boards/machine.c`, `machine_run.c`,
`tests/unit/test_sapporo_235_gps_awake.c`,
`tests/integration/test_firmware_sapporo_235_gps_awake.sh`, README,
current-status, this ledger, ticket786 and its index row. No generic CPU,
scheduler, GPIO/UART MMIO, public API, profile, snapshot or old golden changes.
The2.39 era sweep was not repeated for this explicit2.35-only addition;
its prior30/43 failures remain open. Requested integrator action: review786
separately and keep ongoing GPS/phone behavior outside its eight-pulse scope.
Status remains ready; the overall functional-emulator goal is incomplete.


### E-EMU-SAP235-SCROLL-001 — phone-instructions scroll submission fault

2026-09-22, read-only follow-up diagnosis after integration786. The five-layer
SDL walk passes HEIGHT with MIDDLE at step12 and the Connect screen with
MIDDLE at13. LOWER at step14 (the phone instructions) then provokes a native
HardFault/reset. It is not evidence of a Bluetooth or GPS exchange failure.

Exploratory command: the E-SAP-0048 SDL command with
`SEMU_SDL_SETUP_WALK_POST=mmlmmmmmmmmmmmmmmmmmmmmmmmmmmmmm`, caps10B/70s,
and PPM output to existing `/tmp/sap235-awake/skip-frames` outside Git.
`skip-1.log` SHA-256
`0ec8971ccf06e49c3482ad97fcf18b8afe5ec1753b318b8abd1ab6830c682371`.
The native reset request is at PCcdf5a /4708408494 instructions /
19206046973ns, LRffffffed, SP1005ff58, xPSR29000003. The run restarts setup
and ends budget at PCe1862 /9843851847 instructions /70073361127ns.
The frontend's cumulative budget includes the reset epoch; device logs use
the reset scheduler clock. This restart is not successful onboarding.

A diagnostic CPU wrapper pauses at the first HardFault after4B instructions,
without altering execution before that boundary. Source
`/tmp/sap235-awake/fault_observer.c` SHA-256
`49b7bf7b8d6ad89a361edcbcdc8677a082c1c63aac359f56f25f4a01084b081b`.
Compile with `cc -O2 -std=c99 -Iinclude -Isrc/cpu/armv7m
/tmp/sap235-awake/fault_observer.c build/obj-sdl/src/frontends/cli.o
build/obj-sdl/src/frontends/main_sdl.o build/libsemu.a -L/opt/homebrew/lib
-Wl,-rpath,/opt/homebrew/lib -lSDL3 -o /tmp/sap235-awake/fault-sdl`.
Run the same mml-prefixed SDL sequence, all five layers and exact manifest,
with5B/22s caps. Both complete `fault-1.log` / `fault-2.log` match, SHA-256
`ad57215b87364b7a649aa15c0d38e4dfe4ace927d23f394e4b7edc9ccbc683aa`.

Derived fault: valid native STR r1,[r2,r0] atc1932, instruction5011;
R0=ec, R1=1014389c, R2=40090000. The write to400900ec (CMDRINGSTOP)
escalates a precise BusFault: CFSR8200, HFSR40000000, BFAR400900ec,
handler1be85a, LRffffffed, MSP1005ff58, PSP10034630. Diagnostic halt at
4708408415 instructions /19206046894ns. Stacked PCc1934, LRcb087,
xPSR09000000; the firmware requests reset79 instructions later. Existing
E-EMU-NEMA-GPU-001 proves that a rejected GPU transaction takes this CPU/bus
fault path. No instruction workaround or transaction success fallback is
permitted; identify and evidence the specific renderer refusal next.

The paired bus-boundary diagnostic identifies the original refusal precisely:
`address=400900ec width=4 value=1014389c status=6 detail=backend: unsupported source format 0x06`.
It is captured before the CPU turns the failed write into an architectural
fault. External `/tmp/sap235-awake/bus_observer.c` SHA-256
`b888d32ca574b59cbd885912af2d767e7fe1f6250884ce9d8baaf62afdcf45e6`
is an unchanged bus implementation plus a diagnostic wrapper around writes to
that address. Compile alongside `fault_observer.c` with additional
`-Isrc/core`, the same SDL objects/library/link flags, output
`/tmp/sap235-awake/gpu-fault-sdl`; run the same5B/22s diagnostic twice.
`gpu-fault-1.log` / `gpu-fault-2.log` compare byte-identical, SHA-256
`00d4e297bf424d6c985631b685b931ce87a18bd52734d2988dd2deb4c3f08408`.
Removing only the new GPU_REFUSAL row yields the earlier complete fault
transcript byte-for-byte. No diagnostic changes production files or pixels.

Next reference located, not yet adopted: read-only lane
`emulator/renode/graphics/SapporoNemaP.cs`, SHA-256
`a304a577ad5bcd551ec4c764727f82b661892d03585f648394020d4b4ddfa165`,
contains `IsProvenRgba4444Texture`, `ExecuteRgba4444Texture`,
`BilinearRgba4444` and `Rgba4444`. Its exact pairing-strip predicate includes
Texture1FormatStride060101e0, resolution006000f0, RGB565 target040001e0 /
00f000f0, shader941e8000, finite affine coordinates and bounded SRAM. This
is a concrete lead for a separate observed rendering integration; native draw
state and lane execution must still be captured and reproduced before adding
format06 support. Ticket786 does not change rendering or bypass this refusal.

## E-NEMA-RGBA4444-001 — Replayed pairing strip and bilinear pixels

**Status:** verified lane evidence, 2026-09-22. Ticket 787 integration input.
The read-only `../suunto-firmware/emulator/renode/graphics/SapporoNemaP.cs`
SHA-256 `a304a577ad5bcd551ec4c764727f82b661892d03585f648394020d4b4ddfa165`
is the sole machine oracle. Relevant methods: IsProvenRgba4444Texture,
ExecuteRgba4444Texture, BilinearRgba4444, Interpolate, TryRectangle.

Two native 2.35 captures repeat E-EMU-SAP235-SCROLL-001's five-layer
`setup-walk`, POST `mmlmmmmmmmmmmmmmmmmmmmmmmmmmmmmm`, with
`--until setup-next --max-instructions 5000000000 --max-time 22000000000`.
The read-only draw observer snapshots format06 state/source/current surface
before the unsupported draw; the earlier CPU fault observer stops after the
first HardFault beyond4B instructions. Both native logs and both inputs are
byte-identical. The native snapshot has source1009774c, target1010ed80; source
format06/sampling1/stride480/240x96; target04/sampling0/stride480/240x240.
Clip=(0,162)-(240,240), quad=(0,237)-(240,240), draw5, matmult0,
codeptr941e8000, IMEM=(0,004e0002,804b1286), white texture tint,
matrix=(1,0,0;0,1,-237). Draw colorff000000 is unused by this path.

Replay commands run from the read-only sibling:
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt
/tmp/sap235-rgba/{native,synthetic,refusal}-{1,2}.resc`. Each script includes
the 2.35 lane, holds CPU halted (zero instructions), loads bounded source,
destination and60-word child list, submits through the real command ring,
and runs exactly 0.001 virtual seconds before dumping 115200 destination bytes.
The final scripts below are authoritative; the earlier generator's SaveBinary
line was replaced by SystemBus.ReadBytes/File.WriteAllBytes in those scripts.
No firmware, captured pixels, probe binaries or raw logs enter Git.

Native replay changes exactly 720 pixels (240x3 visible strip). Synthetic
replay uses source10040000/target10080000, clip/quad(0,0)-(240,100), matrix
(1,0,-0.25;0,1,-0.5). Source texel=((x&15)<<12)|((y&15)<<8)|
(((x+y)&15)<<4)|((3*x+7*y)&15); old RGB565=((x%32)<<11)|
((y%64)<<5)|((x+y)%32). It changes 23038 pixels, including transparent
texture borders. Wrong-shader control changes codeptr to941e8001 and changes
zero pixels, refuses at child byte offset232/draw5, completion refused,
clid1/irq0. Accepted runs each produce 13 normalized NEMA rows; refusal 12.
Raw logs include host timestamps; normalization extracts `NEMA_[^\r\n]+`
rows in order. Every normalized census and full output repeats byte-identically.

Decode little-endian texels as R/G/B/A nibbles times 17. Affine mapping and
left-associated bilinear products/sums use binary32, with transparent samples
outside 240x96; channels round via trunc(value+0.5). RGB565 destination
expansion uses c5*255/31 and c6*255/63 (floor); alpha-over is
(src*a+dst*(255-a)+127)/255; encoding uses (c*31+127)/255 or
(c*63+127)/255. Exact shader/descriptor, bounded SRAM, ordered clip,
rectangular signed16.16 geometry bounded [-1024,1024] and finite matrix are
required. Mapping overflow, inaccessible memory and source/destination overlap
remain explicit emulator refusals; these are safety boundaries, not new lane
behavior claims. Physical panel and arbitrary shaders are unproven.

All following paths are relative to volatile `/tmp/sap235-rgba/`; hashes are
SHA-256. Input sizes: source 46080, before/output 115200, child lists 240 bytes.

| Artifact | SHA-256 |
|---|---|
| `draw_observer.c` | `f0339ac53262158a0c0f2b6f65c2692deab39bd25e161347b39ee45b9555e282` |
| `make-probes.py` | `44b4045b2b35826c191d757bcf4f00b48457b9d4bc4a20318424ab750bd8b99b` |
| `normalize.py` | `3c3c383b91b61f91f4f08a92ba395fe3a71f0ec5800e1cd386f7c731da2edd92` |
| `native-1.log` | `6bf35e6d3c38c817760e69e1805bad00c9bfb2bb3719237ebd9fd27183d8b019` |
| `native-2.log` | `6bf35e6d3c38c817760e69e1805bad00c9bfb2bb3719237ebd9fd27183d8b019` |
| `native-1/source.bin` | `eca77b04b10f01f01a62d30d630ab31c939c5afeb22b05692a4d5ca3aa68f5b8` |
| `native-1/before.bin` | `58059fa607496284614a5e7d881b6ba6ecad9c31b24afa57bb347546be17d227` |
| `synthetic-source.bin` | `7d2bc1e2af26fcc193e4eaaac3d9586265676a4ba8b51140c6fe8b398ae0fced` |
| `synthetic-before.bin` | `5a2249552fc4b2ac5fce392ba82129084a1a769c5b30c0145cfebd5caec71a66` |
| `native-list.bin` | `9ff4c8eed3f253bc7e96130489f5cb470b83e1fb23e1b910781069e13dffbba8` |
| `native-1.resc` | `330773aa0a4895ac072808a007079dfffe7926a588512cdf0517644fa3a7b616` |
| `lane-native-1.log` | `26441fcd96cc1abd70da0128bb8760cf9e15b13fb88b31b5b5089ba31025952d` |
| `lane-native-1.census` | `da99ca333257c2bbb7558b21e7bb730c24691fd1a00764535d966af31fa0d98c` |
| `native-1.rgb565` | `ae6001a5c2b6e0910bff0aba61b45f8d92d1e25d166d55972cf367bc895ee05c` |
| `native-2.resc` | `557dd3ee97ef68e6f8c75f16d7309eb793bac852e709f71241520b80233d95d1` |
| `lane-native-2.log` | `9d448d069b60bf18eadc1996395007cb7698af9e539c24790882e864fe51ba57` |
| `lane-native-2.census` | `da99ca333257c2bbb7558b21e7bb730c24691fd1a00764535d966af31fa0d98c` |
| `native-2.rgb565` | `ae6001a5c2b6e0910bff0aba61b45f8d92d1e25d166d55972cf367bc895ee05c` |
| `synthetic-list.bin` | `b8a6259ef268a35da3e5e93f741a94f4b59a16179086516d03801a9eb2d67df9` |
| `synthetic-1.resc` | `6a3e38b363eaeaa12206c004f0769248a9673c8c98787271c78a66ab7af5c56d` |
| `lane-synthetic-1.log` | `9eff6690fa67de65680dd659bdb57587dfa921c72ac0f8ac03442edca8e41c20` |
| `lane-synthetic-1.census` | `037303231c9e9c4113b7ad00307e9ec1a50c4969794179af565ce37510a00c20` |
| `synthetic-1.rgb565` | `f2a0a0e64d568e95741790f4ea55fd07ebca33743e32fa697f2e195f432318b3` |
| `synthetic-2.resc` | `134092f2e6da4033e3de78485e48afa2668404c4e7a1c2cb4a6d22adf749c76d` |
| `lane-synthetic-2.log` | `17ea6e04dbb0975b01f99f56c1a759ead795305a952f83147d02d01e04249181` |
| `lane-synthetic-2.census` | `037303231c9e9c4113b7ad00307e9ec1a50c4969794179af565ce37510a00c20` |
| `synthetic-2.rgb565` | `f2a0a0e64d568e95741790f4ea55fd07ebca33743e32fa697f2e195f432318b3` |
| `refusal-list.bin` | `0f22e005517916a8fd22e56757955ef7e1c69f12390392dd79a35a8c247ea48e` |
| `refusal-1.resc` | `f5ecafe21a534fb4e0d0b1c58efdf403bcf7ad1b7c0197310be63ef21846f810` |
| `lane-refusal-1.log` | `eb5a5c35c30269488c1a4702b708a6aadbf0ea7aca5eff0c946aa000e50a8ae2` |
| `lane-refusal-1.census` | `8bb91445512009968d9f4dbd1ff87334a936a4947dc11275cf2e669436efb812` |
| `refusal-1.rgb565` | `5a2249552fc4b2ac5fce392ba82129084a1a769c5b30c0145cfebd5caec71a66` |
| `refusal-2.resc` | `f4268defffe7bba4197b1501dceb85302d9cad594c9cce120f3134a3a008cc0b` |
| `lane-refusal-2.log` | `4a7609f5f3ca885c5c9c5e816e99833ca5310db22457d8eb5ca436b16af2b9b9` |
| `lane-refusal-2.census` | `8bb91445512009968d9f4dbd1ff87334a936a4947dc11275cf2e669436efb812` |
| `refusal-2.rgb565` | `5a2249552fc4b2ac5fce392ba82129084a1a769c5b30c0145cfebd5caec71a66` |

Additional binary32 cross-check: two cold `complex-{1,2}.resc` lane replays
use the same synthetic pixels/rectangle, with matrix bit patterns
(3f7cd6ea,3c4a4533,bed70a3d;bda3d70a,3f95c28f,40466666). This exercises
non-dyadic fractional interpolation and skew/scaling. Both full outputs and
the independent interpreter replay match byte-for-byte, with 19765 changed
pixels. Thirteen normalized NEMA rows repeat; the census is the same as the
other accepted synthetic run because it logs descriptor/commit boundaries,
not the six coefficients. Final list hash pins those coefficients separately.
Launch command and zero-instruction/1ms bounds are the same as above, using
`complex-{1,2}.resc`; normalization is the same extracted NEMA-row rule.

| Additional artifact under `/tmp/sap235-rgba/` | SHA-256 |
|---|---|
| `complex-probe.py` | `fb3c14cb4ff686a7a0c51ac3fbd434f086ba22f78e6e3d540160a6cdd9375d7b` |
| `complex-list.bin` | `c92b11640ff44acf2a7543a2a2c415fb95a7a05c1ea1d0c10fe8b60021661d4d` |
| `complex-replay.c` | `fc8782242d499a1805d4f67af856512e221d532ab68ea89bb031f2ac030a5c73` |
| `complex-emulator.rgb565` | `52f32d7eaf3e1ec7bdfa6c74ebe760e58dcb0fcebab6f122af3219f26bd5a880` |
| `complex-1.resc` | `cd52a182bf08df17e64acd3a3832bb2dc553a8e416f1462fa7edff1fa2ac16b3` |
| `complex-2.resc` | `eb0c8774492841f93423150a03831a438b35de140ad7e9bad6da08b1cbf7d0cd` |
| `lane-complex-1.log` | `d1c91559818a004f7854b9db1e47ba2360639b5ab91164e41ce24aefce62552a` |
| `lane-complex-2.log` | `7807a4f30286391f79f632281edfddd8e920d6aa380140de8402fffc05ac5c96` |
| `lane-complex-1.census` | `037303231c9e9c4113b7ad00307e9ec1a50c4969794179af565ce37510a00c20` |
| `lane-complex-2.census` | `037303231c9e9c4113b7ad00307e9ec1a50c4969794179af565ce37510a00c20` |
| `complex-1.rgb565` | `52f32d7eaf3e1ec7bdfa6c74ebe760e58dcb0fcebab6f122af3219f26bd5a880` |
| `complex-2.rgb565` | `52f32d7eaf3e1ec7bdfa6c74ebe760e58dcb0fcebab6f122af3219f26bd5a880` |

Implementation verification, ticket 787: the first `make test
TEST_FILTER=nema_rgba4444` fails with one selected test on the pre-change
unsupported-format refusal. Follow-up regressions independently fail for an
absent zero-valued program register and for an observed black clear after an
inherited RGBA source; both are corrected within the integration scope.
The final seven cases pass (including 33 malformed-state variants, 11 missing
program variants, full lane pixel pins, overlays/holes with zero MMIO reads,
allocation/mapping atomicity, later-child refusal/retry and inherited clear).
The complex-matrix extension also passes under a focused sanitizer run.

Exact successful commands:

- `make test TEST_FILTER=nema_rgba4444` — 7 cases.
- `make test TEST_FILTER=nema` — 119 cases; `make test TEST_FILTER=fpu` — 23.
- `make check` — 984 passes; `make sanitize` — 979 passes.
- `make sanitize TEST_FILTER=nema_rgba4444` — 7 cases, including both synthetic matrices.
- `make check-task-contracts` — 152 indexed tickets; `make check-lines` — advisory warnings only.
- `make all`; `make sdl` —successful builds, no new runtime dependency.
- `make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235` — all seven private scripts pass with component validation.
- `sh tools/test_sdl_sapporo_235.sh` —existing two-layer language-menu pins unchanged.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh` — existing 2.22 SDL pins unchanged.

`/tmp/sap235-rgba/replay.c`, compiled with
`cc -std=c99 -Wall -Wextra -Werror -pedantic -Iinclude -Isrc/display
/tmp/sap235-rgba/replay.c build/libsemu.a -o /tmp/sap235-rgba/replay`,
reads only the captured private inputs and invokes the production renderer.
`cmp native-emulator.rgb565 native-1.rgb565` passes. The skewed synthetic
replay uses `complex-replay.c` and the same library and also matches fully.
Neither replay enters Git. CPU arithmetic implementations/FPSCR, scheduler,
GPU framing/MMIO, snapshots, old pins and compatibility ceilings are unchanged.

Two SDL cold runs using the earlier capture command without observers
(`build/suunto-emu-sdl`,5B instructions/22s) match completely, with source
format06 now accepted: step 15 generation 1756/CRC f0ff828c at 19706966235ns;
endpoint `budget /000e1862 /4896065682 /22000000000ns` (CLI exit 3 for budget).
No machine-reset-request, refused device transaction or compatibility refusal.
The screenshot shows scrolled pairing directions and the watch name, not
onboarding completion. The optional scroll runner pins this full transcript.

Further paired input observations use the same five layers and `setup-walk`,
POST `mmlllmmmmmmmmmmmmmmmmmmmmmmmmmm`,10B instructions/40s, with the first
run's PPM dump directory `/tmp/sap235-rgba/later-frames`. After three LOWER
scrolls, step 17 generation 1948/CRC 895a642c displays `Later`; MIDDLE selects it.
Step 18 generation 1957/CRC e0c2f63d shows the native recommendation to connect a
phone. MIDDLE there produces no further settled frame; both runs finish
`budget /000e1862 /5570776834 /40000000000ns`, six bounded awake admissions,
no reset/refusal. This is an observed UI waiting point, not an emulator failure
or completed onboarding. Raw transcripts repeat byte-identically. Earlier
single-run LOWER-only exploration is diagnostic only and backs no hardware
implementation claim.

Other-profile limitation: the full 2.39 era sweep and full 2.22 onboarding gate
were not repeated here. Their pins may have drifted with the shared rendering
extension. The pre-existing 2.39 census remains 30 failures among 43, tracked by
ticket 783; no era golden is weakened or re-pinned. Phone pairing, full setup,
GPS acquisition/ongoing service, 2.35 snapshots and physical panel are unproven.
Ticket 787 remains ready; integrator review owns its status.

| Implementation artifact under `/tmp/sap235-rgba/` | SHA-256 |
|---|---|
| `before-test.log` | `d24c187ac3ab29373b2c8e7c8a9c175beb4030c4ebea34f5e87eb19a2103782f` |
| `missing-before.log` | `d3b7d10bc8c55e98905dd74c178326ca5790a9e42995b0c4811b3128ae5a24eb` |
| `clear-before.log` | `1be0d1dfcd0baec3d47b00b0d2a1584ad1638ede3c51fad753d1805f21bad2d3` |
| `after-test.log` | `17d32ebb7b8f15fdf6c52ef72fb476fab86f654c25bd476c70ca1810dbbf6810` |
| `nema-tests.log` | `891f119b83d69a419806e22b5dd68895040852c4f8db1a279a65ce4819138de3` |
| `fpu-tests.log` | `06ced1117ff54911a683835f99fbbf2dca569f3c2f2318bded8491261ae3c774` |
| `check.log` | `e51f8b653abe174dd46af4077ec65484d4012f0ffa00d495fffaf86084dab7e8` |
| `sanitize.log` | `4c0f6f2d377e458347f9fff525faafdce4c2127ec6fdb82ab970728cef070b65` |
| `rgba-sanitize.log` | `9bb201cf84c0a70cd7d605f4f99688f306f4e9c44e94282bcf07ff77e9df62da` |
| `contracts.log` | `ef345e23aeb742332c967213d4b2242cceaa759f3e3983ee22555b848a49b80c` |
| `lines.log` | `67c5053bf19f4c2dc03ba63d1e271fed709637ef18a4657f29006a42a93ef3fc` |
| `firmware.log` | `4af13c88189146fd7434fac06a93054a194c74dd84e1e8b55fd699c65362cbeb` |
| `sdl-prefix.log` | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |
| `sdl-222.log` | `228575cb7c370d51649adcd05eb6ae3bc055c22bcd177c7ba3c9381300d5d263` |
| `replay.c` | `27faf3a3d6cd2e2f30fccd0ec7fb51af3459d1af18fa2dc241bca34f27386687` |
| `native-emulator.rgb565` | `ae6001a5c2b6e0910bff0aba61b45f8d92d1e25d166d55972cf367bc895ee05c` |
| `scroll-1.log` | `c79d7bd489bba9f280b8f64e233f2ab796688258267e045709e51a0242a7ad6c` |
| `scroll-2.log` | `c79d7bd489bba9f280b8f64e233f2ab796688258267e045709e51a0242a7ad6c` |
| `later-1.log` | `057f938336109e8b266e60d1f9ea310ba7894217b8f044f819c264dce916cbd0` |
| `later-2.log` | `057f938336109e8b266e60d1f9ea310ba7894217b8f044f819c264dce916cbd0` |

Final optional gate: `sh tools/test_sdl_sapporo_235_scroll.sh` exits 0 after
two fresh runs of the final SDL build, matching the full c79d7bd4…2a7ad6c
transcript pin and 22s endpoint above. Runner output `/tmp/sap235-rgba/
sdl-scroll-gate.log` SHA-256 `b3da4b332833d04ee702b7c045ef251c8b46800a7744301666c02f901bf18fa2`.

## E-EMU-SAP235-MAIN-TSC6A-001 — Done reached; compressed icon refuses

**Status:** verified refusal boundary, 2026-09-22. No positive compressed
texture pixels are established. Runtime and existing goldens are unchanged.

The five-layer 2.35 interpreter reaches Time/date, the time-zone selector and
Done. A usable LocalTime value is obtained in this run, so manual clock entry
is unnecessary; its source was not traced and no phone/GPS time is inferred.
The firmware faults while opening main about three seconds after Done.

Common native command, with CLI validation of all three firmware components:

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
SEMU_SDL_SETUP_WALK_POST=mmlllmlllmmmmmmmmmmmmmmmmmmmmmm \
build/suunto-emu-sdl run --profile sapporo-2.35.34 \
  --firmware tests/private/sapporo-2.35.34.18929/firmware.semu \
  --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
  --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
  --layer sapporo-2.35-gps-awake --until setup-next \
  --max-instructions 10000000000 --max-time 40000000000
```

`three.log` captures this uninstrumented run, with optional PPM output under
`/tmp/sap235-setup/three-frames`. Settled checkpoints:

| Step | Generation | CRC32 | Virtual ns | Screen |
|---|---:|---|---:|---|
| 21 | 2299 | `ba55af9e` | 23089359324 | Connect later |
| 22 | 2302 | `d13391e9` | 23584737521 | Time/date |
| 23 | 3906 | `13021279` | 29084851413 | Time zone, UTC+00:00 selected |
| 24 | 3981 | `1c1f9064` | 29855991832 | Done |

The reset request is `cdf5a / 7486616944 / 32533939640 ns`. After reset,
the OHR fixture refuses at `1be85a / 8018066412 / 36105130734 ns` (exit3).
That later refusal is not the cause of the main-screen failure.

Control `two.log` changes POST to `mmlllmllmmmmmmmmmmmmmmmmmmmmmmm`.
Two LOWER presses select Connect and return to pairing instructions; it
ends `budget / e1862 / 6222680298 / 40000000000 ns`. The exploratory
manual-clock variant changes POST to `mmlllmlllmmlmmmmm`, adds timeline
`32000:l`, and uses 18B instructions/60s. It also reaches Done, then resets
at `cdf5a / 7703011117 / 33109623252 ns`, ending at a later OHR refusal
`1be85a / 8234582705 / 36680814343 ns`. These exploratory controls are
single runs and authorize no new hardware behavior.

Paired first-fault runs (`fault-{1,2}.log`) use the common command with
`/tmp/sap235-setup/diagnostic-sdl`. The external CPU observer stops at the
first HardFault after 4B instructions; the bus observer logs refused GPU writes.
Both are unchanged sources from E-EMU-SAP235-SCROLL-001:

- `/tmp/sap235-awake/fault_observer.c`, SHA-256
  `49b7bf7b8d6ad89a361edcbcdc8677a082c1c63aac359f56f25f4a01084b081b`.
- `/tmp/sap235-awake/bus_observer.c`, SHA-256
  `b888d32ca574b59cbd885912af2d767e7fe1f6250884ce9d8baaf62afdcf45e6`.

Build: `cc -O2 -std=c99 -Iinclude -Isrc/core -Isrc/cpu/armv7m
/tmp/sap235-awake/fault_observer.c /tmp/sap235-awake/bus_observer.c
build/obj-sdl/src/frontends/cli.o build/obj-sdl/src/frontends/main_sdl.o
build/libsemu.a -L/opt/homebrew/lib -Wl,-rpath,/opt/homebrew/lib -lSDL3
-o /tmp/sap235-setup/diagnostic-sdl`.

The complete logs are byte-identical. A valid STR at `c1932` (opcode 5011)
writes `1014379c` to `400900ec`, receiving `SEMU_ERR_UNSUPPORTED`:
`nema_tsc6a: unsupported mask resolve state`. CFSR8200, HFSR40000000,
BFAR400900ec; handler1be85a, MSP1005ff58, PSP10034540, LRffffffed;
stacked PCc1934/LRcb087/xPSR09000000. Diagnostic endpoint:
`halt / 1be85a / 7486616865 / 32533939561 ns`, exactly 79 instructions
before the unmodified firmware reset. No instruction repair is implicated.

Paired draw-capture runs add `draw-observer.c` and `-Isrc/display` to that
build, producing `/tmp/sap235-setup/draw-diagnostic-sdl`. They use the same
command with `SEMU_CAPTURE_DIR=/tmp/sap235-setup/capture-{1,2}`. The observer
saves working pixels before the draw and records the first refusal's state and
memory-only source bytes. Removing DRAW_REFUSAL/DRAW_SNAPSHOT rows reproduces
the first-fault logs exactly. Both full logs and both inputs repeat identically.

Refused snapshot:

- target10121d40, format04/sampling0/stride480/dimensions240x240;
- source100a490c, format 17/sampling1/stride 180/dimensions60x60;
- clip(0,81)-(240,162), quad(171,90)-(231,150), draw5;
- drawcolorff555555, tintffffffff, matmult0, code941e8000;
- IMEM=(0,004e0002,804b1286), matrix present;
- matrix bits=(3f800000,0,c32b0001;0,3f800000,c2b40000).

The source capture is the bounded stride-times-height span of 10800 bytes,
not a claim that the compressed asset occupies that many bytes. It is not
the existing semantic 480x480 transition surface from E-NEMA-TSC6A-001.

`make-lane-probe.py` emits the exact 60-word child list and two lane scripts.
Each includes the original 2.35 lane, halts the CPU (zero instructions), loads
the captured inputs, submits the real ring and runs for exactly 1 ms. Command
from the read-only sibling root:
`.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt
/tmp/sap235-setup/lane-{1,2}.resc`.

Oracle `emulator/renode/graphics/SapporoNemaP.cs` SHA-256:
`a304a577ad5bcd551ec4c764727f82b661892d03585f648394020d4b4ddfa165`.
Both lane runs refuse draw 5 at child byte offset 232 as unknown-state, refuse
completion (clid1, IRQ0), and preserve all 115200 destination bytes. Twelve
NEMA rows repeat identically after removing host timestamps. There is no
positive decoded-pixel oracle in these results.

The existing lane note `../suunto-firmware/docs/research/native-tsc6a-transition-surface.md`
(SHA-256 `d9ae4eaec1cb5c53c5322f28911dc6f4d04a7df8c8e610c5d80eb6e0aa9301dd`)
explicitly describes a semantic shadow, not a compressed codec.
Ambiq's [Apollo4 Plus datasheet](https://ambiq.com/wp-content/uploads/2022/03/Apollo4-Plus-SoC-Datasheet.pdf),
section 21.3.4.18, identifies format 17 as TSC6A with sixteen pixels and alpha
in 96 bits; that identification supplies no successful lane decode or complete
algorithm. The sibling Ulsan `ulsan_tsc4.py` labels its nibble interpretation
diagnostic and unproven; it is not a TSC6A reference.

`read-views.py` extracts views from resource SHA-256
`f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22`.
w-ltim tests LocalTime>=1677628800 after 5 s; w-done marks the wizard executed
and requests main after 3 s. The scripts guided controls; the observed frames
above are the runtime evidence. Firmware-derived XML and pixels stay outside
Git. The missing evidence is a permitted positive compressed-texture reference,
including block layout, alpha, stride, addressing, filtering and output pixels.
Ticket 788 tracks that blocker; no shadow enlargement or transparent substitute
is authorized. The overall emulator goal remains incomplete.

| Artifact relative to `/tmp/sap235-setup/` | SHA-256 |
|---|---|
| `two.log` | `539850de28f13d1a56a913757f2539e9255f4d9c33760f48b5d1c17e25ef1009` |
| `three.log` | `89d84f28e42e8916ae315ba1fd15e30b94059c35e33449394318c1a52dda82af` |
| `manual-1.log` | `35ec40bcb5fe3143c3561ce5fcb0ecd44fd100d91b3109c5c34c57e24aff38e9` |
| `fault-1.log` | `f63e252b852eaa3f0a17e45c80f4719f103a5705ceb82393fcb2f4d55d2191be` |
| `fault-2.log` | `f63e252b852eaa3f0a17e45c80f4719f103a5705ceb82393fcb2f4d55d2191be` |
| `make-draw-observer.py` | `7316637efa66656e4f46d851fdff5b564f71b39e6439d4539228d586683f176a` |
| `draw-observer.c` | `189f1f7d62a4a5606e2d3fce7f43da8c567f71b69b3b1f19e9b101c4754d6980` |
| `draw-fault-1.log` | `a31fe7cae62d225581c46841043f40ce91fc701064de16ed4e01513f2c365c99` |
| `draw-fault-2.log` | `a31fe7cae62d225581c46841043f40ce91fc701064de16ed4e01513f2c365c99` |
| `capture-1/before.bin` | `25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340` |
| `capture-2/before.bin` | `25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340` |
| `capture-1/source.bin` | `b403fb2454fe9ed8206ad3e5cc79af7789f272450527b7fe76a480ed8c44cdee` |
| `capture-2/source.bin` | `b403fb2454fe9ed8206ad3e5cc79af7789f272450527b7fe76a480ed8c44cdee` |
| `make-lane-probe.py` | `b8dd0343a207767d5f186d94b15e67bde7756ebed9117c4caad7aa39b2537f1a` |
| `native-list.bin` | `29b19753e57ccf54d7b48b63c3a99643416bd5f5a0d60532cc02b9010721df1f` |
| `normalize.py` | `1afd323e40509eaacf1357bd5167a3a844c32435e82e69ef451a0dd01b9b56be` |
| `lane-1.resc` | `89480c432a655e0fed9be8fdb2828252367c520fdaf9d73188db08012f47786b` |
| `lane-2.resc` | `fc85262f7fb2eb0f2706856cab948c091aa9837b20c92f02d53cf32fc257cd1f` |
| `lane-1.log` | `e06d687882bb7da9de59f2e38e0b77df5bb9d18dfcee824282ea3dc7ef4ea319` |
| `lane-2.log` | `8ca01cd0bada7871f82ad7eddee01deff12c81c97b46e5bd9fbde0e0f7177ed7` |
| `lane-1.census` | `407f4d8c1ee7d5b507ce1248911b921b669e0b3d57f0a7643c8496a442fcaf73` |
| `lane-2.census` | `407f4d8c1ee7d5b507ce1248911b921b669e0b3d57f0a7643c8496a442fcaf73` |
| `lane-1.rgb565` | `25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340` |
| `lane-2.rgb565` | `25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340` |
| `read-views.py` | `dbc6da152559142b163f81178dacbf06db299da0e6e071186fead228d2728abc` |
| `w-ltim.xml` | `e04cbc3d48b2656b619bf651c3d51fb06980a46b1f733c45ab3578f1f9b97754` |
| `w-done.xml` | `6665a9dd2484305073a75d3dec56d24164fe8f6e398ac1e6ed8bc95d92bd4048` |

Evidence/planning maintenance verification: `make check-task-contracts` exits0
with153 indexed tickets; `make check` exits0 with984 PASS records. Its optional
firmware-gated SDL walks are skipped by the quick target; the authentic
observations above were executed separately. `git diff --check` passes. No C
behavior changed, so sanitizers/other-profile era sweeps were not repeated.
Changed files: README, current-status, this evidence ledger, plans/index.tsv
and new ticket788. The overall goal remains active; this is a specific decoder
evidence blocker, not a claim that all remaining work is blocked.

`/tmp/sap235-setup/contracts.log` SHA-256 `1588fe36c7338a390bf91f05d183a779c0b4e100d1896bde3f1ee053fac73445`.
`/tmp/sap235-setup/check.log` SHA-256 `964f3ebd0b1039e06740302d9c61f623fd1c69b651dc3aa9847a8f15e9e44cce`.


### E-EMU-TSC6A-DIAGNOSTIC-001 — compressed asset refusal clarity

Maintenance, 2026-09-22. E-EMU-SAP235-MAIN-TSC6A-001 supplies the observed
60x60, stride-180 descriptor and paired lane refusal. The existing error
`unsupported mask resolve state` concealed the distinction between compressed
asset storage and the supported 480x480 semantic shadow. No decoding evidence
has been added. This change only specializes the error text after the existing
mask-resolve acceptance predicate has already rejected the operation.

Changed files: `src/display/nema_tsc6a_raster.c`,
`tests/unit/test_nema_tsc6a.c`, current-status and this ledger. No public
interface, accepted state, return code, persistent format, firmware component,
profile, compatibility limit or golden changes. Ticket 788 remains blocked.

The new unit regression uses only the recorded descriptor and synthetic
0xa5 destination bytes. Before the fix, `make test TEST_FILTER=nema_tsc6a`
selects three cases and fails exactly the new diagnostic assertion. Afterward
all three pass, including the supported triangle/resolve control. The new
case checks the complete unchanged destination, null error sink, successful
shadow-mask control and preservation of the generic invalid-shader diagnostic.

`/tmp/sap235-codec/replay.c` validates the three private capture hashes from
E-EMU-SAP235-MAIN-TSC6A-001 before loading them. It submits exactly one
60-word list, without CPU execution or scheduled virtual-time advancement.
It seeds the diagnostic panel with the captured destination; its timestamp
argument 32533939561 ns is metadata, not elapsed execution. Both current
runs return REFUSE (2), UNSUPPORTED (6), zero frames, generation zero and
unchanged=1. Full destination SHA-256 remains
`25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340`.
A baseline replay compiled with the original raster source extracted from
HEAD 5980046 returns the same census and hash. Only the diagnostic line differs.
No pixels, capture bytes or probe sources enter Git.

Exact commands (logs under `/tmp/sap235-codec/`):

```sh
make test TEST_FILTER=nema_tsc6a
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/display /tmp/sap235-codec/replay.c build/libsemu.a -o /tmp/sap235-codec/replay
/tmp/sap235-codec/replay > /tmp/sap235-codec/replay-1.log
/tmp/sap235-codec/replay > /tmp/sap235-codec/replay-2.log
cmp /tmp/sap235-codec/replay-1.log /tmp/sap235-codec/replay-2.log
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/display /tmp/sap235-codec/replay.c /tmp/sap235-codec/before-raster.c build/libsemu.a -o /tmp/sap235-codec/replay-before
/tmp/sap235-codec/replay-before > /tmp/sap235-codec/replay-before.log
make check
make sanitize
make test TEST_FILTER=nema
make sdl
git diff --check
```

Final verification: 985 normal cases, 980 sanitizer cases and 120 focused NEMA
cases pass, with zero failures. `make check` includes 153 task contracts and
the advisory line check. SDL builds. Its quick check skips the optional private
walks; they and the full era sweeps were not rerun for this diagnostic-only
change. Existing 2.39 era drift and all unresolved firmware boundaries remain.
The prior authentic cold-run stop tuple was not remeasured here; the captured
command comparison establishes the unchanged refusal/output boundary only.

Bounded vendor-reference search: Ambiq's
[graphics guide](https://ambiq.com/wp-content/uploads/2022/04/Apollo4-Graphics-Getting-Started-Guide.pdf)
documents NEMA PIX-Presso as an image converter. The official
[SDK 4.5.0 download](https://contentportal.ambiq.com/documents/20123/387817/AmbiqSuite-R4-5-0.zip)
redirected to a contact/login and license form; no form was submitted. The
public Ambiq HAL tree at `1577f6e52eebdf5a3a2e2082d5161c1c81858948` exposes
Nema headers, port code and hardware libraries; no converter/decoder was
identified in its tree. Other inspected trees are pinned below. A filename
census is not proof that no decoder exists elsewhere. A vendor decoder would
also require an explicit exception to AGENTS.md's sole-lane-oracle constraint;
the user has been asked, and no exception is presumed. No downloaded GPU
library was executed or treated as pixel evidence.

| Artifact relative to `/tmp/sap235-codec/` | SHA-256 |
|---|---|
| `before-test.log` | `b1f5bd844a639d332e6960e70c701f400b67f09371cc4214c660450649fd086b` |
| `after-test.log` | `04cb1e901956e877a85ffb85447d77ce1363bd37e954da16b7a580de1d2bc32a` |
| `before-raster.c` | `78bc1d3fc130915bc6bcff2b824cafef60c8245ef097da8043a57556f26e0bcd` |
| `replay.c` | `8e2d188dbffb9c0fe47d4a1df1e6095b31df30b4c316a2ee6cd0f09e09613ecf` |
| `replay-before.log` | `afa6135b7843619fc3e37edff3772e9b08614c1adb9567ff5cb8367407a22a62` |
| `replay-1.log` | `2aafe3df2387a8d302f182eecf1ecded30fb2496a5cdb05a17c145594b4c9df8` |
| `replay-2.log` | `2aafe3df2387a8d302f182eecf1ecded30fb2496a5cdb05a17c145594b4c9df8` |
| `check.log` | `4ff6defdab0b42d1c05e3e883d16ed1a2f8a644c6caa38077514feedd90d73e3` |
| `sanitize.log` | `74d59166cae19c02bc54cdb0cb5193a773f41f05fd3fc7c68d1764210b40f90b` |
| `nema.log` | `be267dd17517950053075ac9efe7879e67d5815d366f6f38a2bbfe649b91cc3d` |
| `sdl.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `AmbiqMicro-ambiqhal_ambiq.json` | `2627ede7d832ccbfc80f1c1823f3c40f04a6b4939f917501e5adfdd54101a459` |
| `AmbiqMicro-lv_port_ambiq.json` | `29aed4a1fadf176226513525fb6fa316afeb3b6266ed4e253b951755cc372f77` |
| `STMicroelectronics-STM32CubeN6.json` | `22a90a4ba3515c87cb3bceaf44344a43bee99c664f243c4a66d692ea8468539e` |
| `STMicroelectronics-STM32CubeU5.json` | `f5bd9d96327930739e9afdb4885b9f47e04b532399007daf4b744b5c8b351dac` |
| `goodix-ble-GR5526-Smart-Watch.json` | `a2791e98dc761f93b15d65ff7cb75625f03aee75ca999b66768ba4b0c28dbb3e` |

### E-EMU-SAP222-HEAP-001 — exhausted live set and bounded allocator/GC comparison

Diagnostic maintenance, 2026-09-22, following E-EMU-SAP222-PANIC-001. Only
current-status and this ledger change. No firmware, runtime, pool sizes,
compatibility policy, ticket status or expected stop changes. All probes,
logs, snapshots and captured bytes remain under `/tmp/sap222-heap/`, outside
Git. The results identify a narrower investigation target, not a working fix.

The current SDL build reproduces the earlier cold checkpoint exactly:
`budget / 0006e78e / 14000000000 / 43612174532 ns`, main frame CRC
`fb8e0155`, generation 4403. Both the log and complete snapshot retain their
E-EMU-SAP222-PANIC-001 hashes. The observer then reproduces all eleven final
80-byte allocation failures and panic callback instruction 14168090291.
The first failure is instruction 14156108860; its allocator entry was
14155276327 at `6a6de`, context `10000000`, request 80. Observer process exit
0 at the panic callback is a diagnostic boundary, never a clean guest halt.

**Input control.** The SDL setup harness defaults to Lower after exhausting
the supplied sixteen-character post-phone sequence. It sends another Lower
at main frame step 30, 38231199108 ns. A fresh cold run stopped at exactly
38 seconds, before that press, produces `budget / 000b019a / 8387825468 /
38000000000 ns`. Two unmodified headless resumes provide no further input;
both exit 3 at `budget / 0005a8f8 / 18000000000 / 47612174532 ns`, with
byte-identical logs and snapshots. Thus the final step-30 press is unnecessary
for reaching the fatal loop. This control does not remove earlier setup input
or establish an exact panic instruction for the no-click path.

**Pool census.** Twelve 20-byte descriptors at `10000000` contain a free
head, fixed-region begin/end, class capacity and total slot count. Fixed
slots have no size header. Dynamic slots in `[10007bd0,1000fff8)` (295 slots)
and `[100575dc,100579e4)` (12 slots) have four-byte capacity headers. The first
failure's primary bump has eight bytes left and its secondary bump has one;
neither can add a fitting slot. Reconstructed counts are:

| Class capacity | Total slots | Free | Live |
|---:|---:|---:|---:|
| 20 | 140 | 1 | 139 |
| 36 | 241 | 3 | 238 |
| 52 | 60 | 0 | 60 |
| 64 | 20 | 0 | 20 |
| 128 | 264 | 0 | 264 |
| 256 | 20 | 0 | 20 |
| 384 | 4 | 0 | 4 |
| 512 | 4 | 0 | 4 |
| 1024 | 3 | 0 | 3 |
| 1568 | 1 | 0 | 1 |
| 2048 | 1 | 0 | 1 |
| 4096 | 0 | 0 | 0 |

All 758 slots reconcile to 754 live and four free. Live capacity is 64,932
bytes, exactly the firmware counter at `100575c4`; total slot capacity is
65,060, equal to the captured peak at `100575c8`. The remaining 128 bytes
are distributed across a 20-byte block and three 36-byte blocks, so none
satisfies the request. This is a class-capacity census, not requested payload.

A cold allocation observer records the latest successful birth and caller
for each address. Its raw free hook covers only dynamic frees and its raw
capacity field incorrectly assumes every slot has a header. Therefore raw
`RETAINED` free/reuse/failure/live/payload/capacity totals are not evidence of
leaks or live capacity. `analyze.py` instead rebuilds all slots from the
captured layout, walks bounded free lists without cycles, removes their four
nodes and checks every class count and both firmware counters independently.
It requires a matching allocation record for each of the 754 live pointers.
In-place realloc payload sizes are not reconstructed and are not used.

The resulting live-block birth census uses half-open five-second intervals:

| Birth interval, virtual seconds | Live blocks | Capacity bytes |
|---|---:|---:|
| [0,5) | 101 | 9,952 |
| [15,20) | 1 | 1,024 |
| [35,40) | 649 | 53,820 |
| [40,45) | 3 | 136 |

Of the 264 live 128-byte blocks, 257 were born in [35,40), seven in [0,5).
The immediate allocator return address is `5f307` for 750 live blocks,
`6a7e9` for two, and `5847f`/`584c5` for one each. Among 128-byte blocks,
249 have `600f5` as the first plausible Thumb return address in eight captured
caller-stack words; the others yield `5a735` (13), `61759` (1), `5fa11` (1).
This stack-word heuristic is not a formal unwind. Disassembly verifies that
`600f0` calls `5f2f4` and returns at `600f4`, supporting property-table growth
as the next trace target without proving each block's owner or reachability.

**Lane comparisons.** Renode 1.16.1 (`d66b0c2a-202602160921`) includes the
read-only original `emulator/renode/sapporo.resc`. It loads captured RAM and
registers, explicitly including SP, then executes bounded firmware functions.
The resident/application hashes are checked before execution and match the
private manifest (`a409b088…7a2f522`, `c8f2d9e4…5a9bfc`). The original lane
script hash is `877b780702b7170827e59a99c4ee0d70a53c863786fed82c71e55640907a412c`;
platform hash is `e731cec46ae2fc43040f3e5108b51c36b505d9acae054f678913a3ab8d6ac732`.
The lane's existing MOV.W substitutions are outside these executed allocator
and collector regions. Its existing startup SilenceRange overlap warning is
recorded in the raw logs; no allocator/collector MMIO fallback is introduced.

The refusal scan stops at `6a73e`, before the fallback callback can invoke
RTOS services: 517 instructions, all 16 registers and all 393,216 copied RAM
bytes match the standalone interpreter. The matching output SHA-256 is
`a8260ba5121acf169b76d2ef42a7dbec48cca533fd3ae6fa1c69c19260435632`.
A separate success control supplies one synthetic free 128-byte slot at
`10020004` through descriptor `10000050`, writes its header at `10020000`,
and terminates its free chain with zero. It returns that pointer at `5f306`
after 116 instructions; all registers and bytes again match, output SHA-256
`6daff01fbae16e7dccee7fa94ce7261323e5ba67aeaad6b83f6b8afb56eb0e3a`.
This synthetic input is a probe control, not a firmware modification or fix.
Both lane cases run twice, with identical extracted census and output bytes,
under a 1 ms virtual cap; core probes cap at 10,000 instructions.

The first final GC retry starts at `5f31a / 14156108868`, heap context
`100047b0`, flags zero. Both full-machine captures return at `5f31e /
14156473307`, elapsed 364,439 instructions. Correct isolated replays must
copy the complete mapped RAM range `[10000000,10160000)`: the collector also
reads a root at `10088f74`. Initial 384 KiB-only attempts omitted that root
and are explicitly invalid for GC comparison. They are not cited as evidence.

With the complete mapped RAM, both lane replays and the standalone interpreter
return at `5f31e` after exactly 364,254 instructions. All 16 registers and
1,441,792 RAM bytes match; output SHA-256 is
`b75ad55ce5be931a19ece7c8a1b48c2404e52895d92838a5b5b39dfb7ea49cae`.
The lane cap is 20 ms and the core cap is two million instructions. Primary
pool bytes `[10000000,10010000)` and secondary pool/counter bytes
`[100575c0,100579e5)` are unchanged from GC entry in the isolated and
full-machine captures. No capacity is released. The full-machine return
has 69 differing bytes elsewhere and 185 extra instructions relative to the
isolated replay; the scheduler/interrupt context is not reproduced by this
probe. It is not a claim of complete machine-state equality.

These comparisons establish allocator and first-retry behavior only for the
provided emulator-derived states. They do not prove that cold lane execution
would construct the same object graph. The unresolved target is why the
35–40-second allocations remain live, including their roots and allocation
history. Enlarging the heap, skipping GC, bypassing assertions, or changing
CPU semantics is unsupported by these results.

Commands and bounds (paths below are volatile; no probe is a runtime dependency):

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
SEMU_SDL_SETUP_WALK_TIMELINE=30000:l \
build/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --max-instructions 14000000000 --max-time 300000000000 \
  --snapshot-save /tmp/sap222-heap/beforepanic.sems
# Repeat this cold command with --max-time 38000000000 and
# --snapshot-save /tmp/sap222-heap/before-main-click.sems for the input control.
build/suunto-emu run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device \
  --snapshot-load /tmp/sap222-heap/before-main-click.sems \
  --max-instructions 18000000000 --max-time 50000000000 \
  --snapshot-save /tmp/sap222-heap/no-click-1.sems
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/cpu/armv7m \
  /tmp/sap222-heap/observer.c build/obj/src/frontends/cli.o \
  build/obj/src/frontends/main_headless.o build/libsemu.a \
  -o /tmp/sap222-heap/observer
SEMU_HEAP_CAPTURE=/tmp/sap222-heap/final-1 /tmp/sap222-heap/observer run \
  --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --snapshot-load /tmp/sap222-heap/beforepanic.sems \
  --max-instructions 14181000000 --max-time 45000000000
# The same build/resume command with gc-observer.c, gc-observer and gc-1
# captures GC entry/return. Each observer run is repeated using output suffix 2.
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/cpu/armv7m \
  /tmp/sap222-heap/retention.c build/obj-sdl/src/frontends/cli.o \
  build/obj-sdl/src/frontends/main_sdl.o build/libsemu.a \
  $(pkg-config --libs sdl3) -o /tmp/sap222-heap/retention-sdl
python3 /tmp/sap222-heap/run-retention.py
python3 /tmp/sap222-heap/analyze.py > /tmp/sap222-heap/retention.census
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude \
  /tmp/sap222-heap/core-probe.c build/libsemu.a -o /tmp/sap222-heap/core-probe
/tmp/sap222-heap/core-probe refusal /tmp/sap222-heap/core-refusal.sram
/tmp/sap222-heap/core-probe success /tmp/sap222-heap/core-success.sram
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude \
  /tmp/sap222-heap/gc-full-core.c build/libsemu.a -o /tmp/sap222-heap/gc-full-core
/tmp/sap222-heap/gc-full-core refusal /tmp/sap222-heap/gc-full-core.sram
# From ../suunto-firmware, each final .resc runs twice with a 60-second wall cap:
.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt \
  /tmp/sap222-heap/allocator-lane-1.resc
# Likewise success-lane-{1,2}.resc and gc-full-lane-{1,2}.resc.
make check
git diff --check
```

All final observer/core/lane runs exit 0; the unmodified bounded firmware
runs exit 3 as stated above. The no-click resumes use 600-second wall caps,
allocation/GC observers 120 seconds, and cold retention observers 900 seconds.
`make check` exits 0 with 985 PASS records and 153 task contracts. Its quick
SDL target explicitly skips private walks; the cold/control runs above are
separate authentic firmware observations. Sanitizers and full profile era
sweeps were not repeated for this documentation-only maintenance. Existing
2.39 era drift and the 2.35 compressed-texture blocker remain unresolved.

Hashes below identify each relied-on raw log and final probe source. Paired
lane raw logs contain host timestamps; extracted `HEAP_` census lines and
output RAM are byte-identical. Paired in-tree logs are byte-identical without
normalization. Snapshot identities above and below do not include firmware
bytes in Git.

| Artifact relative to `/tmp/sap222-heap/` | SHA-256 |
|---|---|
| `cold.log` | `10f0a6b4d73f2a83e105d472d326cd58fafbf2ecaadc4ff21381c6f1e7128c58` |
| `beforepanic.sems` | `88d89cc8bc06084912eaf61126cf9548b04d9712c74e695cb7831136c1868925` |
| `before-main-click.log` | `3e813e63497d8aee537b38b5778643f2cabf611a6e306958914e6513a5b5e7ef` |
| `before-main-click.sems` | `d038e273424f8579c230a14a84f4448dc3cf9e7aec91c4d5696b3fc21e042826` |
| `no-click-1.log`, `no-click-2.log` | `331ed18d8f415def2a2a38964d0cfcd63f8521489c6b411b62527be4ac26d8a2` |
| `no-click-1.sems`, `no-click-2.sems` | `5d5ada9648527f5865483609ca6add461017ee6bdf7c465c14f179e6afe3cdc7` |
| `observer.c` | `f9adbfa5535fd73ce5664537ac0ff90ee6e192d40748e67c43f286ecae5194d1` |
| `final-1.log`, `final-2.log` | `e7636635eb265a87fc65100a11f158abc57d54581f8af7488cf25bcec6c6cd31` |
| `final-1/entry.regs`, `final-2/entry.regs` | `953288cb6cc83f296018d9afe804cac57cb5f25bc3d2064fba6eded876533450` |
| `final-1/entry.sram`, `final-2/entry.sram` | `bdb9d72458074632e8285c652c2864ccd5ff8753fa658eab9a7bfcc34c746fe1` |
| `final-1/failure.sram`, `final-2/failure.sram` | `2dc6c68866cc4b9c4de376927e43c0998549cb63fb406a9bcfaad36255623a54` |
| `retention.c` | `37667196094a89c19df1aaa7505db00ba405e14d89051d249a8a602bba8ab20a` |
| `run-retention.py` | `1e8648c4ef825c7390b4b5900b8d26e1a94ed29c76b6069b3d059a2b67b53ce6` |
| `retention-1.log`, `retention-2.log` | `f000de6a4625152b35a944544ac5318a473c3298f1c468c5a793083e2a43141a` |
| `analyze.py` | `cdc99c4475c88f4d8a34033e6532766f21961ce0d4e7a9db0368a8794146fc24` |
| `retention.census` | `494e1ab54ccabd33a2129a655c9241b766be15016817e52f518c1aeca2120373` |
| `core-probe.c` | `8107c83314394da31b4a6c84b01838f5f7ee7f1f022b735bda329d4f6bd58ff5` |
| `core-refusal.log` | `cf901d8931d48815900a181e020b62d69ca1b3611649785e33816a62bf652e33` |
| `core-success.log` | `6d3fff65f2fc031e90358938c72af8a37dbbf976e0b90afa19cc4be92fc19c23` |
| `allocator-lane-1.resc` | `230e79a6cb17ba59fd9cc9f3249c5cb8f8ecf54fdfbe938609b39ebf7a5a7b41` |
| `allocator-lane-2.resc` | `3a2c090b94845d21fcc5915d7585fde3dbc66caa3f8e5f5b17980c047293792e` |
| `allocator-lane-1.log` | `c364a3b909b0fd2c883fe5f1f366c50c356fa62d2658ed739b961e38e6c557bf` |
| `allocator-lane-2.log` | `04d4b7317aa5d22d30edb39e6f3bdb38368aba0c8ab90fc33d75ad6061e88df1` |
| `allocator-lane-1.census`, `allocator-lane-2.census` | `b0d14111adabcde6c8988223389f2de72591ce997b0e7a18e68b1b303d759ffa` |
| `success-lane-1.resc` | `160660352848c7fcdcbc256c4eb578bc17e0116f330b6cad6ffe901c6e4945cf` |
| `success-lane-2.resc` | `4f21f34d67adff1cb1213a8191f91db47c21e9f4481190dd6a07417078d895d6` |
| `success-lane-1.log` | `e3f8764bc170e1abb3f299027c5489ce8e328dd660f49ad639b00fdba4275daa` |
| `success-lane-2.log` | `38138b16f91a676b316f67aa4a0ec4e417eaf387ab73deb1fbdc37c5cdf211d7` |
| `success-lane-1.census`, `success-lane-2.census` | `0a4bec3023ba9a971aa4aafe1cab294759399abee42bb1c247d65edb42af04b2` |
| `gc-observer.c` | `4a4c988841e29d3f43db46faec4ea5c256ed0b5055ad3bdfcac37b900a38f4f6` |
| `gc-1.log`, `gc-2.log` | `fa4d46715487279ffe12e7a86002a87040102b3d38523a2fd9886ffafc9dc15e` |
| `gc-1/entry.regs`, `gc-2/entry.regs` | `b1d46954cc7c01413d49d05552f2c5c72d7a706fa4f0324a162cf9d2fd274629` |
| `gc-1/entry.sram`, `gc-2/entry.sram` | `2dc6c68866cc4b9c4de376927e43c0998549cb63fb406a9bcfaad36255623a54` |
| `gc-1/return.sram`, `gc-2/return.sram` | `0703c80e54f281816ecb7d468412bd1dfd4162e98ccf11cd1270458675ab7401` |
| `gc-full-input.sram` | `e57db646dfd8e4b590ea47a0016996a843e06accd92976be1c378b92200ea9dc` |
| `gc-full-core.c` | `7d820903b1ac350a626ffa0a66d9e4d6fb31d8518fc81f3a178ae367ec4b1897` |
| `gc-full-core.log` | `79e0145fe047001551a1c3db0edafd7af32d24fd6be662a766aaf06a90b785cf` |
| `gc-full-lane-1.resc` | `4bc385ac6d1dc09d95a77927a508a8d38c92787f79d6be46286a723071320666` |
| `gc-full-lane-2.resc` | `8ae42baddb438a4482e70876af6ba69edbed491516175e7dbc0377e59cbb278a` |
| `gc-full-lane-1.log` | `ededbca49fb703b5d8ea521dba085125c3adaa7c357d6a69a64958f03f64feb9` |
| `gc-full-lane-2.log` | `7864e59fdae825f196dcc84712eebbc544a6ed30532b3f7f5ba1f398374af151` |
| `gc-full-lane-1.census`, `gc-full-lane-2.census` | `4aff04dafa8adafe7660514c2085b6deea04a1436166476b5cc70e77be458bef` |
| `check.log` | `e77cab52f9fc941e3607dbcec3b9b6405d776f940143925898eb0dc3a35c4c9e` |


## E-CPU-F57F-001 — Thumb F57F dispatch and the 2.22 script heap loop (2026-09-22)

Scope: bounded CPU bug maintenance, not a firmware workaround. E-CPU-0002
pins ARM DDI 0403E.e, A5.3 Thumb branch encoding and A7.7 branch/barrier
pseudocode. `F57F AF87` is BPL.W T3; the old system dispatch mistakenly
accepted it as an A32-style barrier and returned without taking the branch.
The same predicate incorrectly consumed `F57F AF07`, `F57F 9F07` and
`F57F BF07`. Remove that predicate and handler; the existing Thumb branch
decoder now owns these encodings. Actual `F3BF` Thumb barriers are unchanged.

Five implementation/test files change: `src/cpu/armv7m/thumb32.c`,
`thumb32_system.c`, and `tests/unit/test_cpu_thumb32_{dispatch,data,memory}.c`.
The narrow dispatch regression failed before the fix and passes afterward.
An existing test called three of these branches barriers and pinned them as
no-ops. It is removed in favor of the new exact-target/state matrix, justified
by the architecture and paired lane results below. This corrects an erroneous
CPU expectation; no firmware golden, heap size, instruction substitution,
compatibility hook, persistent format or public header is changed.

### Provenance and isolated lane results

All raw probes, RAM, logs and rendered pixels stay outside Git under
`/tmp/sap222-graph/`; prior allocator input comes from E-EMU-SAP222-HEAP-001.
The only machine oracle is the read-only sibling Renode lane, version 1.16.1,
using `../suunto-firmware/emulator/renode/sapporo.resc` SHA-256
`877b780702b7170827e59a99c4ee0d70a53c863786fed82c71e55640907a412c`
and its platform SHA-256
`e731cec46ae2fc43040f3e5108b51c36b505d9acae054f678913a3ab8d6ac732`.
The private manifest validates all three 2.22 components before cold execution:
resident `a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522`,
application `c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`,
resources `ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1`.
Isolated core probes validate their resident, application and RAM input hashes.
No sibling file is modified.

`branch-mapped-lane.resc` allocates synthetic RAM at `20000000..21ffffff`
and executes each pair once at PC `21000100`, with N clear and set. Two runs
produce identical numeric PC/xPSR censuses and exactly eight instructions:

| Thumb halfwords | N=0 target | N=1 target | xPSR after |
|---|---|---|---|
| F57F AF87 | 21000012 | 21000104 | unchanged |
| F57F AF07 | 20ffff12 | 21000104 | unchanged |
| F57F 9F07 | 2057ff12 | 2057ff12 | unchanged |
| F57F BF07 | 20d7ff12 | 20d7ff12 | unchanged |

The in-tree test relocates PC to `100`, checks all CPU state except the
expected PC/instruction count, and covers taken/not-taken conditional branches
and both unconditional encodings. Existing dispatch refusal tests still pass.

The native software double-add entry at `699e8` receives r1:r0 =
`3ff00000:00000000` (1.0), r3:r2 = zero. The problematic branch is at `699f2`,
with the taken target `69904`. Old dispatch returns zero after 42 instructions.
The corrected interpreter and both `add-valid-lane` replays return 1.0 after
18 instructions, stopping before the synthetic mapped return at `10010000`.
All 16 general registers and all 393,216 output RAM bytes match; output SHA-256
`50b78d236ed2e47b4c1e1d447b9e7e11c35740bcf04207a8c46add9544472b5c`.

### Captured script increment and causal chain

A read-only heap census of the previous failure finds 375 non-string headers:
336 objects and 39 buffers. Of those, 257 objects have the same four property
keys (`id`, `src`, `autoUnload`, `class`) and form a 257-object wrapper chain
ending at the original main-menu identifier. This is a derived census, not
committed resource source. It directs attention to the script loop increment;
it alone does not prove interpreter semantics.

Two cold observers capture the same increment input at PC `66884`, bytecode
pointer `100064c4`, instruction 8271213988, virtual time 37883388520 ns.
Their general registers and full 1.5 MiB SRAM captures are byte-identical.
The second additionally captures FPU state: CPACR `00f00000`, FPSCR `82000010`,
FPCCR `c0000000`, FPCAR zero; D9 already contains 1.0. Isolated replay copies
1,441,792 bytes of mapped SRAM, all general registers and the captured S/D
registers, explicitly enables the lane FPU, and stops at PC `660f2` with
r0=`100064c8`, immediately after the increment bytecode.

Old dispatch takes 127 instructions and leaves the double at `10007478` zero.
The corrected core and two `inc-fpu-lane` replays take 103 instructions and
store 1.0 there. All 16 general registers and every copied RAM byte match;
output SHA-256 `a7cee50dd9813eded80e273bde6360f0913f9155626389f460579973580414ed`.
The matched final registers are r0=`100064c8`, r1=`1000129c`, r2=1,
r3=`02000000`, r4=`10007478`, r5=`10007480`, r6=`10005bb0`, r7=`7a`,
r8=`100063c0`, r9=0, r10=`10`, r11=2, r12=`00200000`, SP=`10040498`,
LR=`00066d09`, PC=`000660f2`. This explains why the old loop repeatedly
wrapped the same object until allocation failed. The allocator/GC match in
E-EMU-SAP222-HEAP-001 remains valid; changing allocator policy is unnecessary.

These are isolated lane comparisons using captured emulator input, not a
cold lane execution of the whole watch. Exploratory runs with an unmapped
return/branch target, invalid FPU setter or disabled lane FPU are excluded;
they are not evidence of a semantic mismatch. Paired final censuses strip
host logging prefixes only; output RAM is compared byte-for-byte.

### Corrected authentic execution and verification

Two cold SDL walks with the manual-time input sequence finish step 31:

| Checkpoint | Generation | CRC32 | Virtual ns |
|---|---:|---|---:|
| step 29 | 4319 | 2a01c517 | 37663749660 |
| step 30 | 4406 | 73d569a5 | 38240928644 |
| step 31 | 4510 | 040ebb03 | 38818426902 |

Both exit 0 at `user / 0800009e / 8500057344 / 38818426902 ns`.
Logs are byte-identical (SHA-256
`7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa`),
as are 4,800,287-byte snapshots (SHA-256
`7082666171efcd66ddf315684c6bfd040f97e126c0cfa5886fc900f47c7ccb1a`).
Paired idle resumes exit 3 at the budget boundary
`000d4a8c / 8807319394 / 60041792981 ns`; WFI advances to the next event
past the requested 60-second bound. Both remain active without reset,
refusal or the former fatal allocation loop. Logs match with SHA-256
`9ef28bb9e945d140e21b702b5c50e1b83f15fd994263da80747557a3d62d2074`;
snapshots match with SHA-256
`5239ac0e98f71de202c85ceb8c6b19b2c7e4b59ff3d3205c1b59258da5510bfc`.
This is a bounded stability result, not proof of every main-menu function.

Exact commands (each cold/resume repeated with suffixes 1 and 2):

```sh
make test TEST_FILTER=cpu_thumb32_dispatch
make test TEST_FILTER=cpu_thumb32
make check
make sanitize
make sdl
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
  SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
  SEMU_SDL_SETUP_WALK_TIMELINE=30000:l \
  build/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --max-instructions 18000000000 --max-time 60000000000 \
  --snapshot-save /tmp/sap222-graph/fixed-walk-1.sems
build/suunto-emu run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device \
  --snapshot-load /tmp/sap222-graph/fixed-walk-1.sems \
  --max-instructions 18000000000 --max-time 60000000000 \
  --snapshot-save /tmp/sap222-graph/fixed-idle-1.sems
cc -std=c99 -O2 -Iinclude /tmp/sap222-graph/inc-core.c \
  build/libsemu.a -o /tmp/sap222-graph/inc-core
/tmp/sap222-graph/inc-core refusal /tmp/sap222-graph/inc-core.sram
cc -std=c99 -O2 -Iinclude /tmp/sap222-graph/add-fixed-core.c \
  build/libsemu.a -o /tmp/sap222-graph/add-fixed-core
/tmp/sap222-graph/add-fixed-core refusal /tmp/sap222-graph/add-fixed-core.sram
# From ../suunto-firmware, twice each, 60-second wall caps:
.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt \
  /tmp/sap222-graph/branch-mapped-lane.resc
# Likewise add-valid-lane-{1,2}.resc and inc-fpu-lane-{1,2}.resc.
sh tools/test_sdl_live_input.sh
make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235 \
  SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
python3 -P /tmp/sap222-graph/run-era235.py
```

Cold walks use a 900-second wall cap; resumes and era runners 600 seconds.
The isolated add/core increment caps are 10,000/100,000 instructions. Lane
add/increment caps are 1/20 virtual milliseconds; the branch matrix steps
exactly eight instructions. The old core comparisons link the pre-fix
`thumb32.c` and `thumb32_system.c` extracted from HEAD `5980046` ahead of
the current archive; other core behavior is identical.

The new dispatch regression fails before the correction; afterward the
Thumb-32 selection passes 34 cases. `make check` passes 985, `make sanitize`
980, and `make sdl` exits 0. `make check` quick SDL intentionally skips the
private walks. `sh tools/test_sdl_live_input.sh` exits 1 on changed stop pins,
though its three frame CRCs remain `4979f432`, `629da47e`, `d4ed66c7`.
The 2.35 Make selection stops on its first failure (exit 2); the separate
seven-runner sweep records pressure/production exit 0 and block erase,
OHR, GPS startup/reopen/awake exit 1. New ticket 789 owns attribution and
paired re-derivation; no old firmware pin is silently updated. The 2.39 era
suite was not rerun and may have additional drift beyond tickets 777/783.
No claim is made about other profiles, long sessions, physical peripherals
or compressed 2.35 icon support.

### Volatile artifact identities

Paths below are relative to `/tmp/sap222-graph/`. Raw lane logs differ in
host prefixes; the listed paired censuses and output RAM are byte-identical.

| Artifact | SHA-256 |
|---|---|
| `inspect.py` | `d4fc20021a79b0d72811b9fa09c65f14cde95ed98b48bd85bd37a1afe685d93b` |
| `graph.py` | `9a24ec487fea892330d72f15f6154b8ed935e6b0d9440987acf1806ce229e7a6` |
| `graph.log` | `bce7fa0148d0fa282ab19a454a5a92b216e9e59b525daa23566350cd0fee05c3` |
| `increment-observer.c` | `14f5da99aa6626cfa2d6090507b84ec006fccbe536a211ebd9b583cb9f35f014` |
| `increment-fpu-observer.c` | `990c163452c8b7cc8360e2d8c08d0642371fe4910bc1403f4e78b490af6e6045` |
| `increment.log` | `cc7b9ae06c5c62b2fa4098957a62168c48433ba28a7ddc7f69d64df9ef60eca7` |
| `increment-fpu.log` | `0daf110d8d8730fa497be6abf03e6f4cfb5026333c9b2a5d83ef71d01089272a` |
| `increment/entry.regs` | `1a4cc1f1c79975fc4bfe8d7a8b16a5ba33c145d94a859f989248b0ffbe72af7b` |
| `increment/entry.sram` | `302a553912768d02ced52c0c0c0a25a4d7cd1710f3395f0f3b6e682904187de2` |
| `increment-fpu/entry.regs` | `1a4cc1f1c79975fc4bfe8d7a8b16a5ba33c145d94a859f989248b0ffbe72af7b` |
| `increment-fpu/entry.sram` | `302a553912768d02ced52c0c0c0a25a4d7cd1710f3395f0f3b6e682904187de2` |
| `increment-fpu/fpu.bin` | `0fbb697e4016527dd3dbb2b7fe6d58a99d1490e993a09477cb726e7ea2496d91` |
| `inc-core.c` | `d6e0f6e9abdaede8ea5b35730032a4002a5202f4012432c76556c9e0668aa1cb` |
| `inc-core.log` | `9a9764e87d376e2b0b70b388ed473d0a6a2cbbf3346d9c1360556cfa1539b82e` |
| `inc-before-core.log` | `9f784392c2a1750e63ffde0368bab9f43ec13abcd9025bd3550ceaab80406309` |
| `inc-input.sram` | `3ff35bfd3fcd42b15320a8f0c94a602ffc84b937afae50dd05d652bec4b24911` |
| `inc-core.sram` | `a7cee50dd9813eded80e273bde6360f0913f9155626389f460579973580414ed` |
| `add-core.c` | `8afad365534c466c85878dcd50be02f5417d26536a3dffc84f20286385c5ed2f` |
| `add-core.log` | `15b82faeef018bb92df0c1beb6ede5388d61e878524e283b7bdfb9196ba991ca` |
| `add-fixed-core.c` | `c520d738bd67f9bc4bef05a1c69330b34b9bb03958b7c84b46de19fe2bb314fc` |
| `add-fixed-core.log` | `4113e5b5933fe6aa0f0e85c2c24fb52c491ffb0dd46439d0be707527fe9256b1` |
| `add-fixed-core.sram` | `50b78d236ed2e47b4c1e1d447b9e7e11c35740bcf04207a8c46add9544472b5c` |
| `branch-mapped-lane.resc` | `a07c724821861caa81b07cc1519d990436f99876bb07b73614d852078aa8ecef` |
| `branch-mapped-lane-1.log` | `3737479619fffd055a3f167051f502ce1e9f6fa17c916f9f22b52931d6d25296` |
| `branch-mapped-lane-2.log` | `05bcfc53617e450654501b8956ea1b72edc15e0fc536c8d3ca6af08e1bd7b7d2` |
| `branch-mapped-lane-1.census` | `a1da76b01d51b5fd3bf47346cdac022518674eae37a9fb62266299d69bd22a20` |
| `branch-mapped-lane-2.census` | `a1da76b01d51b5fd3bf47346cdac022518674eae37a9fb62266299d69bd22a20` |
| `add-valid-lane-1.resc` | `73bf151bf1e9bb85f9f6d85468750a8c7239d520dccd0e878e9726481b0ad989` |
| `add-valid-lane-2.resc` | `61885343aa7139d1ee6c20dee85c72e22f6c9e4d06781922ea81e94385c95c48` |
| `add-valid-lane-1.log` | `b763e3c40e255ec7503ac05cd974215eaffd90f4cb3246ffe83f67c2d403c716` |
| `add-valid-lane-2.log` | `ff9dff07095b032ac1ca5b0d77051495af7c526c2c2c08b00a8da02dbe0c3f1d` |
| `inc-fpu-lane-1.resc` | `9617caf3e5028add1dba61d8c5cbe9cf444511ad0317745b4cbb8147be284d2a` |
| `inc-fpu-lane-2.resc` | `398d19f23cfd8858a9e9d86f6ae1c8fcde9975f4ebbe1cd9068d5b89dd7ac7ac` |
| `inc-fpu-lane-1.log` | `9a275c5186edc8a2069176bd932a2d9dba3644ab6bbb1c815968a8c956eac621` |
| `inc-fpu-lane-2.log` | `8e56593de9b6eb90ec18099018ea2d930d66515a9b46828175d6b7239f4c9cfb` |
| `inc-fpu-lane-1.census` | `6439d3e4370890fd297eed8912d56a20a20f15150b6116721629caa1aadaa9bc` |
| `inc-fpu-lane-2.census` | `6439d3e4370890fd297eed8912d56a20a20f15150b6116721629caa1aadaa9bc` |
| `before-regression.log` | `0d65de672519abcd00fefaa2ff2ad669c09bab70679ccfc18a2b456066f87f9a` |
| `after-regression.log` | `92fffea89589e6ad5aa1aa113ed3db95a0395af4ea27eafe4543725326f60772` |
| `check.log` | `ccd344f706e18cc671b3be5c6c07817c1cc540cd5f626f88304c30bcb62cf7d3` |
| `sanitize.log` | `2dd107ae1368dd825ab6f58cbd3cfff07b6d059169b2b2f332a6ba63c6a1078b` |
| `sdl.log` | `2e19e5f76159ae3c98e3ebf516bd7c4dc655e6bd2d3f40b71781ffadabbdd21f` |
| `sdl-input.log` | `016cceae97bce3b9cffa96df7b6a26faf86ef19bf1344d84f187bb44ebbf7829` |
| `firmware235-valid.log` | `51024cbcee9eeada7382c40f60ccfcfa8ccd7c0711d59b966391eaa46100b6e9` |
| `run-era235.py` | `74385fd7ce1424e4b242231b2142fd420a3c1b3f5503684cd6c142299d1779ac` |
| `era235-results.json` | `6fe37dded8dbf610a95e75d88bbee6f7dd0208f38f6a0f80dd2d92c7caf315da` |
| `test_firmware_sapporo_235_block_erase.log` | `943c901726a5be99b2ea1d9873e657a70df80c586b3ece96187d795922ac6b38` |
| `test_firmware_sapporo_235_gps_awake.log` | `943c901726a5be99b2ea1d9873e657a70df80c586b3ece96187d795922ac6b38` |
| `test_firmware_sapporo_235_gps_reopen.log` | `943c901726a5be99b2ea1d9873e657a70df80c586b3ece96187d795922ac6b38` |
| `test_firmware_sapporo_235_gps_startup.log` | `943c901726a5be99b2ea1d9873e657a70df80c586b3ece96187d795922ac6b38` |
| `test_firmware_sapporo_235_ohr.log` | `943c901726a5be99b2ea1d9873e657a70df80c586b3ece96187d795922ac6b38` |
| `test_firmware_sapporo_235_pressure.log` | `651ca57415e0c4c4d35b33a6337cccc75f155ac0e29df030b552079a1a20a322` |
| `test_firmware_sapporo_235_production.log` | `18c0f3956a9691ecab8e927e91482750cc577fa709b2b4238fb9d9f314c29a19` |


### Retained 2.35 failure audit and 2.22 visual check

To identify the first failing assertion, external copies of the five failing
runners change only their EXIT cleanup trap to print and retain the temporary
run directory; `sh -x /tmp/sap222-graph/test_firmware_sapporo_235_<name>-retain.sh`
uses the same `TEST_PROFILE`, `SEMU_FIRMWARE_MANIFEST`, `SEMU_EMULATOR` and
600-second wall caps as `run-era235.py`. Repository runners remain untouched.
All five first failures are transcript SHA-256 comparisons. Each compared
pair is byte-identical, with the following observed stop tuples (hex PC;
instructions; ns). Later assertions after the hash comparison were not run,
so this is not a replacement passing gate:

| Runner/case | Stop | PC | Instructions | Virtual ns |
|---|---|---|---:|---:|
| block erase/frame | user | 000a6bbe | 864000000 | 3782156506 |
| OHR/enabled | budget | 0008ce2e | 500000000 | 2719206417 |
| OHR/disabled | budget | 000a6bc8 | 280000000 | 1590729375 |
| GPS startup | budget | 00093222 | 500000000 | 3343660033 |
| GPS reopen | compat-refused | 001be85a | 1080305993 | 16306637984 |
| GPS awake | compat-refused | 001259fe | 1638733422 | 54660679480 |

The last two still report the bounded fixture refusal; awake has exactly
eight admissions. Ticket 789 must validate all remaining assertions and
before/after causality before changing any pin.

A third corrected cold 2.22 walk adds only
`SEMU_SDL_PPM_DIR=/tmp/sap222-graph/visual` to the command above (no snapshot
save). Its transcript is byte-identical to both earlier walks. Inspection of
the actual 240x240 PPM output shows step 30 selecting Navigation and step 31
selecting Logbook, with Media controls/Timer visible below. Thus a post-setup
button changes the native main-menu selection. Step 29 is black; that
transient CRC alone must not be described as a complete rendered screen.
PPMs and display PNG conversions remain outside Git.

A separate SDL resume of `fixed-walk-1.sems` with no input reaches
`budget / 000d4a8c / 8517764324 / 40014147571 ns` but publishes no fresh frame.
A read-only backend capture immediately after snapshot load is black. The
current SDL setup gate rejects input until it has a published frame; this
snapshot-to-interactive-window path therefore still needs work. The matched
headless continuation above does not imply a ready-to-use restored UI.

| Artifact relative to `/tmp/sap222-graph/` | SHA-256 |
|---|---|
| `test_firmware_sapporo_235_block_erase-retain.sh` | `a472989def69f4f3a85db5f4270b34f3ae565b1d6641cca36f78f51eff777257` |
| `test_firmware_sapporo_235_block_erase-retain.log` | `0c6d9cc3ec28e1cccc9d3196312247936b32851d65dfa65de374aa46620585c2` |
| `era235-retained/test_firmware_sapporo_235_block_erase/frame-1.log` | `2d479fc3f6c4f059037e39c139cf939315e8bfdf3b5574166727e5f5dfcb37f8` |
| `era235-retained/test_firmware_sapporo_235_block_erase/frame-2.log` | `2d479fc3f6c4f059037e39c139cf939315e8bfdf3b5574166727e5f5dfcb37f8` |
| `test_firmware_sapporo_235_gps_awake-retain.sh` | `4ebae51e37d5760c119112a27cf3879c3c9d2592fdc48f21cc371df7ea2160f0` |
| `test_firmware_sapporo_235_gps_awake-retain.log` | `10333cedaa943cbd3ee71327e02482058dbcd1fc07ccbdf4bc000010d2d3b9e9` |
| `era235-retained/test_firmware_sapporo_235_gps_awake/reopen-1.log` | `988c24e77f3b25b2be945f3d81a7f6ca3f17dfae94bd63cbb1eecb14533fbfe3` |
| `era235-retained/test_firmware_sapporo_235_gps_awake/reopen-2.log` | `988c24e77f3b25b2be945f3d81a7f6ca3f17dfae94bd63cbb1eecb14533fbfe3` |
| `test_firmware_sapporo_235_gps_reopen-retain.sh` | `2874022490e2d02949209065a28707038dabaabe34214f97baa846a46122ee56` |
| `test_firmware_sapporo_235_gps_reopen-retain.log` | `575df7718135579c7be0bbb0214e850f5eb2d1315563ec0d3a27e06b9148e773` |
| `era235-retained/test_firmware_sapporo_235_gps_reopen/reopen-1.log` | `74dca7ea9589a670d9058b253e9f591ae50fb8281b633f1f40471f8653d40589` |
| `era235-retained/test_firmware_sapporo_235_gps_reopen/reopen-2.log` | `74dca7ea9589a670d9058b253e9f591ae50fb8281b633f1f40471f8653d40589` |
| `test_firmware_sapporo_235_gps_startup-retain.sh` | `e2605cbf5c875bb9f388efeb0dae570f8fd72c590fdc281842a862c2ad27b27f` |
| `test_firmware_sapporo_235_gps_startup-retain.log` | `156d91c4169cd90880ccfde5f6e97bfd0483725f223b6b533d0e47b402fb8380` |
| `era235-retained/test_firmware_sapporo_235_gps_startup/startup-1.log` | `7627f2125bbd7d5ebf644ec551b125c049b50f6f3d589a5922352ebb15fe7994` |
| `era235-retained/test_firmware_sapporo_235_gps_startup/startup-2.log` | `7627f2125bbd7d5ebf644ec551b125c049b50f6f3d589a5922352ebb15fe7994` |
| `test_firmware_sapporo_235_ohr-retain.sh` | `1c70cc4950e958f7f64cc66f366fae21c8a33293150ddea8ae364b1d2a2f9db6` |
| `test_firmware_sapporo_235_ohr-retain.log` | `316265eeab01bd644ba27247bc2d0922badddf0d0ef339d17054d40287a21746` |
| `era235-retained/test_firmware_sapporo_235_ohr/disabled-1.log` | `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879` |
| `era235-retained/test_firmware_sapporo_235_ohr/disabled-2.log` | `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879` |
| `era235-retained/test_firmware_sapporo_235_ohr/enabled-1.log` | `f69c9d8a7c176da98190eed09e14c0c14db32d282cc5ee4b62bbf2bb94201317` |
| `era235-retained/test_firmware_sapporo_235_ohr/enabled-2.log` | `f69c9d8a7c176da98190eed09e14c0c14db32d282cc5ee4b62bbf2bb94201317` |
| `visual-walk.log` | `7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa` |
| `visual/frames.log` | `50e0527c1dead2a134dadb77bd63dbe20c06d032f2edb90d256f4adb2d9273b8` |
| `visual/suunto-frame-040ebb03.ppm` | `61ee3965fff07e08aaa6b297ddead800e155b3620f1c68c42b4037c28b3f2ad8` |
| `visual/suunto-frame-73d569a5.ppm` | `28dba8c12577d0e52afc1c8004234192b2d2138cd27ca9cf5b42dd5baeb42496` |
| `visual/suunto-frame-2a01c517.ppm` | `1c9647599609f484387d350da404471cb44fa2b7a7f3a182f431c58e35509699` |
| `display-resume.log` | `56c9aae62a542c0a069ec16925ebb2e89fb4a47749398ac63766e72502693340` |
| `frame-probe.c` | `1c9442f2d6148323f6c246c11151759f18dfad8a21e0e70917c751e6f50461fa` |
| `frame-probe.log` | `345bc18f10b79a21746553ae949ee757f7cc109b19ed32f9dc4e667639805459` |
| `current-menu.ppm` | `1c9647599609f484387d350da404471cb44fa2b7a7f3a182f431c58e35509699` |

Final documentation/planning validation: `make check-task-contracts` validates
154 tickets, `make check-lines` exits 0 with advisory size warnings, and
`git diff --check` is clean. Final `make check` exits 0 with 985 PASS records
and 154 task contracts; `/tmp/sap222-graph/check-final.log` SHA-256
`403e0066a04fcbc60b68fc296da41b91fafeabbab4b76ae6b2c389f991e20e6e`.

## E-EMU-SAPPORO-BRANCH-GATES-001 — Post-F57F firmware regression derivation

2026-09-22, ticket 789. Dependencies 220, 225, 670 and 705 are done. Scope is
only the eleven named shell runners and README/status/evidence; no engine,
public interface, profile, firmware component, compatibility budget or existing
ticket status changes. The architecture and paired lane increment evidence
remain E-CPU-0002 and E-CPU-F57F-001. This entry records current interpreter
regressions, not new hardware observations. No physical device is available.

### Causal control

Build a control by compiling the pre-fix `thumb32.c` and `thumb32_system.c`
(from HEAD `5980046`, volatile copies retained by E-CPU-F57F-001) before the
current archive. The rest of the current engine and all frontend objects are
identical. Commands:

```sh
cc -std=c99 -O2 -Iinclude -Isrc/cpu/armv7m \
  /tmp/sap222-graph/before-thumb32.c /tmp/sap222-graph/before-system.c \
  build/obj/src/frontends/cli.o build/obj/src/frontends/main_headless.o \
  build/libsemu.a -o /tmp/sapporo-789/before-headless
cc -std=c99 -O2 -Iinclude -Isrc/cpu/armv7m \
  /tmp/sap222-graph/before-thumb32.c /tmp/sap222-graph/before-system.c \
  build/obj-sdl/src/frontends/cli.o build/obj-sdl/src/frontends/main_sdl.o \
  build/libsemu.a $(pkg-config --libs sdl3) -o /tmp/sapporo-789/before-sdl
python3 -P /tmp/sapporo-789/audit-before.py
python3 -P /tmp/sapporo-789/collect-after.py
python3 -P /tmp/sapporo-789/compare-censuses.py
```

The external before driver runs original runners with explicit manifests and
the two control binaries, retaining volatile logs by changing only cleanup.
All ten original control runners exit 0, including the seven 2.35 gates,
the two 2.35 SDL prefixes and 2.22 live input. Their repeated pairs compare
byte-identically. The after driver
uses the production binaries and exact existing command/input budgets. Each
runner/capture has a 900-second wall cap in addition to its guest budgets.
Firmware components are validated before execution; component identities
remain those in E-CPU-F57F-001 (2.22) and E-SAP-0038 (2.35). Original runner
sources remain under `/tmp/sapporo-789/original-runners/` for pin comparison.

Remove only the leading `time_ns=N ` from ordered device/compat event lines
for a before/after event census; do not normalize paired-run full logs.
The 2.35 frame prefix retains 42 event payloads; OHR enabled/disabled 42/4;
GPS startup and its boundary 44 each; reopen 46; awake 54; SDL language 42;
SDL phone scroll 49. The 2.22 live-input prefix retains 37 event payloads.
Each before/after payload sequence is identical. Existing stop reasons and
refusal diagnostics remain required; instruction/time/PC pins change only
where the corrected branch changes execution. The live-input SDL frame CRCs
remain unchanged for both versions.

### 2.22 setup acceptance correction

E-EMU-SAP222-HEAP-001 and E-CPU-F57F-001 disprove the historical description
of a fatal script halt as clean idle. The completion regression now requires
all manual-time checkpoints, a normal voluntary harness exit after step 31,
the rendered Navigation and Logbook-selection frames, and an exact full-log
hash. It saves a temporary snapshot and continues with no input through
60 virtual seconds. The continuation must stop on its budget with its exact
PC/instruction/time tuple and transcript hash, without reset or refusal.
It does not claim a complete renderer snapshot. The transient black step-29
frame is retained as a transition checkpoint, never as proof of menu pixels.

The successful cold sequence and idle continuation retain E-CPU-F57F-001's
paired full-log hashes `7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa`
and `9ef28bb9e945d140e21b702b5c50e1b83f15fd994263da80747557a3d62d2074`.
A fresh SDL continuation also repeats the latter hash exactly. Step 23's
changed CRC still shows YEAR with 2023 selected; the corrected renderer
capture is inspected rather than assuming a changed CRC is harmless.
The malformed-timeline checks remain unconditional. Disabling the manual-time
press sequence still requires the finite GPS-cap refusal, not an assertion
or generic timeout.

### Required renderer-snapshot integration

`src/boards/machine_snapshot.c` serializes machine, NEMA GPU registers and
completion state, but the externally owned backend in
`src/display/nema_backend_internal.h` is absent. It owns the committed RGB565
surface/generation, inherited register presence/values and TSC6A semantic
shadow. `include/semu/display.h` offers only prepare/commit/abort, with no
persistence or frame-restoration contract. Restoring CPU/RAM while constructing
a fresh backend loses those states; an idle snapshot emits no fresh frame,
and the SDL gate rejects input until a frame exists.

Smallest required integrator change: add a versioned backend save/validated
restore contract and a way to borrow or republish the restored frame; wire
it into machine snapshot atomicity/rollback and CLI presentation without
advancing guest time or publication generation. Preserve the last published
image separately from the committed drawing surface: inline draws after the
last published child can change the latter without a new frame callback
(`nema_backend_transaction.c`). Persist committed inherited state and semantic
shadow as well as both pixel states; refuse active transactions,
unknown backend/version, malformed lengths and missing required state.
Old snapshots cannot invent missing renderer state. Synthetic uninterrupted
versus restored drawing and refusal atomicity are required, followed by an
authentic menu restore and button check. This interface/format work is outside
789 and is not worked around through a private parallel API.

### Changed checkpoint pins

Old values below are preserved from original runner sources, not inferred
from current code. Full raw-log identities appear in the next table.

| Runner / case | Before | After |
|---|---|---|
| `test_firmware_sapporo_235_block_erase.sh` #1 | `user / 0x000bdd2a / 813500000 / 3733351422` | `user / 0x000a6bbe / 864000000 / 3782156506` |
| `test_firmware_sapporo_235_gps_awake.sh` #1 | `compat-refused / 0x001259fe / 1579930110 / 54642979249` | `compat-refused / 0x001259fe / 1638733422 / 54660679480` |
| `test_firmware_sapporo_235_gps_reopen.sh` #1 | `compat-refused / 0x001be85a / 1020576082 / 16288236023` | `compat-refused / 0x001be85a / 1080305993 / 16306637984` |
| `test_firmware_sapporo_235_gps_startup.sh` #1 | `budget / 0x000ee120 / 500000000 / 3343660033` | `budget / 0x00093222 / 500000000 / 3343660033` |
| `test_firmware_sapporo_235_gps_startup.sh` #2 | `compat-refused / 0x001254ec / 996415389 / 14881889213` | `compat-refused / 0x001254ec / 1059785208 / 14811876715` |
| `test_firmware_sapporo_235_ohr.sh` #1 | `budget / 0x000bdc36 / 500000000 / 2719206417` | `budget / 0x0008ce2e / 500000000 / 2719206417` |
| `test_sdl_live_input.sh` #1 | `user / 0x000bacf4 / 774081920 / 6520939902` | `user / 0x080000a0 / 772290112 / 6520978802` |
| `test_sdl_sapporo_235.sh` #1 | `user / 0x0800009e / 1202094208 / 7971442704` | `user / 0x080000a0 / 1262036864 / 7966140811` |
| `test_sdl_sapporo_235_scroll.sh` #1 | `budget / 0x000e1862 / 4896065682 / 22000000000` | `budget / 0x000e1862 / 4961334596 / 22000000000` |
| onboarding `expected_press` | `SDL live test timeline press index=0 virtual_ns=30005853579` | `SDL live test timeline press index=0 virtual_ns=30003929586` |
| onboarding `expected_step_22` | `SDL live test settled step=22 generation=3992 crc32=5321867e` | `SDL live test settled step=22 generation=3991 crc32=5321867e` |
| onboarding `expected_step_23` | `SDL live test settled step=23 generation=4074 crc32=c683e828` | `SDL live test settled step=23 generation=4073 crc32=8b6879f9` |
| onboarding `expected_step_24` | `SDL live test settled step=24 generation=4137 crc32=cd1b0979` | `SDL live test settled step=24 generation=4136 crc32=cd1b0979` |
| onboarding `expected_step_25` | `SDL live test settled step=25 generation=4194 crc32=455b603a` | `SDL live test settled step=25 generation=4193 crc32=455b603a` |
| onboarding `expected_step_26` | `SDL live test settled step=26 generation=4257 crc32=53d3f0c1` | `SDL live test settled step=26 generation=4256 crc32=53d3f0c1` |
| onboarding `expected_step_27` | `SDL live test settled step=27 generation=4260 crc32=17e1772c` | `SDL live test settled step=27 generation=4259 crc32=17e1772c` |
| onboarding `expected_step_28` | `SDL live test settled step=28 generation=4312 crc32=578e2601` | `SDL live test settled step=28 generation=4311 crc32=578e2601` |
| onboarding `expected_step_29` | `SDL live test settled step=29 generation=4318 crc32=1c62ab1a` | `SDL live test settled step=29 generation=4319 crc32=2a01c517` |
| onboarding `expected_step_30` | `SDL live test settled step=30 generation=4403 crc32=fb8e0155` | `SDL live test settled step=30 generation=4406 crc32=73d569a5` |
| onboarding `expected_stop` | `stop=halt pc=0x000727ca instructions=14178200857 virtual_time_ns=43790375389` | `stop=user pc=0x0800009e instructions=8500057344 virtual_time_ns=38818426902` |
| onboarding `expected_baseline_step_21` | `SDL live test settled step=21 generation=1928 crc32=8362b9bc` | `SDL live test settled step=21 generation=1927 crc32=8362b9bc` |
| onboarding `expected_baseline_stop` | `stop=compat-refused pc=0x0010fbde instructions=13181432148 virtual_time_ns=68142804403` | `stop=compat-refused pc=0x0010fbde instructions=13184192858 virtual_time_ns=68141114950 detail=layer sapporo-2.22-no-device trigger gps-awake-pulse exceeded budget` |

New assertions additionally pin step 31 (generation 4510, CRC `040ebb03`),
`user / 0800009e / 8500057344 / 38818426902 ns`, and idle
`budget / 000d4a8c / 8807319394 / 60041792981 ns`.

### Before/after full-log hash pins

| Runner / pin index | Before SHA-256 | After SHA-256 |
|---|---|---|
| `test_firmware_sapporo_235_block_erase.sh` #1 | `2813f2dfdad153de7f2f00250911d86cced75ec15ce01c73d86c822b4470a528` | `2d479fc3f6c4f059037e39c139cf939315e8bfdf3b5574166727e5f5dfcb37f8` |
| `test_firmware_sapporo_235_gps_awake.sh` #1 | `0fc177712c29f58a0b51a306db09538497bc0dd899164968f5019de6f96eb9fa` | `988c24e77f3b25b2be945f3d81a7f6ca3f17dfae94bd63cbb1eecb14533fbfe3` |
| `test_firmware_sapporo_235_gps_reopen.sh` #1 | `690422bf19ff32bb5de8da3db63a825ba11102a326c112dcbf30ab52e5e7c9ca` | `74dca7ea9589a670d9058b253e9f591ae50fb8281b633f1f40471f8653d40589` |
| `test_firmware_sapporo_235_gps_startup.sh` #1 | `f40ab5506257140b44724ad8522a515ffdc4a3bd98cf2af5d21dbe7b47f22284` | `7627f2125bbd7d5ebf644ec551b125c049b50f6f3d589a5922352ebb15fe7994` |
| `test_firmware_sapporo_235_gps_startup.sh` #2 | `94acff58c93ab408b34b4a5233670740d93464cbab739f02717f35eb5f62d878` | `d00ed24b150258249f030451d427247b6ce2226779b6f2de0ab79e1869ba46dd` |
| `test_firmware_sapporo_235_ohr.sh` #1 | `b6c35ad981bdceef823937e68fb57bdf101853e62c0ae52caf63fea586aabeb0` | `f69c9d8a7c176da98190eed09e14c0c14db32d282cc5ee4b62bbf2bb94201317` |
| `test_firmware_sapporo_235_ohr.sh` #2 | `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879` | `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879` |
| `test_firmware_sapporo_235_pressure.sh` #1 | `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba` | `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba` |
| `test_firmware_sapporo_235_pressure.sh` #2 | `c8b5c60d805fe012671575b9f850d4c42a94420d448e4bbdd0c3759fc7127f07` | `c8b5c60d805fe012671575b9f850d4c42a94420d448e4bbdd0c3759fc7127f07` |
| `test_firmware_sapporo_235_production.sh` #1 | `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba` | `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba` |
| `test_firmware_sapporo_235_production.sh` #2 | `0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81` | `0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81` |
| `test_sdl_live_input.sh` #1 | `9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c` | `278cc6dbc76e7359ff217c00821b9f4b08d3a7814435b11dc02bbc589a3d4d81` |
| `test_sdl_sapporo_235.sh` #1 | `523bbceff0a44a5e5eb64ba19e8bce2ca86bc10b2aa0d8ad7bbc50e157a4bdac` | `1bea6fedcaeee7f0fb5d47212ea40d8a2570455adbda948c4db6bcec178d3e5c` |
| `test_sdl_sapporo_235_scroll.sh` #1 | `c79d7bd489bba9f280b8f64e233f2ab796688258267e045709e51a0242a7ad6c` | `77402db41d8dc1260cb4226ce37edd618bd654a6b37c6c4dfd5f6be6c1b5c696` |

The pressure/production and OHR-disabled hashes are unchanged. The disabled
2.22 control gains a full-log pin
`07250eea92a667f06cc4e666289cd56e54effe6ad90ff1f4f6d52466ad62f5bb`
and exactly eleven GPS pulse admissions, strengthening its previous tuple-only
check. Old onboarding halt pixels are not accepted as success.

### Volatile raw-log identities

All paths below are relative to `/tmp/sapporo-789/`. Paired files sharing a
hash are exact byte matches; no timestamp normalization is used for pairs.
Firmware, snapshots and frame pixels remain outside Git.

| Artifact | SHA-256 |
|---|---|
| `after/222-disabled-2.log`, `after/222-disabled-1.log` | `07250eea92a667f06cc4e666289cd56e54effe6ad90ff1f4f6d52466ad62f5bb` |
| `after/222-live-1.log`, `after/222-live-2.log` | `278cc6dbc76e7359ff217c00821b9f4b08d3a7814435b11dc02bbc589a3d4d81` |
| `after/235-boundary-2.log`, `after/235-boundary-1.log` | `d00ed24b150258249f030451d427247b6ce2226779b6f2de0ab79e1869ba46dd` |
| `after/235-scroll-2.log`, `after/235-scroll-1.log` | `77402db41d8dc1260cb4226ce37edd618bd654a6b37c6c4dfd5f6be6c1b5c696` |
| `after/235-sdl-1.log`, `after/235-sdl-2.log` | `1bea6fedcaeee7f0fb5d47212ea40d8a2570455adbda948c4db6bcec178d3e5c` |
| `before/test_firmware_sapporo_235_block_erase/frame-2.log`, `before/test_firmware_sapporo_235_block_erase/frame-1.log` | `2813f2dfdad153de7f2f00250911d86cced75ec15ce01c73d86c822b4470a528` |
| `before/test_firmware_sapporo_235_block_erase/runner.log` | `15a469c287560b4e7642e332c58a36a1a2739d247c3b5e5910be91cd4c11a05d` |
| `before/test_firmware_sapporo_235_gps_awake/reopen-1.log`, `before/test_firmware_sapporo_235_gps_awake/reopen-2.log` | `0fc177712c29f58a0b51a306db09538497bc0dd899164968f5019de6f96eb9fa` |
| `before/test_firmware_sapporo_235_gps_awake/runner.log` | `627068abc80c291220d120148dd9748300dc76cc1717c1b3deefbfb3acf029b3` |
| `before/test_firmware_sapporo_235_gps_reopen/reopen-1.log`, `before/test_firmware_sapporo_235_gps_reopen/reopen-2.log` | `690422bf19ff32bb5de8da3db63a825ba11102a326c112dcbf30ab52e5e7c9ca` |
| `before/test_firmware_sapporo_235_gps_reopen/runner.log` | `4def79fccf31d1d39c9099f9b18144b901c801e133aa5b332475c9f6a324d150` |
| `before/test_firmware_sapporo_235_gps_startup/boundary-2.log`, `before/test_firmware_sapporo_235_gps_startup/boundary-1.log` | `94acff58c93ab408b34b4a5233670740d93464cbab739f02717f35eb5f62d878` |
| `before/test_firmware_sapporo_235_gps_startup/runner.log` | `0c65fa55e2b57c391f6f8045c6e9bd0dfe88b793afc1c5744258f4d62e683e42` |
| `before/test_firmware_sapporo_235_gps_startup/startup-1.log`, `before/test_firmware_sapporo_235_gps_startup/startup-2.log` | `f40ab5506257140b44724ad8522a515ffdc4a3bd98cf2af5d21dbe7b47f22284` |
| `before/test_firmware_sapporo_235_ohr/disabled-1.log`, `before/test_firmware_sapporo_235_ohr/disabled-2.log` | `b452c5669e536c0227a43b319807ad7ccc6fc9fe49fddaba9967051d0f7bd879` |
| `before/test_firmware_sapporo_235_ohr/enabled-2.log`, `before/test_firmware_sapporo_235_ohr/enabled-1.log` | `b6c35ad981bdceef823937e68fb57bdf101853e62c0ae52caf63fea586aabeb0` |
| `before/test_firmware_sapporo_235_ohr/runner.log` | `50e462f3e5a55b39bbe10aea620736bff84bbfc4b6e73a3bc80068b31297ead7` |
| `before/test_firmware_sapporo_235_pressure/boot-2.log`, `before/test_firmware_sapporo_235_pressure/boot-1.log`, `before/test_firmware_sapporo_235_production/boot-2.log`, `before/test_firmware_sapporo_235_production/boot-1.log` | `113607e088231e666455ab8e585af603b5fc2a4de7a571c93a23fa2c77ea35ba` |
| `before/test_firmware_sapporo_235_pressure/next-1.log`, `before/test_firmware_sapporo_235_pressure/next-2.log` | `c8b5c60d805fe012671575b9f850d4c42a94420d448e4bbdd0c3759fc7127f07` |
| `before/test_firmware_sapporo_235_pressure/runner.log` | `113276d03abfdf1828ccef2691f668929c86c02c468cb060b7bb07002def55fd` |
| `before/test_firmware_sapporo_235_production/next-1.log`, `before/test_firmware_sapporo_235_production/next-2.log` | `0ab8519944dce5e574e8cbb5b16d06bd798872204c5f602958c3c0cafabd2d81` |
| `before/test_firmware_sapporo_235_production/runner.log` | `a937de78906bfeab313048cdd5afa8554d15358df71fc16b79df7d8baa541e20` |
| `before/test_sdl_live_input/live.log` | `9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c` |
| `before/test_sdl_live_input/runner.log` | `aa444e32a0a1cd79aa7cd10724f00fa00051f065cd27ed323403a178a0986c01` |
| `before/test_sdl_sapporo_235/run-1.log`, `before/test_sdl_sapporo_235/run-2.log` | `523bbceff0a44a5e5eb64ba19e8bce2ca86bc10b2aa0d8ad7bbc50e157a4bdac` |
| `before/test_sdl_sapporo_235/runner.log` | `bb1a2a8017fb6650acd6d2d44dfa9a70383d9820ff98adcfa9e1cf06afe17708` |
| `before/test_sdl_sapporo_235_scroll/run-1.log`, `before/test_sdl_sapporo_235_scroll/run-2.log` | `c79d7bd489bba9f280b8f64e233f2ab796688258267e045709e51a0242a7ad6c` |
| `before/test_sdl_sapporo_235_scroll/runner.log` | `9a2be7f70e50d88f0b2764833ad07fed9f5832c9b4fac7b93b3773cee7bc756f` |
| `audit-before.py` | `6072161b9ee3f2a771b3438d14904ae2d5224b2514ec94583e40bc30b9583456` |
| `collect-after.py` | `6da0289884f2290750d5eda9b4a60e967b2b20a0704be800c5b2eb8c36459143` |
| `compare-censuses.py` | `a36cf39fec55a7f856c49a3e13ad7a8e5c90e8dda2341ad7ec032b28fa9f300a` |
| `pin-tables.py` | `c317210b2fe6747b089e8c9e640dbfc1354b42991c2a10278dbd6367996e6c13` |
| `verify-firmware.py` | `38f8f4debe40428aa21c1ae1a79ff9484fd0ba5727c3af76efcf3f65252b01c0` |
| `verify-sdl-prefixes.py` | `43b18efb01d5920d7df9c54b13d4c22fb38a3752ba492ebda02bc54e7ae48f74` |
| `verify-onboarding.py` | `fcad76cde067778640071693da826491ea475ca020ca856789cf5f94ba64f56c` |
| `engine-before.json` | `9f74d01f79d7a36bbf2be54d162f031b3a344e31dcbec0462d0eec01d6455919` |
| `before-results.json` | `c7302068a283de54c345967387311958bffca43a6563152e26e6a4fd3ad5fce5` |
| `after-results.json` | `4a24c0edfa786678c788bbc6c53b8b125bd281200af756c5b1c0f55df7aa76f8` |
| `event-census.json` | `fe2288bc5c9528dfe6eef6a783f99a5ef87690e565f279bd8010f1862c34a6f0` |
| `event-census.log` | `fe2288bc5c9528dfe6eef6a783f99a5ef87690e565f279bd8010f1862c34a6f0` |
| `idle-sdl-1.log` | `9ef28bb9e945d140e21b702b5c50e1b83f15fd994263da80747557a3d62d2074` |
| `idle-sdl-2.log` | `9ef28bb9e945d140e21b702b5c50e1b83f15fd994263da80747557a3d62d2074` |
| `step23.png` | `df34a74fb91b642d6dd4b970d2cc00f8f8ec587b9db38b1328f07340dcdc1c9c` |


### Bounded button input after snapshot restore

Two additional diagnostic resumes use the public CLI input replay and frame
callback, with no runtime modification or renderer-state injection. From the
E-CPU-F57F-001 step-31 snapshot, replay LOWER at 39000000000 ns and release
at 39100000000 ns, bounded by 10 billion total instructions / 41 virtual
seconds and a 120-second wall cap. The first paired SDL attempt publishes a
frame but its PPM diagnostic does not capture every frame outside live-test
mode; absence of PPM files is not absence of GPU output.

A standalone `frame-observer.c` calls `semu_cli_main` with a read-only frame
callback, saves the last RGB565-to-PPM conversion outside Git, and reports
publication CRCs. It produces 100 frames: first CRC `c1760fc8`, last
`0cb272ba`. Both full transcripts and final PPMs are byte-identical. The final
image visibly selects Media controls, with Navigation, Logbook, Timer and
Alarms also present. Both runs exit 3 at `budget / 000d4a8c / 8744080727 /
41088770901 ns`, matching the uninstrumented SDL stop tuple. The saved
snapshot had generation 4510; restored publication restarts at generation 1,
confirming a missing renderer generation even on this successful redraw.

This narrows the gap: explicit replay can provoke a usable redraw on this
menu, but idle restore lacks its initial image and frame-gated live input
cannot provoke that redraw. It does not prove missing inherited state/shadow
is harmless on other screens, nor authorize synthesizing input as a restore
workaround. No new guest hardware behavior is inferred from this probe.

```sh
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc/frontends \
  /tmp/sapporo-789/frame-observer.c build/obj/src/frontends/cli.o \
  build/libsemu.a -o /tmp/sapporo-789/frame-observer
SEMU_CAPTURE_PPM=/tmp/sapporo-789/observe-resume-1.ppm \
  /tmp/sapporo-789/frame-observer run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device \
  --snapshot-load /tmp/sap222-graph/fixed-walk-1.sems \
  --input-replay /tmp/sapporo-789/resume-lower.replay \
  --max-instructions 10000000000 --max-time 41000000000
# Repeat with suffix 2. Uninstrumented controls use build/suunto-emu-sdl,
# SDL_VIDEODRIVER=dummy and the same arguments/budgets.
```

| Artifact relative to `/tmp/sapporo-789/` | SHA-256 |
|---|---|
| `frame-observer.c` | `6e0c028df99dd98b6c5d689fa2d34ef63de518e33f1e84e777617203d474b27e` |
| `resume-lower.replay` | `537a91acb17872eb6911ea047844de7ae5a239793b4b2bdeba1e3aeecc31dd74` |
| `resume-lower-1.log` | `54aac2ca2bba1a90ba9383a3c68c8ac7ff0a5bd0c3e7d93fc2c2eaff4add7bcf` |
| `resume-lower-2.log` | `54aac2ca2bba1a90ba9383a3c68c8ac7ff0a5bd0c3e7d93fc2c2eaff4add7bcf` |
| `observe-resume-1.log` | `bd210eb30645da96fce0fcfd0ab75810456d4932831b4c2516af63c3a7ee8f04` |
| `observe-resume-2.log` | `bd210eb30645da96fce0fcfd0ab75810456d4932831b4c2516af63c3a7ee8f04` |
| `observe-resume-1.ppm` | `f085ab8896e22e910ffaeb1191113369179bb10f0d5cada2807193fdc567794f` |
| `observe-resume-2.ppm` | `f085ab8896e22e910ffaeb1191113369179bb10f0d5cada2807193fdc567794f` |


### Ticket 789 final verification and handoff

All eleven named runners exit 0 twice on the final scripts. Both 2.35 Make
runs select all seven firmware runners; each itself compares paired native
transcripts. All four SDL runners pass twice; their configuration/refusal
checks are preserved, and the 2.22 onboarding script adds exact idle and
post-setup input assertions. The original control's ten runners also pass.
No new unsupported behavior was turned into a passing golden.

Exact final commands: the five authentic firmware/SDL invocations run twice
with 900-second wall caps; build/static checks run once, with final diff checks
repeated after documentation edits:

```sh
make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235 \
  SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu \
  sh tools/test_sdl_live_input.sh
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu \
  sh tools/test_sdl_onboarding_completion.sh
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu \
  sh tools/test_sdl_sapporo_235.sh
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu \
  sh tools/test_sdl_sapporo_235_scroll.sh
make sdl
make check
make check-lines
make check-task-contracts
git diff --check
```

All listed commands exit 0. `make check` has 985 PASS records and 154 task
contracts; its SDL quick target intentionally skips private walks, which the
explicit commands above supply. Line checks produce advisory warnings only.
All eleven shell runners pass `sh -n`. All 334 files under `src`, `include`
and `profiles` have identical before/after SHA-256 inventories (inventory hash
`9f74d01f79d7a36bbf2be54d162f031b3a344e31dcbec0462d0eec01d6455919`).
Sanitizers are not repeated for this shell/documentation ticket; the unchanged
engine retains E-CPU-F57F-001's 980 passing sanitizer cases.

Changed files: `tools/test_sdl_live_input.sh`,
`tools/test_sdl_onboarding_completion.sh`, `tools/test_sdl_sapporo_235.sh`,
`tools/test_sdl_sapporo_235_scroll.sh`; the five
`tests/integration/test_firmware_sapporo_235_{block_erase,gps_startup,gps_reopen,gps_awake,ohr}.sh`
runners; `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`.
The pressure/production runners and their pins remain unchanged. Every old
pin is recorded above. Ticket status remains ready for integrator review.

Remaining gaps: the renderer-snapshot interface/format work specified above;
2.35 device/layer snapshot coverage; unbounded GPS; 2.35 compressed main icons;
broader main functions and other-profile coverage. No new 2.39 era run is
claimed; its previously recorded drift remains owned by 777/783. This ticket
adds no engine behavior, so it introduces no additional CPU/device era drift.

Final validation log and original-runner identities, relative to
`/tmp/sapporo-789/`:

| Artifact | SHA-256 |
|---|---|
| `check.log` | `403e0066a04fcbc60b68fc296da41b91fafeabbab4b76ae6b2c389f991e20e6e` |
| `make-sdl.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| `lines.log` | `30f1f1df34c0c2e7fff4d2af41e719f792bfafd32933ff0c448d37aec5ae699e` |
| `contracts.log` | `0b2072a03f6c4a0a523d87b99ceb9d11d51fb4cdf1ad6753e7e140b52f305f69` |
| `verify-firmware-1.log` | `4af13c88189146fd7434fac06a93054a194c74dd84e1e8b55fd699c65362cbeb` |
| `verify-firmware-2.log` | `4af13c88189146fd7434fac06a93054a194c74dd84e1e8b55fd699c65362cbeb` |
| `verify-firmware-results.json` | `7211ad359c55dcd75c38c53c5da3fe7c80724fec884b8ac9042a261f3e14caa4` |
| `verify-sdl-results.json` | `4fb1475c12d64e5748b323cc259c4846773adc1f4921b9c21dddd196f165aba4` |
| `verify-onboarding-1.log` | `3b28cac7dad4f4fa7d92d2625f3218e8aab9b64fde67d9908931cb6c3ff6cd93` |
| `verify-onboarding-2.log` | `3b28cac7dad4f4fa7d92d2625f3218e8aab9b64fde67d9908931cb6c3ff6cd93` |
| `verify-onboarding-results.json` | `7211ad359c55dcd75c38c53c5da3fe7c80724fec884b8ac9042a261f3e14caa4` |
| `verify-test_sdl_live_input-1.log` | `599e4fbec468555b97ddcf853324e7de4fe39447a3672ece445a03af1ba6dd7f` |
| `verify-test_sdl_live_input-2.log` | `599e4fbec468555b97ddcf853324e7de4fe39447a3672ece445a03af1ba6dd7f` |
| `verify-test_sdl_sapporo_235-1.log` | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |
| `verify-test_sdl_sapporo_235-2.log` | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |
| `verify-test_sdl_sapporo_235_scroll-1.log` | `b3da4b332833d04ee702b7c045ef251c8b46800a7744301666c02f901bf18fa2` |
| `verify-test_sdl_sapporo_235_scroll-2.log` | `b3da4b332833d04ee702b7c045ef251c8b46800a7744301666c02f901bf18fa2` |
| `original-runners/test_firmware_sapporo_235_block_erase.sh` | `8b33ed882f8d514c7d1597bf31531ee10123656e43558b3f0681a49cfdd85ba9` |
| `original-runners/test_firmware_sapporo_235_gps_awake.sh` | `6187f8064cdda5faec5fb76ae6d68a4a097dd7d50fe4c634f5d436cab906f1e9` |
| `original-runners/test_firmware_sapporo_235_gps_reopen.sh` | `7bbfe4a2e8ae0d27cb5d14a59263701ae567d0c04ffb6785a486376f8959f963` |
| `original-runners/test_firmware_sapporo_235_gps_startup.sh` | `10e016ee38fff0bc38896eb56832f34e00aac7a56daf8453366d43dae25d547c` |
| `original-runners/test_firmware_sapporo_235_ohr.sh` | `de755efa47f9058a34b904502283ad31fe3c5f1e64df88ca7e27b6b262404ab0` |
| `original-runners/test_firmware_sapporo_235_pressure.sh` | `4b2fc8bdebc8f30759a9548718897e21c02455710cd71bd799ae7d230de05df8` |
| `original-runners/test_firmware_sapporo_235_production.sh` | `813281dad39356f865d9d8dff0ac0249cec521f29a62f692908edba8700423d0` |
| `original-runners/test_sdl_live_input.sh` | `97e165a451905f71f1ab64a43afa4c02cf0e04ee29f77c5a2f56404ff606ae8d` |
| `original-runners/test_sdl_onboarding_completion.sh` | `ff4ff14068ec31f263a59d019a26fb2da74d438fb658f3d32a2789d152ba0654` |
| `original-runners/test_sdl_sapporo_235.sh` | `20be0e3e432c3afa864976fcb8cb81c06d2a47bc6558f46a8877ac037ec21a6f` |
| `original-runners/test_sdl_sapporo_235_scroll.sh` | `46ceccae4c47f9435448718fc64e9defbb6da520d8f0cd9a2f82ef27866a3ee9` |


## E-EMU-RENDERER-SNAPSHOT-001 — complete renderer restoration (ticket 791)

Date: 2026-09-22. Scope: persistence and host presentation, authorized by the
existing display ownership/transaction architecture; no new hardware behavior
is inferred. References: E-EMU-NEMA-ATOMIC-001 and
E-EMU-SAPPORO-BRANCH-GATES-001. Dependencies 513, 615 and 761 are done.
Ticket 791 owns the public codec contract and version-2 format integration;
its index status remains ready for integrator review.

### Regression and implementation

The original machine-only snapshot restores no renderer frame. The new
`test_machine_restores_published_renderer` failed before implementation at
`1u == cb.count` (zero callbacks), then passed after integration. The public
machine option copies a save/load/published-frame table tied to the existing
backend context. Display section 10 records backend identity and the bounded
little-endian image specified in `docs/execution-model.md`. A backend without
persistence support refuses; absent backends use identity zero. Old version-1
files refuse with `snapshot: unsupported version 1`, CLI exit 2.

The backend preserves inherited registers/presence, list/draw counters,
working RGB565 pixels, last published RGB565 pixels/generation, and the TSC6A
shadow. Last published pixels are distinct from later inline drawing pixels.
No diagnostic history, transient transaction storage, host pointer, or callback
is encoded. Full component validation precedes an allocation-free load.
Malformed identity/length/version/dimensions/flags/presence and active
transactions refuse. No new rasterization, compressed decoding or compatibility
intervention is introduced.

Synthetic tests compare continuation with an uninterrupted backend across
inherited quad draws, inline pixels and a real shadow triangle/resolve. They
also verify unchanged output arguments on busy save, reset with no published
image, copied codec tables, and whole-machine byte identity after a failed
load. An orphan scheduler event fails AFTER renderer loading and exercises
rollback of the changed renderer and guest state; refusal emits no frame.
Successful load republishes exactly once without executing an instruction,
incrementing generation, or advancing virtual time.

SDL accepts any button at restored checkpoints and flushes the held restored
frame before waiting. It also releases the final held composite at settled
input checkpoints, where guest time is paused. This affects host presentation,
not guest frame generation, commands or scheduling. Native live inspection
using Computer Use showed the restored Logbook selection, changed menu
selections, and the native `LOGBOOK EMPTY` page after interaction. UI artifacts
stay outside Git; key/click automation timing is not a deterministic golden.

### Paired authentic census

Firmware is the unchanged identity-pinned 2.22.60 bundle from the preceding
entry (resident `a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522`,
application `c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`,
resources `ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1`).
All authentic artifacts are volatile under `/tmp/sapporo-791/`.

Two cold version-2 runs use setup POST `mlllmlllmmlmmmmm`, timeline `30000:l`,
`--until setup-next`, maximum 18,000,000,000 instructions and 60,000,000,000 ns.
Both retain the preceding cold log hash, native stop `user`, PC `0800009e`,
8,500,057,344 instructions, time 38,818,426,902 ns, and menu generation 4510,
CRC `040ebb03` (Logbook). Both snapshots are 5,952,483 bytes and byte-identical.
Display payload is 1,152,184 bytes including the four-byte backend ID.

Two idle restores publish generation 4510 / CRC `040ebb03` immediately, then
reach the SAME previous budget stop: PC `000d4a8c`, 8,807,319,394 instructions,
60,041,792,981 ns. The restored frame line is the sole difference from the
previous idle transcript: old SHA-256
`9ef28bb9e945d140e21b702b5c50e1b83f15fd994263da80747557a3d62d2074`, new
`0e21563113837f9d0d27c2e13c53f8746e23f601041689542cd4c00cff16eceb`.
Only that restore-specific pin changes in the onboarding runner.

Two LOWER replays press at 39,000,000,000 ns and release at 39,100,000,000 ns,
with maximum 10,000,000,000 instructions / 41,000,000,000 ns. Both finish at
budget, PC `000d4a8c`, 8,744,080,727 instructions, 41,088,770,901 ns, with
current and published frame CRC `0cb272ba` (Media controls), generation 4610.
The complete resulting machine snapshots are byte-identical. This preserves
the previous pixel/CPU checkpoint while fixing generation reset and initial
blank presentation. No replay is needed to display the initial saved menu.

Exact paired cold command (substitute pass 1/2 in the external output path):

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
  SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
  SEMU_SDL_SETUP_WALK_TIMELINE=30000:l build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --max-instructions 18000000000 --max-time 60000000000 \
  --snapshot-save /tmp/sapporo-791/cold-1.sems
```

Each cold/idle/LOWER capture uses a 900-second `subprocess.run` wall cap.
The new `tools/test_sdl_snapshot_restore.sh` independently checks a cold-created
snapshot, initial publication with execution budgets already exhausted, and
paired LOWER continuation, with 900-second per-process wall caps. Supplied
`SEMU_SDL_TEST_SNAPSHOT` must match the exact fresh version-2 checkpoint hash.
No snapshot is migrated by patching an old version.

Raw artifact identities (same-named numbered pairs were compared bytewise):

| Artifact | SHA-256 |
|---|---|
| `before-regression.log` | `49ba51ede2f1dac98ac5b5850f65eede85e7f7dac31eb4eca0b0eb269cbf03c2` |
| `after-regression.log` | `f7c15e00a9be5557061f484e40f2b927c096512121d53b5a062d494917a6f1cd` |
| `renderer-final.log` | `1bcbdd706868c54668715dd7b0531c0547c9ddf7ed9a63308d4a286d666f0eb3` |
| `snapshot-tests.log` | `f4589bc11e76484648278a240cf8e9905b608cb53bf152c4c677efcea423316e` |
| `cold-1.log` | `7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa` |
| `cold-2.log` | `7186e3eb3f16474294628d6753932f9635c8a3ce2a7fc8cb66138cabf831eafa` |
| `cold-1.sems` | `f829b2fa514c65b0e10d1f7faa20f4564ba95a442ffd8d217fa21b28b711e592` |
| `cold-2.sems` | `f829b2fa514c65b0e10d1f7faa20f4564ba95a442ffd8d217fa21b28b711e592` |
| `idle-1.log` | `0e21563113837f9d0d27c2e13c53f8746e23f601041689542cd4c00cff16eceb` |
| `idle-2.log` | `0e21563113837f9d0d27c2e13c53f8746e23f601041689542cd4c00cff16eceb` |
| `lower-1.log` | `163b8d0e752e1f0f817f9d887760316377388af12a973a91e9dfdd393cd67c7d` |
| `lower-2.log` | `163b8d0e752e1f0f817f9d887760316377388af12a973a91e9dfdd393cd67c7d` |
| `lower-1.sems` | `e5c5dd57e7ad7b7ba1941f2b1bdd8123d42e6ab286ed152d55ecd38f885a9ab6` |
| `lower-2.sems` | `e5c5dd57e7ad7b7ba1941f2b1bdd8123d42e6ab286ed152d55ecd38f885a9ab6` |
| `version1-refusal.log` | `b74b683d3e6a1f6c60743138710804b6aacb063886a88b96d9c650e4e0417cb8` |
| `sanitize.log` | `999960d5a5d0532f9d492ce435117241e556c1af3972df4effa4f05bdd754fcc` |

The public CLI frame observer from `/tmp/sapporo-789/frame-observer.c` was
rebuilt against the new library into `/tmp/sapporo-791/frame-observer` and run
twice with the same LOWER replay and bounds. Each emits 101 callbacks: restored
4510 / `040ebb03`, then 4511 through 4610, ending `0cb272ba`. Both final PPMs
match the earlier continuation pixel hash exactly. The observer only records
callbacks; it does not inject frames or alter renderer state.

| Artifact | SHA-256 |
|---|---|
| `/tmp/sapporo-789/frame-observer.c` | `6e0c028df99dd98b6c5d689fa2d34ef63de518e33f1e84e777617203d474b27e` |
| `/tmp/sapporo-791/frame-observer` | `d94aa9781a48632696bd237835ef7c71f61434e89695f7b756de19671b492d3b` |
| `/tmp/sapporo-791/frames-1.log` | `bbe49601d14d1c10154f235bac90cdfc81431d78ac6d75b91a310a490d6804cf` |
| `/tmp/sapporo-791/frames-2.log` | `bbe49601d14d1c10154f235bac90cdfc81431d78ac6d75b91a310a490d6804cf` |
| `/tmp/sapporo-791/frames-1.ppm` | `f085ab8896e22e910ffaeb1191113369179bb10f0d5cada2807193fdc567794f` |
| `/tmp/sapporo-791/frames-2.ppm` | `f085ab8896e22e910ffaeb1191113369179bb10f0d5cada2807193fdc567794f` |


### Verification commands and integration handoff

```
make test TEST_FILTER=renderer_snapshot
make test TEST_FILTER=snapshot
make check
make sanitize
make sdl
make check-lines
make check-task-contracts
make test-firmware TEST_PROFILE=sapporo-2.35.34 SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_onboarding_completion.sh
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_snapshot_restore.sh
sh tools/test_sdl_live_input.sh
sh tools/test_sdl_sapporo_235.sh
sh tools/test_sdl_sapporo_235_scroll.sh
git diff --check
```

The renderer's four cases pass, all 276 snapshot-selected cases pass,
`make check` has 989 PASS records (including five SDL input cases), and the
full sanitizer suite has 984 passing cases. The final renderer cases also
pass separately under ASan/UBSan. All seven 2.35 firmware runners and both
paired 2.35 SDL prefixes pass without changed pins. The 2.22 SDL live-input
gate passes. Line checks remain advisory; 155 task contracts validate.
Unselected 2.39 runners explicitly skip under the 2.35 profile; their existing
drift is not claimed fixed and no pin was changed. The 2.35 Make runner has
explicit guest budgets; SDL prefix subprocesses have 900-second wall caps.

Implementation/header changes: `include/semu/{display,machine,trace}.h`;
`src/boards/{machine.c,machine_internal.h,machine_snapshot.c,machine_snapshot_display.c}`;
`src/display/{nema_backend.c,nema_backend.h,nema_backend_internal.h,nema_backend_transaction.c,nema_backend_snapshot.c,nema_state.c,nema_state_internal.h,surface.c}`;
`src/frontends/{cli.c,main_sdl.c}`. Tests/runners:
`tests/unit/{test_renderer_snapshot.c,test_snapshot.c}`,
`tools/{test_sdl_onboarding_completion.sh,test_sdl_snapshot_restore.sh}`.
Documentation: README, current-status, migration-evidence and execution-model.
Planning adds ticket 791 and its index row before implementation; no existing
status or dependency is modified. All unrelated working-tree changes remain.

Unsupported cases remain: version-1 migration, 2.35 device/layer snapshot
codecs, unsupported compressed TSC6A main icons, unbounded GPS, and broader
watch-function/other-profile validation. The existing 2.39 era drift remains
777/783's work. Integrator action: review 791's API/format ownership and evidence,
then update its status; no private parallel interface or firmware workaround
is requested by this implementation.

Additional terminal validation identities, relative to `/tmp/sapporo-791/`:

| Artifact | SHA-256 |
|---|---|
| `era-235.log` | `e40c65ec10a96789749203be0b00ed8a1bc9a80ff5a7fabcdb5d852aacb38f47` |
| `test_sdl_live_input.log` | `599e4fbec468555b97ddcf853324e7de4fe39447a3672ece445a03af1ba6dd7f` |
| `test_sdl_sapporo_235.log` | `529b27af7a24a9f4b054ebe1f670613601bdbcb186da331cc64ac810891191d9` |
| `test_sdl_sapporo_235_scroll.log` | `b3da4b332833d04ee702b7c045ef251c8b46800a7744301666c02f901bf18fa2` |
| `sanitize-renderer-final.log` | `73bc4d4684b9760b585a2349a327b9215fcd8b3e595c197c21d9ee556c216811` |
| `contracts.log` | `9a518e8984ccd42e9a0c0e752c0c6cc0f92f4c93e2349266f0e747021a34b904` |
| `lines.log` | `11916af63f91489e5498d680941eecdefc89076df6d77c0d05c77ae04efcb5f6` |
| `restore-supplied.log` | `a66ba4cb041918aa91c4fcf3cabaf6630c666a0db81eb09f2cb5b53defd67685` |
| `onboarding-1.log` | `3b28cac7dad4f4fa7d92d2625f3218e8aab9b64fde67d9908931cb6c3ff6cd93` |

The full new restore runner exits 0 after cold capture and both immediate/LOWER
restore pairs. Its cold-only setup environment is confined to that subprocess.
The supplied-snapshot path also exits 0. Final generic checks pass again after
public contract documentation and the copied-table constructor regression.

| Artifact | SHA-256 |
|---|---|
| `restore-accepted.log` | `a66ba4cb041918aa91c4fcf3cabaf6630c666a0db81eb09f2cb5b53defd67685` |
| `check-final2.log` | `830d8e29c3fccf69436f7a404706fb4e0a9c2fe9cd19d6e546c1ba2957e0957d` |
| `snapshot-final.log` | `577732a2da84f70d0a6e663be02a0f013881a4067209729b569192222d8a66aa` |
| `live-initial.png` | `04afdd3d7ae620a6492a6d93f9c6add5e8633afe06c3d0d98de5f8ccac318269` |
| `live-logbook.png` | `cf895c9ad679560a30d6c4c1c637212b59d6803b53da88e991ac5cb40e5f7751` |

Both complete onboarding runner invocations finish with exit 0 and identical
output, including the unchanged eleven-pulse disabled-control refusal. These
three-stage runners were supervised with a 2700-second whole-script cap; the
independent paired cold/idle/LOWER captures and the new restore runner use
900-second per-emulator caps. Every native run also has explicit guest bounds.
No acceptance stop, pixel hash, instruction/time pin, or cold transcript was
weakened. Only the restore-specific idle log hash changed.

Final checks: `make sdl`, `make check-lines`, `make check-task-contracts`, both
runner syntax checks and `git diff --check` exit 0. No firmware-derived assets
are present in the Git change list. The live SDL window remains available;
its host UI proof does not establish untested watch functions or 2.35 main.

| Artifact | SHA-256 |
|---|---|
| `onboarding-1.log` | `3b28cac7dad4f4fa7d92d2625f3218e8aab9b64fde67d9908931cb6c3ff6cd93` |
| `onboarding-2.log` | `3b28cac7dad4f4fa7d92d2625f3218e8aab9b64fde67d9908931cb6c3ff6cd93` |
| `lines-final.log` | `ae723a501293fc63635625edd043a22c876a3c8172dd93382d399ea3e8a22d6b` |
| `contracts-final.log` | `9a518e8984ccd42e9a0c0e752c0c6cc0f92f4c93e2349266f0e747021a34b904` |
| `sdl-final.log` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |


## E-SAP-0049 — lane-observed GPS awake cadence extends to 64 admissions

2026-09-23, ticket 710 instance-9 (2.35.34 sustained-session push, owner-directed).
E-SAP-0048 observed nine poll boundaries with eight injected pulses at a
60-second virtual cap. The next observed frontier is that boundary itself: the
zero-pulse control's retry/assert path and the positive probe's untested ninth
admission. This instance's probes are E-SAP-0048's exact script (same
`0x1259fe` awake-poll hook, same four status responses, same controlled-flash
experiment, no firmware instruction/register/state write and no source-lane
edit) with the admission ceiling `n < 8` raised to `n < 16` and `n < 64` and
the virtual cap raised to 120 s and 430 s.

Commands from `/Users/cyril/projects/suunto-firmware` (needs the .NET bundle
cache and Renode's `config.lock`; both are host-tool artifacts, not lane state):

```sh
.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt \
  /tmp/sap235-awake2/pulse-16.resc
.tools/renode/Renode.app/Contents/MacOS/renode --console --disable-xwt \
  /tmp/sap235-awake2/pulse-64.resc
```

E-SAP-0048's normalizer law (its `census.py`, unmodified regex) is applied
here as `census16.py`/`census64.py`. External artifacts, SHA-256:

| Artifact | SHA-256 |
|---|---|
| `pulse-16.resc` | `ea19e085748617b057600a3f550147fde0de440c619d0722b57240de6aeb50f8` |
| `pulse-64.resc` | `5630d9df97a593ad50123cbcbbc9f99586b445a9942ccbbb1806f18291ddf1e3` |
| `pulse-16-1.log` | `7f0386d970334d5f770bc579180a93dfceb59938f1eec2d0b1f5a7702d555f8c` |
| `pulse-16-2.log` | `e27cdcebb682f145049228205bb06a0d0a1f694eb1e42accfa9f4a2ab1333d0a` |
| `pulse-16-1.census` | `9325efbe914140d1867a9178c4d88946dec25c1fcb03c7fd1fbafe2b36edbc69` |
| `pulse-16-2.census` | `9325efbe914140d1867a9178c4d88946dec25c1fcb03c7fd1fbafe2b36edbc69` |
| `pulse-64-1.log` | `5f29837ff352dda55ca4e1c6ce4d1f5c9ccb646729935ed03a75944a441efadf` |
| `pulse-64-2.log` | `8c7b79519492840b6b3459208e01d02e456b663173a0fcd2260488248e852eac` |
| `pulse-64-1.census` | `20d856ad1ed65caa80365206747063d4461666564d20521b45cd7ac75333a782` |
| `pulse-64-2.census` | `20d856ad1ed65caa80365206747063d4461666564d20521b45cd7ac75333a782` |
| `census16.py` | `a0a4cae41045227b6271825c8c0ec83b4fe07eadaea306c94a32bc7b516bf3bd` |
| `census64.py` | `2665484b669c5ad769d77734479025d6c3d6a991134bc69f4bf053ca34425797` |

Derived census. 16-run (222 rows, pair identical): sixteen successful
`AWAKE_INJECT ordinal=1..16` admissions, each with the invariant
`state=12 pending=10 flags=1,0,0 config=00000093` and the fixed
`GPS_SCHEDULE delay=5500` cadence; after the sixteenth admission the native
poll boundary re-enters three times (`ordinal=17` rows) with `flags=0,0,0`
and state descending 12 to 4, the driver arms with `state=4 pending=2`,
transmit grows to exactly `@VER @GSR @GSTP @GSR` (25 TX bytes) and the run
ends at the 120-second cap with PC `0xe1862`, no assertion. 64-run (564 rows,
pair identical): sixty-four admitted admissions with the same per-poll
invariant and the same 5500 ms cadence through ordinal 64; after the
sixty-fourth, `ordinal=65` re-enters with `flags=1,0,0` then `flags=0,0,0`
and state 12 then 4, the retry cycle reaches
`SAP_ASSERT_DETAIL file=CXD5610GF-driver.cpp line=894` with the same
`00079424 / 0012572d / 1002f7d0` tuple as E-SAP-0048's negative control.
There is no state, cadence, or register difference between admission 9 and
admissions 10 through 64; the observed law is cadence-invariant across the
whole observed range, and exhaustion behavior at 65 equals the E-SAP-0048
nine-boundary exhaustion path.

Interpreter integration (this instance): `sapporo-2.35-gps-awake`
`maximum_hits` 8 → 64 with the predicate and refusal paths unchanged; the
budget 64 is a hash-pinned observed ceiling, not a recurring timer or a
physical-cadence claim. Unit module asserts sixty-four admits plus refusal on
the sixty-fifth, hit/level/time preservation, the 36-case refusal matrix at
`hits=64`, reset cancellation, and the machine-level stop. Bounded firmware
runner `tests/integration/test_firmware_sapporo_235_gps_awake.sh` re-derived:
the bounded 3B/70s window now ends `stop=budget pc=0x000e1862
instructions=1860847385 virtual_time_ns=70000000000` with eleven hits and
transcript SHA-256 `17cc9087bf600002960d5d18d13517ec4a868c3186720ce3532ca9ad82a20794`
(paired `cmp` identical); the old ninth-admission refusal pin
(`1638733422 / 54660679480`) is superseded by observation, not weakened: the
65-admission refusal remains fail-closed in the unit module. SDL scroll gate
`tools/test_sdl_sapporo_235_scroll.sh` transcript pin re-derived to
`1d44ea9814417241f8a66f604c093edad61545c4947856b6eb8af915d7b7e23a`; a
paired run with only the old layer file differs solely in three `layer-hit`
metadata rows (`maximum=8`/`E-SAP-0048` versus `maximum=64`/`E-SAP-0049`);
guest pixels, generations, `crc32=f0ff828c`, stop
`4961334596 / 22000000000` and every other row are byte-identical.

New observed frontier (records the next gap; no behavior authorized yet):
paired cold 5-layer runs to 26B instructions / 400 seconds
(`/tmp/sap235-awake-impl/extended2-{1,2}.log`, transcript
`0cd3308a5242038fca5dbee520e7ee06f1b4ffa2551720c9af9386c1eeff3ea5`, pair
identical) now deliver fifty-five admissions with invariant cadence through
`virtual_time_ns=306707503135`, then the session becomes instruction-bound
at PC `0x000ccac4` at `26000000000 / 328673254682`. After the last awake
admission the guest executes about thirteen billion instructions — half the
whole 26-billion budget — across the next twenty-two virtual seconds, roughly
14 times the mean rate of the first 307 virtual seconds; the scheduler keeps
advancing virtual time with no reset, no assertion, and no device refusal. The lane's
paired `pulse-64` census shows the same clock state clean at the 430-second
cap, so this is an interpreter-side high-rate region to name with a bounded
slice census (next ticket-710 instance), not an awake-cadence gap and no
throttling, park, or clock guessing is authorized by this entry.

Verification: `make check` 989 PASS / 0 FAIL; focused awake module 4/4;
awake firmware runner passes its re-derived pins; the scroll pair passes with
the attributed metadata-only diff. 2.39 era gates were not re-run here (no CPU,
scheduler, bus or device law changed; only this profile-pinned fixture
budget); their pins are unaffected by this change by construction and ticket
783/777 remain the owners of any real era drift.


## E-RE-SAP235-TSC6A-001 — offline RE derivation of the format-17 (TSC6A) block law

2026-09-23, ticket 788. First application of the owner-authorized offline
reverse-engineering evidence class (AGENTS.md Lane Oracle human decision,
2026-09-23): the lane refuses to model this draw (E-EMU-SAP235-MAIN-TSC6A-001
keeps standing as the machine-observed refusal boundary), so the block law was
derived by static analysis of the hash-pinned 2.35.34 resource partition and
the captured refused draw, cross-checked against public vendor documentation
and US 9,640,149 B2. No lane file or firmware byte was modified; no firmware
or asset bytes are recorded here — only layout facts, counts and hashes.

Inputs and tooling (SHA-256):

| Input | SHA-256 |
|---|---|
| `tests/private/sapporo-2.35.34.18929/resources.raw` | `f281385acc8bab169976f9e506c230fd25124c44bfdbe2048393763a7d85ae22` |
| refused-draw capture `source.bin` (capture-1 = capture-2) | `b403fb2454fe9ed8206ad3e5cc79af7789f272450527b7fe76a480ed8c44cdee` (first 2700 B: `f311e1ef2267f07528ce18403f1601ce4b51e7a897b516bcf540b773af8ab371`) |
| refused-draw capture `before.bin` | `25eadc1a96223d4b4c67b5a2257e17e6d8773ab184d17ece6889d8cea15c6340` |
| `apollo4plus-datasheet.pdf` (§21.3.4.18) | `e0c60833dc4573fcae6fde886d059238f00a40599d3a3e74283ac62e662710e5` |
| `Pixpresso_Starting_Guide.pdf` | `124d548094f5dd2933b13d69fc75da02343d75f1a2503c80e2d8e3bfd6247414` |
| `NemaGFX_API_Manual.pdf` | `0aaa65a1b1db013b414fa97df31b0cf4f28b00fe95798e83cee2dc5c859f1043` |
| `US9640149.pdf` (Think Silicon, image compression) | `856ff7b2ed0d04b7cdf8cf907a5f02ccb1b76b412a0249a926c789e7b1bc5991` |
| `/tmp/sap235-tex17/decoder.py` (stdlib Python) | `9face3a6c37563d02f9a9173315c5095a2335e12325aa969d5eacca2a974b875` |
| `/tmp/sap235-tex17/scan_layouts.py` | `76f2cff67a11f30b60bcdf7b1a762cbd77cf2ca51e79616db97f5fee895c434d` |
| `/tmp/sap235-tex17/solve_endpoints.py` | `599a2c52110cb694259ec18084c99a017f7dc9250eb232f04e0ca9d8031a260c` |
| `/tmp/sap235-tex17/DERIVATION.md` | `1fd79c0a92345e84b74b08d895fe9583ec8f2a5c7e20045d7aad3e87f3d806b9` |
| `/tmp/sap235-tex17/INTEGRATION-NOTE.md` | `2c4e15cf908b60af7899176be8cd70766f1e46d9997697cc3fe74dc6261c4fb9` |

Format identification: four independent public sources agree the ticket's
"format 17" is TSC6A — datasheet §21.3.4.18 "TSC6A 16-pixels with Alpha /
96-bits … Value: 0x17"; Think Silicon `NEMA_TSC6A 0x17U`; `Imgfmt::TSC6a = 23`;
PixPresso "4x4-pixels block in … 96-bits (TSC6 and TSC6a)". None prints the
bit layout; the layout below is data-derived.

Block law (12 bytes per 4x4 pixels; block-row advance `ceil(w/4)*12` —
capture stride 180 = 15·12 ✓; bit0 = LSB of byte0):

- bits 0..31 — sixteen 2-bit color indices, pixel p = 4r+c raster, index p at
  bits 2p..2p+1.
- bits 32..47 — endpoint E0, bits 48..63 — endpoint E1, RGBA4444-packed
  (top three nibbles ×17; R duplicated into the A nibble in 94.7% of corpus
  blocks).
- bits 64..74 — 11-bit alpha, constant per block, scaled ×255/2047
  (0x7FF opaque, 0 transparent).
- bits 75..95 — auxiliary region: zero in every block of the pinned capture,
  nonzero in ~63% of corpus blocks; semantics UNVERIFIED — any decoder built
  on this entry must fail closed when it is nonzero.
- colors: four quantized points in thirds, idx0=E0, idx1=(2E0+E1)/3,
  idx2=(E0+2E1)/3, idx3=E1 (patent eqs. 12-19 four-point mode). The
  reference decoder's exact integer operation (authoritative for golden
  matching, verified in `decoder.py` after this entry was written):
  `(2*E0.ch + E1.ch + 1) // 3` and `(E0.ch + 2*E1.ch + 1) // 3` per channel —
  round-to-nearest integer division, not floor.

Derived census (twice reproduced; independent re-run by the integrator):
`decoder.py` on the capture's first 2700 B twice →
`/tmp/verify-decode-a.bin` = `decode-1.bin` = `decode-2.bin`, SHA-256
`2f30fe186ba7edd5ef39498f4ad69736684a1611ee1f5d581f05b2afba756b2f` (60x60
RGBA8888, 14400 B); census texts `ccbb5db2af4b59a88699fdec098ae1e41f06da03f3d24d3b677fdd5ed3e24833`
(both runs). Output: 12 unique RGBA colors; 225/225 blocks expand to exactly
2 distinct colors (capture uses idx∈{0,2}); alpha set {0,255}; silhouette is
exactly a 4-armed 4-px-wide crosshair with arms reaching all four borders
(16 of 240 border pixels opaque) around an empty 36x36 center — the expected
compass icon. Index-field law is additionally CONFIRMED by exact icon
symmetry: `grid(C)=transpose(grid(A))`, `grid(E)=hflip(grid(D))`,
`grid(F)=vflip(grid(B))` of the decoded 2-bit grids. Alpha law by corpus
mass: 46.0% of 22,296 blocks exactly 0x000, 31.5% exactly 0x7FF (only an
11-bit full-scale-2047 reading concentrates both), boundary-coherence winner
0.758 versus ≤0.60 for rival windows. Endpoint positions win a 278-candidate
× 4-table corpus sweep; 96.6% of opaque corpus blocks land on the 4-bit
white/black Suunto palette.

Asset container corroboration (integrator-verified directly): the refused
source equals the resource-partition asset at `0x9db613`, under a 19-byte
`PXB2` header at `0x9db600` — bytes
`32 42 58 50 | 3c00 3c00 | 0000 | 11 | 8c0a` (magic 0x50584232, w=h=60,
stride 0, format byte 0x11, size 0x0a8c=2700); the 2700 B at `0x9db613` hash
to `f311e1ef…` exactly. A strict walk of `resources.raw` validated 380
assets: 302× format 0x05 (2bpp paletted), 61× format 0x11 (TSC6A, every size
= `ceil(w/4)·ceil(h/4)·12`), 17× format 0x19 (raw RGBA4444).

Recorded ambiguities (contained, not hidden): A1 — endpoint 16-bit packing
RGBA4444-with-R-duplication (chosen; corpus palette mass and nibble
symmetry) versus RGB565 (indistinguishable on the capture's coarse palette);
A2 — mid-alpha block semantics (capture uses only 0x000/0x7FF); A3 — the
auxiliary region bits 75..95 (fail-closed by law above). Next discriminating
probe: Ghidra disassembly of the PXB2 loader around the magic constant at
`../suunto-firmware/artifacts/analysis/sapporo-2.35.34.18929/component-04-type-4-v2.raw`
file offset `0x4039c` (strings `%s%s.pxb`, `pxBlitC64.cpp`).

Scope authorized: a deterministic C99 expansion of zero-aux TSC6A blocks,
integrated only behind the capture-pinned acceptance tuple enumerated in
`INTEGRATION-NOTE.md` (src 0x17 sampling 1 stride 180 60x60 bounded SRAM;
RGB565 240x240 target 480 stride; pinned codeptr/matmult/matrix bits; ordered
clip (0,81)-(240,162); 60x60 quad; tint 0xffffffff; drawcolor 0xff555555;
all-block validate-before-mutate; any aux nonzero ⇒ refusal with zero
writes). Blending reuses the lane-observed SRC_OVER path of
E-NEMA-RGBA4444-001. This entry authorizes no per-asset guessing, no
non-identity-matrix sampling, and no behavior beyond what the pinned
firmware exhibits. Runtime integration is ticket 793.

Addendum (2026-09-23, integrator): an independent strict-container re-walk of
the same pinned `resources.raw` was run twice identically
(`/tmp/sap235-pxb2-assets/run-{1,2}.txt`,
`b8fb477edd15c00a7538e64982f54f214146d506f7bd8ebe58febd70b55e0f62`; walker
`/tmp/sap235-pxb2-assets/walk.py` `f3295f06b0e4af95e3284e318cbedc4a4bff5a20ca5014c88a39d652946f6a0e`).
Its greedy validator (magic + panel-bounded w,h + `size ==
ceil(w/4)·ceil(h/4)·12` for format `0x11`, non-overlapping forward scan)
validated 59 format-`0x11`, 285 format-`0x05`, 19 `0x0f`, 28 `0x10`, 20
`0x13` hits (plus 9 constraint-rejected magic hits); the counts differ from
the original §3 walk because the two walkers use different acceptance
heuristics over the same byte stream — both are self-consistent, and only the
TSC6A aux-plane conclusion below is used as planning evidence. Result: of the
59 format-`0x11` assets, exactly 2 have all blocks auxiliary-zero (total
3654 px — including the 60x60 crosshair); 57 contain at least one block with
bits 75..95 set (324402 px). Consequence, recorded as planning fact: the
ticket-793 aux-zero-gated expansion unblocks the observed main-entry crosshair
draw and nothing richer; main will fail closed again on the next compressed
asset it attempts. Full main rendering therefore requires the auxiliary-plane
law (ambiguity A3), whose discriminating probes are the PXB2-loader
disassembly and the patent's multi-color block arrangement.

## E-RE-SAP235-PXB2LOADER-001 — PXB2 loader is a GPU descriptor builder; no software TSC6A decode

**Status:** verified static RE probe, 2026-09-23 (offline-RE class, owner
decision 2026-09-23). Single probe execution; the two claims the integrator
reproduced independently are marked as such. Authorizes no implementation
value by itself; it constrains what later probes must examine.

Input: `component-04-type-4-v2.raw` SHA-256
`36a14dc5bad7b9cb8a7c8164bfaaedaf68c75a9611bc3a9e6efaa47418a5a38a`
(the pinned 2.35.34 application, size `0x182ffe`, loaded VA base `0x40000`
per E-SAP-0030). Tooling: GNU objdump (Binutils 2.47.20260726,
`arm-none-eabi-objdump`, force-thumb, `--adjust-vma=0x40000`) full dump
`/tmp/sap235-pxb2/full-force-thumb.lst` `d7e726bc059d92872da2ef37ba3a7b3a3aa8608e03276704b02b38b47a041e70`,
plus capstone 5.0.7 PC-relative/immediate scans; report
`/tmp/sap235-pxb2/FINDINGS.md` `d22b3593029cfa999b1359b9c5c1c11a6f779c96e4b4885452af745b1eeaadaa`
and six spot listings (`uiCache_magic_chain_7fc60.txt`
`e0182124b6e55dd2…`, `tex_setup_packer_ca9c0.txt` `6471a67d5c72e529…`,
`tex_pack2_cab12.txt` `28b17f5aaab73b81…`, `nema_reg_helpers_c1900.txt`
`bf24533107c19d6b…`, `ring_writer_caea0.txt` `fe17e40f676d0b7a…`,
`sha256.txt` `f09afe8dbb01f2f7…`; volatile). Correction of the tasking
slip: the container magic word sits at VA `0x8039c` (file `0x4039c`).

Census (probe): the byte sequence `32 42 58 50` (PXB2 magic) occurs exactly
once in the application and is never referenced — no PC-relative load
resolves to `0x8039c`, no absolute pointer word, no `movw/movt` forms its
halves; the nearby `uiCache.cpp` compare chain at `0x7fc76..0x7fcbe` tests
three sibling pool words and skips it. The application contains zero
software TSC6A decode: no `ceil(/4)` block-walk pattern, no ×17 nibble
expansion, no divide-by-3/×2047 reciprocal constants (the `0x7FF` clusters
are soft-float exponent math), zero immediate `0x17`, and no format-byte
compare chain. Texture format reaches the GPU as **data**: packers at
`0xcaacc`/`0xcab52` (`orr.w r2,r2,rX,lsl #24` with `rX = [texstruct+8]`)
compose the descriptor word, command words are written into an SRAM ring
(`0x10143xxx`, init `0xc1954..0xc1968`), and the ring pointer is kicked to
NEMA `+0xec/+0xf0/+0xf4` through MMIO helper `0xc1930` — whose sibling
`0xc1932` is exactly the refused STR of E-EMU-SAP235-MAIN-TSC6A-001. The
strings `%s%s.pxb` (`0x82d48`) and `pxBlitC64.cpp` (`0xDDD74`) have zero
code references (compiled-out literals); no sibling partition contains the
magic.

Integrator independent re-runs (`/tmp/sap235-pxb2/reverify.py`, capstone
5.0.7, input hash above): magic occurrences = 1; PC-relative loads with
EA `0x8039c` = 0. The `movw/movt`-absence, immediate-`0x17`-absence, and
format-dispatch negatives are probe-reported (their exhaustive scan is
linear-disassembly-based; a naive all-offset rescan produces only
mid-instruction artifacts and was not treated as a census).

Consequences, scoped: ambiguity A1 (RGBA4444 vs RGB565 endpoint unpack)
stays UNRESOLVED — the unpack is NEMA1280 hardware-internal and no
software path exists in the pinned application to disambiguate; the
capture-verified RGBA4444 law of E-RE-SAP235-TSC6A-001 remains the only
observed-behavior basis, and its golden remains the check. Bits 75..95:
no new evidence; fail-closed stands. Positive fact for the model: since
the guest only builds GPU descriptors and the hardware decodes natively,
in-tree decoding in the renderer (ticket 793) is the architecturally
correct placement, mirroring the semantic-shadow precedent. The next
discriminating probe for A1/A3 is a scan of `resources.raw` for descriptor
templates pairing byte `0x17` with PXB2 asset offsets, and the patent's
multi-color block arrangement (pair-plane) reading.

## E-SAP-0050 — 2.33.16 reset-loop gate is the fsimage VSF footer; the in-tree reset is the HardFault branch

**Status:** verified lane probe, 2026-09-23 (read-only Renode, twice
reproduced). Inputs: `sapporo-2.33.resc` lane with 2.33.16 app
`ba286a7b…` + 2.33.12 resident, plus the authentic
`component-05-type-1-v3.raw` resource partition and the existing
`suunto-sapporo-storage.repl` staging; report
`/tmp/sap233-reset/FINDINGS.md` with per-file SHA-256 census (§0/§7), clean
log pairs byte-identical: fault `ecafbc47…`, check-bare `03b13e5d…`,
check-fixture `b8a7bd32…`, string `a285c765…` (full hashes in the FINDINGS
census). Renode
v1.16.1.16858, capstone 5.0.7.

Observed (lane): the boot startup task (`bl 0x000f49b8` → `0x000a5f30`,
gate `bl #0xa6028` at `0x000a5fc6`) validates the external-flash resource
partition footer at flash offset `0xFC0000` (XIP `0x14FC0000`): 0x24 bytes
(via MSPI driver `0xdf15a`, byte loop at PC `0xF7DF2`) must have word0 ==
`0x46535631` ("VSF1") and word[0x14] == CRC32 of the first `0x14` bytes
(validator `0x000c9804`; the pinned component-05 footer carries
`VSF1|"2.33.16"` with stored CRC `0xd4c76c92`, verified equal). On failure
(boot-stage byte `[0x1005b8f1]==5`) the guest logs
`Attempting to start APP without valid fsimage!` (string VA `0x001A9714`,
captured `r0=0x600`), persists reboot reason `0x11`, and resets via
`bl #0xc97e0` at `0x000a5fe4` — CMSIS `NVIC_SystemReset` (literal
`0x05FA0004`/`0xE000ED0C` at `0xc97f0`, the recorded `0xc97f2` is the DSb).
Bare lane (no XIP backing): reads return 0, footer check 0, deliberate
policy reboot; with authentic staging: `r0=0x46535631 check=1`, boot
continues past the gate with zero warnings. No 2.33.16-specific lane script
existed before this probe; 2.33.12/2.33.16 share the validator and the
HardFault analyzer byte-identically.

Attribution for the in-tree reset (E-SAP-0015, PC `0x000c97f2`): the tree
profile already backs `0x14000000`, and the recorded registers are
internally consistent with the *HardFault analyzer* branch into the same
helper — `xPSR 0x29000003` (IPSR=3), `LR 0xffffffe9` EXC_RETURN, and
`R3 0x49000000` equal to the stacked xPSR copied by the analyzer's
frame-dump loop before its tail `b.w 0xc97e0` at `0x001ab0ce` (R0–R2 are
helper literals). So the in-tree first reset is a fault escalation, not the
policy path. HYPOTHESIS (explicitly not observed): the first faulting
access is among the early PWRCTRL-family writes the lane only warns about
(`0x40020000` offsets `0x58/0x60/0x78/0x80`, `[no-name]` offset `0x2C0`,
offset `0x4` PWRENMSPI2 — the E-SAP-0018 family). Actionable next probe:
the firmware analyzer parks tag `0xFE0E8700` + HFSR/CFSR/MMFAR/BFAR + 8
exception-frame words at RAM `0x1005FFC0` (literals `0x1ab0d8/0x1ab0dc`)
immediately before reset; an in-tree dump of that region at the reset names
the faulting address. Never inject a synthetic footer; the authentic one is
already valid.

## E-EMU-SAP235-COMPRESSED-001 — main-entry compressed draw accepted in-tree

**Status:** verified in-tree runtime result, 2026-09-23 (ticket 793
implementation; law per E-RE-SAP235-TSC6A-001, capture tuple per
E-EMU-SAP235-MAIN-TSC6A-001). Implementation: second accepted state of
`nema_tsc6a_resolve_mask` (exact capture tuple, `semu_bus *bus` parameter
added — fail-closed NULL), `tsc6a_expand_block` in
`src/display/nema_tsc6a_expand.c`; validate-before-mutate with zero-write
refusals; pixel-center mapping through the existing fixed-point helpers.

Common-command setup-walk control (the five-layer invocation of
E-EMU-SAP235-MAIN-TSC6A-001, POST `mmlllmlllmmmmmmmmmmmmmmmmmmmmmm`,
10^10-instruction / 40 s budgets) post-change, paired byte-identical
transcript `b2820edff0e119bc18796079c87128aa9f250997c0b73fd05db93ad15699c286`:
no compressed-source or `nema_tsc6a` refusal anywhere; the first
post-Done settled frame appears at step 25, generation 3994,
`crc32=6a446900` (the main screen with the 60x60 crosshair composited via
TSC6A expansion + SRC_OVER); the guest then self-requests the machine reset
at `0xcdf5a` (`pc=0x000cdf5a instructions=7554756551
virtual_time_ns=32447955715`, `reset_count=1`) — ~67M instructions past the
old refusal/fault point — and ends at the pre-existing OHR-fixture compat
refusal `stop=compat-refused pc=0x001be85a instructions=8135889023
virtual_time_ns=36070752064` (exit 3). The former refusal→BusFault→reset
chain at `7486616944`-instructions is gone; the `0xcdf5a` self-reset and
the exhausted-OHR-fixture tail are the new honest boundaries, owned by
ticket 794. The five-layer headless window (pre-Done) is byte-unchanged
(`17cc9087…`, four runs) — the acceptance state never fires before main
entry, so no pre-existing pin moved.

Acceptance probe (volatile, `/tmp/sap235-793-derive/probe_composite.c`
`60a0c80b…`): the real renderer over the real capture-1 `before.bin` target
plus the dd-derived 2700 B asset and the exact tuple produced
115200 bytes byte-identical (COMPOSITE-MATCH, zero diffs, twice) to an
independent law re-composite (192 opaque texels, 176 target pixels changed,
all inside the quad window) using the tree's `pack_round` convention.
Verification census: `make check` 996 PASS / 0 FAIL; `make sanitize` zero
findings; `make check-lines` advisory-only; `make test
TEST_FILTER=nema_tsc6a_expand` 7/7 with the golden byte-exact against
`decode-1.bin` `2f30fe18…` (negative control: one flipped fixture byte
fails); all nine 2.35 firmware runners green; `check-sdl`, the 2.35 scroll
gate (`1d44ea98…`), the SDL startup/language gate, and snapshot restore all
unchanged; 2.35 era scripts zero drift (production, OHR, pressure,
gps-startup/reopen/awake, block-erase).

### E-RE-SAP235-RESOURCES-INDEX-001 — resources index, aux-plane statistics, A1 negative census

Class: owner-authorized offline RE of hash-pinned firmware/resources
(Lane Oracle decision 2026-09-23; second application after ticket 788).
Probe period 2026-09-23. Repo tree untouched by the probe.

Inputs (all re-verified every run): `sapporo-2.35.34.18929/
component-05-type-1-v3.raw` size 16519168, sha256 `f281385a…ae22`
(the pinned 2.35.34 resources partition, PIN_MATCH). Tools: Python
3.14.7 (stdlib-only scan batteries), capstone 5.0.7. Scan batteries:
`scan_descriptors.py` sha `bd4bbe4b3282358a…`, `scan_aux.py` sha
`7cb49c2d981092d7…`; each run twice `cmp`-byte-identical — descriptor
census output sha `9516b566990d4017…`, aux census sha `5b76412e…`
(volatile paths `/tmp/sap235-descriptors/`, FINDINGS sha `0a20d0e7…`).

Census (derived, authoritative):
- Container validation over the whole partition: PXB2 fmt census
  {0x05:294, 0x0f:20, 0x10:28, 0x11:61, 0x13:20}, 9 rejected magic hits
  itemized. Non-greedy whole-blob walk finds 61 fmt-0x11 assets (the
  earlier 59 count skipped assets inside blobs; both self-consistent,
  61 used here). All 61 carry header stride 0 — GPU-side strides
  (180 capture / 480 target) are never asset-header fields.
- Auxiliary plane (block bits 75..95) over all 61 fmt-0x11 assets,
  22296 blocks: nonzero in 14275 blocks (59 aux-nonzero assets, 2
  aux-zero incl. the captured crosshair — confirms the 2/57 split);
  only **1239 distinct values of 2^21**; `0x1FFFFF` (all-ones) is the
  most common value (39.5% of nonzero blocks); every bit used (46–55%
  set per bit — no padding); spatial lag-1 aux equality 69% (~0.005%
  expected random) — a smooth per-block plane, not dither; under a
  3x7-bit split 80% of nonzero values are gray triples led by
  (127,127,127) white, plus Suunto teal (31,122,117) and blue
  (0,85,127); triple equals E1 in only 40.8% — not a duplicate field.
  Verdict A3 = **CONSTRAINED**: a third representative color at
  7-bit/channel precision (patent US 9,640,149 B2 multi-precision
  representative-color reading); subalpha/padding/dither rejected by
  the arithmetic and statistics. The GPU's *usage rule* (which index
  patterns select it, channel order) is NOT resolvable from data.
- A1 (GPU 0x17 endpoint format) = **STILL-OPEN, resource-side
  EXHAUSTED** (twice-reproduced negative census): 46066 asset-offset
  reference sites; 38 within 0x17-proximity, all classified font/blob/
  dir noise; stride-phase census shows only a mod-64 FAT-chain artifact
  (EXPLAIN1); (w,h)+0x17 content join yields 78 false positives; the
  capture tuple (stride 180 / 0xffffffff / 0xff555555) exists only as
  pixel data (7458 hits), never as descriptor fields. The partition's
  master index is a fixed-stride 16-byte record family (205 tables at
  block offset 0xf800, 23131 records, [u32][0fff][idx][u32 slot][u32
  ptr]) with **no format field**; the single constant-0x17 table
  (@0x21f800) is a file/slot-id table spanning three container formats
  at noise rate. The container-format→GPU-format mapping lives only in
  application code (41 `lsl #24` candidate sites listed in the probe
  logs; the two known packers 0xcaacc/0xcab52 use stack-procedural
  records). NOTE: for every draw already captured, the accepted law
  (RGBA4444 top-3-nibble x17 endpoints) reproduces lane pixels with
  zero diffs (E-EMU-SAP235-COMPRESSED-001), so A1 gates only
  as-yet-unobserved states.
- Manifest: VSF1 footer @0xfc0000, version string "2.35.34", stored CRC
  equals recomputed CRC of the footer's first words (MATCH), "SCSF" tag
  @+0x18; FAT-style directory census 574 records in the first MB
  (region to 0xfbb060); the crosshair page's cluster is not any SFN
  entry head — filename attribution remains open.

Implications: the ticket-794 refused post-Done draw is most plausibly an
aux-bit asset; the decisive discriminator is now a lane capture of that
submit (in progress) or a synthetic lane GPU A/B render — no in-tree
value change is authorized by this entry alone, and the fail-closed aux
refusal stands.

### E-EMU-SAP233-GAUGE-FIXTURE-001 — AvgVCell fixture supersession and 2.33 first-fault closure

Class: current-lane-model re-derivation of a host-side fixture VALUE
(device law E-SAP-MAX17050-001 states fixture values are host-side battery
fixtures, not physical gauge evidence; the current lane model is the oracle
for values while selectors stay pinned to the authentic command census).

Evidence inputs (twice byte-identical): lane pair normalized sha256
`9119ef13e60dbe71483f28bf02a962997252d7c476b4444a47f3fab9f06b6d76` (raws
`c4dea835f159cdcafec0edf45b31ae1b41c8c0e5d9e1c48a76d2f8546e88c169` /
`ecbeda19efc3cf2dfbd174475737da20f1434ff6ce6403942ae4416be725e244`,
volatile `/tmp/sap233-fault/f4v2_{1,2}.log`, 22 payload reads; the lane
never issues register 0xF4). Lane fixture table source
`emulator/renode/iom4/SapporoApollo4Iom4.cs` sha `b1d1dc8e…`
`SapporoMax17050.Reset()` lines 689-706: 0x00=0x0000, 0x06=0x3200,
0x08=0x1900, 0x09=0xC000, **0x19=0xC000**; all other 251 words 0x0000.
Tree diff: exactly `MAX_AVERAGE_VCELL_VAL` 0x0000 to 0xC000
(`src/devices/sapporo_max17050.c`); the earlier 0x0000 pin traced to a
payload-free trace of a different register census and is superseded.
Selector set unchanged: 0xF4 remains a refused unobserved selector
(unit test `test_unobserved_f4_and_ff_refuse`); 2.35 IOM4 mirror and the
Ulsan gauge already carried 0xC000 (this aligned the last divergent
endpoint).

Acceptance (twice reproduced by implementer AND independently by the
integrator, pair `5d7c9daf29adff26ac7430542ecafe41f55fb95227a04f761003488dc71b0c0c`):
2.33.16 boot in a 200M-instruction/200 ms window has ZERO
machine-reset-request and ZERO refusals; the old first fault
`pc=0x000c97f2` (HardFault after the refused 0xF4 gauge read, instr
42130231, vt 175577735, PRE pair `c889ee15…`) is fault-free;
`stop=budget pc=0x000dbc0a instructions=48412217
virtual_time_ns=339421286`.

2.22 deep-boot consequence (control-build attribution, all pins re-derived
twice; every frame CRC and PPM byte unchanged): the 2.22 guest consumes
register 0x19 in its pinned startup census; under the lane-consistent
0xC000 answer it no longer issues the `E-SAP-COMPAT-RESOURCE-001`
resource-status probe at t~1.228 s, shifting downstream timing only.
Re-pinned values (old -> new, twice each): live-input stop tuple
`0x080000a0/772290112/6520978802` -> `0x0800009e/770457344/6521343631`,
cold transcript `278cc6db…` -> `ccc4ea6008192eb3981695c208004a8f530ea6152cfc8d3053770de354f96e93`;
onboarding press vt `30003929586` -> `30010093175`, walk transcript
`7186e3eb…` -> `2c6910c2d53d0dfd3fa9c00aa046615a3075a725ce33e67876a2706c8a68e0b8`,
stop `8500057344/38818426902` -> `8491624576/38819797929`, idle transcript
`0e215631…` -> `91cc708109a2e0405d385fc894674832c58c5457a2845d36c97571d9bd4ab1bf`
(stop budget `8807319394/60041792981` -> `8798037004/60043686593`),
disabled-case transcript `07250eea…` ->
`9fe0c259b48a59ada20064d68f7be32834dc463df3958b23e8d3f2072efa8a5b` (stop
`13184192858/68141114950` -> `13176760848/68149538779`, GPS-cap line count
11 unchanged), snapshot `f829b2fa…` ->
`87a8dca8925aeb4f4eb9adbefb3240d5be77eb04ffc7274d9093f3076dd02dbf`,
restore-chain next-snapshot `e5c5dd57…` -> e802a9b715b16fb366c8b53fdad2c2f35f706238bf9cae46b1e1397c43e79fcd, next-stop
`8744080727/41088770901` -> 8734743608/41090034111.

Gate census: make check 998 PASS / 0 FAIL (996 + 2 new gauge cases; unit
`test_sapporo_max17050` 14/14, failing-first on 0x19); make sanitize zero
findings; 2.35 era set (8 runners) zero drift; 2.35 SDL main/scroll gates
hold their pins; 2.33 boot advances; 2.22 gates green on the re-derived
pins above. Unchanged open items: the 0x06-early lane-order divergence is
still unexplained; HFSR escalation-bit fidelity (bit30 vs FORCED bit1,
`src/cpu/armv7m/scb.c`) remains open pending a lane-observable scenario;
2.39 era (same-day follow-up audit at this HEAD, ticket 777): the
full-flash fixture was rebuilt to its pin from read-only inputs and the
full 43-runner era set ran twice — 12 pin-held, 25 re-pin candidates whose
dominant delta is a timing-only vt shift at unchanged instruction counts
(the same fixture-value signature; cold log `47e8aaa7…`, Δvt
-1,556,396 ns across the `0x0014e8ea` family), 0 unexplained failures, 6
environment-blocked scripts needing a quiesced-tree re-run, and ONE
green-to-red flip (`timer_pattern`, -62 instructions) awaiting
rebuild-bisect attribution before any re-pin. All 2.39 re-pins are tracked
in ticket 777 (in-progress), not applied silently here.
