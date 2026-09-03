# 706 — Sapporo 2.39.20 Exact Profile

**Status:** done
**Phase:** 7
**Dependencies:** 700

## Goal

Add the exact Sapporo `2.39.20.22297-P` profile and reach its first recorded
reset boundary without changing existing Sapporo behavior.

## Execution Budget

One model-day for the strict profile, registry integration, focused tests, and
two-run authentic-firmware comparison.

## Required Reading

Ticket 700, E-SAP-0011 and E-SAP-0018, `docs/profile-format.md`, the existing
2.33 profile integration, and the board/profile registry paths named below.

## Current Baseline

The exact component contract exists, but `sapporo-2.39.20` is not a built-in
profile. Diagnostic-only runs required an external profile and temporary board
registration, all of which were reverted after recording E-SAP-0018.

## Allowed Files

- `profiles/sapporo/2.39.20/**`
- `profiles/index.semu`
- `src/frontends/cli.c`
- `src/boards/machine.c` (exact profile registration only)
- `tests/unit/test_sapporo_profile_239.c`
- `tests/integration/test_firmware_sapporo_239_profile.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/706-sapporo-239-profile.md`

## Frozen Interfaces

Manifest grammar, Sapporo memory map and wiring, Apollo4 behavior, compatibility
layers, and every 2.22/2.33 checkpoint remain unchanged. The profile introduces
no inferred version sharing beyond the already evidenced board identity.

## Evidence Inputs

E-SAP-0011 pins the version, load addresses, component sizes, hashes, and direct
vector words. E-SAP-0018 pins the first bounded reset at 11,897,027 ns and the
underlying precise fault at `PWRCTRL.DSP0MEMPWREN` (`0x40021058`).

## Implementation

Add profile and example-manifest metadata, CLI/index registration, exact board
admission, focused valid/wrong-hash/wrong-version tests, and a private runner
that compares two bounded logs at the first reset boundary. Do not implement
the missing PWRCTRL registers in this ticket.

## Tests and Commands

`make test TEST_FILTER=sapporo_profile_239`, `make check`, and
`make test-firmware SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_profile`
must pass. The private runner performs two 20,000,000-instruction runs and
requires byte-identical logs with the first `machine-reset-request` at PC
`0x000d2f6e`, virtual time 11,897,027 ns, and zero compatibility hits.

## Acceptance

Exact inputs validate and reach the recorded reset twice identically; invalid
hashes and versions fail before mapping; the built-in list/show commands expose
only the exact contract; existing profile tests and checkpoints do not change.

## Forbidden Scope

No DSP power, Reset/BoD, watchdog, storage, compatibility, display, CPU, or
firmware-byte changes. No wildcard profile IDs or inherited version ranges.

## Handoff

Implemented the built-in `sapporo-2.39.20` profile with the three E-SAP-0011
component hashes, CLI/index/board registration, five focused metadata and
refusal tests, and a private deterministic runner. Two canonical 20,000,000-
instruction runs were byte-identical (SHA-256
`5ba6187563478deda274fad86942f937b615d5b0a7c03884c957ace80233e7ba`): first
reset PC `0x000d2f6e` at 11,897,027 ns with zero compatibility hits, then bounded
stop PC `0x000d1644` at 20,000,000 ns. E-SAP-0018 is the next observed hardware
gap; this ticket adds no peripheral behavior.
