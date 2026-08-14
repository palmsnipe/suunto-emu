# 305 — Apollo4 GPIO and Interrupt Edges

**Status:** blocked
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Implement verified GPIO MMIO, edges, latches, and IRQ delivery. This unlocks Apollo4 integration 320 and Sapporo panel/input ticket 404; board pin policy remains deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Implement evidence-backed GPIO MMIO, pin direction/data, input edges, interrupt status/masks, and CPU IRQ delivery.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`semu_apollo4_set_gpio_input/get_gpio_input` only mutate a 128-byte level array initialized high. No GPIO region is mapped; direction, output, edge selection, status/clear, NVIC route, and reset behavior are absent.

## Allowed Files

Only `src/soc/apollo4/{gpio.c,gpio.h}` and `tests/devices/test_apollo4_gpio.c`.

## Frozen Interfaces

Opaque GPIO create/destroy/reset and bus ops; input injection is `(pin, level)`, output observer is optional, IRQ sink is `(irq, level)`. Validate pin before state change. Equal-level injection is idempotent; edge IRQ state follows evidenced latch/clear semantics.

## Evidence Inputs

`E-A4-GPIO-001` must cite `sapporo-extensions.repl` IRQ routes and an authentic GPIO MMIO trace. `E-SAP-BUTTONS-001` supplies pins 57/58/59 polarity separately and cannot be used to infer GPIO register layout. Block if register offsets, bank layout, or IRQ line are missing.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_gpio` runs only `test_apollo4_gpio` and exits 0; expected cases cover reset, in/out, rising/falling/both/disabled, status clear/reassert, invalid pin 128, wrong width/offset, and repeated IRQ transcript. `make check` exits 0.

## Acceptance

All verified MMIO and IRQ events match; pin 127 boundary passes; rejected access is atomic; no board pin numbers occur in module source/tests except generic boundary vectors.

## Forbidden Scope

No button mapping, backlight PWM, guessed GPIO banks, polling host input, direct CPU-state writes, generic interrupt auto-clear, or integration-file edit.

## Handoff

Report register/bank layout, IRQ sink calls, evidence hash, and board APIs consumed by 400/410.
