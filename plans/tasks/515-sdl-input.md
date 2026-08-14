# 515 — SDL3 Frontend and Board Input

**Status:** blocked
**Phase:** 5
**Dependencies:** 420

## Goal

Add an optional SDL3 frontend that presents RGB565 frames and injects semantic three-button input without affecting headless builds.

## Allowed Files

`src/frontends/sdl3_*`, SDL-specific tests, `Makefile` SDL targets, frontend option parsing; no display renderer, board internals, or mandatory dependencies.

## Frozen Interfaces

Use frame/input APIs frozen by 120/500 and the same run options as headless. Add only `--scale` and explicit key/input mapping options. SDL3 is discovered with `pkg-config`; `make` never probes or links it, while `make sdl` builds `build/suunto-emu-sdl`.

## Evidence Inputs

`E-SAP-0001`; SDL3 public API available on target hosts.

## Implementation

Create/update a texture with declared RGB565 format/stride; map default keys to upper/middle/lower press/release; preserve virtual-time determinism by queuing semantic input at run boundaries.

## Tests and Commands

`make clean && make`; `make sdl`; `SDL_VIDEODRIVER=dummy make check-sdl`; `make test TEST_FILTER=input_mapping`; `otool -L build/suunto-emu 2>/dev/null || ldd build/suunto-emu`.

## Acceptance

Headless builds without SDL3; SDL target reports a clear missing-package error; dummy-driver smoke publishes a frame and all press/releases; input replay is identical across two runs.

## Forbidden Scope

No SDL dependency in core/public headers, host-time guest advancement, renderer implementation, audio/touch/crown, global key state polling, or source firmware access.

## Handoff

Report SDL version tested, default mappings, headless link audit, and dummy-driver results.

