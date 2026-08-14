# Repository Agent Contract

These instructions apply to every file in this repository. A more specific
`AGENTS.md` may narrow them for a subdirectory but may not weaken determinism,
evidence, firmware-safety, or fail-closed requirements.

## Mission

Build a standalone deterministic C99 emulator for Apollo4-era Suunto watches.
The authoritative runtime is the in-tree interpreter. SDL3 is optional and is
the only permitted installed runtime dependency. Firmware and proprietary
frame pixels never enter Git.

## Required Reading

Before editing, read in order:

1. `README.md` and `docs/current-status.md`.
2. `plans/index.tsv` and the assigned ticket in `plans/tasks/`.
3. `docs/architecture.md`, `docs/execution-model.md`,
   `docs/testing-strategy.md`, and `docs/compatibility-policy.md`.
4. The exact public headers, evidence entries, existing implementation, and
   tests named by the ticket.

Do not infer completion from existing code. Later-phase scaffolding is partial
until the corresponding ticket acceptance conditions pass.

## Task Selection and Ownership

- Work on one assigned ticket whose status is `ready` or `in-progress`.
- Confirm every dependency is `done` before implementation.
- Edit only `Allowed Files`. Preserve all unrelated or pre-existing changes.
- Public headers, `Makefile`, registries, profiles, and `plans/index.tsv` are
  integration-owned unless explicitly allowed.
- Do not change ticket status. The integrator updates the index after review.
- If an interface is insufficient, stop and report the smallest required
  integration change; do not work around it with a private parallel API.

## Evidence and Firmware Safety

- Implement only behavior supported by the ticket's architecture reference or
  an entry in `docs/migration-evidence.md`.
- Missing evidence is a legitimate blocker. Add a refusal/diagnostic and report
  the missing observation instead of guessing a register or command.
- Treat `../suunto-firmware` and user firmware roots as read-only evidence.
  Never modify them or copy firmware/resource bytes into this repository.
- Compatibility behavior must be named, hash-pinned, opt-in, hit-bounded, and
  logged. Valid CPU instructions are never compatibility hooks.

## Implementation Rules

- Use ISO C99, fixed-width integers, checked arithmetic, and explicit
  little-endian access. Avoid undefined signed behavior and host-time input.
- Unknown instructions, MMIO, device commands, sizes, and state transitions
  fail closed. Do not add global read-as-zero or opcode-as-NOP fallbacks.
- Preserve deterministic virtual time and stable event ordering.
- Validate the full operation before state mutation, especially DMA, storage,
  parser, multi-register, and rendering operations.
- Keep handwritten files below 500 lines and normally below 300. Split by
  responsibility before expanding a file already above the review threshold.
- Add no dependency to the normal headless build. Do not add platform-specific
  assembly, JITs, Unicorn, or a Renode runtime requirement.

## Verification

Run the ticket's exact commands. `make test TEST_FILTER=name` must select at
least one matching test binary. Also run:

```sh
make check-lines
make check
```

Run `make sanitize` for memory, parser, CPU, storage, DMA, or rendering changes.
Use explicit instruction or virtual-time limits for hang-prone tests. Authentic
firmware tests are optional locally but must validate every component before
execution; absence may skip, a mismatch must fail.

Every device or protocol change needs both a successful case and a refusal
case. Every bug fix starts with the narrowest regression that fails before the
fix. Do not weaken a golden, expected stop reason, or hash.

## Handoff

Report:

- ticket ID and outcome;
- changed files;
- evidence/reference IDs used;
- exact commands and results;
- deterministic hashes/checkpoints where applicable;
- unsupported cases and remaining gaps;
- any requested integrator-owned change.

Do not claim the ticket is done when an acceptance condition was skipped or
when private evidence was unavailable.
