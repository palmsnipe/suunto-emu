# 500 — Nema Ring and Command Framing

**Status:** blocked
**Phase:** 5
**Dependencies:** 285, 298, 490

## Goal

Decode verified ring/wrap/child-list framing into atomic register-write records. This unlocks state, texture, and diagnostic tickets 502/504/506; register semantics and pixels remain deferred.

## Execution Budget

Two agent-days. Decode ring bootstrap/wrap and complete child-list framing into validated register-write records.

## Required Reading

`fixtures/display/nema/**`, `native-nema-ring-bootstrap-decode.md`, `SapporoNemaP.cs:CompleteNativeMarker/ExecuteSubmittedChildren/ExecuteObservedChild`, and Ambiq command constants cited by the evidence note.

## Current Baseline

There is no Nema MMIO/command parser. The prior Renode model reads ring words and accepts only observed suffixes, child sizes, hold commands, and register pairs; it also contains rendering/state logic that must not be copied into framing.

## Allowed Files

Only `src/display/{nema_framing.c,nema_framing.h}` and `tests/unit/test_nema_framing.c`.

## Frozen Interfaces

Parser input is bus plus ring base/word count/old/new pointer; output callback receives ordered `{prefix,register,value,source_address}` records and list boundary. Validate alignment, wrap, word arithmetic, `CL_NOP`, hold prefix, `CMDADDR`, `CL_PUSH|CMDSIZE`, and complete child range before callback. Return `OK/REFUSE`; no partial callbacks on malformed input.

## Evidence Inputs

`E-NEMA-RING-001` establishes bootstrap/no-IRQ and wrap trailer; `E-NEMA-LISTS-001` establishes complete suffix/list sizes. Source symbols are the three methods above. Prefix-only captures remain refusal tests, never valid list inputs.

## Implementation

Stage decoded records in a bounded buffer before emitting; use checked bus reads and size/word conversion; keep all register semantics opaque; produce deterministic refusal category/address/ordinal.

## Tests and Commands

`make test TEST_FILTER=nema_framing` runs only its binary and exits 0; bootstrap, wrap, NOP, complete child, multiple child, prefix/truncation, bad alignment/range/size/prefix/tail, callback refusal, and repeat output pass. Each case caps list words at 4096. `make check` exits 0.

## Acceptance

Synthetic/evidenced framing outputs match corpus; bootstrap emits no IRQ/draw; malformed lists emit zero records; no texture/pixel/register state is interpreted.

## Forbidden Scope

No render/state/texture semantics, unbounded allocation, treating size as bytes when evidence says words, firmware memory write, completion IRQ, or support for unobserved suffixes.

## Handoff

Report accepted grammar/limits, refusal categories, evidence/corpus cases, tests, and callback contract for 502/504/506.
