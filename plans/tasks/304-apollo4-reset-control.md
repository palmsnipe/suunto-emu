# 304 — Apollo4 Reset and MCU Control

**Status:** done
**Phase:** 3
**Dependencies:** 285, 295, 298, 300, 302

## Goal

Implement deterministic reset/MCU-control values and ordered block callbacks. This freezes reset sequencing for integration 320; CPU reset and fabricated factory data remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Model reset-visible MCU control values and deterministic reset/gate ordering without hiding unimplemented peripherals.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`semu_apollo4_reset` only fills GPIO levels and clears five regbanks. `machine.c:semu_machine_reset` resets scheduler, bus, SoC, loads components, installs compatibility, then resets CPU. `suunto-sapporo.repl:mcu_ctrl_sapporo` returns CHIPREV `0x21`; this block is not mapped locally.

## Allowed Files

Only `src/soc/apollo4/{reset.c,reset.h,mcu_control.c,mcu_control.h}` and `tests/devices/test_apollo4_reset.c`.

## Frozen Interfaces

Reset controller exposes create/destroy/reset, checked bus ops, and an ordered callback list registered once before first reset. MCU control maps only evidenced range/offsets. Callback order is registration order and cannot allocate/schedule implicitly.

## Evidence Inputs

`E-A4-RST-001` must cite `../suunto-firmware/emulator/renode/sapporo.resc:reset`, `suunto-sapporo.repl:mcu_ctrl_sapporo`, exact vector MSP/PC evidence, and an authentic reset MMIO trace hash. Any reset register or effect absent from that row remains refused.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_reset` runs only its binary and exits 0; expected cases cover CHIPREV, reset order, repeated reset, wrong width/offset, stale scheduled-event absence, and callback failure propagation. `make check` exits 0.

## Acceptance

Reset order is deterministic and matches the contract; CHIPREV reads exactly the verified value; callback failure stops reset without continuing; unknown reset/MCUCTRL accesses refuse.

## Forbidden Scope

No CPU reset implementation, INFO1 fabrication, clock/power semantics beyond invoking their reset callbacks, board profile, permissive MCU register page, or firmware patch.

## Handoff

Report ordered reset phases, supported MCU offsets, evidence hash, and required 320 wiring.
