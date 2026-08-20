# Current Implementation Status

This file records the implemented baseline without weakening the roadmap
gates. The ticket index remains authoritative for roadmap work; bounded
maintenance follows `AGENTS.md` directly. A roadmap ticket is `done` only when
its full acceptance conditions pass, even if useful pieces of later tickets
already exist.

## Implemented Baseline

- Dependency-free C99 headless build and optional SDL3 build.
- Strict profile and firmware manifests with exact size and SHA-256 checks.
- Fail-closed little-endian memory bus and stable integer-time scheduler.
- Immutable storage bases with sparse session-only program/erase overlays.
- Bounded machine execution, structured stop reasons, and semantic board input.
- An in-tree ARMv7E-M/Thumb-2 interpreter with exception, NVIC, SysTick,
  sleep/wake, DSP, and single-precision FPU coverage; a deterministic synthetic
  RTOS guest exercises nested interrupts, context switching, WFI, and FP state.
- Evidence-scoped Apollo4 clock, power, reset, GPIO, timer, STIMER, UART, IOM,
  MSPI, DMA, and MRAM models with unknown-offset and invalid-shape refusal.
- Sapporo flash, pressure, motion, magnetic, haptic, ambient-light, fuel-gauge,
  GPS, and OHR transports with deterministic fixtures and refusal tests.
- Exact Sapporo 2.22.60 component metadata and an opt-in, hit-bounded synthetic
  manufacturing-state compatibility layer.
- NEMA command framing/state, RGB565/A2LE rasterization, native-renderer frame
  publication, SDL3 presentation, semantic three-button input, and versioned
  input replay.
- Identity-pinned machine snapshots covering CPU, guest RAM, scheduler events,
  Apollo4 controller state, Sapporo device state, flash overlays, NEMA state,
  virtual time, and compatibility counters. Snapshot load is atomic on a
  malformed or incompatible image and keeps firmware/resource files external.
  Save and restore reject unsupported scheduler kinds, unowned scheduler
  entries, detached device-owned events, and mismatched event identities
  before the scheduler queue is committed.

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
without reset or display refusal. This exposes the evidenced setup UI in SDL.
The three declared Phase 5 renderer goldens remain exact for their named
checkpoints; this does not claim physical-panel equivalence or bit-identical
behavior for every unobserved A2LE edge. The
`--until middle-language` replay checkpoint still stops at the first
post-input non-black setup frame. Live SDL checkpoints wait for a bounded
350-ms virtual-time quiet window after the last post-input renderer submission,
so an intermediate logo/text transition is not frozen as the interactive frame.
SDL button edges hold active-low for 70 ms of guest time and keep the released
level stable for 70 ms before another press, matching the native debounce
boundary (E-SAP-LIVE-0001). The SDL first-frame diagnostic also includes a
bounded CRC32 of the presented RGB565 bytes; the live checkpoint accepts only
visible pixels from a
new renderer generation, excluding stride padding and stale submissions.

SDL also accepts the `middle-language`, `lower-transition`, and neutral
`setup-next` checkpoints without `--input-replay`. In that live mode Arrow Up,
Return/Enter, and Arrow Down are delivered through the semantic input mapper.
Left clicks in the upper, middle, and lower window thirds use the same path, and
the SDL window requests focus when its first validated frame creates the native
surface. After a quiet settled post-button frame the window pauses for the next
live button edge so the setup UI can be navigated manually. The named checkpoint
button is required only for the first edge; subsequent setup edges accept any of
the three mapped buttons.
Pressing that edge returns control to the guest immediately and rearms the next
settled frame; replay checkpoints retain their deterministic stop behavior.
The optional authentic `check-sdl` flow now queues one SDL Return key-down/up
pair and two successive middle-screen mouse clicks, verifying setup CRC32
checkpoints `629da47e`, `d4ed66c7`, and `83309716`, then exits through an SDL
quit event at the repeatable checkpoint `pc=0x000bf102`,
`instructions=1196657344`, `virtual_time_ns=10905302525`. The neutral
`setup-next` checkpoint is also available for a snapshot-loaded, middle-button
replay continuation; it reports the first visible post-input frame without
naming an unverified screen. Invalid automation configuration is always
checked and fails closed; absent private firmware skips only the authentic run.

The next native transition is now provenance-pinned as E-SAP-ONBOARD-001: an
English-row selection reaches a native software-rendered `Define your profile`
frame (raw SHA-256 `0930d2cc...`). The capture is comparison evidence only;
its source marks physical NEMAP output and complete watch UI as unavailable, so
the emulator keeps `setup-next` neutral until an equivalent guest contract is
reproduced.

The deterministic emulator comparison is recorded as E-SAP-ONBOARD-EMU-001.
Two identical bounded `setup-next` probes stop at the first visible frame
(generation 2, raw SHA-256 `0096f059...`, CRC32 `bbf3549e`); a longer diagnostic
continuation's last distinct frame is generation 59 (raw SHA-256 `688ca665...`,
CRC32 `d4ed66c7`). Neither matches the native onboarding capture. The external
snapshot and replay are hash-pinned but have no provenance sidecar, so this
remains a diagnostic gap rather than authorization for a screen-specific
checkpoint or pixel golden.

