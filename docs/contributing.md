# Contributing and Agent Delegation

## Working Agreement

Use a roadmap ticket for guest-visible hardware behavior, execution semantics,
persistent formats, profiles, compatibility layers, and release goldens. A
component ticket owns only its listed files and consumes frozen public
interfaces.

Bounded maintenance does not require a pre-existing roadmap row. Bug fixes,
behavior-preserving refactors, performance work, tooling/build changes,
documentation corrections, and frontend usability changes may start from an
explicit scope statement. Public headers, the Makefile, registries, profiles,
and task indexes must be named in that scope; roadmap component work still
routes them through its integration ticket.

Do not combine unrelated phases, opportunistic refactors, or inferred hardware behavior. When evidence is missing, stop at a fail-closed diagnostic and record the gap. Never commit proprietary firmware, extracted resources, screenshots containing private firmware assets, or user machine paths.

## Small-File Policy

Prefer focused hand-written C, headers, and tests. Review files above 300
lines and consider splitting by responsibility above 500, but keep related
code together when that improves clarity. Both thresholds are advisory;
line count alone does not require a split or fail verification. Generated
tables require a checked-in generator or provenance note.

## Delegation Flow

1. The integrator selects a ready ticket from `plans/index.tsv` and freezes required interfaces.
2. The integrator dispatches one ticket using `plans/agent-prompt.md`.
3. An implementer works only in `Allowed Files` and runs the ticket commands.
4. The implementer reports changed files, tests, evidence used, and unresolved gaps.
5. The integrator reviews fail-closed behavior, line counts, public-interface drift, and deterministic results.
6. Registry/header/Makefile changes land through the named integration ticket after component tests pass.

Parallel tickets must have disjoint allowed paths. If overlap is unavoidable, sequence them by dependency instead of relying on conflict resolution.

For maintenance, the implementer records the scope, affected contracts, and
verification before editing. Review checks that the change stayed within that
scope and preserved deterministic checkpoints or supplied a narrow regression
and evidence for any correction.

## C99 and Portability

Use ISO C99 and fixed-width integer types. Avoid compiler extensions, platform-specific assembly, implicit narrowing, host endianness assumptions, and undefined signed overflow. Production headless code cannot require SDL3. The supported build matrix is Clang and GCC on macOS and Linux with GNU Make 3.81+.

## Definition of Done

A roadmap ticket is done only when every acceptance item passes, required
evidence is cited, negative tests exist, source firmware remains unchanged,
documentation is synchronized, and no forbidden scope was touched. Maintenance
is complete when its stated scope and proportional verification pass without
unexplained observable drift.

`AGENTS.md` is the repository-wide execution contract. Ticket text may narrow
ownership and behavior but cannot weaken its safety or fidelity rules.
