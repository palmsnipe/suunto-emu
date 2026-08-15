# 420 — Sapporo 2.22 Headless Integration

**Status:** done
**Phase:** 4
**Dependencies:** 400, 402, 404, 410, 411, 412, 413, 414, 415, 416, 417, 418

## Goal

Replace legacy aggregate wiring and reach the exact `sapporo-startup-complete` headless checkpoint twice. This unlocks Nema trace/replay work 490; frame rendering and SDL remain deferred.

## Execution Budget

Two to three agent-days. Replace legacy aggregate wiring and reach the exact bounded startup/WFI checkpoint with optional explicit compatibility.

## Required Reading

`src/boards/machine.c`, `src/devices/sapporo_devices.{c,h}`, `tests/devices/test_sapporo_devices.c`, all Phase 4 handoffs, `docs/research/sapporo-2.22-startup-peripheral-map.md`, and `emulator/results/sapporo-startup-completion-gate.log`.

## Current Baseline

Machine validates/maps three RAM ranges and Apollo4, loads components, optionally installs production records, and runs CPU. It never instantiates any Sapporo endpoint; input only changes GPIO levels; frame callback is unused. Legacy aggregate tests cover pressure, accelerometer, and OHR in isolation.

## Allowed Files

Only `src/boards/machine.c`, `src/devices/sapporo_devices.{c,h}`, `tests/devices/test_sapporo_devices.c`, `tests/integration/test_sapporo_headless.c`, and `tests/private/sapporo-2.22.60/headless.expected`.

## Frozen Interfaces

Machine consumes 400 wiring, owns all endpoint lifetimes, attaches through Apollo4 320, passes optional display backend from 298, and resets in documented order. NULL display backend completes only verified GPU/panel submissions and publishes no frame. Legacy factory is removed after all callers migrate.

## Evidence Inputs

All dependency rows must be verified or the absent device must remain unattached and authentic run must stop at that exact refusal. The success gate requires a trace hash for named `sapporo-startup-complete` PC/state and exact firmware manifest; compatibility hits must match 418 budgets.

## Implementation

Instantiate/wire/reset/destroy each verified device, route IRQ/ready/events, retain source-image guards, and normalize ordered checkpoint/device/layer summary. Add synthetic bounded coverage independent of private firmware and private test skip only for absent manifest.

## Tests and Commands

`make test TEST_FILTER=sapporo_headless` runs only its synthetic integration binary, reaches `sapporo-startup-complete` under 1000000 instructions/500000000 virtual ns, and exits 0 twice. With private firmware: `build/suunto-emu run --profile sapporo-2.22.60 --firmware "$SEMU_FIRMWARE_MANIFEST" --layer sapporo-2.22-no-device --until wfi --max-time 1000000000` exits 0 with evidenced `stop=wfi-deadlock` and no unexpected refusal. `make sanitize TEST_FILTER=sapporo_headless` and `make check` exit 0.

## Acceptance

Exact firmware reaches the evidenced checkpoint; source hashes are unchanged; no unsupported instruction/unmapped/unexpected device transaction occurs; two summaries match; layer-off fails at the documented missing-state boundary.

## Forbidden Scope

No Nema rendering/frame hash, permissive endpoint, hidden compatibility, firmware patch, private bytes, later profile, unbounded run, component semantic edit, or CLI redesign.

## Handoff

Report final wiring/lifetime/reset table, bounded normal/layer-off summaries, compatibility hits, source before/after hashes, and Phase 5 display attachment seam.
