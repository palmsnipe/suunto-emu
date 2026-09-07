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
- Compatibility snapshot restore now rejects excessive aggregate/per-trigger
  counters, inconsistent totals and sum overflow. Runtime intervention commits
  enforce the aggregate bound. Stale 2.22 aggregate metadata is corrected to
  twenty, matching its unchanged individual allowances. GPS final-fragment
  refusal/WAIT, delayed-RX scheduling errors and the legacy startup response
  retain transport/counter/log state (E-EMU-COMPAT-ATOMIC-001). Valid 2.22
  snapshots and 2.39 logo/halt checkpoints remain byte-identical.

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
through NEMA snapshots and applies the native TSC6A semantic shadow/resolve
path without decoding private compressed bytes. The observed TSC6A target
triangles, A2LE masks, and 480x480-to-240x240 resolves are accepted
transactionally; unknown programs and geometry still refuse. Two fresh SDL
dummy runs with middle (5.400/5.470 seconds) and lower (8.000/8.070 seconds)
semantic button pulses were byte-identical through the language/profile
transition and reached the bounded budget without renderer refusal. This
exposes the evidenced setup UI in SDL. The three declared Phase 5 renderer
goldens remain exact for their named checkpoints; this does not claim
physical-panel equivalence or bit-identical behavior for every unobserved
A2LE/TSC6A edge. The
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
pair and two successive middle-screen mouse clicks. From the initial validated
frame CRC32 `2a01c517`, it verifies settled setup checkpoints `4979f432`,
`629da47e`, and `d4ed66c7`, then exits through an SDL quit event at the
repeatable checkpoint `pc=0x000bacf4`, `instructions=774081920`,
`virtual_time_ns=6520939902` (E-SAP-ONBOARD-EMU-012). The former 011
checkpoint included a nonfatal haptic timeout from an incorrect register
selector; the existing haptic corrections remove it. Cold live-input checks
also require the complete transcript SHA-256. The neutral
`setup-next` checkpoint is also available for a snapshot-loaded, middle-button
replay continuation; it reports the first visible post-input frame without
naming an unverified screen. Invalid automation configuration is always
checked and fails closed; absent private firmware skips only the authentic run.

The opt-in `SEMU_SDL_LIVE_TEST=setup-walk` diagnostic drives the authentic
2.22.60 onboarding from a fresh boot. With the corrected board mapping (semantic
upper/middle/lower → GPIO57/58/59, matching the verified `sapporo_wiring.c`
table; a prior build had upper/lower inverted to 59/57), steps 1–11 advance on
the middle button through the profile screens, and step 12 settles on the
`Connect with mobile` frame (`261712ad`). From a fresh boot that screen advances
only on the middle button; a middle press then reaches the `Continue the setup
on your phone` handoff (`ea3bc5f8`), and three lower presses settle three more
post-handoff frames: CRC32 `9b58f243`, `eb868d29`, and `ed7eeb7a` (the rendered
`Watch info: SUUNTO 9 PEAK PRO` / "Later" step). MIDDLE on `ed7eeb7a` then
settles the phone-pairing recommendation sequence `74a5e6ab`, `b26dd658`,
`0b93f6c9` ("Connect / Connect Later"), `a797ec30`, and MIDDLE on `a797ec30`
settles `8362b9bc` "Time/date" (`w-tida`, ~23.0 s virtual). MIDDLE on
`8362b9bc` *does* advance the onboarding: it opens `w-ltim`, whose
"Searching for GPS" ring animates every ~1–4 ms of virtual time and therefore
never reports a settled frame, so the frame-stepped walk cannot observe the
advance and no further step-driven press is possible there
(E-SAP-ONBOARD-EMU-010 corrects the earlier phone-gated reading of this
boundary). Every walk without a time-scheduled driver stops byte-stably at
`pc=0x0010fbde`, `stop=compat-refused`, when the background GPS
power-cycling exhausts the eleven-hit `gps-awake-pulse` budget
(E-SAP-COMPAT-GPS-005) at virtual time ~71 s.
The time zone screen `w-ltim` (next view after `w-tida`) puts
`Navigation/State=1`, waits 5 s, then subscribes to `Dev/Time/LocalTime`:
`t >= 1646092800` (2022-03-01Z, unix seconds) routes to the UTC-offset menu and
then `w-done`; otherwise it shows "Search for GPS" whose down button
("Set manually") opens `w-year`. The manual chain `w-year` → `w-mont` →
`w-day` → `w-time` → `w-tset`/`w-done` (which auto-opens `main` after 3 s) is
fully present in the resource. The 2.22.60 application binary has no
standalone clock source: the authoritative path is the `GpsTimeSynchronizer`
worker (NMEA GGA/RMC time-of-day), and no OHR2/phone time-sync command exists
in 2.22.60. Two experiments bound the alternatives: a valid time-bearing
GGA/RMC/EPU NMEA group injected on the `@GSR` running-status exchange makes the
firmware issue repeated deliberate `SYSRESETREQ` writes from `0x000be93e`
(synthetic NMEA time is not usable), and a gated value at RTC `0x40004820`
leaves the onboarding byte-identical (the register does not feed
`Dev/Time/LocalTime`). Onboarding **does complete** standalone through the
firmware's own manual-entry chain, verified deterministically: MIDDLE on
`8362b9bc` opens `w-ltim`; an opt-in time-scheduled LOWER press
(`SEMU_SDL_SETUP_WALK_TIMELINE`, ~30 s virtual) hits "SET MANUALLY" and opens
`w-year`; the walk's POST letters then confirm `w-year` → `w-mont` → `w-day` →
`w-time` (its `next` button toggles hour/minute focus; the second MIDDLE saves
hour+minute+local) → `w-done` "Done" → `main`, with settled CRCs
`5321867e`/`c683e828`/`cd1b0979`/`455b603a`/`53d3f0c1`/`17e1772c`/`578e2601`/
`1c62ab1a` and final main-menu frame `fb8e0155` (Navigation/Logbook/Media
controls); the run then idles and stops at `stop=halt` ~43.8 s virtual (no
phone time source needed, and no `OHR2` time command exists to build one).
Note: a manual time below the `1646092800` gate (e.g. 2022-01-01) still
finishes the wizard, but the next boot routes to `n-sync-rec` instead of
`main`; year ≥ 2023 (or month ≥ Mar) keeps `LocalTime` post-gate
(E-SAP-ONBOARD-EMU-008/009/010).

