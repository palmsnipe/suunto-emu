# 700 — Later Sapporo Profiles

**Status:** blocked
**Phase:** 7
**Dependencies:** 610

## Goal

Add exact Sapporo 2.33, 2.35, and 2.39 profiles sequentially, using opt-in missing-cache/logical-storage layers only where evidence requires them.

## Allowed Files

New `profiles/sapporo-*.semu`, version-specific Sapporo board/profile/compat modules, matching tests/golden metadata, registries, docs coverage/evidence, Makefile lists, `plans/index.tsv` status only.

## Frozen Interfaces

Existing CPU/SoC/device/display contracts remain unchanged. Each version pins every component hash and gets independent checkpoint/golden metadata. Compatibility IDs include product/version and never inherit hash sets.

## Evidence Inputs

Exact user-supplied firmware manifests, version-specific memory/traffic traces, and new ledger entries for cache/storage gaps.

## Implementation

For each version in order: validate profile; reach bounded reset fault; add only observed version behavior; obtain native display transaction; enable buttons; then consider a documented compatibility layer.

## Tests and Commands

`make check`; `make test TEST_FILTER=sapporo_profiles`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=sapporo-2.33`; repeat for 2.35 and 2.39; run repeated-record comparison per version.

## Acceptance

Each exact profile independently validates, boots to its declared checkpoint, publishes native output, accepts input, preserves source hashes, and refuses wrong-version layers; earlier Sapporo goldens still pass.

## Forbidden Scope

No family-wide hash wildcard, implicit layer, assuming identical memory/wiring, direct package extraction, Ulsan work, or weakening earlier device refusal behavior.

## Handoff

Report a per-version evidence, layer, hardware-status, checkpoint, and regression table.

