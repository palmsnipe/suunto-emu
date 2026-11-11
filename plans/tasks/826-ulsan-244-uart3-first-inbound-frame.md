# 826 — Ulsan 2.44.52 First Inbound UART3 Radio Frame

**Status:** blocked
**Phase:** 7
**Dependencies:** 825

## Goal

Capture the first peer-to-watch UART3 frame in two cold Race S
Renode boots. Pin the raw bytes, arrival and completion time relative
to UART3 open, and the native parser's result.

## Execution Budget

Limit each cold run to 100 virtual seconds and six host minutes.
Timestamp ingress bytes and retain the first complete frame, counts,
and the stop point. Store probe sources and raw logs outside Git.

## Required Reading

Read ticket 825, E-ULS-0101 through E-ULS-0103, the Race S Renode
platform and firmware manifest, the execution and testing contracts,
and the Hello, Service Mode, and onboarding regressions.

## Current Baseline

The controlled CTS experiment keeps UART3 open. The BLE adapter stays
in START while the application waits for the peer's startup frame.
The current Renode lane has no UART3 radio endpoint.

## Allowed Files

- This ticket and `plans/index.tsv`
- `docs/migration-evidence.md`

## Frozen Interfaces

Preserve the firmware, lane, media, standalone runtime, and pinned
Race S checkpoints.

## Evidence Inputs

Use the pinned 2.44.52 application, E-ULS-0103 CTS stimulus and
E-ULS-0100 crown-only route. Completing this ticket requires a
hash-pinned source of the radio peer's output and timing that the lane
can observe. Host parser constraints alone cannot supply a transcript.

## Implementation

Trace the UART3 receive endpoint, native data loads, and frame
validation. Record the first ingress and parser times in Renode virtual
ticks. Retain the peer's provenance and compare the two transcripts.

## Tests and Commands

- `make test TEST_FILTER=ulsan` with private manifest and eMMC
- `make test-firmware TEST_PROFILE=ulsan-2.44.52 TEST_FILTER=ulsan_244_` with private inputs
- `make check-task-contracts && make check-lines && make check`

## Acceptance

Two independent cold boots reproduce the radio's first actual inbound
frame and timing relative to UART3 open. Both report the same native
parser outcome. Pass Hello, Service Mode, and both pairing guidance
routes. A zero-byte trace records the blocker and leaves acceptance
open.

## Forbidden Scope

Guessed peer traffic, forced parser or adapter state, tracked raw logs
or firmware, and production runtime changes.

## Handoff

E-ULS-0104 records two cold boots through 60,000 virtual ms. The
native UART3 open returns 1 at tick `11492358369`. Both boots
receive and transmit zero bytes. The BLE adapter stays in START,
and the normalized traces match SHA-256
`f021fb69ea7ae80300dd61fa3fde1c7e29754c1c12e72b8511a035d8e97a493e`.

The firmware requests 1 Mbaud, 8N1. The receive parser's framing
and checksum checks are known, and the first peer frame and arrival
time remain unobserved. A hash-pinned source of the peer's output
and timing is required to unblock the capture.

All 89 Race S test cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only guidance and mixed-input
guidance. Both firmware-era scripts, task contracts, line checks and
`make check` passed.

The private regression logs under `/tmp/semu-826.KrYfyJ` have these SHA-256 values.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0105 traces the updater to `firmware/ble_3_0_44_moqr.bin`.
All eight retained packages contain `BambiTL_nRF53` headers.
Reconstruction from 2.44.46 and 2.44.52 yields the same 386,726-byte
bundle with three valid SFI3 headers and payload CRCs. Bundle SHA-256
is `0cfab67c4632604459a70446fcc99436b62c3c64bef59566dfbe728cbf3cac11`.
The FCC internal photos show an nRF5340 marking on the OW233 board.

The bundle is an update image with no write records at either core's
flash base. Establish the installed reset path, then boot the peer
and capture its UART3 bytes and timing in two cold Renode runs. The
ticket remains blocked on that observation.

E-ULS-0106 records two radio reset attempts in Renode. The bundle
supplies neither flash-base vector. Renode halts the application
core before it executes an instruction. Direct-entry diagnostic
pairs reach their recovered vectors and stop before the first
unavailable factory reads, at `0x00ff0130` on the application core
and `0x01ff0300` on the network core.

The later BambiTL handoff and UARTE1 startup-frame path are
identified in disassembly. The installed reset stages and the
factory configuration required by startup remain unavailable.
The frame and its timing remain unverified, so status stays blocked.