E-SAP-ONBOARD-EMU-007 recorded that, under the pre-fix board mapping, replayed
upper/middle/lower presses were delivered to the guest's `INPUT_READ1` button
polling yet the 2.22.60 guest submitted zero NEMA command lists on the
`Continue the setup on your phone` screen across all press patterns and a
51-second idle continuation. That "inert button" symptom is now explained by
the inverted upper/lower mapping (semantic lower drove GPIO57, the physical
back pin, instead of GPIO59, the bottom/Skip pin); with the corrected mapping
the same press advances the onboarding, so the conclusion in 007 that the
handoff button path is inert and that the post-handoff view definitions are
missing from the resource is superseded by E-SAP-ONBOARD-EMU-008. The
post-handoff onboarding strings (`Skip` `750d4b9f`, `Time/date` `6a5c2eec`,
`Time zone` `6072e392`, `All done!` `2bb84502`) and their view definitions
(`w-conn-1`, `w-tida`, `w-ltim`, `w-year`, `w-mont`, `w-day`, `w-time`,
`w-done`) are present in the recovered 2.22.60 resource. Skipping phone pairing
now works; the walk advances through the phone-pairing recommendation screens
to the `Time/date` step (`w-tida`, `8362b9bc`), one internal-viewset advance
before the `w-ltim` "Time zone / Search for GPS" screen, and stops at ~73.1 s
when the eleven-hit `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005) is
exhausted (E-SAP-ONBOARD-EMU-009).

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

The later-Sapporo audit is recorded as E-SAP-0017, and the first executable
2.39 C-emulator boundary is now recorded as E-SAP-0018. The 2.33 MSPI power
change remains a static hotfix candidate without a recovered failing runtime
state, while the exact 2.39 image now has a concrete fail-closed sequence: its
HardFault handler resets after a precise access fault at the missing Apollo4
DSP0 memory-power register `0x40021058`. Fully reverted diagnostic experiments
show that the six-register DSP0/DSP1 enable/status/retention cluster advances
the image to the next distinct initialization boundary. The image writes
watchdog CFG at `0x40024000`, configures Reset/BoD routing at `0x40000000`, then
accesses watchdog INTEN at `0x40024200`. The 2.39 storage/UI probes still
require their native owner events; the power/reset/watchdog result does not
authorize fabricated storage opens. The exact built-in `sapporo-2.39.20`
profile pins all three component
hashes under ticket 706. Ticket 711 now implements the evidenced DSP0/DSP1
memory enable/status/retention registers, including reset and snapshot state;
two byte-identical firmware runs advance the first reset from 11,897,027 ns to
11,897,251 ns. Ticket 712 implements only the evidenced watchdog CFG register,
including reset, snapshot, reserved-bit, selector, offset, and width checks;
two byte-identical runs advance three more instructions to the precise
Reset/BoD fault and first reset at 11,897,254 ns. Ticket 713 implements only
RSTGEN CFG, with a zero reset value, valid bits 0..1, strict access refusal, and
snapshot validation. Two byte-identical post-713 logs (SHA-256
`540b62b500147fffa74f44f5ba4d0f1f02c9fa1c7713512a8834c78700fee7b5`)
advance the first reset to 11,897,266 ns and stop at the next unsupported
watchdog INTEN access. Ticket 714 implements only WDTIEREN at offset `0x200`,
with reset zero, valid bits 0..1, strict access refusal, and snapshot
validation. Two byte-identical post-714 logs (SHA-256
`493e50f23db1149402e2aadb20cbe6108c7e37fec27be6700e23c8821d5c35fa`)
advance the first reset to 11,897,284 ns. The next unsupported access is the
watchdog restart-key write at `0x40024004`. Ticket 716 implements only the
write-only `0xb2` restart command and zero readback, without inventing timer or
expiry behavior. Two byte-identical post-716 logs (SHA-256
`0a092da13d76a589b189bc43a22461bd5e280d68e791927d81dd2193f96958f6`)
advance the first reset to instruction 19,945,598 and virtual time 25,285,493
ns. Ticket 717 adds only deterministic zero-valued CHIPID0/CHIPID1 reads,
matching both PAC reset values and the pinned reference environment, while
keeping writes and all other MCUCTRL identity offsets refused. Two
byte-identical post-717 logs (SHA-256
`3d182aea65869a4414579e79ce5f942610570257b606d5b99b71c3f2674a483a`)
advance the first reset to instruction 19,948,596 and 25,288,491 ns. The next
precise fault is CTIMER auxiliary offset `0xe8` refusing firmware value
`0x3f`. Ticket 719 identifies that address as OUTCFG26 and accepts only the
observed whole-register value while retaining strict refusal and the existing
reset value. Its snapshot validator now accepts the same value without a
format change. Two byte-identical 30,000,000-instruction logs (SHA-256
`8e079f452fdc7f6485d6688746a1db93f0688fe517b01f1ca295ad6db5e8cb23`)
advance the first reset to instruction 24,771,518 and 30,111,413 ns. The next
precise fault is a 32-bit USB CLKCTRL read at `0x400b2000`; firmware PC
`0x000f8c02` reads it before PC `0x000f8c08` writes `0x02000000`. Ticket 721
implements exactly that zero read and trace-matching no-output write. Two
byte-identical 100,000,000-instruction logs (SHA-256
`06e69fa86a9034a491bc7381a4b51b6dea538e9d326d29fb81bba10945d79374`)
advance the first reset to instruction 77,220,237 and 368,259,842 ns. The next
precise fault is CTIMER observed-pattern offset `0x104` refusing value
`0x00012300` at firmware PC `0x000f7d60`. Ticket 722 accepts only that
temporary pattern value and preserves snapshot validation. Two byte-identical
200,000,000-instruction/2,000,000,000 ns budget runs (SHA-256
`96e428de7caf01f866ed3a91193a7e45ff2c37d700a63a9deb9764d8f0506890`)
now contain no reset or unsupported access. They reach the firmware WFI/ISB
idle path and stop at PC `0x000e955a`, instruction 84,856,118, virtual time
6,372,873,793 ns solely because of the configured time budget. Ticket 723
then exercises the production storage path with an explicit, hash-gated
synthetic full-flash fixture. The Sapporo flash endpoint now applies the
reference model's NOR page-program rule (`old & requested`) before calling the
unchanged strict storage API, so requested zero-to-one bits remain clear
instead of causing a bus fault. Two production logs are byte-identical
(SHA-256
`21c415dee3c52ef4f77f0da42c3ea4020e0c7dc0e7092bf3bcab08d2c6661cc5`)
and advance beyond the former `0x0010272a` HardFault to instruction
41,435,661 at 278,677,270 ns. The new independent fail-closed boundary is a
write of `0x00004000` to CTIMER address `0x40008068` at PC `0x000cb882`;
the completed flash DMA is no longer the fault owner (E-SAP-0026). Ticket 724
identifies that write as Timer7 CMP0's write-one-to-clear INTCLR bit and maps
it to the timer model's channel-7 pending/IRQ state. Two complete logs are
byte-identical (SHA-256
`32a5bc1df226ca20da0c94aa90dc121fea14125f45890397f3671d6cb95c0b33`)
and advance 22 instructions and 22 ns. The next precise fault is the same
firmware routine writing `0x00004000` to CTIMER INTEN at `0x40008060`, with
stacked PC `0x000cb854` (E-SAP-0027). Ticket 726 accepts and retains only that
additional INTEN value, preserving the existing IRQ model and snapshot format.
Two complete logs are byte-identical (SHA-256
`5e0d8dd23c863aaa00b44489d9235b4967183d44d26d49f538f094e55e6affe0`)
and advance the first reset to instruction 49,456,422 at 441,085,096 ns. The
next precise fault is IRQ21's handler reading `0x00004000`, ORing Timer0 CMP0
bit zero, and writing combined INTEN value `0x00004001` at PC `0x000f7d7e`
(E-SAP-0028). Ticket 727 accepts only that combined value. Two complete runs
are byte-identical (SHA-256
`db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`),
contain no reset, and halt at PC `0x00079e1e`, instruction 72,774,982,
virtual time 521,257,564 ns. The halt follows firmware `BKPT #0`; its caller
identifies `StartupClient.cpp` line 67, so the next boundary is application
startup-state reverse engineering rather than another unsupported MMIO
transaction (E-SAP-0029).

Ticket 728 resolves that application boundary with the opt-in,
exact-hash-pinned `sapporo-2.39-synthetic-wbsto` layer. Read-only firmware
tracing identifies `WbStoPreload` command zero as the owner: four persisted
WbStorage files omitted from the compact OTA fragment return native status 204,
which the provider collapses to 500. The layer validates the exact empty native
session-cache vectors, installs four explicitly synthetic values, and
translates only that final command-zero preload result after revalidating the
cache byte-for-byte. Layer-off behavior remains the E-SAP-0029 halt. Two
layer-on runs are byte-identical (SHA-256
`b1156669803cbd2c09e16599fa3719ff2adeecb493eb3749e20fcec34b8f37c0`)
and advance without reset to the next fail-closed boundary: a native 32-byte
FAT-cache write to unmapped `0x0f676e34` at PC `0x0007038c`, instruction
78,496,951 and virtual time 526,979,533 ns
(E-SAP-COMPAT-WBSTO-239-001). This confirms that the compact OTA fragment is
not a coherent writable filesystem; the address must not be broadly mapped.

Ticket 729 extends that same exact-build layer with an in-memory logical-file
adapter at the firmware's public wrapper ABI. It retains only native bytes
created for eleven observed writable paths, enforces the reference's exact
per-path size ceilings, passes missing read/update opens through to native
storage, and refuses unknown writable paths, modes, handles, ranges, or budget
excess before mutation. Two exact runs and their snapshots are byte-identical;
the firmware creates `settings/sync.txt` (0 bytes), `settings/uiv2.txt` (235),
and `settings/general` (1505) in 118 operations, with no return of the FAT
underflow. A saved checkpoint resumes exactly into the next reset one
instruction later. Exception-frame recovery pins that reset to an IOM2 access
at `0x40052120` from stacked PC `0x0014e8ee`, matching the reference's I2C
probe of address `0x5c` (E-SAP-COMPAT-FILES-239-001). No generic IOM or sensor
response is yet implemented.

Ticket 731 implements that independently evidenced boundary as
E-SAP-LPS22-239-001. The board factory now adds a strict LPS22HB endpoint at
I2C address `0x5c` only for `sapporo-2.39.20`; 2.22 and 2.33 continue to
refuse that address and retain byte-identical device snapshots. The IOM2
adapter forwards the command's high-byte selector only for the newly evidenced
address. Authentic firmware performs the reference's exact fourteen reads and
writes, observes identity `0xb1`, completes both self-clearing CTRL_REG2
commands, and finishes with CTRL_REG1 `0x1e`. Two 79,000,000-instruction runs
and snapshots are byte-identical (log SHA-256
`fa74015d06b9aa988787724e666f4e37c1223c14fc36f996cd773b2a93d61592`,
snapshot SHA-256
`75f0f534bfc9aae60adabd642f0d4fa982146ed9a743abb7e584aa0e1664e290`),
stop at PC `0x000a7b2e` and virtual time 520,829,069 ns, and contain no reset.
Snapshot continuation advances one instruction identically. A longer
exploratory run reaches a distinct firmware reset request at instruction
122,452,650 and virtual time 1,230,991,337 ns; its owner is not inferred here.

That reset is now attributed to an IOM4 haptic selector adapter error rather
than firmware reset policy. Native command `0x22000112` reads autotune register
`0x22` at address `0x50`, but the adapter supplied stale byte `0xa0` from a
fixed DMA-adjacent location. Address-scoped command-selector forwarding makes
the poll succeed without changing the haptic endpoint or compatibility layer.
Two fresh runs stop byte-identically before the next strict boundary at PC
`0x0014e8ea`, instruction 122,457,908, virtual time 1,230,996,595 ns (log
SHA-256 `f1c41ec3d40617174d8cbb299883c69b028a6bfb445b44a0bb7aaf2622915354`,
snapshot `629ba604acfbb1eb1265a755283c6133b9650d45dcf2c44d9439752365e22247`).
The next command is the evidenced haptic calibration read `0x23000112`;
registers `0x23`/`0x24` remain intentionally unsupported pending a bounded
fixture contract (E-SAP-IOM4-HAPTIC-239-001).

