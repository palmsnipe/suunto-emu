# 787 — RGBA4444 Pairing Strip Rendering Integration

**Status:** done
**Phase:** 7
**Dependencies:** 502,512,513,761

## Goal

Render the lane-observed RGBA4444 pairing strip and pass the Sapporo2.35
phone-instructions scroll without the current GPU fault/reset.

## Execution Budget

One bounded renderer module, backend dispatch, focused tests and an optional
SDL regression. Reuse existing deterministic arithmetic through a public
contract; no CPU arithmetic implementation changes.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
E-EMU-SAP235-SCROLL-001 and E-NEMA-RGBA4444-001, tickets502/512/513/761,
`include/semu/display.h`, `src/display/nema_state.h`, `nema_backend.h`,
`nema_backend_internal.h`, `nema_backend_draw.c`, `nema_backend_transaction.c`,
`src/cpu/armv7m/fpu_softfloat.h`, `fpu_softfloat.c`, `fpu_softfloat_convert.c`,
`tests/unit/test_nema_backend_atomic.c`.

## Current Baseline

A valid STR to CMDRINGSTOP is refused because source format06 is unimplemented.
Native setup resets after LOWER on the phone instructions. Paired native draw
captures and lane replays establish the exact supported strip, pixels and
wrong-shader refusal. Software binary32 evaluators already exist, but their
cross-module declarations must become public before the display can use them.

## Allowed Files

- `include/semu/fpu_math.h` (new public pure-arithmetic declarations)
- `src/cpu/armv7m/fpu_softfloat.h` (consume those same declarations)
- `src/display/nema_rgba4444.c`, `src/display/nema_rgba4444.h`
- `src/display/nema_backend_draw.c`, `src/display/nema_state.h`,
  `src/display/nema_state.c` (format06 required-program presence only)
- `tests/unit/test_nema_rgba4444.c`
- `tools/test_sdl_sapporo_235_scroll.sh`
- `README.md`, `docs/current-status.md`, `docs/migration-evidence.md`

The public header is explicitly integration-owned here. Planning adds this
ticket/index row before implementation. Integration scope refinement on
2026-09-22 adds the existing state validator: zero-valued MATMULT/IMEM_ADDR
must have actually been written, as the lane Matches predicate requires.
No new parallel state interface is needed. No existing ticket status changes.

## Frozen Interfaces

No CPU arithmetic implementation, opcode, scheduler, GPU MMIO/framing,
persistent format, profile, old golden or compatibility-budget changes.
Existing public display prepare/commit/abort contract remains authoritative.

## Evidence Inputs

E-NEMA-RGBA4444-001 pins native/synthetic lane pixels and refusal. Read-only
SapporoNemaP.cs methods IsProvenRgba4444Texture, ExecuteRgba4444Texture,
BilinearRgba4444 and Rgba4444 define the exact shader, descriptor, bilinear
sampling, transparent border, tint and rounded RGB565 blend. No other format
or general shader execution is authorized.

## Implementation

Admit only the observed quad/shader, 240x96 source stride480/sampling1,
240x240 RGB565 target stride480, white tint, bounded SRAM, ordered clip,
rectangular signed16.16 geometry and finite affine matrix. Decode RGBA nibbles
in little-endian texels. Match lane binary32 operations with the existing pure
integer softfloat evaluators; keep CPU FPSCR untouched. Preflight source bytes
with memory-only reads and stage the complete draw before mutation. Refuse
unsupported state/overlap, nonfinite/overflow mapping or missing memory.
Route format06 to this validator without widening existing texture paths.

## Tests and Commands

`make test TEST_FILTER=nema_rgba4444` must fail before implementation then pass.
`make test TEST_FILTER=nema`; `make test TEST_FILTER=fpu`;
`make check-task-contracts`; `make check-lines`; `make check`; `make sanitize`.
Paired native/synthetic outputs must byte-match the lane. Test wrong descriptors,
geometry/shader/filter/matrix, transparent borders, source holes/MMIO overlays,
atomic later-child refusal and successful retry. Run all seven2.35 firmware gates
with the private manifest and `sh tools/test_sdl_sapporo_235.sh`.
New scroll regression uses fixed input/instruction/time bounds and two identical
runs. Run affected other-profile SDL/era gates or explicitly report drift risk.

## Acceptance

Exact pixel comparisons, focused successful/refusal/atomicity tests and full
checks pass. Native LOWER scroll passes the old failure twice, with next
boundary reported. No firmware-derived pixels/logs enter Git. Physical panel
behavior and full onboarding are not inferred from one successful scroll.

## Forbidden Scope

No arbitrary RGBA formats/shaders, host floating-point dependence, broad
read-as-zero, CPU fault bypass, old pin weakening or compatibility ceiling lift.

## Handoff

Report evidence, files, commands, exact hashes and next gap. Leave status for
integrator review. Interface additions are limited to existing pure evaluators.

## Integrator acceptance (2026-09-23, delegated)

Verified on the consolidated commit: the ticket's recorded paired
firmware/SDL regressions and focused cases pass; `make check` reports 989 PASS
/ 0 FAIL, `make sanitize` 984 PASS / 0 FAIL, and 155 task contracts validate.
Status set `done`. Gaps listed in the handoff stay open and are owned by their
named follow-up tickets. Per the standing era-drift rule, 2.39 era pins may
have drifted after this change; tickets 777/783 own re-derivation.

