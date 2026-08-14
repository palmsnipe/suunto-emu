# 418 — Sapporo Compatibility Records and Device Fixtures

**Status:** blocked
**Phase:** 4
**Dependencies:** 295, 298, 400, 416, 417

## Goal

Pin production records and GPS/OHR fixtures to exact hashes, triggers, and hit budgets. This enables the explicit no-device path in 420; automatic layers and broad device fallbacks remain deferred.

## Execution Budget

Two to three agent-days. Harden `sapporo-2.22-no-device` into exact-hash-pinned production, GPS, and OHR interventions with field-level provenance.

## Required Reading

`include/semu/compat.h`, `src/compat/{layer,sapporo_222}.*`, `tests/unit/test_{compat,sapporo_compat}.c`, `fixtures/sapporo/production-data.provenance.semu`, `docs/compatibility-policy.md`, and the three compatibility evidence rows from 295.

## Current Baseline

The layer matches profile ID only, has one global hit, reads a firmware CRC table, and installs synthetic ProductionData/ACCR/ACCC/MAGN/HLAT. It does not pin component hashes, distinguish recovered/synthetic fields in runtime events, enforce per-trigger budgets, or supply explicit GPS/OHR fixture providers.

## Allowed Files

Only `include/semu/compat.h`, `src/compat/{layer.c,sapporo_222.c,sapporo_222.h}`, `fixtures/sapporo/**`, `tests/unit/{test_compat.c,test_sapporo_compat.c}`, and `tests/devices/test_sapporo_fixture_providers.c`.

## Frozen Interfaces

Layer descriptors pin exact profile component hashes and contain interventions with trigger ID, predicate, effect, provenance, and individual maximum hits. Providers expose exact request→response lookup for GPS and OHR body requests. Every hit emits stable layer/trigger/ordinal/effect fields; wrong hash/state/budget returns compatibility refusal.

## Evidence Inputs

`E-SAP-COMPAT-PROD-001` must verify record layout/checksum and label each synthetic field; `E-SAP-COMPAT-GPS-001` and `E-SAP-COMPAT-OHR-001` must hash exact request/response transcripts. Any missing row removes that intervention; do not infer from current zero-fill code.

## Implementation

Split records and providers into files before 300 lines; validate exact hashes at enable time; make installation session-local; count each intervention separately; leave layer disabled by default.

## Tests and Commands

`make test TEST_FILTER=sapporo_compat` and `make test TEST_FILTER=sapporo_fixture` each select only matching binaries and exit 0; disabled, exact/wrong hash, wrong trigger/state, per-hit budget, log summary, record checksum, GPS/OHR match/miss, reset, and repeat events pass. `make check` exits 0.

## Acceptance

Every byte/field has provenance; wrong firmware cannot activate; unexpected requests refuse; layer-off behavior is unchanged; event sequence/hit counts repeat exactly.

## Forbidden Scope

No wildcard hash/profile, auto-enable, CPU instruction hook, broad device fallback, authentic identity claim, hidden zero body, physical device edits, or proprietary transcript bytes without permitted fixture provenance.

## Handoff

Report descriptor hashes, intervention table/hit budgets, provenance, event transcript, missing interventions, and 420 enable/install calls.
