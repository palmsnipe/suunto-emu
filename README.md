# suunto-emu

`suunto-emu` is a standalone C99 emulator for Apollo4-era Suunto watches. It
uses an in-tree ARMv7-M interpreter, deterministic virtual time, strict device
contracts, immutable firmware inputs, and optional SDL3 presentation.

The first Sapporo target is functional: the deterministic interpreter runs the
exact pinned firmware through startup and native renderer traffic, publishes a
240x240 UI, accepts three-button interaction, and supports replay and machine
snapshots. Hardware coverage remains evidence-scoped; physical-panel behavior,
unobserved device commands, and unpinned firmware versions are not implied.

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

`make check-sdl` always verifies invalid live-test configuration fails closed.
When an authentic manifest is available, it also drives one Return/Enter edge
and two successive middle-screen clicks through the dummy SDL frontend and
verifies each new settled setup frame:

```sh
SEMU_FIRMWARE_MANIFEST=/path/to/firmware.semu \
SEMU_SDL_TEST_SNAPSHOT=/tmp/suunto-ui-preframe.sems make check-sdl
```

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
  --max-instructions 30000000000 --max-time 60000000000
```

`setup-walk` settles 19 observed renderer transitions and then exits through a
synthetic SDL quit event. It is a diagnostic for the fixed language/TSC6A path,
not a claim that all onboarding screens or the phone-pairing boundary are
implemented. On the current OTA-only run the firmware requests `SYSRESETREQ`
at `0x000be93e` before the full native setup sequence; the emulator preserves
that fail-closed boundary until a native reset/post-reset trace is available.

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
Machine snapshots preserve guest and NEMA state, not the SDL surface that was
already presented; use a pre-frame boundary when the resumed session must
immediately show the saved UI.
The SDL frontend reports a bounded CRC32 alongside the first-frame dimensions
and generation, so repeated runs can be compared without storing frame pixels.

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
