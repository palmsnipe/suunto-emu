# Contributing and Agent Delegation

## Working Agreement

Start from a ticket in `plans/tasks/`. A component ticket owns only its listed files and consumes frozen public interfaces. Makefile, public headers, board/profile registries, and task indexes are integration-owned unless a ticket explicitly permits them.

Do not combine unrelated phases, opportunistic refactors, or inferred hardware behavior. When evidence is missing, stop at a fail-closed diagnostic and record the gap. Never commit proprietary firmware, extracted resources, screenshots containing private firmware assets, or user machine paths.

## Small-File Policy

Hand-written C, headers, and tests should remain under 300 lines and must remain under 500. Split by responsibility: decoder families, register blocks, device protocols, raster operations, and test scenarios. Generated tables require a checked-in generator or provenance note and an explicit line-count exemption.

## Delegation Flow

1. The integrator selects a ready ticket from `plans/index.tsv` and freezes required interfaces.
2. An implementer works only in `Allowed files` and runs the ticket commands.
3. The implementer reports changed files, tests, evidence used, and unresolved gaps.
4. The integrator reviews fail-closed behavior, line counts, public-interface drift, and deterministic results.
5. Registry/header/Makefile changes land through the named integration ticket after component tests pass.

Parallel tickets must have disjoint allowed paths. If overlap is unavoidable, sequence them by dependency instead of relying on conflict resolution.

## C99 and Portability

Use ISO C99 and fixed-width integer types. Avoid compiler extensions, platform-specific assembly, implicit narrowing, host endianness assumptions, and undefined signed overflow. Production headless code cannot require SDL3. The supported build matrix is Clang and GCC on macOS and Linux with GNU Make 3.81+.

## Definition of Done

A ticket is done only when every acceptance item passes, required evidence is cited, negative tests exist, source firmware remains unchanged, documentation is synchronized, and no forbidden scope was touched. A partial implementation stays `in-progress` or `blocked` in the index; it is not marked done because it compiles.

