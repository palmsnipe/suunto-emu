# suunto-emu

`suunto-emu` is a standalone C99 emulator for Apollo4-era Suunto watches. It
uses an in-tree ARMv7-M interpreter, deterministic virtual time, strict device
contracts, immutable firmware inputs, and optional SDL3 presentation.

The first Sapporo target is functional: the deterministic interpreter runs the
exact pinned firmware through startup and native renderer traffic, publishes a
240x240 UI, accepts three-button interaction, and supports replay and machine
snapshots. Hardware coverage remains evidence-scoped; physical-panel behavior,
unobserved device commands, and unpinned firmware versions are not implied.

Current stability limits (2026-09-22): a Thumb branch-decoding fix removes the
2.22 script loop that exhausted its heap after setup. Paired button-driven
runs now finish the setup navigation and remain active through 60 virtual
seconds. The main menu renders and responds to a selection change; broader
functions and restoring an interactive window still need verification. Ticket
789 re-derives the bounded 2.22/2.35 regression checks against the corrected
CPU, with control runs proving the cause of the changed checkpoints. The 2.35
firmware gates pass, including their intentional unsupported boundaries.
Later Sapporo profiles remain at different bring-up stages; they are not yet
a uniformly passing firmware suite. See `docs/current-status.md`
for the current audit and gaps.

For the pinned 2.35.34 OTA, `--layer sapporo-2.35-production-data` explicitly
supplies synthetic manufacturing records and reaches normal boot mode. This
is a bring-up option: startup now passes the two absent pressure-sensor
probes and haptic initialization. Add `--layer sapporo-2.35-ohr-startup`
to supply the lane's eight synthetic OHR startup responses. This avoids the
OHR reset. Correct 64 KiB flash erases now let the firmware create and reopen
its logbook, then render the native “Select language” prompt. A later GPS-driver
assertion still prevents a stable session. A bounded middle-button run opens
the language menu with English selected. Both fixtures are explicit, hash-pinned bring-up options. The bounded
display-prefix regression is:

```sh
make test-firmware TEST_PROFILE=sapporo-2.35.34 \
  TEST_FILTER=sapporo_235_block_erase \
  SEMU_FIRMWARE_MANIFEST=/path/to/2.35/firmware.semu
```

For the bounded 2.35 display and button regression, build SDL3 support with
`make sdl`, then run `sh tools/test_sdl_sapporo_235.sh`. It auto-detects the
private 2.35 bundle or accepts `SEMU_FIRMWARE_MANIFEST`; no firmware is bundled.
To explore that prefix interactively:

```sh
build/suunto-emu-sdl run \
  --profile sapporo-2.35.34 --firmware /path/to/2.35/firmware.semu \
  --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
  --until middle-language --wait-for-quit \
  --max-instructions 3000000000 --max-time 11000000000
```

Press Enter through the startup checkpoints to reach the language menu; Up,
Down and Enter then control the watch buttons. This is a bounded preview:
GPS startup and full onboarding remain unfinished.

The opt-in `--layer sapporo-2.35-gps-startup` supplies two synthetic
status/version responses and passes the initial GPS assertion. Add
`--layer sapporo-2.35-gps-reopen` to supply the separately observed reopen
status and exact GSR reply. Both layers are hash-pinned and limited to two
hits each. With all four layers, paired button-driven runs reach the native
Welcome, birth-year and unit-system screens. Ongoing GPS remains unsupported:
the cold run stops on `@GSTP` at about 16.3 virtual seconds, and the scripted
setup walk stops at about 16.0 seconds. Full onboarding is still incomplete.

To reproduce the bounded reopen regression:

```sh
make test-firmware TEST_PROFILE=sapporo-2.35.34 \
  TEST_FILTER=sapporo_235_gps_reopen \
  SEMU_FIRMWARE_MANIFEST=/path/to/2.35/firmware.semu
```

