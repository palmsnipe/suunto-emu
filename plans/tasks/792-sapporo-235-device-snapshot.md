# 792 — Sapporo 2.35 Live RTC and IOM4 Snapshot Codec Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 615,791

## Goal

Make `--snapshot-save`/`--snapshot-load` accept a Sapporo 2.35.34 machine by
serializing the live AmbiqApollo4_RTC and SapporoApollo4Iom4 state that the
current refusal guard (E-EMU-SAP235-SNAPSHOT-001) rejects, so the SDL
interactive/snapshot workflow works for 2.35. Mid-transaction IOM4 state and
any uncovered field keep refusing explicitly; full-watch coverage stays out.

## Execution Budget

Two to three model-days. Expected: two device codec sections (or one small
new file), one focused device-snapshot test module, one private-firmware
runner pair, no fixture bytes.

## Required Reading

`docs/migration-evidence.md` entries E-SAP-0035 (RTC register law), E-SAP-0036
(IOM4 law), E-EMU-SAP235-SNAPSHOT-001 (guard), E-EMU-SAP235-RTC-OWNER-001,
E-EMU-RENDERER-SNAPSHOT-001 (v2 hook ownership); `src/devices/sapporo_rtc.c`,
`src/devices/sapporo_iom4.c`, `src/devices/sapporo_iom4_regs.c`,
`src/soc/apollo4/apollo4_snapshot.c`, `src/soc/apollo4/iom_snapshot.c`,
`src/boards/machine_snapshot.c`, `include/semu/machine.h`,
`tests/devices/test_apollo4_snapshot.c`.

## Current Baseline

`src/boards/machine.c`/snapshot paths refuse 2.35 save/load while live RTC or
IOM4 state exists; 2.22-era device snapshots and the v2 renderer hook are
complete. The RTC owns clock/alarm events through the SoC; IOM4 owns a FIFO,
command state, and owned DMA/scheduled events. Code must be extended, not
replaced.

## Allowed Files

`src/devices/sapporo_rtc.c`, `src/devices/sapporo_rtc.h`,
`src/devices/sapporo_iom4*.c`, `src/devices/sapporo_iom4*.h`,
`src/soc/apollo4/apollo4_snapshot.c`, `src/soc/apollo4/iom_snapshot.c`,
`src/soc/apollo4/iom*.h`, `src/boards/machine.c`,
`src/boards/machine_snapshot*.c`, `tests/devices/test_apollo4_snapshot.c`,
one new `tests/devices/test_sapporo_235_snapshot.c`, and
`tests/integration/test_firmware_sapporo_235_snapshot.sh`.

## Frozen Interfaces

v2 machine snapshot format and section-order contract; scheduler owned-event
identity rules; existing public headers. No new public include API without an
integrator-owned change; if the codec cannot fit v2, stop and report.

## Evidence Inputs

E-SAP-0035, E-SAP-0036 (register/lane laws), E-EMU-SAP235-SNAPSHOT-001
(refusal boundary being replaced), 615/791 format records. No new guest
behavior may be invented; codec must round-trip only state the running model
already exposes.

## Implementation

Serialize RTC counters/config/alarm line and reschedule the owned one-second
alarm event; serialize IOM4 registers, FIFO contents, selector, and idle
command state. Mid-command or unmapped-destination DMA state refuses on save
and load. Validation happens into scratch before mutation; load stays atomic
across all components.

## Tests and Commands

`make test TEST_FILTER=sapporo_235_snapshot` exits 0 with at least one test;
round-trip equality, save-twice byte identity, truncated/section-missing/mid-
transaction refusals, and event-identity rearm. Private-firmware pair:
bounded cold run to `000932a8 / 30000000 / 35339893` with the
production-data layer, save, reload, and a continuation that reproduces the
uninterrupted suffix twice identically (derive the exact suffix pins, record
them, no silent re-pin of other gates). `make check-lines && make check &&
make sanitize` pass. State in the handoff that 2.39/2.22 era pins may have
drifted if shared codec files changed.

## Acceptance

2.35 save/load succeeds for covered idle state, byte-identical saves repeat,
restored execution matches the uninterrupted transcript exactly, uncovered or
corrupt state refuses atomically, and no 2.22/2.39 snapshot byte drifts.

## Forbidden Scope

No new RTC/IOM4 register behavior, no mid-command checkpointing shortcut, no
read-as-zero field defaults, no snapshot version bump, no era repins, no
2.35 compressed-texture or GPS work.

## Handoff

Report changed files, exact commands/results, save/restore hashes, refusal
cases, drift statements for other profiles, and any integrator change needed.