Ticket 733 supplies that bounded fixture under E-SAP-HAPTIC-CAL-239-001.
After autotune completes, the strict haptic endpoint now exposes registers
`0x23` and `0x24` as separate, one-byte, read-only zero values. Zero is the
hash-pinned reference endpoint's reset fixture, not a recovered physical
calibration. Writes and multi-byte spans refuse before mutation, and the
fixture adds no writable state or snapshot bytes. Two fresh authentic runs
advance byte-identically to PC `0x000cceb2`, instruction 357,033,113, virtual
time 1,878,381,357 ns (log SHA-256
`3aee5f2f3271f54448ab2ca681908e6dfa766348b4dfbe0e2099add4c7b24ca7`,
snapshot `c287c2c1e256e55c100b083a6db1b35730a646ad9aabeea21600347873a9e95e`)
without reset or a new compatibility hit. The next instruction is a native
word read from GPIO address `0x40010218`; one-instruction continuation takes
the precise fault vector at PC `0x001c0db4`. GPIO offset `0x218` remains
fail-closed pending independent register evidence.

Ticket 734 identifies that boundary as GPIO WT1 under
E-SAP-GPIO-WT1-239-001. WT1 is the output-state register for pins 32–63;
firmware operation 1 reads it to query pin 53. The existing WTS/WTC state is
`0x00040000`, making pin 53 low. Only aligned 32-bit WT1 reads are added;
direct WT writes and the other three banks remain fail-closed, and no state or
snapshot byte changes. Two fresh authentic runs advance byte-identically to
PC `0x0014e8ea`, instruction 359,772,704, virtual time 1,881,120,948 ns (log
SHA-256 `48c514ba4504a25122c60e71e2b3966fba9463edc9a3641b4ba3ab5854ff9e02`,
snapshot `20febdf889a8d8baf7d146f6a1f1bcac6182d009b4ad3ed4b4ace4bd9f28d81a`)
without reset or a new compatibility hit. The next instruction submits OHR2
command `0x0010`, sequence zero, while the endpoint is in BSL state; its strict
refusal enters the precise fault vector.

Ticket 735 implements that command under E-SAP-OHR2-BOOT-239-001. The native
2.39 trace defines data byte `0x01` plus forty-nine `0xff` bytes and a reply
whose data is all zero; the same exchange occurs in both BSL and MAIN without
changing state. The strict transport accepts only that exact payload, and the
profile-selected 2.39 device provider supplies the observed reply without a
compatibility layer or hit. OHR reset now also drives its evidenced low ready
level onto GPIO62, so the first reply produces the required low-to-high edge
and is consumed normally. Two exact runs and snapshots are byte-identical at
PC `0x0014e8ea`, instruction 359,790,038, virtual time 1,881,138,282 ns (log
SHA-256 `b8977bf8911cc5435e19afc109205c267e822249c17e19fba3809779d046664e`,
snapshot `b7d1d84e2be435635cc6031b8424ece436b6557d3ba3883c59f92b7550916f86`),
with ready high/request success/ready low/response success, no reset, and the
same 118 compatibility operations. The next instruction submits the already
known identity command zero, sequence one, in BSL; its 2.39 response body is
not yet wired and therefore refuses into the precise fault vector.

Ticket 736 implements that BSL identity body under
E-SAP-OHR2-ID-BSL-239-001. A dedicated 2.39 physical provider requires all
fifty request data bytes to be `0xff`, returns the otherwise-zero body with
`BSL\0` at payload offsets 9..12, retains the ticket-735 boot-mode response,
and refuses every unimplemented 2.39 body without falling through to the 2.22
compatibility fixture. Two exact runs and snapshots are byte-identical at PC
`0x0014e8ea`, instruction 368,947,987, virtual time 1,890,296,231 ns (log
SHA-256 `c5599a2faf3016cdeb85bbb2cd6951f70fad49d6732639bbda861d7f5348c1ed`,
snapshot `2a823cb69c1bdb7463233c553a2e55312c462bca99aa1715246cb1fd3866d690`).
They complete BSL identity, fire-and-forget reboot, and the second boot-mode
exchange in MAIN with no reset and the same 118 logical-file operations. The
next instruction submits MAIN identity command zero, sequence three; that
body remains a strict refusal and enters the precise fault vector.

Ticket 737 implements MAIN identity under E-SAP-OHR2-ID-MAIN-239-001. The
same dedicated 2.39 physical provider validates the all-`0xff` request and
selects the exact otherwise-zero `MAIN\0` body from modeled MAIN state; BSL
identity and boot-mode behavior are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 368,958,374, virtual time
1,890,306,618 ns (log SHA-256
`d82ebc5b061787b8cefad7f64f7b70168858bc8da29adb644cd486211a8bfc22`,
snapshot `352cdedcec47360eb478c6eec3649534025c7373b19c9c35c90e3922549c8a81`).
They complete MAIN identity without reset and preserve the same 118 logical-
file operations. The next instruction submits result command `0x000d`,
sequence four; it remains a strict refusal and enters the fault vector.

Ticket 738 implements MAIN result command `0x000d` under
E-SAP-OHR2-RESULT13-239-001. The dedicated 2.39 physical provider validates
the all-`0xff` request in modeled MAIN state and returns the exact all-zero
body; existing response bodies are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 368,995,288, virtual time
1,890,343,532 ns (log SHA-256
`eb76c862ba97bd1b0f5ae569b62dcfd3544ecf39d06e3b791de22ce57c2f2331`,
snapshot `5359e0cdf8f62514c88b6a90cb381e40c55811a748fcf5b510319268680100f4`).
They complete command 13 without reset and preserve the same 118 logical-file
operations. The next instruction submits result command `0x000e`, sequence
five; it remains a strict refusal and enters the fault vector.

Ticket 739 implements MAIN result command `0x000e` under
E-SAP-OHR2-RESULT14-239-001. The dedicated 2.39 physical provider validates
the all-`0xff` request in modeled MAIN state and returns the exact all-zero
body; existing response bodies are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 369,026,992, virtual time
1,890,375,236 ns (log SHA-256
`8e1e68584a5d869f91c07216add9d6d1befbc36aac26718113359b76298fd1c4`,
snapshot `d7c30abd8ff1744c1644b2730953d45012c547977b24c905873b37fa2533b8e0`).
They complete command 14 without reset and preserve the same 118 logical-file
operations. The next instruction submits echo command `0x0006`, sequence six;
it remains a strict refusal and enters the fault vector.

Ticket 741 implements the exact MAIN echo under E-SAP-OHR2-ECHO-239-001.
The dedicated 2.39 physical provider accepts only the complete native body or
the distinct complete body generated by the exact deterministic guest state,
then returns all fifty data bytes unchanged. Two exact runs and snapshots are
byte-identical at PC `0x0014e8ea`, instruction 369,037,329, virtual time
1,890,385,573 ns (log SHA-256
`2ef900dbf79d08a83e94c2e6d8e642c53b51fa40d3faaca977319ac64f4d59cf`,
snapshot `5168ba1e48997e23553370705d49f4ea0f83c407337576bb3c7a56cacb308686`).
They complete echo without reset and preserve the same 118 logical-file
operations. The next instruction submits command `0x0002`, sequence seven; it
was a strict generic-command refusal at that ticket's boundary.

Ticket 742 integrates command `0x0002` into the shared OHR2 enum/MAIN-state
registry and exact 2.39 provider under E-SAP-OHR2-CMD2-239-001. The complete
all-`ff` request produces the observed all-zero body; BSL, malformed bodies,
and absent/legacy providers still refuse atomically. Packet framing, ready
edges, sequence rules, and snapshot format are unchanged. Two fresh authentic
runs and snapshots match at PC `0x00079e1c`, instruction 393,235,868, virtual
time 1,914,584,112 ns (log SHA-256
`9161895c12da70077ec78fb76bae6062196194a80f1df5b8c9609876fa20b17a`,
snapshot `c36512287d4bf7d5a06762334ba261d984d0259a1466ec83076e73ebb253dcb0`).
Command 2 completes without reset/refusal and execution performs 449 existing
logical-file operations without increasing the compatibility budget. A
one-instruction resume executes the real firmware BKPT and stops at
`pc=0x00079e1e`, instruction 393,235,869, time 1,914,584,113 ns. No normal
frame has been reached. E-SAP-STARTUP-SLEEP-239-001 identifies the next
investigation: `StartupClient.cpp`'s failure path for `sleepln`, command zero,
result 500. The underlying cause is not yet established. This is an evidenced
reference startup response, not a claim of physical OHR measurement support;
ticket status remains integrator-owned.

Ticket 743 traces that sleep failure to a missing logical-file size query,
not bad header data. Native header validation succeeds; public wrapper
`0x00092244` then falls through with a synthetic handle and reports zero
instead of the retained 17,888 bytes. The adapter now returns the actual
length through its existing hash-pinned, opt-in, hit-bounded file operation.
It leaves cursor/data/snapshot format unchanged and refuses stale synthetic
handles and exhausted budgets. No startup-status translation was added.
The firmware's own `sleepln` callback now returns 200. Two fresh runs and
snapshots match at PC `0x00079e1c`, instruction 405,895,301, virtual time
1,927,243,545 ns (log SHA-256
`8139068b549a4e2be4baf57c94bc3b8eff385cb2bbb8efca506a8b30469de0d8`,
snapshot `0fa411dde053a15ef42d1b4ce2bf7282ad1990f88532b059dcf1c14ca9824193`).
The run contains 512 logical-file operations, including sleep size 17,888 and
training size 2,384, without a reset or device refusal. One more instruction
halts at PC `0x00079e1e`, time 1,927,243,546 ns. This distinct StartupClient
failure belongs to `TrainingTss`, command zero, result 500; its file-size
check now succeeds, but the subsequent failure's cause remains unresolved.
No normal frame is claimed (E-SAP-COMPAT-FILE-SIZE-239-001).

