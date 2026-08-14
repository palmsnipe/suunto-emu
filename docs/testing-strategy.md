# Testing Strategy

## Test Classes

- Unit tests cover parsers, hashes, endian helpers, scheduler order, bus boundaries, storage overlays, and every CPU instruction family.
- Device tests replay byte-exact valid and invalid transactions, reset transitions, IRQ timing, and DMA bounds.
- Synthetic integration tests run committed, tiny Thumb programs for reset, exceptions, context switching, timers, DMA, storage, graphics, and input.
- Authentic-firmware tests are local-only and require user-supplied files whose size and SHA-256 match the selected built-in or explicit profile.
- SDL smoke tests use the dummy video driver and never gate the dependency-free headless build.

The test harness is C99 and dependency-free. Each test has a bounded instruction or virtual-time budget. Tests are hermetic: no host time, network, random input, writable firmware source, or order dependence.

## Mandatory Negative Coverage

Every parser rule has malformed-input cases. Every memory and DMA success case has boundary, width, or refusal counterparts. A device cannot be marked functional until reset, successful transcript, and unknown-command refusal tests pass. A compatibility layer requires disabled, correct-hash, wrong-hash, wrong-state, hit-budget, and event-log tests.

CPU vectors record initial registers/memory and expected registers, flags, memory, PC, and fault state. Exact vectors cover every instruction previously patched solely because Renode decoded it incorrectly.

## Commands and Gates

- `make test`: dependency-free unit, device, and synthetic integration tests.
- `make test TEST_FILTER=NAME`: run matching test binaries, case names, tags,
  or a declared group from `tools/test_groups.tsv`; no match is an error.
- `make check-lines`: apply the 300-line review and 500-line hard limits.
- `make check-task-contracts`: validate the indexed ticket structure and graph.
- `make check`: headless build, all normal tests, warnings-as-errors, source-size
  policy, task-contract validation, and CLI smoke checks.
- `make check-sdl`: SDL3 build and dummy-driver smoke test; reports a skip only when SDL3 is absent.
- `make sanitize`: supported address/undefined-behavior sanitizer run.
- `make test-firmware SEMU_FIRMWARE_MANIFEST=/absolute/path/to/firmware.semu`:
  validate the private component sizes and hashes and run any matching private
  integration scripts.
- `make test-firmware FIRMWARE_ROOT=/absolute/root TEST_PROFILE=PROFILE`:
  locate either `ROOT/PROFILE.semu` or `ROOT/PROFILE/firmware.semu`, validate it,
  and run matching private integration scripts. With no private-data variable,
  the target reports an explicit skip; an explicitly configured missing,
  ambiguous, or hash-mismatched input is an error.
- `make test-differential`: report an explicit skip unless `RENODE` names a
  caller-provided executable. Renode remains optional and non-authoritative.

`make check` warns for hand-written C/header/test files above 300 lines and fails above 500. Only declared generated data tables are exempt.

## First Release Golden Contract

For exact Sapporo `2.22.60.3383-P`, private integration tests compare:

| State | Bytes | SHA-256 |
| --- | ---: | --- |
| Normal native frame | 115200 | `8503ffbde124e35f914b09eea858ffcda2fc4bc453d3f9c7eb3e88388621d9cc` |
| Middle-button language menu | 115200 | `68a4126a8f908e9dd7c5703982c6fe141e6cc89cc383ee0d9d6d502eeadcb2d9` |
| Lower-button transition | 115200 | `dcec235c8b450c96356b27b49306026ab9d14e7626714cdacb8bf3737a623ad3` |

Two identical runs must produce identical checkpoints, frame hashes, layer counts, device transcripts, stop reason, and virtual time.

## Bug Policy

Before fixing a discovered defect, add the narrowest stable regression that demonstrates it. Never update a golden merely to accept changed output; document evidence showing why the old expectation was wrong.