The optional `--layer sapporo-2.35-gps-awake` supplies up to 64 lane-observed
synthetic GPIO24 pulses (E-SAP-0049) through native interrupt handling. It
requires both GPS layers above; the bounded awake gate now ends at a 70-second
budget stop with eleven healthy polls, and paired long cold runs keep the
invariant cadence through the 55th admission near 307 virtual seconds before a
high-rate instruction-bound region at PC `0xccac4`, recorded in E-SAP-0049 as
the next gap. Button-driven runs reach weight, height and phone-pairing instructions.
It supplies no GPS fix or phone connection. The RGBA4444 pairing strip now
renders, and Down scrolls the phone instructions without the former GPU fault
and reset. The bounded 22-second regression repeats exactly. A longer button sequence
passes phone pairing, selects the time zone and reaches “Done,” but opening
main currently faults on an unsupported compressed TSC6A icon. The reference
lane also refuses it. A usable main screen and stable long sessions remain
incomplete.

To reproduce the phone-instructions scroll regression (SDL3 and private
firmware required):

```sh
make sdl
SEMU_FIRMWARE_MANIFEST=/path/to/2.35/firmware.semu \
  sh tools/test_sdl_sapporo_235_scroll.sh
```

To explore this longer prefix interactively:

```sh
build/suunto-emu-sdl run \
  --profile sapporo-2.35.34 --firmware /path/to/2.35/firmware.semu \
  --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
  --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
  --layer sapporo-2.35-gps-awake \
  --until setup-next --wait-for-quit \
  --max-instructions 10000000000 --max-time 70000000000
```

The startup checkpoints pause for button input. Up, Down and Enter control
the watch buttons; extending the run budget does not remove the unsupported
GPS boundary.

Firmware is not included. Extract a legally obtained Sapporo 2.22.60 package,
copy `profiles/sapporo/2.22.60/firmware.example.semu`, and point its paths at
the three expanded components.

```sh
make
make test
build/suunto-emu list
build/suunto-emu validate \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu
build/suunto-emu run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --max-time 1000000
```

SDL3 is the only optional runtime library:

```sh
make sdl
make check-sdl
```

`make check` runs `check-sdl-quick` when SDL3 is present: it builds the SDL
frontend, runs the input-mapping and live-test-timeline tests (including their
fail-closed refusal cases) and a dummy-driver smoke, and invokes the
firmware-walk scripts in skip mode so their configuration paths still execute.
Missing SDL3 or skipped walks each print one loud banner rather than passing
silently.

`make check-sdl` runs the fuller firmware-gated walks. It auto-detects the
private bundle at `tests/private/sapporo-2.22.60/firmware.semu`; when that
exists no environment is needed. To point at a bundle elsewhere, set
`SEMU_FIRMWARE_MANIFEST` explicitly:

```sh
SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu \
SEMU_SDL_TEST_SNAPSHOT=/tmp/suunto-ui-preframe.sems make check-sdl
```

Without any manifest found it prints a loud SKIP banner (walks skipped), never
a silent pass.

The snapshot is optional; supplying one only shortens the bounded firmware run.

An OTA-only Sapporo renderer session does not require a full 32-MiB device
flash dump. The three validated OTA components, the explicit no-device layer,
and a sufficiently large deterministic run bound are enough to reach the
renderer path:

```sh
make sdl
build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device \
  --max-instructions 14000000000 --max-time 22000000000
```

Arrow Up, Return, and Arrow Down forward the three Sapporo button edges into
the interpreter. A left click in the upper, middle, or lower window third maps
to the same buttons. SDL presents renderer output; this does not claim physical
panel completion or generic factory-runtime behavior. A full-flash image is
still optional for persistence/erase coverage and is rejected if it has the
wrong size or missing footer.

For the observed 2.22 onboarding layout, the semantic upper/previous edge is
GPIO57, middle/select is GPIO58, and lower/next is GPIO59. The mapping is kept
at the board input boundary; the generic button device remains constructor-
driven.

For live setup navigation, omit `--input-replay` and use the SDL checkpoint
with `--wait-for-quit`:

```sh
build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --until middle-language \
  --wait-for-quit --max-instructions 14000000000 --max-time 22000000000
```

After the middle-button setup frame settles, the window waits for a live
button edge; use Up, Down, Return/Enter, or click a screen third to continue
navigating. The named checkpoint button is required only for the first edge;
subsequent setup edges accept any of the three mapped buttons. SDL holds
each button press for 70 ms of guest time and keeps the released level stable
for another 70 ms, matching the native debounce boundary. Replay input remains
the deterministic path for headless checkpoints.

For a deterministic continuation from an already captured setup boundary, use
the neutral `setup-next` checkpoint with a snapshot load and one middle-button
replay pulse. It reports the first visible post-input frame without claiming
which later setup screen the frame represents:

```sh
build/suunto-emu run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --snapshot-load /tmp/suunto-middle.sems \
  --input-replay /tmp/setup-next.replay \
  --max-instructions 900000000 --max-time 12000000000
```

The snapshot and replay are external, identity-pinned inputs; do not commit
firmware, frame pixels, or private snapshots.

`setup-next` is intentionally a neutral first-visible continuation. The native
`Define your profile` capture is comparison evidence only, and its provenance
does not authorize a screen-specific emulator checkpoint or pixel golden.

The SDL frontend also has a bounded renderer/input walk for reproducing the
post-language path without manually injecting each edge:

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --snapshot-load /tmp/suunto-ui-preframe.sems \
  --max-instructions 8000000000 --max-time 90000000000
```

From the preframe checkpoint, `setup-walk` settles deterministic renderer
transitions through the observed native handoff screen, `Continue the setup
on your phone` — the last settled frame, CRC32 `ea3bc5f8`, unchanged from the
original observation (settled frame generation counters drift with accepted
timing commits and are not quoted here). The built-in sequence injects no
input after the handoff screen; the guest then advertises while waiting for
a phone, and its GPS-awake pulses exhaust the no-device layer's evidenced
eleven-hit `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005), so the run ends
`stop=compat-refused` (trigger `gps-awake-pulse`, exit code 3). That refusal
is itself a checkpointed boundary of the layer, not a failure of this path.
This proves the bounded language-to-phone-handoff path; it does not claim
phone pairing, post-setup watch-face assets, or physical-panel completion.
The historical continuation through the main watch face — 30 settled
screens, generations, halt tuple — lives in
`tools/test_sdl_onboarding_completion.sh` and runs under `make check-sdl`.
Its screen checkpoints still match, but its halt expectation currently fails;
the fatal script-engine path described above prevents a passing completion gate.

For fast iteration, save a machine checkpoint after reaching a useful stage
and resume it without replaying startup. The checkpoint is identity-pinned to
the profile and all three firmware component hashes; firmware and immutable
resource files remain external:

```sh
build/suunto-emu run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --max-instructions 14000000000 \
  --snapshot-save /tmp/sapporo-startup.sems

build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --snapshot-load /tmp/sapporo-startup.sems \
  --max-instructions 15000000000 --wait-for-quit
```

When no explicit limit is supplied on a snapshot load, the CLI grants a
bounded continuation budget from the checkpoint's current instruction and
virtual-time totals. Use the same profile, firmware manifest, and enabled
compatibility layers used to create the snapshot.

Sapporo 2.35.34 currently supports bounded cold runs only: its live RTC and
IOM4 state has no snapshot codec. Snapshot save and restore refuse explicitly
instead of producing or accepting incomplete checkpoints. See
`docs/current-status.md` for the verified boundary of each firmware version.

For cold-start iteration, an opt-in LTO build is available without changing the
normal `make` profile:

```sh
sh tools/build_fast.sh all sdl
build-fast/suunto-emu-sdl run ...
```

The measured local improvement to the UI checkpoint is about 16%; use
`SEMU_FAST_BUILD_DIR` to choose a separate artifact directory. Toolchains
without LTO support fail explicitly.

