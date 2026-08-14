# Profile and Firmware Manifest Format

## Lexical Rules

Version 1 files are printable ASCII. Blank lines and lines whose first
non-space character is `#` are ignored. Sections use `[kind]` or
`[kind ID]`; entries are unique `key=value` pairs. Horizontal whitespace
around sections, keys, and values is removed. Inline comments, quoting,
escapes, interpolation, continuation lines, duplicate or unknown keys,
control characters, signs, and integer suffixes are invalid.

Identifiers contain ASCII letters, digits, `.`, `_`, or `-`. Integers are
unsigned decimal or `0x` hexadecimal and must fit their destination type.
SHA-256 values are exactly 64 hexadecimal digits. The parser fails closed.

## Profile Selection and File

`--profile` accepts the built-in ID `sapporo-2.22.60` or an explicit profile
path. Both use the same strict schema. A profile contains one root, one or
more components, and zero or more compatibility layers:

```ini
[profile]
format=1
id=sapporo-2.22.60
board=sapporo
product=Sapporo
version=2.22.60.3383-P
vector_table=0x00040000
display_width=240
display_height=240

[component application]
role=application
load_address=0x00040000
size=1493234
sha256=c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc

[layer sapporo-2.22-no-device]
kind=synthetic-state
evidence=suunto-firmware/docs/research/factory-calibration-records.md
max_hits=1
```

All shown root and component keys are required. Components may not contain a
path. Each layer requires `kind`, non-empty `evidence`, and positive
`max_hits`; runtime code still owns the version-pinned behavior and layers
remain disabled unless selected with `--layer`.

## User Firmware Manifest

The firmware manifest repeats the exact component metadata and supplies a
safe relative path:

```ini
[firmware]
format=1
product=Sapporo
version=2.22.60.3383-P

[component application]
role=application
path=components/component-04-type-4-v2.raw
load_address=0x00040000
size=1493234
sha256=c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc
```

Paths are relative to the firmware manifest directory. Empty segments,
absolute paths, backslashes, drive prefixes, and `..` segments are rejected.
The loader resolves the path, verifies the actual byte count and SHA-256, and
requires every manifest component to match the profile ID, role, address,
size, and hash before machine creation. Extra and missing components fail.

Components are read without write access into emulator-owned memory and the
source files are closed. The emulator never writes the supplied files.
Direct SOF1/XZ loading, URLs, globs, and environment expansion are outside v1.

## Diagnostics

Lexical errors identify the file and line. Semantic diagnostics identify the
mismatching profile or component without printing firmware contents. Normal
run summaries omit component paths.
