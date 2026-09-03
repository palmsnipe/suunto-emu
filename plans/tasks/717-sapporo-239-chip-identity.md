# 717 — Sapporo 2.39 Deterministic Chip Identity

**Status:** done
**Phase:** 7
**Dependencies:** 716

## Goal

Implement only deterministic MCUCTRL CHIPID0/CHIPID1 reads required by Sapporo
`2.39.20.22297-P`, then stop at the next unsupported access.

## Execution Budget

One model-day for the identity policy, bounded MCUCTRL extension, strict tests,
and two authentic-firmware runs.

## Required Reading

Ticket 716 handoff, E-SAP-0021, `docs/architecture.md`,
`docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`, pristine firmware disassembly at
`0x0008a0b8..0x0008a0c2`, Apollo4 Plus PAC 1.0.0 `mcuctrl.rs`,
`mcuctrl/chipid0.rs`, and `mcuctrl/chipid1.rs` at commit
`75e44b7061b5f707907fe33688db46edeef726bb`, and the pinned Renode Apollo4
platform file and MCUCTRL implementation/tests named below.

## Current Baseline

After watchdog initialization, firmware PC `0x0008a0ba` reads CHIPID0 at
`0x40020004`, then PC `0x0008a0bc` reads CHIPID1 at `0x40020008` and XORs the
values for a deterministic stack-protection seed. The first read faults at
instruction 19,945,598 because both exact offsets are absent from the strict
MCUCTRL allowlist.

## Allowed Files

- `src/soc/apollo4/mcu_control.c`, `src/soc/apollo4/mcu_control.h`
- `tests/devices/test_apollo4_reset.c`
- `tests/integration/test_firmware_sapporo_239_watchdog_restart.sh`
- `tests/integration/test_firmware_sapporo_239_chip_identity.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`, `plans/tasks/717-sapporo-239-chip-identity.md`

## Frozen Interfaces

Public APIs, snapshots, SoC integration, profiles, existing MCUCTRL values,
and older checkpoints remain unchanged. No broad MCUCTRL read-as-zero policy,
chip part number, SKU, write behavior, fuse model, random identity, or host
identity source is introduced.

## Evidence Inputs

E-SAP-0021 pins the exact addresses, instruction order, and current boundary.
Apollo4 Plus PAC source hashes are `mcuctrl.rs`
`15222f850ab0df000d34834a7b67ce0d175b5038e9403bd6aaf487af81424cf5`,
`chipid0.rs`
`f0a049c66a6081cfd498484902c8d1ac235a18c4ca3b635b787ec34d724ca5ff`,
and `chipid1.rs`
`e972abcc8d3316f308f8ab024933f2963730ed2b8114ad4763c7a4178130721c`;
they pin offsets `0x04/0x08`, 32-bit fields, and reset value zero. The pinned
Renode Apollo4 platform SHA-256
`e1f8560bd32f82fb2aaf7e9a8877b1b1c4aff7bd0a35821f8761a76243b74376`
silences `0x40020000..0x40020fff`, independently making both reference reads
zero. Zero is therefore the explicit deterministic emulated-chip identity.

## Implementation

Add only CHIPID0 and CHIPID1 to the exact 32-bit MCUCTRL read allowlist and
return zero for each. Keep writes, wrong widths, CHIPPN, SKU, and every other
unlisted offset refused. Advance exact firmware only to the next distinct
unsupported boundary.

## Tests and Commands

`make test TEST_FILTER=apollo4_reset`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_chip_identity`, `make
check-lines`, `make check`, and `make sanitize TEST_FILTER=apollo4_reset` must
pass. The private runner requires two byte-identical logs and exact advanced
checkpoints.

## Acceptance

Both exact reads return zero; writes, widths, and adjacent unknown offsets
refuse; exact firmware advances to a newly identified fail-closed boundary
twice identically; no generic fallback or host identity exists.

## Forbidden Scope

No CHIPPN/SKU/feature/fuse register, identity write, host/random input,
permissive MCUCTRL page, compatibility hook, firmware bytes, profile change,
snapshot change, or unrelated cleanup.

## Handoff

Implemented only 32-bit MCUCTRL CHIPID0/CHIPID1 reads at offsets `0x04/0x08`,
returning the explicit deterministic identity `0/0`. Writes, wrong widths,
CHIPPN, SKU, and all unlisted offsets retain strict refusal. Focused tests
cover both values, write refusal, and width refusal.

Two authentic 20,000,000-instruction runs are byte-identical (SHA-256
`3d182aea65869a4414579e79ce5f942610570257b606d5b99b71c3f2674a483a`).
The first reset advances to instruction 19,948,596 and 25,288,491 ns with PC
`0x000d2f6e` and zero compatibility hits; the terminal checkpoint is budget PC
`0x001b4b56` at 25,339,895 ns. Debugger inspection pins the new precise fault
to CTIMER auxiliary address `0x400080e8`; pristine firmware PC `0x000ea56a`
writes `0x3f`, which the existing older-firmware contract refuses. That value
requires a separate evidence-gated ticket.
