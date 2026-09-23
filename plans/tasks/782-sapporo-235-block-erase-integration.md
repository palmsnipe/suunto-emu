# 782 — Sapporo 2.35 Block Erase Integration

**Status:** done
**Phase:** 7
**Dependencies:** 402, 705

## Goal

Implement the 64 KiB erase requested by native 2.35 storage, recover logbook
create/reopen, and stop at the next unsupported boundary. Ticket 710 instance-12.

## Execution Budget

Two device/controller edits, one storage allocation-safety correction, two
focused test files, bounded firmware gates and evidence documentation.

## Required Reading

Tickets 402 and 705; E-SAP-FLASH-001, E-SAP-0038, E-SAP-0041/0042/0043;
`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; `src/devices/sapporo_flash.c`, `sapporo_flash.h`,
`src/soc/apollo4/mspi.c`, `mspi.h`, `mspi_internal.h`, `src/core/storage.c`,
`storage_internal.h`, `include/semu/storage.h`, `tests/devices/test_sapporo_flash.c`,
`test_apollo4_mspi.c`, `tests/unit/test_storage.c`, `test_nema_backend_atomic.c`.
Architecture reference: Micron N25Q256A, n25q_256mb_65nm.pdf Rev. X, June 2018,
command table (4-byte sector erase DC), memory map and ERASE Operations p60;
STMicroelectronics stm32-n25q256a header, sector geometry/opcode definitions.
Neither reference identifies the physical watch's package.

## Current Baseline

The lane and interpreter silently complete DC without dispatching an erase.
Native helper f66ac selects DC for length 10000 and 21 for length 1000.
Paired unmodified-lane runs lose a newly created logbook file. The controlled
lane experiment explicitly decomposes each aligned DC request into sixteen
existing 21 operations; it is synthetic, not an unmodified DC observation.
The experiment recovers create/reopen. E-SAP-0043 distinguishes the evidence.

## Allowed Files

- `src/devices/sapporo_flash.c`
- `src/soc/apollo4/mspi.c`
- `src/core/storage.c`
- `tests/devices/test_sapporo_flash_block_erase.c`
- `tests/unit/test_storage_erase_atomic.c`
- `tests/integration/test_firmware_sapporo_235_production.sh`
- `tests/integration/test_firmware_sapporo_235_pressure.sh`
- `tests/integration/test_firmware_sapporo_235_ohr.sh`
- `tests/integration/test_firmware_sapporo_235_block_erase.sh`
- `tools/test_sdl_sapporo_235.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

Planning explicitly adds this ticket and its `plans/index.tsv` row before
implementation. Existing 2.35 era pins may be re-derived only where the erase
correction changes their path; retain all strict comparisons and controls.

## Frozen Interfaces

Public headers, storage API, device geometry constructor, snapshot formats,
profiles, layer registry and firmware identities remain unchanged. Existing
4 KiB erase remains 21. High address byte uses existing transaction metadata.

## Evidence Inputs

E-SAP-0043 records paired lane logs, firmware-selected lengths, source hashes,
controlled 16-subsector intervention and successful handles. Architecture
references authorize DC's 64 KiB aligned range; the unchanged lane does not
implement DC and is explicitly not a positive DC oracle. The existing lane's
21 semantics supply the byte/enable model. No timing beyond current synchronous
completion is claimed. If the controlled prefix fails to reproduce, stop.

## Implementation

Dispatch DC with the full address, align its erase to 64 KiB, validate shape,
range and write-enable before mutation, and consume enable once. Reject
unknown non-DMA commands before completion or endpoint mutation. Preserve
the separately lane-probed B9/AB completion-only boundary and all existing
no-fixture input pins. Allocate all
missing overlay pages before mutating a multi-page aligned erase; allocation
failure preserves bytes and page ownership. Do not change program semantics.

## Tests and Commands

First reproduce failing regressions with `make test TEST_FILTER=block_erase`
and `make test TEST_FILTER=storage_erase_atomic`. Then both must pass, plus
`make test TEST_FILTER=apollo4_mspi`, `make test TEST_FILTER=sapporo_flash`,
`make test TEST_FILTER=storage`, `make check-lines`, `make check-task-contracts`,
`make check` and `make sanitize`. Run `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
TEST_PROFILE=sapporo-2.35.34`; all selected scripts must pass two-run strict
checks with explicit instruction/time bounds. Run `sh tools/test_sdl_sapporo_235.sh` when SDL3 is available; it verifies
two identical bounded native-frame/button transcripts. Run affected 2.22/2.39 era
scripts or report their potentially stale pins; never silently re-pin them.

## Acceptance

Direct and controller tests prove lower/upper-bank alignment, full 64 KiB
extent, neighboring-byte and immutable-source preservation, reset/enable
lifecycle, malformed/range refusal, unknown-command refusal and allocation
failure atomicity. Paired 2.35 runs pass the old logbook-open assertion and
record the next boundary; working UI is not implied. Document any era drift.

## Forbidden Scope

No firmware patches, fake filesystem handles, physical-device claims, unknown
command completion fallback, new dependencies, or unrelated era repins.

## Handoff

Record changed files, exact commands/results, hashes, successful file handles,
new frontier and unsupported cases. Leave status for integrator review.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