The normal `-O2` runtime now dispatches successful SCS accesses directly to
SysTick, NVIC, or SCB instead of constructing speculative refusal diagnostics,
and it calls the 2.22 compatibility dispatcher only at its exact, hash-pinned
trigger PCs. CPU state and virtual-time accounting are cached across ordinary
one-tick instruction steps, while WFI jumps, reset, and refusal paths still
read the scheduler directly. On the same host, five SDL dummy continuations
from the identity-pinned pre-frame snapshot to the fixed 650,800,000-
instruction limit averaged 5.284 seconds before the accounting changes and
4.700 seconds after them, an 11.1% wall-time reduction. Both paths produced
first-frame CRC32 `4979f432` and stopped at `pc=0x0009a7e4`, 650,800,000
instructions, and 15,707,372,848 ns of virtual time. This is a host performance
measurement; guest execution and virtual time are unchanged.

Fixed-width bus access now uses explicit little-endian 1-, 2-, and 4-byte
operations and bypasses the general region search only when the regular-region
cache proves that no overlay can apply. A five-run paired continuation against
commit `07ad14c` averaged 4.658 seconds before these bus changes and 4.180
seconds after them, a further 10.3% wall-time reduction. All ten runs produced
the same output SHA-1 `c0db13d31ba201deeee2453d328aa4027c990b59` and the same
stop checkpoint above.

The CPU instruction path now uses an internal, exact 16-bit bus fetch that
keeps overlay and fault handling unchanged while avoiding generic-width work
on cached ROM/RAM hits. The regular-region cache stores invalidation-safe
pointers instead of reconstructing them from indexes. A second five-run paired
continuation against commit `44e545b` averaged 4.132 seconds before and 3.880
seconds after these changes, a further 6.1% wall-time reduction, with the same
output SHA-1 and stop checkpoint.

The machine run loop now updates the public logger timestamp field directly
after its existing null check instead of making an out-of-line call after every
instruction. Five paired runs against commit `f17e638` averaged 3.874 seconds
before and 3.786 seconds after this change, a further 2.3% reduction, with the
same output SHA-1 and stop checkpoint.

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
refusal (E-SAP-0016). These are reproducible emulator observations, not a
later-version behavior fix; a native reset-register or post-reset transaction
trace is still required before changing Apollo4 reset semantics.

The later-Sapporo audit is now recorded as E-SAP-0017. The 2.33 MSPI power
change remains a static hotfix candidate without a recovered failing runtime
state, and the current E-SAP-0014 profile run reaches its bounded max-time
checkpoint without a device/storage stop. The 2.39 storage and UI probes still
require a native `storage/` open and watch-face notification before any binding;
fabricating those events would bypass the observed owner. No implementation-
eligible later-Sapporo device/storage gap is currently available.

A fresh current-build snapshot/frame-loop baseline was measured on 2026-08-20
with the external Sapporo 2.22 manifest (SHA-256 `ac9b381b...`) and a
450,800,000-instruction checkpoint (snapshot SHA-256
`8767a3f9990f1272c55e55566c7479037e0a2fbe3d5a711efb686feff7026e70`). Three
cold starts to the 450,900,000-instruction `normal-frame` boundary averaged
8.253 seconds of host wall time. Five resumes from that snapshot averaged
0.142 seconds headless and 0.174 seconds through SDL3; every run stopped at
`pc=0x0009a3fc`, virtual time `5333307331`, and every SDL run published
`240x240 generation=1 crc32=4979f432`. The current binary hashes were
headless `c80b8191...`, SDL `1d26a03f...`, and `libsemu.a` `9feafe6d...`.
This is a host-only guardrail; it authorizes no performance or guest-behavior
change by itself.

## Next Actionable Work

Phases 0–6 and the first-target functional milestone are complete. The Phase 7
behavior/release templates remain blocked until their product-specific evidence
is instantiated; independent product evidence inventories no longer wait on
another product's release. Bounded maintenance may proceed under `AGENTS.md`
without manufacturing a roadmap row. The practical work queue is:

- Hold later-Sapporo gap work until an exact failing transaction, native
  provenance, and an allowed device/storage module boundary are available;
  E-SAP-0017 records the current refusal boundary.
- Recover a native provenance sidecar and an equivalent settled command/text
  contract for E-SAP-ONBOARD-001 before adding a screen-specific emulator
  checkpoint; until then keep `setup-next` neutral. This remains an SDL
  renderer milestone, not a physical-panel claim.
- Preserve the pinned snapshot/frame-loop baseline before any performance
  change: rerun the cold and resumed probes, requiring the exact stop, virtual
  time, and SDL CRC32 while retaining deterministic guest behavior.

The Sapporo UI helper invalidates its cached checkpoint when the selected
headless or SDL executable is newer than the snapshot and verifies a sidecar
containing the manifest, selected binary, library, helper, profile, layer, and
capture-boundary hashes. It retains the explicit refresh switch for copied or
otherwise ambiguous artifacts.

The blocked Phase 7 templates must not be treated as permission to infer later
product wiring, storage, display, or input behavior. Missing evidence remains a
refusal until a read-only native package or trace supplies the exact contract.
