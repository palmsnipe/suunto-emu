# 300 — Apollo4 Map, Clock, Reset, and GPIO

**Status:** blocked
**Phase:** 3
**Dependencies:** 230

## Goal

Freeze Apollo4 controller/board boundaries and implement the evidence-backed memory map, clock/power/reset, GPIO, and external IRQ routing needed for Sapporo startup.

## Allowed Files

`include/semu/{soc,peripheral_bus}.h`, `src/soc/apollo4/{soc,map,clock,power,reset,gpio}*`, `src/buses/**`, corresponding `tests/{unit,devices}/**`, Makefile source lists.

## Frozen Interfaces

Typed SPI/I2C/UART/storage transactions return `OK/WAIT/REFUSE`. SoC controllers own registers/DMA/IRQ; devices own protocol state; boards own addresses, chip selects, pins, and IRQ wiring. Explicit placeholder offsets require evidence and tests.

## Evidence Inputs

New ledger entries derived from `suunto-firmware` memory maps and traces; no status may advance without IDs.

## Implementation

Register only proven ranges; model reset values, write masks, clock/reset gates, GPIO direction/data/interrupt edges, and CPU external IRQ lines. Record unknown offsets rather than accepting them.

## Tests and Commands

`make test TEST_FILTER=apollo4_map`; `make test TEST_FILTER=apollo4_gpio`; `make test TEST_FILTER=peripheral_bus`; `make check`.

## Acceptance

Range/width/reset/GPIO edge and refusal tests pass; disabled/gated blocks behave explicitly; no global read-as-zero exists; controller/device contracts are frozen for parallel 310/315.

## Forbidden Scope

No timers, UART, IOM, MSPI, DMA implementation, board-specific pin constants in generic blocks, guessed offsets, or direct CPU-state mutation.

## Handoff

Report evidence IDs, register coverage, frozen bus callbacks, and paths reserved to tickets 310/315.