Ticket 744 corrects the seek wrapper's success return under
E-SAP-COMPAT-SEEK-239-001. The native ABI returns the requested offset bit
pattern, not a zero status or the computed absolute cursor. The adapter's
existing signed cursor calculation, range checks, state, and hit budget stay
unchanged. TrainingTss now reads all 42 native 56-byte records and returns
startup status 200 without a callback override. Two fresh runs and snapshots
match at PC `0x00079e1c`, instruction 416,256,851, virtual time
1,955,213,393 ns (log SHA-256
`4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e`,
snapshot `92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16`).
There are 595 logical-file operations, with no reset/device refusal or source
flash change. One-step resume executes BKPT and halts at PC `0x00079e1e`,
instruction 416,256,852, time 1,955,213,394 ns. The new StartupClient failure
is `WbStoPreload`, command one, result 500; the existing synthetic cache layer
handles command zero only. The command-one missing state still needs tracing,
and the current milestone remains short of a normal frame.

Ticket 746 traces that command-one failure to the same four unavailable
persisted records (E-SAP-COMPAT-PRELOAD1-239-001). It adds a separate one-shot
`wbsto-preload1-result` intervention, requiring both prior cache interventions
and byte-exact revalidation of the unchanged synthetic cache. No new values or
file bytes are supplied. Existing per-trigger limits remain unchanged; total
layer capacity becomes 2,674. Older two-/three-counter snapshots restore with
the appended counters at zero; unknown counts still refuse atomically.

The advanced checkpoint is PC `0x000921dc`, instruction 435,333,559, virtual
time 1,974,290,101 ns: log SHA-256
`476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71`,
snapshot `27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f`.
The preload translation fires once at 1,955,180,209 ns. Execution then reaches
the unchanged 2,671-operation logical-file limit during a sleep-record scan,
at cursor 11,480 of 17,888 bytes. The next attempted instruction refuses
without advancing PC/time/count. A real three-counter snapshot from instruction
405,860,000 resumes to the identical new snapshot. The next task is to trace
the repeated scans and establish a justified finite file-operation budget or
identify a native file-contract defect; no budget increase is included here.
No normal frame is claimed. Historical checkpoint tests remain unchanged.

Ticket 747 establishes that the repeated sleep reads are finite native history
queries, not a retry or an adapter ABI defect (E-SAP-COMPAT-HISTORY-239-001).
The firmware performs 42-, 60-, and 42-day windows over 248 records: 35,712
successful 72-byte reads and matching record seeks. Empty histories return
416 normally. The existing file limit is now exactly the measured 75,764
operations to the next independent boundary; the three other one-hit limits,
all paths/capacities and file semantics are unchanged.

The new checkpoint is `stop=budget pc=0x000920b4 instructions=439081594
virtual_time_ns=1978038136`, log SHA-256
`6f47fad1eeb3b6032955b463e2c4ba26310dbf5ddc453ae3f0f350acf15a9348`,
snapshot `3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1`.
The guest has created and read the allowlisted Activity Timeline database.
The next operation is enum-create of `actitmln/ongoing.bin` at wrapper
`0x000920b4`, LR `0x000b944d`; that path remains unknown and is refused
before mutation. Its native schema and bounded storage requirements are the
next reverse-engineering task. No new provider-status translation, persisted
payload, or normal frame is claimed. The ticket-746 pre-refusal log/snapshot
hashes remain exact; its subsequent budget refusal is historical.

Ticket 748 supersedes the ongoing-file refusal using direct native layout
evidence E-SAP-COMPAT-ONGOING-239-001. Only `actitmln/ongoing.bin` is appended,
with the exact 152-byte capacity: 24-byte header, eight padding bytes and
three 40-byte records. Firmware creates every byte, reopens and validates the
header and size, and reads the records without an activity-state repair.
There are 20 ongoing-file operations plus 408 `settings/personal` and 66
`zapp/storage.sbm` operations on already-supported paths. The measured file
ceiling is now 76,258, aggregate 76,261; no diagnostic headroom is retained.
S29F version one accepts only eleven/twelve slots and still emits the old
eleven-slot encoding until the appended file exists. Historical checkpoint
bytes remain unchanged.

The new checkpoint is `stop=budget pc=0x000920b4 instructions=451511675
virtual_time_ns=1990468217`, log SHA-256
`33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e`,
snapshot `42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`.
Two fresh runs match exactly; the ticket-747 prefix keeps both original
hashes and resumes to the identical new snapshot. The next attempted
instruction refuses open mode nine for `zapp/zwspee01.zip`, LR `0x000843e9`,
without advancing PC/time/count. Native mode semantics and the ZIP
resource path need separate recovery; neither is enabled here. The exact
private test reports no reset/device refusal before this boundary and verifies
immutable source flash. No normal 2.39 frame is claimed.

Ticket 749 lets the exact normalized `zapp/zwspee01.zip` / mode-nine request
execute in firmware (E-SAP-COMPAT-ZIP-READ-239-001). Native disassembly shows
read mode plus quiet open-failure logging; the adapter now leaves CPU, RAM,
files and counters untouched for that pair. The real filesystem returns
handle `0x30`, which firmware later closes. No ZIP slot, fabricated archive,
host overlay, mode translation, hit-budget increase or snapshot change is
introduced. Other unsupported modes and mode-nine paths remain refused.

Two fresh exact runs stop at `stop=budget pc=0x000920b4
instructions=459796107 virtual_time_ns=1998752649`: log SHA-256
`ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732`,
snapshot `13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
The old ticket-748 prefix retains both hashes and resumes to the identical new
snapshot. Logical-file hits remain 76,258. The next attempted instruction
refuses mode nine for `ui/js/config.js`, LR `0x000843e9`, without advancing
PC/time/count. Source flash remains unchanged and no reset/device refusal
occurs before the new boundary. This proves native ZIP open/close and later
startup progress, not archive completeness, installation or a normal frame.

Ticket 751 replaces the ZIP-specific exception with native quiet-read routing
under E-SAP-COMPAT-QUIET-READ-239-001. Exact mode nine passes through only for
validated paths outside the synthetic file table. The twelve table-owned paths
still refuse that mode, absent or present; normal synthetic modes and retained
contents are unchanged. Native firmware owns path resolution, missing-file
errors, content and return values. There is no file substitution, new handle
format, compatibility hit or budget increase.

The firmware opens `ui/js/config.js` with native handle `0x40`, then loads
scripts and styles including `ui/js/fonts.js`. The first traced segment to
600 million instructions records 134 opens over 105 distinct paths. Two fresh
runs match at `stop=budget pc=0x000cb852 instructions=607105617
virtual_time_ns=2146062159`: log SHA-256
`740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec`,
snapshot `15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
The ticket-749 prefix retains both hashes and resumes to that identical state;
file hits remain 76,258 and source flash is unchanged.

The next instruction writes `0x08004001` to CTIMER INTEN `0x40008060` and
takes the precise fault vector at PC `0x001c0db4`, instruction 607,105,618,
time 2,146,062,160 ns. A longer diagnostic observes the firmware-owned reset
79 instructions later. The added interrupt-enable bit is not supported by
this task. There is no normal 2.39 frame yet; UI resource reads now proceed
natively, and the next independent gap is the timer contract.

Ticket 752 identifies bit 27 as the already-modeled Timer13 CMP1 enable
(E-SAP-CTIMER13-INTEN-239-001) and accepts only the new combined value
`0x08004001`. Snapshot validation accepts the same value without a format
change. IRQ gates, compare deadlines, reset values and compatibility state
are unchanged. Both new regressions fail before and pass after the correction;
all 737 normal and sanitizer cases pass.

Two fresh runs stop identically at `stop=budget pc=0x00079e1c
instructions=608140266 virtual_time_ns=2147096849`, with log SHA-256
`2223de22981528b7cd2df049be68ea2e4022627763da13aab9293ef1fbbf7e16` and
snapshot `74e45df965216d809cf41e090ee0fc56b740affbc6d32aec35413dd65db1aa0c`.
The ticket-751 prefix retains both hashes and resumes to the identical state;
there are no resets/device refusals, file hits stay 76,258 and source flash
is unchanged. The next instruction executes native BKPT and halts at
PC `0x00079e1e`, instruction 608,140,267, time 2,147,096,850 ns.

E-SAP-SERIALIZER-ARRAY-239-001 traces this new failure to
`ChunkSerializer.cpp:38`. Native serialization of the synthetic LID `0xa432`
value reads the JSON bytes at `0x100002b4` as an array count (24,946), then
requests 199,568 bytes from a 16-byte buffer whose cursor is already 12.
The bounds check correctly fails. The synthetic value's full native object/
array ABI needs recovery; this is not permission to enlarge the buffer or
bypass the assertion. No normal 2.39 frame is claimed.

Ticket 753 corrects only that synthetic cache value under
E-SAP-COMPAT-WIDGETS-NATIVE-239-001. Native schema/copy tracing establishes
a twelve-byte empty object instead of JSON: zero scalar, zero array count and
null source pointer. The firmware performs its own copy/pointer relocation
and returns status 200. Entry lengths become twelve, while alignment, arena
extent, other values and every compatibility budget remain unchanged. Both
preload checks validate the complete corrected cache and reject legacy JSON
or any altered payload/length without mutation. Installation logs the new
evidence provenance. All 739 normal and sanitizer tests pass.

