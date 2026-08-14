# 300 — Apollo4 Clock Generator

**Status:** blocked
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Replace the clock placeholder with the exact `E-A4-CLK-001` behavior. This supplies the clock block required by Apollo4 integration 320; power, reset, and board policy remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Replace the retained-word clock placeholder with the exact Sapporo-startup clock behavior proven by trace `E-A4-CLK-001`.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`src/soc/apollo4/apollo4.c:init_bank` maps `apollo4.clock` at `0x40004000/0x800` and permits only offset `0x84`; `semu_regbank_*` retains arbitrary values. There are no masks, reset values, readiness transitions, gates, IRQs, or negative-width tests.

## Allowed Files

Only `src/soc/apollo4/{clock.c,clock.h}` and `tests/devices/test_apollo4_clock.c`.

## Frozen Interfaces

`clock.h` declares opaque `semu_apollo4_clock`, create/destroy/reset, and width-aware bus ops. Create accepts bus and scheduler; registration base/size stays `0x40004000/0x800`. All offsets and masks are private; unknown offsets/widths return `SEMU_ERR_UNSUPPORTED` before mutation.

## Evidence Inputs

`E-A4-CLK-001` must be `verified`, cite `../suunto-firmware/emulator/renode/suunto-sapporo.repl:clkgen_sapporo`, and include an ordered 2.22.60 MMIO trace showing offset `0x84`, reset value, write/read masks, and any observed polling. A permissive Python register bank alone is insufficient; if the trace is absent, block.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_clock` runs only `test_apollo4_clock`, prints its pass summary, and exits 0. The test covers reset, every evidenced access, wrong width, adjacent `0x80`, and two-run equality. `make check` exits 0.

## Acceptance

The evidenced transcript matches byte-for-byte; reset and mask assertions pass; `0x80` and every unlisted offset refuse; the module contains no board/product branches.

## Forbidden Scope

No power/reset/GPIO/timer behavior, invented stabilization delay, broad retained register bank, board wiring, public-header edit, or fallback read zero.

## Handoff

Report supported offsets/masks/reset values, evidence trace SHA-256, test output, and integration calls needed by 320.
