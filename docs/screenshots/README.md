# Sapporo README captures

Six unretouched 240×240 frames from the in-tree SDL frontend, captured on
2026-10-02 at source revision `05801b4`. The owner explicitly authorized these
six UI screenshots in Git on that date. Firmware/resource bytes, raw PPMs,
logs, and snapshots remain private; this exception does not cover additional
images automatically.

| File | Firmware | Settled step | Generation | RGB565 CRC32 |
| --- | --- | ---: | ---: | --- |
| `sapporo-235-language.png` | 2.35.34.18929-P | 3 | 79 | `405422e1` |
| `sapporo-235-profile.png` | 2.35.34.18929-P | 5 | 969 | `405d1af6` |
| `sapporo-235-birth-year.png` | 2.35.34.18929-P | 8 | 1197 | `2ce89ebe` |
| `sapporo-235-phone.png` | 2.35.34.18929-P | 14 | 1642 | `a077755a` |
| `sapporo-235-watchface.png` | 2.35.34.18929-P | 31 | 4770 | `500b350f` |
| `sapporo-222-menu.png` | 2.22.60.3383-P | 31 | 4510 | `040ebb03` |

[provenance.json](provenance.json) contains full PNG, source PPM, RGB565, and
run-transcript SHA-256 hashes. The two transcripts match the existing pins in
`tools/test_sdl_onboarding_completion.sh` and
`tools/test_sdl_sapporo_235_nav.sh`. These pictures document observed emulator
output; they are not physical-device evidence or new release goldens.
The phone page is an instruction screen only. Clock, date and sensor fields
are firmware/fixture state, not host time or real measurements.

## Reproduce

Run from the repository root with the validated private manifests in their
conventional locations, or substitute your own manifest paths. Cold walks
take several minutes. Raw outputs go to a fresh temporary directory; Python 3
is used only by the optional exporter, with no third-party libraries.

```sh
make all sdl
capture_dir=$(mktemp -d /tmp/semu-readme-capture.XXXXXX)
mkdir "$capture_dir/222" "$capture_dir/235"

SDL_VIDEODRIVER=dummy SEMU_SDL_PPM_DIR="$capture_dir/222" \
  SEMU_SDL_LIVE_TEST=setup-walk \
  SEMU_SDL_SETUP_WALK_POST=mlllmlllmmlmmmmm \
  SEMU_SDL_SETUP_WALK_TIMELINE=30000:l \
  build/suunto-emu-sdl run --profile sapporo-2.22.60 \
  --firmware tests/private/sapporo-2.22.60/firmware.semu \
  --layer sapporo-2.22-no-device --until setup-next \
  --max-instructions 18000000000 --max-time 60000000000 \
  --snapshot-save "$capture_dir/222-main.sems" >"$capture_dir/222.log" 2>&1

SDL_VIDEODRIVER=dummy SEMU_SDL_PPM_DIR="$capture_dir/235" \
  SEMU_SDL_LIVE_TEST=setup-walk \
  SEMU_SDL_SETUP_WALK_POST=mmlllmlllmmmmmmmmmmmmmmmmmmmmmm \
  build/suunto-emu-sdl run --profile sapporo-2.35.34 \
  --firmware tests/private/sapporo-2.35.34.18929/firmware.semu \
  --layer sapporo-2.35-production-data --layer sapporo-2.35-ohr-startup \
  --layer sapporo-2.35-gps-startup --layer sapporo-2.35-gps-reopen \
  --layer sapporo-2.35-gps-awake --until setup-next \
  --max-instructions 10000000000 --max-time 40000000000 \
  --snapshot-save "$capture_dir/235-main.sems" >"$capture_dir/235.log" 2>&1

python3 tools/export_readme_screenshots.py "$capture_dir"
```

Each emulator run validates all three firmware components before execution.
The exporter requires both exact regression transcript hashes, a settled-frame
record for each image, the exact PPM format, and a pixel CRC matching the
RGB565 source. It validates all six inputs before writing any output. PNG
conversion preserves the RGB bytes emitted by SDL; there is no scaling,
filtering, recoloring, or generated content.

To review output separately, pass `--output /tmp/semu-gallery-review`.
When an engine change moves these pins, investigate against the existing
evidence and regression gates before regenerating the gallery. Do not weaken
an expected hash to make export succeed.