The exact 2.39 firmware now publishes its first visible 240x240 Suunto boot
logo (E-SAP-BOOT-LOGO-239-001): generation two, CRC32 `4979f432`, pixel
SHA-256 `3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373`.
Two fresh logs/snapshots match at `stop=user pc=0x00093be2
instructions=609300000 virtual_time_ns=2148256583`: log SHA-256
`63eb4997ff645958e70ed0586613762f88ee5e6e699434c1fbae48f0f435528b`,
snapshot `30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
This satisfies the existing first-nonblack `normal-frame` gate, not settled
setup or interactive operation. Source flash remains immutable, with no
reset/refusal before the frame and unchanged 76,258 file operations.

After the logo, update-open of `actitmln/247.bin` (mode three, LR `0x000b9e0d`)
hits the existing file budget at PC `0x000920b4`, instruction 610,599,945,
time 2,149,556,528 ns when resumed from the logo snapshot. The activity
sequence needs a measured finite budget or a separately evidenced file-contract
correction; no increase is included here. Old JSON-bearing snapshots are not
migrated and must be
regenerated from reset or a pre-install checkpoint. Layer-off and pre-install
execution retain their hashes; historical defective-fixture hashes are not
silently re-pinned.

Ticket 754 measures and enables exactly 21 post-logo activity operations
(E-SAP-COMPAT-ACTIVITY-239-001): nine on `actitmln/247.bin`, twelve on
`actitmln/ongoing.bin`. The firmware updates records and headers, then closes
both files, retaining sizes 46,112 and 152. Only the file ceiling changes to
76,279 (aggregate 76,282); no paths, modes, bytes, status translations,
snapshot format or other budgets change. The new regression fails at the old
ceiling and passes at the new one, including atomic excess refusal. All 740
normal and sanitizer tests pass.

The rendering-backed production continuation reaches
`stop=budget pc=0x00079e1c instructions=932397949 virtual_time_ns=11388431926`:
log SHA-256 `3a625809c79c1fdb8937ac36cd6e912b026ffcdc8fbc80c7ed888680d8bb11a7`,
snapshot `8d9b262474b00c6c0a2b5423ce4100363eb4582a96205eae8d045b70c8ae50a7`.
The next instruction executes native BKPT, halting at PC `0x00079e1e`,
instruction 932,397,950 / 11,388,431,927 ns. A read-only trace with the normal
NEMA backend identifies `CXD5610GF-driver.cpp:910`, LR `0x00128f55`;
the native branch increments a retry byte and asserts at three. The missing
GPS exchange/state transition still needs recovery. No reset or extra file
operation occurs after the 21 updates; no settled or interactive 2.39 UI is
claimed. Existing boot-logo hashes remain the required prefix.

An initial backend-less diagnostic instead reaches mode ten for `wui_dump.bin`
at 639,161,545 instructions. This is not the production CLI boundary: attaching
the CLI's NEMA backend reproduces the GPS halt exactly. Earlier backend-less
diagnostics and rendering-backed checkpoints must not be mixed. The unknown
dump mode/path remains refused; this task does not implement it.

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

## Sapporo 2.39 Initial GPS Integration (Ticket 756)

The separately selected `sapporo-2.39-gps-startup` layer now implements the
two evidenced synthetic status lines from E-SAP-COMPAT-GPS-STARTUP-239-001.
It requires the exact profile/component hashes, validates the initial native
driver/UART boundary, and queues startup RX and the exact `@VER\r\n` reply
through the existing delayed UART/IRQ path. Each response has one hit; the
aggregate ceiling is two. No CPU, RAM, native-state or firmware-byte patch is
made by this layer. Instance-owned counters survive the unchanged snapshot
format; reset and restore explicitly bind the fixture. Unsupported state,
commands, repeats, budgets and scheduling failures refuse atomically.

Two fresh two-layer production runs reach native state 15 with retry zero:
`stop=budget pc=0x00128ed8 instructions=393785845 virtual_time_ns=2564070074`.
Log SHA-256 `b5b23c9f6a96d9ecfbf4f17aa4f3b70801d08cd9b9636330d11c08f2e0647123`,
snapshot `bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9`.
Both artifacts compare byte-identically. Snapshots before startup, with either
response pending and at native state 14 reproduce the same final snapshot and
log suffix. Completed exchanges also resume deterministically to the later
retry. One-layer images are rejected by two-layer configurations; historical
one-layer logo/activity/halt goldens remain unchanged. All 752 normal and
sanitizer tests and both private GPS/activity gates pass. The ticket index
now marks ticket 756 done after review of implementation commit `fa56b33`.

With only the initial layer selected, this remains initial GPS lifecycle
progress, not full GPS or settled 2.39 UI.
The distinct later reopen sets pending state seven at instruction 672,044,891;
its missing response still times out. The subsequent retry reaches
`0x00128d14` at instruction 908,321,039 / 14,978,258,084 ns and is refused
without another status or hit. This two-layer behavior is preserved.

Read-only follow-up E-SAP-COMPAT-GPS-REOPEN-239-001 now recovers that bounded
exchange. One external delayed status at the post-arm `0x00128e8c` boundary
advances native states 7/8/9 to exact `@GSR\r\n`; one delayed synthetic reply
advances states 10 and 12 with retry zero. Two diagnostic runs are identical,
and wrong-prefix controls still time out. The later liveness-recovery command
`@GSTP\r\n` is the next precise UART refusal, at instruction 940,963,736 /
16,349,008,531 ns. These experiments establish synthetic parser acceptance,
not a physical receiver transcript.

## Sapporo 2.39 Bounded GPS Reopen (Ticket 757)

The explicit `sapporo-2.39-gps-reopen` layer now implements the two evidenced
responses separately from startup. It requires the separately selected initial
GPS layer and all three exact component hashes. At post-arm `0x00128e8c`,
it validates the complete driver/UART spans, state, flags, cached GNS and
completed initial exchange before queuing one status after ten ms. The exact
six-byte `@GSR\r\n` then receives one status after ten ms. No CPU/RAM/native
event/GPIO patch is made. Instance-owned counters, reset and restore binding,
dependency validation and atomic refusals use the unchanged snapshot format.

Two fresh three-layer runs compare byte-identically at native state 12,
retry zero, with exactly two initial and two reopen interventions:
`stop=budget pc=0x00128ed8 instructions=825147087 virtual_time_ns=10875951888`.
Log SHA-256 `f63cab509a2da82a867580bf9eac35b3e764df53d08155bb11c47c6dfa328007`,
snapshot `0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`.
Snapshots before reopen, with either response pending and at native state ten
resume to that identical image and full log suffix. Wrong lifecycle, ownership,
layer sets and order refuse; one-/two-layer images do not migrate implicitly.
The historical private startup/activity gates retain their existing hashes.

Ticket 757 is accepted by separate integrator review of `9f8c121`; the
implementation and exact verification record are in its handoff. The next
unsupported operation is still `@GSTP\r\n`: at instruction 940,963,736 /
16,349,008,531 ns the native precise UART fault has BFAR `0x4001d000` and
stacked PC `0x00171798`, with no extra compatibility hit. Receiver awake/liveness,
GPS time/fix, later commands and a settled 2.39 UI remain unimplemented.

## Sapporo 2.39 Awake Evidence and Integration Prerequisite

Read-only firmware recovery now identifies the cause of that later recovery:
GPIO24's native callback `0x00128926` sets awake byte `0x100588a2`; state twelve
consumes it between polls. E-SAP-COMPAT-GPS-AWAKE-239-001 independently pins
the 2.39 IRQ table, registration, callback and branch rather than transplanting
the 2.22 hook. External pulses through the existing transport/GPIO path, each
100 ms after a successful poll and high for 1 ms, produce the native IRQ and
avoid GSTP. One pulse postpones recovery by one poll; a late pulse does not
avoid the original fault. Four pulses produce four callbacks and five native
state-twelve successes, retry zero, without another UART command.

Two four-pulse diagnostic runs compare byte-identically through the 35-second
guard: instruction 1,315,882,442 / 35,000,617,152 ns, PC `0x000e955e`, both
existing GPS layers still at two hits. Trace SHA-256
`ca3527d679f889242849f6bfe52ed726cc0c817cb329b45630c6bc2a5ad72010`,
log `5bdb32dc4a6a417d5b16d78d4348681c5ee1cedd47e170ca8dd7bc825171a212`.
These are external synthetic-input experiments, not enabled production
behavior, physical receiver cadence, NMEA/time/fix or a settled UI milestone.

Before integration, E-EMU-CXD-AWAKE-FAILURE-001 exposes a transport prerequisite:
failed pulse admission changes bookkeeping and emits a low callback; a failed
falling-edge schedule emits a zero-duration pulse and reports success. A
six-case host-only reproducer covers time/ID/sequence exhaustion at both
stages. The current void scheduler callback interface cannot report that
second-stage failure. Ticket 758 is ready and explicitly owns the minimal
scheduler/device failure contract; ticket 759's four-hit awake fixture is
blocked until 758 is accepted. No production C, profile, counter, snapshot
encoding or runtime dependency changes in this evidence/planning step.

Verification on unchanged `9f8c121`: `make check` passes 759 cases, and the
exact ticket-757 private reopen command passes without skips or re-pinning.
The prior 759-case sanitizer result remains applicable to unchanged C;
sanitizers were not rerun for this documentation/planning-only step.
The ledger records external source/trace hashes and negative controls; the
new ticket contracts specify the remaining implementation gates.
`make check-task-contracts` validates 129 tickets and `git diff --check`
passes. Changed repository files are this status, `docs/migration-evidence.md`,
`plans/index.tsv`, and tickets 757/758/759 only. No private artifact is added.

## Ticket 758 Implementation — Acceptance Pending

The pulse failure integration now validates complete time bounds and schedules
before changing admission state/signals. Falling-edge failure reports through
the new public copied-first-error scheduler contract without emitting high;
CPU tick and WFI/WFE paths stop with the original device-refusal diagnostic.
Reset/reentrancy behavior is documented in `docs/execution-model.md`. The CXD
snapshot codec is split out without changing encoded bytes. No new 2.39 awake
fixture, UART response, profile or layer is enabled.

Nine new cases raise normal and sanitizer coverage to 768 passing cases.
All focused ticket commands, line checks and 129 task contracts pass. The
unchanged private 2.39 reopen gate passes without re-pinning. Clean baseline
and changed 2.22 headless runs through the first awake pulse have identical
logs and snapshots: stop `budget`, PC `0x000d4a8c`, 599774578 instructions /
12027701702 ns; snapshot SHA-256
`9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.

