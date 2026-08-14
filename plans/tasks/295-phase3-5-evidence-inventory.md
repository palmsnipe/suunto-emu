# 295 — Phase 3–5 Evidence Inventory

**Status:** ready
**Phase:** 3
**Dependencies:** 000

## Goal

Produce the verified/missing Phase 3–5 evidence ledger that gates every hardware and display ticket. This unlocks evidence-backed implementation after CPU gate 285; emulator changes and trace regeneration remain deferred.

## Execution Budget

One to two agent-days. Produce a validated inventory only; implement no emulator behavior.

## Required Reading

`docs/migration-evidence.md`, `docs/compatibility-policy.md`, and the Phase 3–5 sources under `../suunto-firmware/emulator/renode`, `../suunto-firmware/docs/research`, and `../suunto-firmware/emulator/results`.

## Current Baseline

The root ledger has only seed IDs `E-SAP-0001..0004`, `E-CPU-0001`, and `E-COMPAT-0001`. Renode models and traces exist, but permissive models are not proof of hardware behavior and raw captures may contain firmware-derived bytes.

## Allowed Files

Only `plans/evidence/phase3-5.tsv`, `plans/evidence/README.md`, and `plans/evidence/check.sh`. No other ticket may edit these paths.

## Frozen Interfaces

TSV columns are `id status product firmware_sha256 source_path source_symbol trace_path trace_sha256 observation negative_evidence`. Status is `verified` or `missing`; no blank field except trace fields for static metadata. `check.sh` rejects duplicates, unknown status, absent source/symbol, invalid lowercase SHA-256, and a verified behavioral row without a trace hash.

## Evidence Inputs

Inventory exactly: `E-A4-CLK-001`, `E-A4-PWR-001`, `E-A4-RST-001`, `E-A4-GPIO-001`, `E-A4-TIMER-001`, `E-A4-STIMER-001`, `E-A4-UART-001`, `E-A4-IOM-001`, `E-A4-MSPI-001`, `E-A4-DMA-001`, `E-A4-MRAM-001`; `E-SAP-PROFILE-001`, `E-SAP-FLASH-001`, `E-SAP-PANEL-001`, `E-SAP-BUTTONS-001`, `E-SAP-BACKLIGHT-001`, `E-SAP-HSPPAD143-001`, `E-SAP-LSM6DSL-001`, `E-SAP-TLI493D-001`, `E-SAP-HAPTIC-001`, `E-SAP-OPT3007-001`, `E-SAP-MAX17050-001`, `E-SAP-CXD5610-001`, `E-SAP-OHR2-001`, `E-SAP-COMPAT-PROD-001`, `E-SAP-COMPAT-GPS-001`, `E-SAP-COMPAT-OHR-001`; `E-NEMA-RING-001`, `E-NEMA-LISTS-001`, `E-NEMA-TEXTURE-001`, `E-NEMA-A2LE-001`, `E-NEMA-PANEL-001`, `E-SAP-INPUT-REPLAY-001`.

## Implementation

Hash exact source firmware/capture files locally and record source symbols. Do not copy proprietary bytes. Mark missing rather than extrapolating from a C# model. Use repository-relative paths and record the inspected `suunto-firmware` commit.

## Tests and Commands

`sh plans/evidence/check.sh` prints `phase3-5 evidence: valid` and exits 0. `awk -F '\t' 'NR>1 && $2=="missing"{print $1}' plans/evidence/phase3-5.tsv` prints every blocked ID. After 298, `make test TEST_FILTER=manifest` runs only `test_manifest` and exits 0.

## Acceptance

Every required ID exists once; verified behavioral rows name an exact source symbol and reproducible trace hash; hypotheses are `missing`; no user-home path or firmware/capture bytes are added.

## Forbidden Scope

No implementation, regenerated trace, invented register semantics, proprietary artifact, golden/root-doc edit, or promotion based only on an emulator return value.

## Handoff

Report verified/missing IDs, source commit, command results, and downstream tickets that remain blocked.