The repeated Race S suite passed all 89 cases, including two cold
standalone boots each for Hello, Service Mode, crown-only guidance
and mixed-input guidance. Both firmware-era scripts, task contracts,
line checks and `make check` passed.

The private regression logs under `/tmp/semu-826-reset.9VHs29` have these SHA-256 values.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0107 adds a 24-package cross-product census. The identified
nRF5340 update images all omit the flash-base ranges. A public
Nordic revision predicate allowed two bounded direct-entry probes
past the first factory reads. They stop at the BambiTL clock status
and application trim table. Status remains blocked on the installed
reset path and the first timed UART3 capture.

E-ULS-0108 ran the native radio startup encoder twice in isolation.
Both runs produced the same 22-byte frame with the retained-data
default and sequence 0. Two cold watch replays accepted those bytes
at an imposed arrival time and emitted the same 58-byte reply.
The adapter was still in START at the stop. The replay does not
supply the actual radio cold-boot frame or its arrival time.
Status remains blocked.

The repeated private Race S suite passed all 89 cases, including
two cold standalone boots each for Hello, Service Mode, crown-only
guidance and mixed-input guidance. Both firmware-era scripts,
task contracts, line checks and `make check` passed. The regression
logs below `/tmp/semu-826-encoder.gWhYaO` reproduce the previous digests:

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0109 fed the captured 58-byte watch reply through the native
radio parser in two isolated Renode runs. Both passed CRC validation
and produced the same 18-byte zero-payload response through the
native encoder and EasyDMA into the UART model. The bad-CRC control
emitted zero bytes in both runs. Status remains blocked on the
actual first radio frame and timing from a cold peer boot.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-radio-rx.yQPEIt`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0110 replays the radio's 18-byte acknowledgement in two cold
watch boots. The original lane drains its bytes from UARTDR while
waiting for the transmit-complete interrupt. A separate pair with
one diagnostic Apollo4 TX completion parses and matches the
acknowledgement, then sends an opcode 0, subtype 0, sequence 1
request. Neither path reaches ON, and native pairing readiness
remains zero. Status stays blocked on the first cold radio frame
and arrival time.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-watch-ack.JgvBuE`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0111 feeds the watch's opcode 0, subtype 0, sequence 1
request through the native radio parser. Two strict runs stop
before the first factory read at `0x00ff0210` and emit no bytes.
With explicitly labeled FICR inputs, two pairs produce an opcode 0,
subtype 2, sequence 1 reply with 48 payload bytes. The two patterns
isolate the 20 factory-dependent payload bytes. The actual device
values and cold radio startup timing remain unavailable. Status
stays blocked.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-radio-info.GBaP9b`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0112 replays the native subtype-2, sequence-1 reply with
E-ULS-0111's labeled factory pattern A. In two cold watch boots
the native adapter reaches ON for 2,048,499,250 virtual ns,
sends an opcode `0x14`, subtype `0x0b`, sequence 2 request,
then returns to START. Two controls omit the subtype-2 reply
and never reach ON. The replay supplies two TX completion events
and no further radio frames. Ticket 826 remains blocked on the
actual first frame and timing from a cold radio boot.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-watch-info.FATvD2`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0113 feeds the watch's opcode `0x14`, subtype `0x0b`,
sequence-2 request through the native radio parser. Both
isolated Renode runs take the pairing-removal path and emit
the same 18-byte acknowledgement with an empty payload. Both
bad-CRC controls emit zero bytes. The radio's cold-boot first
frame and arrival time remain unverified. Status stays blocked.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-radio-seq2.YRKgnP`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`

E-ULS-0114 replays the native 18-byte sequence-2 acknowledgement
in two cold watch boots. The adapter stays ON through its acceptance
and emits an opcode `0x14`, subtype `0x0a`, sequence-3 request
with eight payload bytes. It leaves ON when that request times out,
after 2,060,443,250 virtual ns in ON. Two controls omit the
acknowledgement and retry sequence 2. All four boots finish in
START. Ticket 826 remains blocked on the first frame and arrival
timing from a cold radio boot.

All 89 Race S cases passed, including two cold standalone boots
each for Hello, Service Mode, crown-only and mixed-input guidance.
Both firmware-era scripts, task contracts, line checks and
`make check` passed. The private logs are below `/tmp/semu-826-watch-seq2.qzhANv`.

- `test-ulsan.log` — `b3ec656ae6eb7024d23cfae34464c95eb5b6d68fd15a2fc92a369d52633acc96`
- `era.log` — `65a61d1075bc1d0e4da4c4424c9658b7bd55bfd3d41a8a05fe6aca1cda497c81`
