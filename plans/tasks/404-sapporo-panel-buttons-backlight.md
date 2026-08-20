# 404 — Sapporo Panel Transport, Buttons, and Backlight

**Status:** done
**Phase:** 4
**Dependencies:** 295, 298, 305, 310, 316

## Goal

Implement physical panel-transfer events, three-button edges, and backlight state without pixels. This unlocks headless wiring 420 and panel/input consumers 513/516; Nema and SDL remain deferred.

## Execution Budget

Two to three agent-days. Implement physical I/O boundary modules only; defer GPU command decoding and SDL.

## Required Reading

`semu_machine_input`, `include/semu/{input,display}.h`, `docs/research/native-panel-transport.md`, `production-button-probe.md`, `sapporo-2.22-ghidra-buttons.md`, `backlight-pwm-boundary.md`, and result logs `sapporo-production-button-*.log`/`sapporo-backlight-pwm-gate.log` in `suunto-firmware`.

## Current Baseline

Machine maps semantic buttons directly to pins 57–59 and inverts value; GPIO lacks IRQ until 305. No panel endpoint/backlight object exists. SDL polls quit only. Research warns MSPI1 staging rows are not themselves an authentic panel frame.

## Allowed Files

Only `src/devices/{sapporo_panel_transport,sapporo_buttons,sapporo_backlight}.{c,h}`, `tests/devices/test_sapporo_panel_io.c`.

## Frozen Interfaces

Panel is a strict typed MSPI endpoint that emits evidenced line/region transfer events but never fabricates pixels. Buttons translate upper/middle/lower press/release to configured GPIO levels. Backlight observes evidenced GPIO/CTIMER output and reports deterministic on/level state. Board pins/controllers are constructor data.

## Evidence Inputs

`E-SAP-PANEL-001`, `E-SAP-BUTTONS-001`, and `E-SAP-BACKLIGHT-001` must each be verified with exact controller/pin/polarity/framing or PWM traces. `E-NEMA-PANEL-001` is not a substitute for physical MSPI evidence. Block any submodule lacking its row.

## Implementation

Keep three state objects/files; validate full panel descriptor and input code before mutation; expose event observers for later replay/publication; reset releases buttons and restores evidenced backlight/panel idle state.

## Tests and Commands

`make test TEST_FILTER=sapporo_panel_io` runs only its binary and exits 0; exact line-transfer success/refusal, all six button edges, invalid code, reset release, backlight off/on/PWM boundary, and two-run event order pass. `make check` exits 0.

## Acceptance

Observed physical transcripts match; no event is labeled a frame; input polarity and release state are exact; unverified panel commands refuse; no SDL/Nema code is present.

## Forbidden Scope

No rasterization, GPU registers, SDL keys, invented DCS/DSI commands, pixel capture from staging rows, controller implementation, board hardcoding, or integration edit.

## Post-completion scope clarification

The deferred Nema/SDL wording describes the ticket-404 boundary. Later tickets
513–520 now consume this transport and publish software-renderer frames through
SDL with semantic input and replay. This ticket still does not prove physical
panel completion or add unobserved panel commands.

## Handoff

Report three interfaces, exact pins/polarity/controller, panel event schema, evidence hashes, and 420/513/516 connection points.
