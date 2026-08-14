# 516 — SDL Semantic Input Mapping

**Status:** blocked
**Phase:** 5
**Dependencies:** 295, 298, 404, 420

## Goal

Translate SDL-normalized key edges into deterministic upper/middle/lower semantic events. This unlocks replay/golden integration 518/520; direct GPIO and host-time behavior remain deferred.

## Execution Budget

One to two agent-days. Translate normalized frontend key events into deterministic upper/middle/lower semantic press/release events.

## Required Reading

`include/semu/input.h`, `semu_machine_input`, `src/frontends/main_sdl.c`, `E-SAP-BUTTONS-001`, `production-button-probe.md`, and native button JSONL/logs referenced by 295.

## Current Baseline

SDL polls only quit. Machine supports button kind/codes 0–2 and maps to GPIO pins, but key choices, repeat suppression, release handling, event order, and injection while running are absent.

## Allowed Files

Only `src/frontends/{semantic_input.c,semantic_input.h,sdl_input.c,sdl_input.h}` and `tests/unit/test_semantic_input.c`.

## Frozen Interfaces

Dependency-free mapper consumes normalized key `{key,down,repeat,sequence}` and emits zero/one `semu_input_event`; default keys are fixed and documented by enum, configurable map rejects duplicates. Repeats emit nothing; state suppresses duplicate press/release; focus loss emits releases in upper/middle/lower order. SDL adapter only normalizes SDL events.

## Evidence Inputs

`E-SAP-BUTTONS-001` proves semantic order and press/release polarity at board boundary. `E-SAP-INPUT-REPLAY-001` must contain expected middle/lower event order from native JSONL; key choices themselves are frontend defaults, not hardware evidence.

## Implementation

Keep mapper SDL-free; track three pressed bits; validate map/event before state mutation; expose quit separately; do not call machine directly so replay and live input share identical semantic events.

## Tests and Commands

`make test TEST_FILTER=semantic_input` runs only its binary and exits 0; all three press/releases, repeats, duplicate state, focus-loss order, invalid/duplicate mapping, quit separation, and two-run event bytes pass. `make check` exits 0; after 520, `SDL_VIDEODRIVER=dummy make check-sdl` exits 0.

## Acceptance

One physical edge yields one semantic event; releases cannot stick across focus/reset; mapper output is SDL/version independent; native expected order matches evidence.

## Forbidden Scope

No direct GPIO pin, host key polling, virtual-time change, replay file parser, SDL presentation, machine/CLI/main_sdl edit, crown/touch, or board polarity.

## Handoff

Report default/configurable map, normalized event API, evidence order, tests, and 518/520 pump calls.
