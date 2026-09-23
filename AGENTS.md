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

## Todo Tool Discipline

When `todo_write` is available, send the complete replacement list on every
call. Each item must contain exactly `content` and `status`; never add `active`,
`activeForm`, IDs, or other fields. Valid statuses are only `pending`,
`in_progress`, and `completed`. For multi-step work, create a fresh list near
the start of the turn and update it as each step changes state.

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
- Ticket statuses are `blocked`, `ready`, `in-progress`, `done`, and
  `deferred`. `deferred` records an explicit human decision to stop pursuing
  a ticket under a standing constraint; only the integrator defers or
  reactivates such tickets.
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

## Lane Oracle and No-Device Constraint

- This project has no physical target device and will not acquire one. The
  read-only Renode lane under `../suunto-firmware` is the sole machine oracle:
  guest-visible behavior counts as observed only when the lane or an existing
  entry in `docs/migration-evidence.md` produced it.
- Keep raw lane logs and probe binaries in volatile workspaces; every evidence
  entry must record the SHA-256 of each log it relies on and keep the derived
  census (counts, tuples, boundaries) in the entry itself, so findings survive
  without committing firmware-derived logs to Git. Probe sources follow the
  same rule: cite path and hash, do not check the firmware-derived output in.
- Lane probe conventions: probe scripts and logs live outside the repository
  working tree, run with explicit instruction/virtual-time budgets, and each
  finding is reproduced at least twice byte-identically before it backs an
  implementation.
- Tickets whose acceptance explicitly requires physical-device captures cannot
  be satisfied under this constraint; the integrator marks them `deferred` with
  a dated deferral note or re-scopes acceptance to lane-equivalent evidence.
- Human decision (2026-09-23, project owner): for a behavior the lane cannot
  observe or refuses to model, offline reverse-engineering of the hash-pinned
  firmware and resource partitions (for example Ghidra analysis under
  `../suunto-firmware/artifacts/analysis`) is an accepted additional evidence
  class. Such an entry must name the exact input files and tooling with
  SHA-256, record the derivation and the derived census in the entry itself,
  and satisfy the same twice-reproduced requirement. It authorizes no value
  the pinned firmware itself does not exhibit. First application: the ticket
  788 compressed-texture (format 17) codec.

## Implementation Rules

- Use ISO C99, fixed-width integers, checked arithmetic, and explicit
  little-endian access. Avoid undefined signed behavior and host-time input.
- Unknown instructions, MMIO, device commands, sizes, and state transitions
  fail closed. Do not add global read-as-zero or opcode-as-NOP fallbacks.
- Preserve deterministic virtual time and stable event ordering.
- Validate the full operation before state mutation, especially DMA, storage,
  parser, multi-register, and rendering operations.
- Prefer focused handwritten files, using 300 lines as a review threshold
  and 500 lines as a prompt to consider splitting by responsibility. These
  are advisory guidelines, not hard limits: exceeding them is acceptable
  when keeping related code together improves clarity. Do not split files
  mechanically just to satisfy a line count.
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
- Opt-in era scripts under `tests/integration/` (selected by `TEST_PROFILE`,
  not by `make check`) are engine-visible: when CPU, scheduler, bus, or device
  behavior changes, run the era scripts of the affected profile or state in
  the handoff that their pins may have drifted. Silent era drift is a defect
  class: when pins are found stale, re-derivation is tracked as roadmap work,
  never silently re-pinned mid-instance.

Use explicit instruction or virtual-time limits for hang-prone tests. Authentic
firmware tests are optional locally but must validate every component before
execution; absence may skip, a mismatch must fail.

Every device or protocol change needs both a successful case and a refusal
case. Every bug fix starts with the narrowest regression that fails before the
fix. Do not weaken a golden, expected stop reason, or hash.

## Local LLM / Agent Concurrency

This project may use Qwen3.8-Flash-Next running locally on a single
NVIDIA DGX Spark through `dgx-spark-qwen38`.

The model supports a 262K context window per request, but KV cache
capacity is shared between all concurrent requests.

When spawning sub-agents:

- Prefer 3-4 concurrent sub-agents for normal development work.
- Avoid unnecessary parallel agents when tasks require large context.
- Give each sub-agent only the files/context relevant to its task.
- Reuse shared project context where possible instead of independently
  loading the entire repository in every agent.
- Do not assume that the 262K context window is available independently
  to every concurrent agent.

The server may use:

- `FLASH_TIER=context`: up to 4 simultaneous requests, optimized for
  long context.
- `FLASH_TIER=concurrency`: up to 8 simultaneous requests, with the
  shared KV cache distributed across more active sessions.

For long-running coding or repository-analysis tasks, favor fewer,
focused agents over maximum concurrency.

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
