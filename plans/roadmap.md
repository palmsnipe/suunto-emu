# Standalone Emulator Roadmap

## Delivery Rules

`plans/index.tsv` is the scheduling source of truth. A ticket becomes `ready` only when every dependency is `done` and its frozen interfaces are present. Component work never edits public headers, the Makefile, board/profile registries, or the task index unless its ticket explicitly owns those files.

Agents receive exactly one ready ticket through `plans/agent-prompt.md` and
must follow `AGENTS.md`. Tickets are sized for roughly one to three model-days.
Repeated “one observed gap” tickets in Phase 7 are templates: the integrator
must instantiate one with a concrete product, evidence ID, file set, filter,
and expected checkpoint before dispatch. A blocked template containing angle
brackets is never assigned directly.

Every phase ends at a reproducible gate. Unknown hardware stops execution; it does not receive a permissive stub. Authentic firmware stays outside the repository and is used only after manifest validation.

## Phases and Gates

| Phase | Outcome | Exit gate |
| ---: | --- | --- |
| 0 | Architecture, policy, evidence, and delegation contract | Docs and tickets are internally consistent; no source implementation implied. |
| 1 | Dependency-free deterministic foundation | Strict malformed-input, bus, scheduler, overlay, and repeatability tests pass on macOS/Linux. |
| 2 | ARMv7E-M/Thumb-2 interpreter with single-precision FPU | Synthetic RTOS guest passes SVC, PendSV, SysTick, nesting, WFI, and FPU context tests without patches. |
| 3 | Apollo4 substrate | Known startup controller transactions and stable RTOS/WFI checkpoint pass without global fallbacks. |
| 4 | Sapporo 2.22 headless bring-up | Exact firmware reaches startup completion with no unsupported behavior or source-image mutation. |
| 5 | Native display and three-button interaction | All three private frame hashes and two-run deterministic comparison pass. |
| 6 | Bounded debugging and hardening | Sanitizers, replay, snapshot, failure reports, and optional differential tests pass. |
| 7 | Product expansion | Each exact profile independently reaches reset, native display traffic, then interaction. |

## Integration Sequence

Phase 1 freezes core interfaces before CPU work. Phase 2 freezes CPU/IRQ interfaces before SoC work. Phase 3 freezes controller transactions before device agents run. Phase 4 first validates headless startup; display and SDL work cannot conceal a boot failure. Phase 7 profiles reuse only lower-level behavior supported by evidence.

Within a phase, tickets with disjoint allowed paths may run in parallel. Integration tickets own cross-module wiring and run the whole phase gate. Any newly discovered behavior first receives an evidence entry and a narrow regression ticket.

Evidence tickets precede behavior tickets. A missing source symbol, reference
revision, trace hash, firmware package, or native golden keeps its consumer
blocked; models do not fill missing contracts from intuition. The task checker
validates index/file/status/dependency consistency, while the integrator still
reviews ownership overlap and the observable command results.

## Release Definition

The first release is a C99 headless executable plus an optional SDL3 executable capable of validating and running exact Sapporo `2.22.60.3383-P`, publishing native 240x240 RGB565 frames, accepting upper/middle/lower input, reporting all compatibility interventions, and reproducing the declared goldens. It does not promise cycle accuracy, wireless connectivity, direct SOF1/XZ loading, or support for unpinned firmware.
