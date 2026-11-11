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
