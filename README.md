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

For an authentic Sapporo UI session, use a validated private 32-MiB device
flash dump in addition to the three firmware components. The dump must be
kept outside the repository and contain the observed `1VSF` footer at device
offset `0x00fc0000`:

```sh
make sdl
build/suunto-emu-sdl run \
  --profile sapporo-2.22.60 --firmware /path/to/firmware.semu \
  --full-flash /private/path/sapporo.full-flash.bin \
  --layer sapporo-2.22-no-device --max-instructions 1000000000
```

Arrow Up, Return, and Arrow Down forward the three Sapporo button edges into
the interpreter. The current recovered OTA resource fragment remains useful
for boot and renderer tests, but cannot provide the factory watch-face assets;
the full-flash option refuses images with the wrong size or missing footer.

See `docs/architecture.md`, `docs/compatibility-policy.md`,
`docs/current-status.md`, and `plans/roadmap.md` for the fidelity rules,
implemented baseline, and remaining gates.

Agents and other models must follow `AGENTS.md`. Select only a `ready` row from
`plans/index.tsv` and dispatch its single ticket with `plans/agent-prompt.md`;
blocked Phase 7 templates must first be instantiated with exact evidence and
expected checkpoints by the integrator.
