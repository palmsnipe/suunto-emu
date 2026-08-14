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

On 2026-08-13, user-supplied components matching all three profile hashes were
validated and run twice with the named compatibility layer. Both bounded runs
produced the same result:

```text
stop=budget pc=0x001a2434 instructions=10000000 virtual_time_ns=10000000
```

Both logs contained exactly one declared compatibility-layer hit. The address
is a default-handler loop reached during reset/static initialization. This is a
determinism checkpoint only: production startup, native Nema traffic, frames,
and physical-button UI transitions have not been reached.

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
