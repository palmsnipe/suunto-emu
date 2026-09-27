# 797 — Sapporo 2.39 OHR2 BSL Refusal-Semantics Attribution

**Status:** ready
**Phase:** 7
**Dependencies:** 729

## Goal

Attribute and law-derive the OHR2 BSL device-semantics change that turned
every pinned-era BSL refusal into `status=ok` on the accepted-batch tree:
find the exact commit whose change flipped the 2.39 OHR2 BSL refusal
behavior, record the lane/RE evidence for the correct semantics, and
re-derive the seven OHR2-era scripts' refusal goldens from the corrected
law.

## Execution Budget

Two to three model-days: one rebuild-bisect over the
69b1b35..947f4bb window (nine OHR/device-touching commits named below),
one evidence entry, then the seven scripts re-derived twice-identically.

## Required Reading

`AGENTS.md`; `docs/migration-evidence.md` entries E-SAP-COMPAT-FILES-239-001
and the 710-series OHR fixture entries;
`src/devices/` OHR-adjacent modules; ticket 777's final classification
record (B3 root cause); the seven era scripts named in Acceptance.

## Current Baseline

On HEAD 947f4bb the 2.39 OHR2 boot session completes
`0x0010 → 0x0000 → 0x0003 → …` entirely `status=ok`, with ZERO refused OHR
transactions through 440M instructions (~2.17 s guest time). The pinned
refusals — `0x0010 seq=0` (gpio_wt1 resume golden), `0x0000 seq=1`
(ohr2_boot_mode), `0x0000 seq=3 MAIN` (ohr2_bsl_identity), `0x000d seq=4`
(ohr2_main_identity), `0x000e seq=5` (ohr2_result_13), `0x0006 seq=6`
(ohr2_result_14) — are all now `status=ok`, and the whole session
relocated ~+2.34M instructions (+~0.8 ms guest time) past the pinned caps.
Cold-side ok-transcript goldens still exist in the new era; only boundary
positions and refuse→ok semantics moved. A 69b1b35 control build
reproduces every old pin exactly, so the change is inside the accepted
batch, not local nondeterminism. gpio_wt1 additionally lost its pinned
boundary OHR line entirely.

## Allowed Files

Attribution stage: no source changes (bisect/rebuild/report only).
Implementation stage follows the evidence: the OHR2 device or compat
module the bisect names, focused unit tests, the seven era scripts named
below (re-derivation only), `docs/migration-evidence.md`,
`docs/current-status.md`, this ticket. `Makefile`, registries, and
profiles stay integration-owned.

## Frozen Interfaces

Deterministic checkpoints, the era-script refusal-grep guards, the
2.35-profile OHR fixture law, firmware safety, and every other profile's
goldens. If attribution lands on a 2.35-scoped change that was intended to
be profile-scoped, the fix restores scoping rather than re-deriving 2.39
around it.

## Evidence Inputs

Bisect candidates (the nine commits in 69b1b35..947f4bb that touch OHR,
BSL, or device-fixture code): 1bc1ce8, 6ae8ce5, 7c8bb59, 40c9287, 655e3cd,
d8bfba9, cd1de52 (unsplittable mega-commit), 0c84673, 43ef2ce/a9c1745/
66ef318/4c99fac/b435217 (profile-scoped RTC/gauge family — check for
shared-code leakage). d8bfba9 is ALREADY attributed for the timer_pattern
-62 drift and must not be double-attributed without its own proof. Probe
window: 440M instructions twice byte-identical per candidate.

## Implementation

Bisect first (rebuild per candidate, run one OHR2 probe window, record
refuse/ok); verify the named commit's diff explains the flip; then derive
the correct law (if the flip is intended lane behavior, the entry records
it and the seven scripts re-derive refuse→ok with session relocations; if
it is leakage, fix the scoping and the old goldens recover). No golden is
rewritten before the law entry exists.

## Tests and Commands

`make check` plus the seven era scripts twice each with
`SEMU_EMULATOR/TEST_PROFILE/SEMU_FIRMWARE_MANIFEST/SEMU_SAPPORO_239_FULL_FLASH`
set; `make check-era` moving the seven names from red to green;
`make sanitize` if the implementation stage touches engine sources.

## Acceptance

A bisect record naming the exact commit (or the cd1de52 mega-commit with
its diff narrowed by file), a hash-pinned evidence entry for the OHR2 BSL
law, and all seven scripts — gpio_wt1, ohr2_boot_mode, ohr2_bsl_identity,
ohr2_echo, ohr2_main_identity, ohr2_result_13, ohr2_result_14 — exit 0
twice with re-derived pins whose stop reasons and refusal semantics match
the recorded law.

## Forbidden Scope

No refuse→ok golden rewrite before the evidence entry, no weakening of
refusal-grep guards, no changes to 2.35 OHR fixture goldens as a
workaround, no silent profile-scope relaxation.

## Handoff

Integrator-created 2026-09-27 from the 777 re-pin batch's B3 finding;
the batch left all seven scripts untouched and the 69b1b35..947f4bb
search space recorded.
