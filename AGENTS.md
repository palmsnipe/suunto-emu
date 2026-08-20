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

Before editing, always read `README.md`, `docs/current-status.md`, and the
working-tree status. Then select the reading set for the work class:

- **Roadmap work:** read `plans/index.tsv`, the assigned ticket, the relevant
  architecture/execution/testing/compatibility sections, and every exact
  header, evidence entry, implementation, and test named by the ticket.
- **Maintenance work:** read the implementation, tests, public contracts, and
  focused guides relevant to the bounded change. A pre-existing roadmap ticket
  is not required.

Do not infer completion from existing code. Later-phase scaffolding is partial
until the corresponding ticket acceptance conditions pass.

## Work Classes and Ownership

- **Roadmap work** adds or changes guest-visible hardware behavior, CPU or
  deterministic execution semantics, persistent formats, profiles, firmware
  compatibility, or release goldens. Work on one assigned `ready` or
  `in-progress` ticket, confirm its dependencies are `done`, and edit only its
  `Allowed Files`.
- **Maintenance work** covers a bounded bug fix, behavior-preserving refactor,
  performance change, tooling/build change, documentation correction, or
  frontend usability change. State the scope before editing, touch only files
  necessary for that scope, and preserve deterministic checkpoints unless a
  regression plus evidence justifies a correction.
- Public headers, `Makefile`, registries, profiles, and `plans/index.tsv` must
  be explicitly in scope. Roadmap component tickets still leave them to their
  named integration ticket.
- Do not change roadmap ticket status as part of implementation. The
  integrator updates the index after review; planning-only maintenance may
  update status or dependencies when that is its explicit purpose.
- If an interface is insufficient, stop and report the smallest required
  integration change; do not work around it with a private parallel API.
- Preserve all unrelated and pre-existing changes in either work class.

## Evidence and Firmware Safety

- Implement guest-visible hardware and firmware behavior only when supported by
  the roadmap ticket's architecture reference or an entry in
  `docs/migration-evidence.md`.
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
- New handwritten files should remain below 300 lines and all handwritten
  files must remain at or below 500. A bounded change may touch an existing
  file above 300 lines without a mechanical split when it does not materially
  grow that file; split by responsibility before exceeding the hard limit.
- Add no dependency to the normal headless build. Do not add platform-specific
  assembly, JITs, Unicorn, or a Renode runtime requirement.

## Verification

Use verification proportional to the change:

- Roadmap work runs the ticket's exact commands, `make check-lines`, and
  `make check`. `make test TEST_FILTER=name` must select at least one test.
- C behavior changes run the narrowest relevant test and `make check`; core,
  CPU, parser, storage, DMA, device-protocol, or rendering changes also run
  `make sanitize`.
- Build, public-interface, registry, or profile changes run `make check` plus
  their focused smoke or validation command.
- Documentation/planning-only changes run `make check-task-contracts` when
  plans are touched and `make check` when commands, contracts, or build claims
  change. Trivial repository metadata may use focused validation only.

Use explicit instruction or virtual-time limits for hang-prone tests. Authentic
firmware tests are optional locally but must validate every component before
execution; absence may skip, a mismatch must fail.

Every device or protocol change needs both a successful case and a refusal
case. Every bug fix starts with the narrowest regression that fails before the
fix. Do not weaken a golden, expected stop reason, or hash.

## Handoff

Report:

- ticket ID and outcome, or the maintenance scope;
- changed files;
- evidence/reference IDs used;
- exact commands and results;
- deterministic hashes/checkpoints where applicable;
- unsupported cases and remaining gaps;
- any requested integrator-owned change.

Do not claim the ticket is done when an acceptance condition was skipped or
when private evidence was unavailable.
