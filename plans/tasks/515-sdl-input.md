# 515 — SDL3 Frame Presentation

**Status:** done
**Phase:** 5
**Dependencies:** 298, 513

## Goal

Isolate validated RGB565 SDL3 presentation behind a dependency-free core. This supplies the presenter consumed by 520; semantic input and CLI wiring remain deferred.

## Execution Budget

One to two agent-days. Isolate SDL3 texture/window presentation behind a testable frame-validation core; do not wire CLI/input yet.

## Required Reading

`src/frontends/main_sdl.c:prepare_frontend/publish_frame/destroy_frontend`, `include/semu/frame.h`, current `Makefile` SDL targets, and SDL3 texture/pitch APIs used by the baseline.

## Current Baseline

`main_sdl.c` creates a 2× window and `SDL_PIXELFORMAT_RGB565` streaming texture, updates/presents frames, and polls only quit. It does not validate size versus stride completely, expose `--scale`, separate presentation for tests, or preserve detailed SDL errors.

## Allowed Files

Only `src/frontends/{sdl_present.c,sdl_present.h,sdl_present_core.c,sdl_present_core.h}` and `tests/unit/test_sdl_present.c`.

## Frozen Interfaces

Core validates RGB565LE format, nonzero dimensions, `stride>=width*2`, and `size>=stride*height`, returning normalized presentation descriptor without SDL types. SDL presenter owns window/renderer/texture, recreates only on dimension/scale change, uses nearest scaling and caller-provided integer scale 1–8.

## Evidence Inputs

No hardware evidence beyond `E-SAP-0001` 240×240 RGB565. This ticket displays supplied frames and makes no authenticity claim.

## Implementation

Move presentation code from `main_sdl.c` conceptually without editing it; make lifecycle partially initialized-safe; copy/update using declared stride; preserve error string/status for integration.

## Tests and Commands

`make test TEST_FILTER=sdl_present` runs only dependency-free core tests and exits 0 for valid 240×240/stride 480, padded stride, bad format/size/scale/overflow. `SDL_VIDEODRIVER=dummy make check-sdl` builds and exits 0 after 520 integrates presenter. Until 520, `make sdl` must compile this module. `make check` remains SDL-free and exits 0.

## Acceptance

Core validation is exhaustive/overflow-safe; presenter handles recreate/destroy/error paths; headless binary has no SDL symbol/link; no guest-visible timing changes.

## Forbidden Scope

No event/input mapping, CLI edit, renderer/raster, frame generation, host-time advancement, screenshot write, mandatory SDL, or `main_sdl.c` edit.

## Post-completion scope clarification

The deferred semantic-input/CLI wording describes the presenter-only boundary.
Tickets 516–520 subsequently wired semantic input, deterministic replay, and
the CLI/live SDL flow. SDL remains optional; the headless build remains free of
SDL symbols and host-time advancement.

## Handoff

Report presenter API, validation cases, SDL version/dummy result, headless link audit, and 520 integration calls.