The authentic SDL live-input gate fails its historical stop tuple on both
the clean starting commit `9f8c121` and the changed build, with byte-identical
logs and all expected CRCs. Both stop at PC `0x000bacf4`, 774081920 instructions /
6520939902 ns rather than the E-SAP-ONBOARD-EMU-011 tuple. No golden is changed.
E-EMU-CXD-AWAKE-FAILURE-001 and ticket 758 record the exact commands and hashes.
That was the implementation handoff's unresolved gate. The separate maintenance
investigation below resolves its cause and passes the corrected strict check.
The subsequent integrator review below accepts 758 and makes 759 ready.

## SDL Live-Input Checkpoint Maintenance

E-SAP-ONBOARD-EMU-012 isolates the old checkpoint's extra startup delay to the
shared haptic fixes already committed in `d6b4235` and `df93397`. Clean builds
before those fixes reproduce the old log hash exactly. Observational traces
show command `0x22000112` incorrectly selecting status register `0x08` from
adjacent SRAM, returning zero for 32 reads and exhausting the firmware's
nonfatal autotune timeout. With the correct command selector, autotune returns
complete (`0x03`) and the two calibration reads succeed. Applying only those
two existing corrections to the historical source reproduces the current
complete SDL log byte-for-byte; ticket 758 is not the cause.

Bounded maintenance changes only `tools/test_sdl_live_input.sh`, this status
and the evidence ledger. The gate now pins PC `0x000bacf4`, 774081920
instructions / 6520939902 ns and, for cold boots, log SHA-256
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`.
All initial/settled CRCs and generations, stop reason, exit code and invalid
configuration checks remain required. There is no runtime, timing, profile,
compatibility, snapshot or release-frame golden change in this maintenance.

Two fresh exact-firmware runs compare byte-identically. The existing authentic
regression fails before the smoke expectation correction and passes afterward:

```sh
build/suunto-emu validate --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
make check-sdl
make check
git diff --check
```

Validation and the focused authentic gate exit 0. `make check` passes 768
cases, line checks and 129 task contracts; `git diff --check` is clean.
An external negative harness
appends one entry to an otherwise exact transcript; all existing checks match
but the new cold hash check exits 1. `make check-sdl` without a manifest passes
five SDL cases and configuration refusals, explicitly skipping its firmware
walks; the short authentic walk was run separately above. The longer manual
time-entry/onboarding gate was not revalidated or re-pinned. No new firmware
fixture or GPS command is enabled. Sanitizers are not rerun for this shell/
documentation-only change; the prior 768-case result applies to unchanged C.

## Ticket 758 Integrator Acceptance

Separate review/planning maintenance accepts 758 and makes 759 ready. Only
the index, those two tickets, this status and the evidence ledger change in
this review. No runtime behavior or private fixture is added. Source review
confirms atomic admission, copied first callback failure, CPU tick/sleep
propagation, reset/reentrancy refusal and unchanged successful serialization.

All ticket commands were rerun: focused scheduler/CXD/sleep/snapshot groups
pass 4/13/4/4 cases; `make check-task-contracts` validates 129 tickets;
`make check-lines`, `make check` (768 cases), and `make sanitize` (768 cases)
pass. The exact private 2.39 reopen gate passes without skips and retains its
state-12 snapshot `0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`
and precise later GSTP refusal. `make sdl`, 2.22 component validation and the
authentic short live-input gate all pass under E-SAP-ONBOARD-EMU-012.

A fresh bounded 2.22 headless run through its first awake intervention has
the expected budget exit 3 and byte-identical log/snapshot to clean `9f8c121`,
including snapshot `9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.
An external deterministic allocator-fault probe passes three additional
cases normally and under ASan/UBSan; E-EMU-CXD-AWAKE-FAILURE-001 records the
source hash and commands. Ticket 758 records the complete acceptance handoff.
The longer manual-entry SDL gate remains a separate audit; no GPS fix/time,
physical receiver cadence or new production awake layer is claimed.

## Ticket 759 — Accepted Bounded GPS Awake Integration

The explicitly selected `sapporo-2.39-gps-awake` layer now supplies exactly
four synthetic GPIO24 pulses through the accepted transport. It requires the
separately selected startup/reopen layers, all exact firmware hashes and both
completed two-hit lifecycles. Each evidenced successful state-twelve poll
admits one pulse after 100 ms, high for 1 ms. Only native GPIO IRQ handling
sets the firmware's awake byte; the hook changes neither CPU nor guest RAM.
R1 scheduling returns are not predicates. VER/GSR providers remain unchanged.

Instance-owned counters, explicit reset/restore binding, atomic refusals and
strict snapshot lifecycle/event validation are covered. The layer codec was
split into `machine_snapshot_layers.c` with unchanged bytes for earlier layer
sets. No implicit snapshot migration or new scheduler/device behavior is added.

