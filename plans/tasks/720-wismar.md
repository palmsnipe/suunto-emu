# 720 — Wismar Profile and Hardware

**Status:** blocked
**Phase:** 7
**Dependencies:** 710

## Goal

Onboard exact Wismar 2.46 with its independent code window, `0xf0` NVIC mask behavior, SRAM alias, Timer14 behavior, MSPI2 startup, native display, and input.

## Allowed Files

New Wismar profile/board/device modules and tests, narrowly evidenced SoC extensions, registries, docs coverage/evidence, Makefile lists, `plans/index.tsv` status only.

## Frozen Interfaces

Board profile declares code window and alias mappings without changing generic bus overlap rules. Wismar-specific NVIC mask/Timer14 behavior is isolated unless evidence proves a reusable Apollo4 feature.

## Evidence Inputs

Exact Wismar 2.46 hashes and traces for code/vector, SRAM alias, NVIC mask, Timer14, MSPI2, panel, and inputs.

## Implementation

Reach bounded reset first; add alias semantics and explicit registers; reproduce MSPI2 startup; obtain native display transaction; then enable semantic input and deterministic replay.

## Tests and Commands

`make check`; `make test TEST_FILTER=wismar`; `make test-firmware FIRMWARE_ROOT="$FIRMWARE_ROOT" TEST_PROFILE=wismar-2.46`; run all Sapporo/Ulsan regression gates.

## Acceptance

Exact firmware validates and reaches display/input checkpoints; alias and mask tests cover boundaries/refusals; Timer14/MSPI2 transcripts repeat exactly; prior products remain unchanged.

## Forbidden Scope

No global `0xf0` mask assumption, generic timer quirk without proof, guessed aliases/registers, profile inheritance by SoC name, or Tianjin/Rostock work.

## Handoff

Report evidence-backed Wismar differences, isolation decisions, checkpoints, and complete regression results.

