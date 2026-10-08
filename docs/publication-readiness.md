# Initial public repository audit

Publication maintenance for `palmsnipe/suunto-emu`. The reviewed runtime is
commit `07e868c6d9b523e070ec8beffd248beb16f6db84`, with tree
`91260621b527b244c0c63f12c0f101e6be2400f1`. Publication adds only `LICENSE`,
the licensing notices in the root and screenshot READMEs, and this report.
The 35 pre-existing uncommitted files remain local, including the later 2.39
snapshot-pin re-derivation. Roadmap statuses and deterministic pins are unchanged.

## Content and history

The audit inspected all 547 reachable commits, 3,108 trees and 3,349 distinct
blobs, plus the historical filename inventory and current tracked files.
No firmware packages, resource extracts, private captures, raw lane logs,
machine snapshots, vendor PDFs, credentials or build products were found in
the published history. No blob exceeds 1 MiB.

The ten binary blobs are six README PNGs and four small synthetic NEMA test
fixtures (4, 8, 120 and 160 bytes). The fixture provenance and authentic-input
separation are recorded in ticket 490 and `tests/golden/nema-corpus.tsv`.
Authentic NEMA inputs remain external and are referenced by hashes.

All six PNG SHA-256 values match `docs/screenshots/provenance.json`; each is
240 by 240 pixels. Their inclusion follows the owner's 2026-10-02 exception
in `AGENTS.md` and `docs/screenshots/README.md`. The MIT source/documentation
license excludes the third-party UI artwork and external firmware/resources.

Gitleaks 8.30.1, using its default rules and redacted output, found zero leaks
in each of these scans:

- `gitleaks git . --log-opts='--all --full-history --root' --redact`
- `gitleaks dir . --redact`
- `gitleaks stdin --redact --max-target-megabytes 200`, supplied all 3,886
  UTF-8 blobs and commit objects, including the initial commit and complete
  historical file contents (108,711,041 bytes with object labels).

The repository ignore rules keep build directories, `tests/private/`, machine
snapshots, `.screenshots/`, local manifests and cached vendor PDFs out of Git.
No history rewrite was needed.

## Clean-checkout verification

Tests ran from a fresh `git archive HEAD` extraction outside the working
checkout, with no private firmware files. Both commands exited zero:

| Command | Passing cases | Failed cases | Additional result |
| --- | ---: | ---: | --- |
| `make check` | 1,042 | 0 | 169 task contracts validated; SDL3 quick checks passed |
| `make sanitize` | 1,037 | 0 | Address/undefined-behavior sanitizer suite passed |

The source-size warnings are advisory under `AGENTS.md`. Private firmware
cases and SDL firmware walks explicitly skipped because their inputs were
absent. The opt-in 2.39 era suite was not run for this metadata-only change;
the public committed baseline still documents its snapshot-pin gap. This
audit supplies no new firmware-compatibility or physical-device evidence.

The build logs remain outside Git. SHA-256:

- `make check`: `d09c8ef7da56df027c1685d5ec1c253b4e33b334cb96183d21844493ab79a508`
- `make sanitize`: `ec273f33b0688c5c33a600a698168472b80cc7d4de59c079a37fad1436c3d2bf`

`git diff --check` and README local-link validation passed. The emulator's
documented partial GPU, sensor, GPS and long-session coverage remains the
release boundary. No integrator-owned runtime change is requested by this audit.
