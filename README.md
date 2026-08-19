# suunto-emu

`suunto-emu` is a standalone C99 emulator for Apollo4-era Suunto watches. It
uses an in-tree ARMv7-M interpreter, deterministic virtual time, strict device
contracts, immutable firmware inputs, and optional SDL3 presentation.

The repository is under active bring-up. The deterministic core, strict
profile/manifest validation, instruction interpreter foundation, Sapporo
memory contract, and explicitly synthetic no-device compatibility state are
implemented first. Full Sapporo firmware/UI coverage is tracked as gated work
in `plans/` rather than being claimed prematurely.

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
the interpreter. SDL presents renderer output; this does not claim physical
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
button edge; use Up, Down, or Return/Enter to continue navigating. SDL holds
each button press for 70 ms of guest time and keeps the released level stable
for another 70 ms, matching the native debounce boundary. Replay input remains
the deterministic path for headless checkpoints.

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
firmware manifest or compatibility layer changes.

For repeated interactive setup sessions, the helper creates that checkpoint
once and reuses it:

```sh
sh tools/run_sapporo_ui.sh /path/to/firmware.semu
```

The first Return/Enter opens the language screen; after each transition settles,
the next Up, Down, or Return/Enter edge continues into setup. Pass a second
argument to choose the checkpoint path. Remove that specific file when the
emulator or compatibility implementation changes and a fresh boundary is
needed.

See `docs/architecture.md`, `docs/compatibility-policy.md`,
`docs/current-status.md`, and `plans/roadmap.md` for the fidelity rules,
implemented baseline, and remaining gates.

Agents and other models must follow `AGENTS.md`. Select only a `ready` row from
`plans/index.tsv` and dispatch its single ticket with `plans/agent-prompt.md`;
blocked Phase 7 templates must first be instantiated with exact evidence and
expected checkpoints by the integrator.