Two fresh four-layer runs and all four snapshot-phase resumes match exactly:
`stop=compat-refused pc=0x001291cc instructions=1272353867 virtual_time_ns=32770943068`.
GPS hits are `2,2,4`, retry zero, GPIO24 low and no pending pulse. Log SHA-256
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`;
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
The private observer verifies four native awake callbacks/STRB results and
the same final image. A repeated fifth-hook attempt changes no snapshot byte
and emits no further hit. This is an intentional evidence-bound refusal.

All 775 normal and sanitizer cases pass, along with 129 task contracts and
line checks. The unchanged private startup/reopen/activity gates preserve
their hashes; the 2.22 headless first-awake log and snapshot remain byte-identical
to baseline. The ticket handoff records exact commands, files and evidence.
Separate integration review on 2026-09-06 found no blocking issues and reran
every ticket command: 775 normal and 775 sanitizer cases, all six focused
groups, 129 task contracts, line checks and all four private gates pass.
The private runs preserve the exact hashes above and the historical gates'
pins. Ticket 759 is now done; only planning/status records changed during
acceptance. Full GPS, fix/time data, physical cadence and a settled 2.39 UI
are still not established.

## Sapporo 2.39 UI Investigation — Renderer Gaps

E-SAP-UI-239-001 measures progress within the accepted four-pulse interval.
Two cold idle runs reach the native `Select language` tile at 5,805,310,840 ns,
CRC `3bd12ac8`, then retain ticket 759's exact final snapshot hash. A normal
middle-button press advances to a language menu; upper/lower controls do not
leave that tile. Cold and resumed middle runs converge to identical final
pixels and machine state. Read-only native tracing confirms button events
2/5/1 and view-open calls without invoking firmware callbacks directly.

The language image is incomplete, not a new correct-frame milestone: 52 of
79 backend submissions refuse during the cold middle-button run. The first
is a fully clipped-out animated A2LE glyph. The shared sampling helper
rejects its offscreen origin; a second synthetic case returns an inverted
intersection. Separately, the GPU drops backend REFUSE and its error, reports
successful MMIO and consumes the ring submission. Small external failing
reproducers now isolate both bugs, including a successful-backend control
and an ASan/UBSan run of the swallowed-refusal case. No runtime fix is made
by this investigation; the passing 775-case suite does not cover them yet.

Two properly held middle clicks advance native view selection and then hit
the existing logical-file ceiling at `settings/general`, mode two,
PC `0x000920b4`, 1,376,488,437 instructions / 14,401,737,146 ns. Two probes
match exactly, GPS hits `2,2,1`. This later boundary follows silently refused
renderer work; it is not authority to raise the file budget. Only this status
and the evidence ledger change; all probes, snapshots and pixels remain
external. `make check` passes 775 tests, line checks and 129 task contracts.
Full sanitizer and SDL suites were not rerun for these documentation changes.

## Shared Sampling Clip Correction

Bounded maintenance under E-SAP-UI-239-001 / E-EMU-SAMPLING-CLIP-001 fixes
the offscreen and inverted-intersection bugs identified above. The shared
helper returns canonical empty bounds for valid disjoint rectangles, checks
layout/endpoint/offset arithmetic, and leaves outputs unchanged on refusal.
Unsigned geometry remains the contract. Changed files for this follow-up:
`src/display/sampling.c`, `src/display/sampling.h`,
`tests/unit/test_sampling_clip.c`, this status and the evidence ledger.

The two original synthetic regressions fail before the fix. Five new cases
now cover them, 1,728 pixel-oracle combinations, malformed-input atomicity,
and all three sampling consumers. All 780 normal and sanitizer tests pass;
27 focused sampling tests, line checks and 129 task contracts pass. The
private 2.39 awake gate and 2.22 short SDL gate preserve their exact pins.

Two corrected cold middle-button runs accept all 79 submissions; a prefix
resume accepts all 76 remaining submissions. They converge to byte-identical
final pixels and machine snapshots. The language list is visually clear,
CRC `6b6aa2dc`; no stale green icon remains. The evidence entry records full
hashes and exact reproduction commands. This is an emulator observation,
not a physical-panel golden. Backend-refusal propagation is still unfixed;
neither file nor GPS budgets are extended, and no integrator change is needed.

## NEMA Atomicity Integration Prerequisite (Ticket 761)

Commit `c7800be` records the accepted GPS-awake work and sampling correction.
The subsequent bounded GPU investigation changes no runtime code.
E-EMU-NEMA-ATOMIC-001 extends the swallowed-refusal reproducer with three
failures using the real backend: refused commands leak inherited register
state; a failed second child leaves the first child's frame published; a
failed second completion admission leaves the first event and consumed ID.
Two normal and one sanitizer run match exactly; success controls pass and
there are no sanitizer findings. The existing 780-case suite still passes
but does not cover these failures yet.

Returning an error alone cannot undo those mutations. The existing public
backend has no prepare/abort boundary, and completion admission is one event
at a time. Following the repository's insufficient-interface rule, runtime
work stops pending the explicit integration in ticket 761. It is ready, with
all dependencies done, and owns the minimal display/scheduler contract,
caller migration and rollback/refusal regressions. It must preserve successful
frame/event order and snapshot bytes; a partial-output error is not sufficient.
This planning step changes only ticket 761, `plans/index.tsv`, this status and
the evidence ledger. `make check` passes 780 cases, line checks and 130 task
contracts. No new firmware execution, full sanitizer rerun or budget change
is claimed; prior runtime checkpoints remain unchanged.

## NEMA Atomic Admission Foundation (Ticket 761, Partial)

E-EMU-NEMA-BATCH-001 records the first implementation slice: scheduler batch
admission validates and reserves all events before consuming any identity;
NEMA completion batches stage entries and preserve complete snapshots on
refusal. Cancel/reset/destroy remove owned callbacks, including protection
against cancelling another owner's reused event ID after scheduler reset.
The completion codec is split without changing its bytes. Existing single
admissions use the same path and preserve successful event ordering.

All 789 normal and sanitizer tests pass. The short 2.22 SDL gate, 2.39 awake
gate, two cold middle-button runs and a 700-million-instruction prefix resume
preserve their exact prior checkpoints. The corrected language frame remains
CRC `6b6aa2dc`; cold traces and final pixels/snapshots are byte-identical.

This is not whole-ring atomicity: the GPU still admits markers individually,
and the backend still lacks staged multi-child publication/inherited-state
rollback. Refusal propagation remains unfinished. Ticket 761 stays incomplete
and its index status is unchanged; no firmware budget or golden is extended.

## NEMA Backend Transaction Foundation (Ticket 761, Partial)

Commit `ce0e529` records the scheduler/completion foundation. The next slice
adds public display prepare/commit/abort operations and an in-tree backend
implementation. It stages inherited registers, pixels, TSC6A shadows and
ordered per-child frame images; commit neither allocates nor fails. The
existing single-list convenience now uses that path. Refused lists preserve
inherited state and return the original detailed error even after diagnostic
saturation. Reset/reentrant submission conflicts refuse before mutation.
The large backend is split into lifecycle/list execution, draw dispatch and
transaction responsibilities; no persistent encoding changes.

E-EMU-NEMA-BACKEND-001 records three regressions failing before implementation,
seven final backend cases, and 796 passing normal/sanitizer tests. The exact
2.22 SDL and 2.39 awake gates pass. Two cold middle runs and a prefix resume
retain every corrected trace/pixel/snapshot pin. The original five-scenario
GPU probe now has two failures rather than three: inherited state is fixed,
but later-child publication and marker admission remain non-atomic because
GPU/machine callers have not yet migrated. Ticket 761 is not complete and its
status remains unchanged. No budget, fixture, physical-panel claim or golden
is extended.

## NEMA GPU Transaction Integration (Ticket 761, Partial)

Commit `0e8900d` records the backend foundation. The continuation migrates
GPU/machine/CLI callers to the public transaction operations: prepare all
children, admit all markers, then commit frames/registers/stop/generation.
Later-child and completion-admission refusals now preserve the GPU snapshot,
emit no frames and allow a corrected same-stop retry. Original errors reach
MMIO; missing backend/scheduler and unexpected callback results refuse.
Reentrant GPU reset/write/snapshot and machine reset are guarded. GPU lifecycle,
submission and snapshot responsibilities are split with unchanged codec bytes.

E-EMU-NEMA-GPU-001 records the two failing pre-migration regressions, six final
GPU cases and the two-instruction synthetic CPU success/fault test. The original
five-scenario probe now reports zero failures on repeated normal and sanitizer
runs. All 803 normal and sanitizer tests pass. Both private firmware gates and two cold/prefix-resumed language-screen
probes preserve the exact earlier pins, including CRC `6b6aa2dc`.

Ticket 761 remains incomplete: strict ring/wrap/odd-tail validation, composed
allocation-failure coverage and the final acceptance audit remain. No ticket
status, golden, budget, firmware data or CPU/bus policy changes.

## NEMA Control Validation and Allocation Coverage (Ticket 761, Partial)

Commit `1733bb8` records the GPU transaction integration. The continuation
corrects non-power-of-two ring wrap arithmetic, checks complete ring-address
arithmetic, validates held-control/marker fields and refuses unsupported GPU
access widths. A malformed control after a valid child changes no GPU snapshot,
frame or event; correcting the same stop executes once.

The firmware gates caught an overstrict draft that accepted only base-wrap
targets. Read-only reverse engineering established that both native 2.22 and
2.39 marker builders also emit a held jump to the immediate continuation word.
E-EMU-NEMA-CONTROL-001 records the exact instruction ranges and first-refusal
trace. The validator now preserves that form without accepting arbitrary jumps.

Deterministic allocator replacement in test-only compilations of production
sources exercises the full MMIO/parser/backend/completion/scheduler path. Every
allocation refusal preserves the existing queue, identities, inherited color,
pixels, generation and GPU snapshot; retries publish both children and complete
both markers in order. No runtime hook or build dependency is introduced.
Ticket 761 remains incomplete pending strict inline/padding/unmatched-tail
validation and final acceptance review; its status/index and goldens are unchanged.

All 809 normal and sanitizer tests pass, along with 130 task contracts and
line checks. Both exact private firmware gates pass after the evidence-backed
control correction. Two cold language runs and the prefix resume preserve
every prior trace/pixel/snapshot hash and the exact endpoint. No newly exposed
refusal was hidden and no acceptance checkpoint was weakened.

## NEMA Unmatched-Tail Refusal (Ticket 761, Partial)

The prior control/allocation work is committed as `611d3c4`. Native research
reconciliation establishes that the apparent rounded tails in early captures
were artifacts of treating CMDSIZE entries as bytes. The framing parser and
backend now refuse odd word counts, including held register tails, before any
callback or renderer staging. They neither ignore the last command nor read
its value beyond the declared list (E-EMU-NEMA-TAIL-001).

Three new regressions fail on the committed baseline and pass with the fix:
backend inherited-color/pixel preservation, later-child GPU/marker refusal and
zero framing callbacks. Complete paired replacements succeed at the same stop.
The native observer finds inline initialization in both pinned firmware
versions; strict inline/padding interpretation and final acceptance review
remain unfinished. This is not ticket completion or a new hardware claim.

All 812 normal and sanitizer tests pass. Both exact private firmware gates,
two cold 2.39 middle-button runs and the 700M-prefix resume pass with unchanged
trace, pixel and snapshot hashes, including frame CRC `6b6aa2dc`. Ticket 761's
handoff records the commands, bounded native syntax inventory and provenance.

## NEMA Inline Ring Transactions (Ticket 761, Partial)

The 2026-09-07 continuation replaces permissive inline-word skipping and the
independent marker scan with one validated ring plan. Unknown inline registers,
prefixes, nonexact NOPs and incomplete pairs refuse. Values resembling ring
opcodes remain values. Inline state and draws now participate in the same
transaction as child lists, without additional child frame publications.
Descriptor flags extend the existing public prepare/commit/abort contract;
no private backend API or persistent encoding is introduced.

E-EMU-NEMA-INLINE-001 records the native paired-stream evidence, the failing
mixed-command regression and corrections to two old malformed synthetic padding
fixtures. Bounds permit 32 children, 64 markers and 64 total child/inline spans.
Wrapped complete runs and atomic plan refusal are tested; a graphics pair split
across the physical ring end remains explicitly unsupported. IRQ-clear semantics
and fragment-program ISA execution are not inferred from initialization writes.
Final malformed-input/lifecycle review remains; ticket status is unchanged.

All 815 normal and sanitizer tests, both private firmware gates, two cold
language-screen probes and the prefix resume pass. Every earlier trace,
frame and snapshot pin is unchanged. Ticket 761's handoff records exact
commands, test counts and the revised read-only observer's source hash.

## NEMA Memory-Only Command Fetches (Ticket 761, Partial)

E-EMU-NEMA-MEMORY-001 identifies device-read side effects during command
validation. Ring, child and direct-backend command fetches now share the
existing bus memory-copy path with explicit little-endian decoding. Device
commands refuse without invoking their callbacks; synthetic RAM/ROM commands
remain supported. Four zero-read regressions fail before the fix and pass
afterward, alongside a ROM success control and corrected RAM retries.

The audit also reproduces read side effects before texture validation,
RGB565 sampling and A2LE sampling refuse. These readers and their tests are
outside ticket 761's Allowed Files, so they have not been changed. Integrator
authorization to extend that scope is required; staged pixels cannot undo
device callbacks. Callback lifecycle acceptance also remains. No ticket status,
bus policy, profile or snapshot format is changed.

All 820 normal and sanitizer tests pass, as do both exact private firmware
gates, the SDL build, task-contract and line checks. Two cold language-screen
runs and the 700-million-prefix resume retain every trace, frame and snapshot
pin. Exact commands, counts, evidence and the scope request are in ticket 761.

## NEMA Memory-Only Texture Reads (Ticket 761, Partial)

The user authorized the texture-reader scope extension on 2026-09-08.
E-EMU-NEMA-TEXTURE-MEMORY-001 promotes the previous external counter probe into
five MMIO refusal regressions, an A2LE output-preservation regression and a
composed later-child texture failure. Validation and RGB565/A2LE reads now use
the existing byte-wide memory-copy contract; a one-byte device overlay cannot
be bypassed with a wider copy. An A2LE read refusal no longer clears RGB fields.
Mapped-memory errors retain their original code/text instead of a generic
validation error. No compatibility hook, bus policy or persistent format changes.

RAM retries and adjacent-ROM RGB565/A2LE/bilinear success controls pass. The
backend refuses a device byte in the second texel after staging an earlier
child draw, retaining pixels, frame count and inherited state. The corrected
transaction retries successfully. Descriptor validation remains a bounded
last-byte probe; actual source bytes are independently checked by the sampler,
and rendering targets stay staged. Callback lifecycle acceptance remains;
the texture scope request is resolved and ticket status is unchanged.

All 828 normal and sanitizer tests pass. Both exact private firmware gates,
SDL build, line and task-contract checks pass; two cold language-screen runs
and the prefix resume retain every prior frame, trace and snapshot hash.
Ticket 761's texture-reader handoff records exact commands and results.

## NEMA Callback Lifecycle (Ticket 761, Accepted)

The previously verified tail/inline/command-memory/texture changes are committed
as `cfce2a1`. The 2026-09-08 continuation fixes the final identified completion
callback lifetime defect (E-EMU-NEMA-CALLBACK-001): dispatch now copies its
notification recipients before making the entry reusable. Scheduling from CLID
cannot redirect INTERRUPT/IRQ, and resetting/cancelling/destroying the standalone
completion owner cannot invalidate the in-flight sequence. Callback contexts
and the scheduler must remain alive; machine/device owner destruction and
recursive scheduler dispatch are not authorized by this contract.

The new regression fails before the fix and passes afterward, including slot
reuse, reset, cancel and owner destruction with live external contexts. It checks
exact recipient/order/value, queued-event cancellation, counters and the next
100-us deadline. All 829 normal and sanitizer tests, both exact private firmware
gates, SDL build, line checks and task contracts pass. Two cold language-screen
runs and the prefix resume retain every prior trace, frame and snapshot hash.

The lifecycle continuation is committed as `2d230a2`. The separate 2026-09-08
planning-only integration pass reviewed all acceptance evidence and accepts
ticket 761, updating its status/index to done. Review reruns of NEMA (112),
scheduler batch (four), and all 829 normal tests pass. No further scope extension
is requested; unsupported command/source/callback cases remain explicit in the
ticket handoff. This is not a claim of new physical GPU or firmware coverage.

## Native General-Settings Boundary (Ticket 762, Evidence Collected)

Two normal-backend, four-layer runs from the pinned 700-million-instruction
prefix reproduce the two held MIDDLE presses with 148 successful submissions
and zero renderer refusals (E-SAP-UI-239-002). Both stop at instruction
1,376,488,437 / 14,401,737,146 ns / PC `0x000920b4`, opening
`settings/general` in mode two; GPS hits remain `2,2,1`. Their traces and all
five saved machine checkpoints match pairwise. No budget or firmware was
changed. The observed native caller tests a pending flag, opens the file,
calls `0x000d5794`, closes the handle, and clears the flag. The runtime stop
precedes the open's success: the complete serialization sequence and required
finite operation count are not yet measured. Ticket 762 scopes that evidence
work before a separate production integration decision.

The 2026-09-08 continuation now measures that complete sequence using an
isolated, bounded diagnostic executable (E-SAP-COMPAT-GENERAL-239-001):
open, 90 successful contiguous writes totalling 1,505 bytes, and close.
The native serializer returns success and clears its pending-save flag.
Exactly 92 additional operations imply a proposed production ceiling of
76,371 logical-file / 76,374 aggregate hits; no ABI change is indicated.
Production remains unchanged at 76,279 / 76,282 until separate integration.

Two pre-screen-prefix runs, two refusal-start runs and a mid-save snapshot
resume agree on final pixels and machine state. The prefix runs accept all
676 submissions and visibly reach `Define your profile` (CRC `405d1af6`).
They stop at the existing fifth GPS-awake refusal, PC `0x001291cc`,
2,363,623,546 instructions / 32,619,070,564 ns, GPS hits `2,2,4`.
No further logical-file hit occurs between save completion and that stop.
This does not reach the watch face or demonstrate post-setup menu navigation.
Ticket 762 records exact repeat/resume hashes, commands and the smallest
proposed production integration scope; its status awaits integrator review.

## Production General-Settings Save (Ticket 763)

The separate integration review accepts ticket 762's evidence; ticket 763
implements exactly its 92-operation allowance. Production now permits 76,371
logical-file / 76,374 aggregate hits. Only the two budget constants and evidence
comment change in runtime code. Other one-hit interventions, firmware hashes,
file semantics/capacities, rendering and the four-pulse GPS limit are unchanged.
The new synthetic regression fails at the native-shaped open before the change
and passes afterward, including all 90 write sizes, exact close at the limit,
mid-write snapshot restore and atomic excess/unknown-operation refusal.

The production observer matches the accepted native profile frame (CRC
`405d1af6`), mid-save snapshot and final machine hash exactly. Further bounded
native MIDDLE presses at requested 20, 22 and 24 seconds reach the birth-year
selector. Both repeats then refuse a mode-two `settings/personal` open at
PC `0x000920b4`, LR `0x000adb2f`, instruction 2,953,605,137 /
24,380,651,994 ns, with GPS hits `2,2,3` and 772/772 renderer submissions
accepted (E-SAP-UI-PERSONAL-239-001). This exposes the next persistence
sequence before GPS exhaustion; it is not evidence for another budget increase.
The watch face and post-setup menu navigation have not yet been reached.

## Next Actionable Work

Phases 0–6 and the first-target functional milestone are complete. The Phase 7
behavior/release templates remain blocked until their product-specific evidence
is instantiated; independent product evidence inventories no longer wait on
another product's release. Bounded maintenance may proceed under `AGENTS.md`
without manufacturing a roadmap row. The practical work queue is:

- Ticket 761 is accepted. Preserve its native rendering and historical checkpoint
  pins; no private backend API or repinning is needed.
- Ticket 762 is accepted; review ticket 763's production integration handoff.
  Next recover the complete native `settings/personal` save at the birth-year
  boundary in E-SAP-UI-PERSONAL-239-001 before proposing an additional finite
  allowance or ABI change. Do not infer its write count from `settings/general`.
  Subsequent profile choices, watch-face activation and menu navigation remain
  the functional goal, not an already-completed milestone. Preserve the four-pulse GPS bound,
  normal NEMA backend and layer sets. No GSTP response, invented GPS fix/time,
  indefinite heartbeat or assertion bypass is authorized by this observation.
- Recover a native provenance sidecar and an equivalent settled command/text
  contract for E-SAP-ONBOARD-001 before adding a screen-specific emulator
  checkpoint; until then keep `setup-next` neutral. This remains an SDL
  renderer milestone, not a physical-panel claim.
- The skip-phone-pairing transition now works via the corrected LOWER/GPIO59
  mapping (E-SAP-ONBOARD-EMU-008). The onboarding now completes standalone,
  end-to-end, to `main` (E-SAP-ONBOARD-EMU-010): MIDDLE on `w-tida`
  (`8362b9bc`, ~23 s) opens `w-ltim` ("Searching for GPS"); after its 5 s
  subscribe fires with the pre-gate `LocalTime`, the "SET MANUALLY" button
  (LOWER) appears at ~28.5 s; a time-scheduled LOWER (new opt-in
  `SEMU_SDL_SETUP_WALK_TIMELINE`) then drives the manual chain
  `w-year` → `w-mont` → `w-day` → `w-time` → `w-done` ("Done") → `main`
  (settled CRCs through `1c62ab1a`, final main-menu frame `fb8e0155`, stop
  `halt` at ~43.8 s virtual). No phone/OHR2 time source is involved: the 2.22.60
  firmware has none (009(c)). The deterministic completion sequence is committed
  under ticket 670 with a regression (`tools/test_sdl_onboarding_completion.sh`,
   wired into `make check-sdl`); the canonical sequence writes a post-gate time
   (2023-01-01), so the next boot routes to `main` instead of `n-sync-rec`.
- The short firmware-gated SDL live-input check now enforces the corrected
  haptic startup sequence under E-SAP-ONBOARD-EMU-012. Preserve its exact
  frames, stop and cold-log hash. Audit the longer manual-entry onboarding
  gate separately against the same haptic correction; do not assume its
  historical time/generation assertions remain valid or weaken them blindly.
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
