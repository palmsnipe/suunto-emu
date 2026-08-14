# 415 — Sapporo GPS/OHR and Compatibility Layer

**Status:** blocked
**Phase:** 4
**Dependencies:** 400

## Goal

Implement observed GPS UART and OHR2 transport, plus a strictly pinned opt-in `sapporo-2.22-no-device` compatibility layer for genuinely missing startup state.

## Allowed Files

`src/devices/{gps,ohr2}*`, `src/compat/sapporo_2_22_no_device*`, matching `tests/devices/**` and `tests/unit/compat_*`; no board/profile registry or public-header edits.

## Frozen Interfaces

Use typed UART/device transports and compatibility API frozen by 400. Layer declaration pins every profile component hash, trigger/effect, evidence, field provenance, and hit budget; wrong activation/runtime state yields compatibility refusal.

## Evidence Inputs

`E-COMPAT-0001` plus new field-level manufacturing and byte-exact GPS/OHR transcript ledger entries. If those entries cannot be produced, leave the associated intervention unimplemented and fail closed.

## Implementation

Prefer real transport behavior; add synthetic persistent records and minimal response fixtures only for evidenced gaps; emit structured event per hit and an end summary.

## Tests and Commands

`make test TEST_FILTER=device_gps`; `make test TEST_FILTER=device_ohr`; `make test TEST_FILTER=compat`; `make test TEST_FILTER=compat_determinism`; `make check`.

## Acceptance

Transport reset/success/refusal tests pass; layer disabled/enabled/wrong-hash/wrong-state/hit-budget/logging cases pass; valid CPU instructions are never intercepted; repeated event records match.

## Forbidden Scope

No auto-enabled layer, wildcard firmware hashes, silent hook, broad firmware patching, fabricated transcript, sensor work owned by 410, or changing normal hardware responses when layer is disabled.

## Handoff

Report which hypotheses gained evidence, exact layer interventions/hit limits, and gaps that must stop Phase 4.

