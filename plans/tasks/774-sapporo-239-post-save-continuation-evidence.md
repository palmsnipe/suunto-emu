# 774 — Sapporo 2.39 Post-Save Continuation Evidence

**Status:** ready
**Phase:** 7
**Dependencies:** 729,766,771,772,773

## Goal

Measure what the 2.39 firmware does after the completed HEIGHT personal
save inside and immediately beyond the current refusal boundary, so the
roadmap records the real next production boundary (file budget, path, GPS or
something else) instead of guessing. Pure evidence: no production change.

## Execution Budget

One model-day. Observation only; no ceiling, gate, checkpoint or source
integration may move.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
tickets 764, 766, 769, 771, 772 and 773; E-SAP-COMPAT-PERSONAL-239-001,
E-SAP-COMPAT-PERSONAL-SUFFIX-239-001, E-SAP-GPS-FIFTH-239-002 and
E-SAP-TIME-NATIVE-239-001; `src/compat/sapporo_239_gps_awake.c`;
`src/compat/sapporo_239.c`; `tests/integration/sapporo_239_five_probe.c`.

## Current Baseline

All dependencies are done. Production permits 76,667 logical / 76,670
aggregate file operations and exactly five GPS-awake admissions. The MIDDLE
HEIGHT save completes at ordinal 76,667 (34,413,753,416 ns close) and the
run stops at the sixth-admission refusal `001291cc / 4,345,171,340 /
37,899,807,613`. Nothing after that refusal has ever been observed.

## Evidence Inputs

The accepted five-pulse prefix `6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de`
and the 772/773 save-completion tuples are the inputs. No new production
behavior is assumed.

## Allowed Files

- `docs/migration-evidence.md`, `docs/current-status.md`,
  `plans/index.tsv`, `plans/tasks/774-sapporo-239-post-save-continuation-evidence.md`

## Frozen Interfaces

No file under `src/`, `include/`, `tests/`, `profiles/` or `tools/` may
change. Production ceilings (76,667/76,670 file operations; five awake
admissions) and every gate, checkpoint and refusal pin remain byte-identical.
The diagnostic raise below never exists outside a separately compiled copy.

## Implementation

Build one separately compiled copy of `src/compat/sapporo_239_gps_awake.c`
that raises ONLY the five-pulse awake ceilings from 5 to a bounded 25
(intervention maximum and layer aggregate together); file budgets stay at
production values. Link it ahead of `build/libsemu.a` into a /tmp probe so
the awake registry, application and snapshot identities remain self-consistent
under the raise, exactly as ticket 772 did for the file ceiling. Run from the
accepted prefix with the recorded MIDDLE press/release pair and observe:
whether a seventh and later wake is granted identically to pulses one
through five; what the firmware attempts after the completed save (further
saves, paths and ordinals); the first new refusal (PC, LR, registers,
instruction/virtual-time tuple, gps hits, counters and reason text); frame
and snapshot state; repeat equality, a refusal-start repeat, and a
mid-continuation snapshot resume. Run the unpressed idle control through the
same diagnostic binary and require the accepted idle endpoint byte-identically,
proving pulses one through five are undisturbed. Record every tuple as
diagnostic-only evidence; the sixth-and-later wake is a diagnostic grant,
never a claim about the physical device.

## Tests and Commands

`make test TEST_FILTER=sapporo_239` and `make test TEST_FILTER=machine_snapshot`
must pass unmodified; `make check-task-contracts` and `make check` must stay
green; the three private gates run before and after to prove byte-identical
pins; no production rebuild artifact may differ.

## Acceptance

The continuation measurement is recorded with exact tuples, hashes and the
first new refusal reason; the idle control through the diagnostic binary is
byte-identical; repeats and resume close as usual; production, gates and
checkpoints are provably untouched; the ledger states explicitly what is
diagnostic grant versus physical observation and names the next candidate
production boundary without integrating anything.

## Forbidden Scope

Any production ceiling raise (file or GPS), any gate repin, any claim that
the physical watch emits a sixth wake, GSTP/fix invention, or payload/
firmware capture into Git.

## Handoff

Registration commit only.
