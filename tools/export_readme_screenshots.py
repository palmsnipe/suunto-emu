#!/usr/bin/env python3
"""Losslessly export six owner-approved screenshots from existing SDL dumps.

Developer tooling only; Python is not an emulator runtime dependency.
See docs/screenshots/README.md for the bounded capture commands.
"""

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zlib


# Existing onboarding/navigation regression transcripts, not new release goldens.
RUNS = {
    "222": ("sapporo-2.22.60", "2.22.60.3383-P",
            "2c6910c2d53d0dfd3fa9c00aa046615a3075a725ce33e67876a2706c8a68e0b8"),
    "235": ("sapporo-2.35.34", "2.35.34.18929-P",
            "14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae"),
}
SCREENS = (
    ("235", "405422e1", "sapporo-235-language", "Language selection"),
    ("235", "405d1af6", "sapporo-235-profile", "Define your profile"),
    ("235", "2ce89ebe", "sapporo-235-birth-year", "Birth year"),
    ("235", "a077755a", "sapporo-235-phone", "Phone pairing instructions"),
    ("235", "500b350f", "sapporo-235-watchface", "Watchface"),
    ("222", "040ebb03", "sapporo-222-menu", "Menu with Logbook selected"),
)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def png_chunk(kind, data):
    return (struct.pack(">I", len(data)) + kind + data
            + struct.pack(">I", zlib.crc32(kind + data)))


def convert_ppm(data, crc):
    # Accept only the exact opaque 240x240 P6 format emitted by sdl_ppm_dump.c.
    header = b"P6\n240 240\n255\n"
    if not data.startswith(header) or len(data) != len(header) + 240 * 240 * 3:
        raise ValueError("expected an exact 240x240 SDL PPM")
    rgb = data[len(header):]
    raw = bytearray()
    for offset in range(0, len(rgb), 3):
        r, g, b = rgb[offset:offset + 3]
        r5, g6, b5 = (r * 31 + 127) // 255, (g * 63 + 127) // 255, (b * 31 + 127) // 255
        if (r5 * 255 // 31, g6 * 255 // 63, b5 * 255 // 31) != (r, g, b):
            raise ValueError("PPM contains colors outside the SDL RGB565 conversion")
        raw.extend(struct.pack("<H", (r5 << 11) | (g6 << 5) | b5))
    if f"{zlib.crc32(raw):08x}" != crc:
        raise ValueError(f"frame pixels do not match RGB565 CRC32 {crc}")
    scanlines = b"".join(b"\0" + rgb[y * 720:(y + 1) * 720] for y in range(240))
    png = (b"\x89PNG\r\n\x1a\n"
           + png_chunk(b"IHDR", struct.pack(">IIBBBBB", 240, 240, 8, 2, 0, 0, 0))
           + png_chunk(b"IDAT", zlib.compress(scanlines, 9))
           + png_chunk(b"IEND", b""))
    return png, sha(raw)


def export(captures, output):
    logs = {}
    for run, (_, _, expected) in RUNS.items():
        data = (captures / f"{run}.log").read_bytes()
        if sha(data) != expected:
            raise ValueError(f"{run}: transcript differs from the existing regression pin")
        logs[run] = data.decode("ascii")
    assets, records = {}, []
    for run, crc, name, caption in SCREENS:
        settled = re.search(rf"^SDL live test settled step=(\d+) generation=(\d+) "
                            rf"crc32={crc}$", logs[run], re.MULTILINE)
        if settled is None:
            raise ValueError(f"{name}: no matching settled frame")
        ppm = (captures / run / f"suunto-frame-{crc}.ppm").read_bytes()
        png, raw_sha = convert_ppm(ppm, crc)
        filename = name + ".png"
        assets[filename] = png
        profile, version, log_sha = RUNS[run]
        records.append({
            "file": filename, "caption": caption, "profile": profile,
            "firmware_version": version, "width": 240, "height": 240,
            "settled_step": int(settled[1]), "generation": int(settled[2]),
            "rgb565_crc32": crc, "rgb565_sha256": raw_sha,
            "ppm_sha256": sha(ppm), "png_sha256": sha(png),
            "transcript_sha256": log_sha,
        })
    # Validate every input before publishing any output. No raw captures copied.
    assets["provenance.json"] = (json.dumps(records, indent=2) + "\n").encode("ascii")
    output.mkdir(parents=True, exist_ok=True)
    for name, data in assets.items():
        (output / name).write_bytes(data)
    print(f"Exported {len(records)} verified screenshots to {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("captures", type=Path, help="external capture directory")
    parser.add_argument("--output", type=Path,
                        default=Path(__file__).resolve().parents[1] / "docs/screenshots")
    args = parser.parse_args()
    try:
        export(args.captures, args.output)
    except (OSError, ValueError) as error:
        parser.exit(1, f"Screenshot export refused: {error}\n")


if __name__ == "__main__":
    main()