For the validated Sapporo OTA image, a useful UI checkpoint is just before the
first native frame. Create it once, then start each SDL iteration at the
frame boundary:

```sh
build/suunto-emu run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --max-instructions 450800000 \
  --snapshot-save /tmp/suunto-ui-preframe.sems

build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --layer sapporo-2.22-no-device --until normal-frame \
  --snapshot-load /tmp/suunto-ui-preframe.sems \
  --max-instructions 450900000 --wait-for-quit
```

The checkpoint is external, identity-pinned state; regenerate it whenever the
firmware manifest or compatibility layer changes. The interactive helper also
keeps a `${SNAPSHOT}.provenance` sidecar containing the manifest, selected
binary, library, helper, profile, layer, and capture-boundary identities. A
missing or mismatched sidecar causes a bounded refresh instead of silently
reusing stale state.
Version-2 machine snapshots preserve inherited drawing registers, the TSC6A
shadow, working pixels, and the last displayed frame with its generation.
SDL presents the restored image before waiting for input; restored checkpoints
accept any of the three buttons. Version-1 snapshots are refused because they
lack renderer state: regenerate them from a cold run. This does not add the
missing 2.35 device snapshot codecs.
The SDL frontend reports a bounded CRC32 alongside the first-frame dimensions
and generation, so repeated runs can be compared without storing frame pixels.

To create a reusable **2.22 main-menu** checkpoint, run the bounded setup walk
once (the cold run takes several minutes), then open it interactively:

```sh
SDL_VIDEODRIVER=dummy SEMU_SDL_LIVE_TEST=setup-walk \
  SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
  SEMU_SDL_SETUP_WALK_TIMELINE=30000:l \
  build/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware /path/to/firmware.semu --layer sapporo-2.22-no-device \
  --until setup-next --max-instructions 18000000000 --max-time 60000000000 \
  --snapshot-save /tmp/sapporo-menu.sems

build/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware /path/to/firmware.semu --layer sapporo-2.22-no-device \
  --snapshot-load /tmp/sapporo-menu.sems --until setup-next \
  --max-instructions 18000000000 --max-time 60000000000
```

This opens the native menu with Logbook selected at generation 4510. Up,
Return/Enter, Down and screen-third clicks provide the three watch buttons.
The checkpoint pauses between settled screen changes. The 60-second guest
budget and existing finite compatibility limits still apply; this is not
full watch-function coverage. `sh tools/test_sdl_snapshot_restore.sh` checks
immediate presentation and paired LOWER-button continuation using private
firmware, with a 900-second wall cap per emulator run.


For repeated interactive setup sessions, the helper creates that checkpoint
once and reuses it:

```sh
sh tools/run_sapporo_ui.sh /path/to/firmware.semu
```

The first Return/Enter opens the language screen; after each transition settles,
the next Up, Down, or Return/Enter edge continues into setup. Pass a second
argument to choose the checkpoint path, or pass `lower-transition` or
`setup-next` as a third argument (or set `SEMU_SAPPORO_UI_CHECKPOINT`) to choose
the required edge at that setup boundary. Set
`SEMU_SAPPORO_UI_BUILD_DIR=build-fast` to run the helper with the
isolated fast build. The helper re-creates the snapshot when it is missing,
older than either selected emulator binary, or fails the provenance check; the
headless binary is needed for that refresh. Remove the snapshot and its
`.provenance` sidecar when the emulator or compatibility implementation
changes and a fresh boundary is needed, or set `SEMU_SAPPORO_UI_REFRESH=1` to
force a refresh.

See `docs/architecture.md`, `docs/compatibility-policy.md`,
`docs/current-status.md`, and `plans/roadmap.md` for the fidelity rules,
implemented baseline, and remaining gates.

Agents and other models must follow `AGENTS.md`. Guest-visible hardware,
firmware-compatibility, profile, format, and release work uses one instantiated
`ready` ticket from `plans/index.tsv`. Bounded maintenance may proceed without
a roadmap row when its scope and proportional verification are explicit.
