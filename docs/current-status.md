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
it does not claim physical-panel or pixel-golden equivalence.

## Next Actionable Ticket

Phase 1 is complete. Three independent tickets are ready:

- 180 pins primary CPU references and the instruction-vector contract.
- 295 inventories Phase 3–5 evidence and explicitly marks missing observations.
- 298 freezes transcript helpers and shared peripheral/display integration seams.

Ticket 190 follows 180. Existing CPU code is deliberately treated as a partial
starting point, not as completion of Phase 2. Full Thumb-2/DSP, FPU arithmetic
and stacking, NVIC priorities/nesting, SysTick/PendSV behavior, and fault
escalation remain required before Apollo4/Sapporo boot work can pass.

The next authentic investigation should capture the fault/exception path that
enters `0x001a2434`, then add the narrowest synthetic CPU or controller
regression before changing execution behavior.
