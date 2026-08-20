# Current Implementation Status

This file records the implemented baseline without weakening the roadmap gates.
The ticket index remains authoritative: a ticket is `done` only when its full
acceptance conditions pass, even if useful pieces of later tickets already
exist.

## Implemented Baseline

- Dependency-free C99 headless build and optional SDL3 build.
- Strict profile and firmware manifests with exact size and SHA-256 checks.
- Fail-closed little-endian memory bus and stable integer-time scheduler.
- Immutable storage bases with sparse session-only program/erase overlays.
- Bounded machine execution, structured stop reasons, and semantic board input.
- An in-tree Thumb interpreter foundation with native regression coverage for
  `MOV.W r0,sp` and `STMDB`.
- Narrow Apollo4 register banks and GPIO input state with unknown-offset refusal.
- Small pressure, LSM6DSL, and OHR2 transaction models with refusal tests.
- Exact Sapporo 2.22.60 component metadata and an opt-in, hit-bounded synthetic
  manufacturing-state compatibility layer.
- RGB565 surface and SDL3 presentation primitives.
- Identity-pinned machine snapshots covering CPU, guest RAM, scheduler events,
  Apollo4 controller state, Sapporo device state, flash overlays, NEMA state,
  virtual time, and compatibility counters. Snapshot load is atomic on a
  malformed or incompatible image and keeps firmware/resource files external.

All normal tests use synthetic inputs. The checked-in repository contains no
firmware bytes or frame pixels.

## Authentic-Firmware Boundary

On 2026-08-19, user-supplied OTA components matching all three profile hashes
were validated with the named compatibility layer. Two fresh bounded OTA-only
runs produced byte-identical stage logs and reached:

```text
time_ns=0 production-data
378713110 ohr-startup
1011008641 gps-state-startup
1209669280 gps-startup
4806834547 resource-status
5192876953 diap-worker-wake
5192877054 diap-worker-irq
SDL first-frame width=240 height=240 generation=3
stop=user pc=0x0009a3fc instructions=450900000 virtual_time_ns=5333307331
```

The run reached production startup, OHR/GPS startup, the OTA resource-status
boundary, NEMA initialization, and the first native command submission without
a reset, assertion, or device refusal. The NEMA model now consumes the observed
marker-only completion transaction and raises the evidenced CLID/INTERRUPT
completion. The SDL3 frontend receives a non-black 240x240 renderer frame from
the authentic firmware command stream. A2LE sub-LSB rounding remains an
explicit software-renderer approximation; physical-panel completion, panel
wire bytes, and generic factory-runtime behavior remain unsupported.

The renderer also now carries the observed binary32 affine matrix registers
through NEMA snapshots and applies the native A2LE destination-to-source
translation path. Two fresh SDL dummy runs with middle (5.400/5.470 seconds)
and lower (8.000/8.070 seconds) semantic button pulses were byte-identical;
both accepted the language/setup command lists and reached the bounded budget
without reset or display refusal. This exposes the evidenced setup UI in SDL;
it does not claim physical-panel or pixel-golden equivalence. The
`--until middle-language` replay checkpoint still stops at the first
post-input non-black setup frame. Live SDL checkpoints wait for a bounded
350-ms virtual-time quiet window after the last post-input renderer submission,
so an intermediate logo/text transition is not frozen as the interactive frame.
SDL button edges hold active-low for 70 ms of guest time and keep the released
level stable for 70 ms before another press, matching the native debounce
boundary. The SDL first-frame diagnostic also includes a bounded CRC32 of the
presented RGB565 bytes; the live checkpoint accepts only visible pixels from a
new renderer generation, excluding stride padding and stale submissions.

SDL also accepts the same `middle-language` and `lower-transition` checkpoints
without `--input-replay`. In that live mode Arrow Up, Return/Enter, and Arrow
Down are delivered through the semantic input mapper; after a quiet settled
post-button frame the window pauses for the next live button edge so the setup
UI can be navigated manually. The named checkpoint button is required only for
the first edge; subsequent setup edges accept any of the three mapped buttons.
Pressing that edge returns control to the guest immediately and rearms the next
settled frame; replay checkpoints retain their deterministic stop behavior.

The reset boundary diagnostic now records the request PC, LR, SP, R0–R3, xPSR,
runtime reset count, compatibility hit total, and virtual time without changing
guest execution. OHR2 also emits a bounded 64-event-per-device-lifetime
transaction/ready trace with command, sequence, BSL/MAIN state, and completion
status. With the exact private Sapporo 2.33.16 package, two fresh bounded runs
are identical (E-SAP-0015): the guest requests `SYSRESETREQ` at `0x000c97f2`
with `compat_hits=0` and then continues to the bounded budget. The exact
2.22.60 no-layer run likewise repeats `SYSRESETREQ` at `0x000be93e` with
`compat_hits=0`; the opt-in layer's OHR trace reaches BSL identity/configure,
MAIN transition, result, and echo exchanges with matching ready edges and no
refusal (E-SAP-0016). These are reproducible emulator observations, not a later-version
behavior fix; a native reset-register or post-reset transaction trace is still
required before changing Apollo4 reset semantics.

## Next Actionable Work

The ticket index currently has no `ready` tickets: Phases 0–6 are complete, and
the Phase 7 expansion templates remain blocked until their product-specific
evidence is instantiated. The practical work queue is:

- Audit the available later-Sapporo packages and traces, then instantiate one
  observed-gap task only if an exact failing transaction and provenance exist.
- Continue the Sapporo setup flow from the cached UI checkpoint, adding only
  the next reproducible transition and its input/refusal regression. This is
  still an SDL renderer milestone, not a physical-panel claim.
- Measure snapshot-resume and frame-loop cost before making any performance
  change; retain deterministic virtual time and guest behavior.

The blocked Phase 7 templates must not be treated as permission to infer later
product wiring, storage, display, or input behavior. Missing evidence remains a
refusal until a read-only native package or trace supplies the exact contract.
