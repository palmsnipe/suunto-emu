# 774 — Sapporo 2.39 Post-Save Continuation Evidence

**Status:** done
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

Measured 2026-09-09 with E-SAP-GPS-CONTINUATION-239-001; no production file
changed. The diagnostic copy (`awake_diag.c`,
`3f3377c2d93ae75f2afaa38ab377e921b12388d4b1c84a15490ef2e451153cee`) raises
only the two awake ceilings 5→25. Controls: cold prefix regenerates
`6e670940…` under the diagnostic binary; time-capped link-variant pairs are
byte-identical at the instruction before the sixth admission on both
branches (idle `e46aa7e6…` @ 4,071,207,675/37,929,735,195; post-save
`6c3e261b…` @ 4,345,171,339/37,899,807,612, adapter logs equal). With the
sixth wake granted the idle branch stays silent (no save, no frame) and the
post-save branch performs no further writable operation
(`compat_hits=76670`, frames frozen at 75/CRC `cd4c0a99`); both then issue
SCB AIRCR VECTKEY|SYSRESETREQ resets at PC `0x000d2f6e` (idle
4,536,286,836/60,828,679,535; post-save 4,875,799,883/60,779,430,750),
are internally reset per E-CPU-0006, and halt post-reset at `0x00079e1e`
(finals `bd5004c0cabe0ce0857bd9167a15ea5a77c10c4a545aa13ad6301dde8d96f35b`
and `411e3551a95a33aa0fb8afbc756ed9b6f27fe94adf1e7959c83d096d0916c6c8`).
Repeat pairs and post-grant mid-continuation resumes (from
`2b5c43a4808462f2a3ad94ba3ac0eaae6716b7085cf2b4f416bccd6e52b61af2`) are
byte-identical. One method note: the admission hook fires at instruction
start, so snapshots cannot straddle it — the honest byte-identity control
is therefore the T−1 cap, which both variants share. All 51 filtered unit
passes and 4 snapshot passes held; gates were untouched. Remaining gaps:
physical GPS behavior past the fifth pulse (or GSTP time) is unobserved —
that, not a ceiling, bounds further continuation; HEIGHT-selection visual
acceptance remains unasserted (no frame change to pin). Integrator review
should rebuild both probe variants from the recorded sources and re-run the
control pair plus one continuation before flipping 774.

## Integrator Acceptance

Accepted 2026-09-09. Acceptance binaries were rebuilt from the recorded
sources (`probe774.c` `b46eccf82e3d17dcc749af63c20e67cb781f03b2324d3cccb782a27a162efd92`,
`probecap.c` `598de23c7d24aea78a6b2d8a8e1360ce12f66dfe7e1464883ec29926d2442642`,
`awake_diag.c` `3f3377c2d93ae75f2afaa38ab377e921b12388d4b1c84a15490ef2e451153cee`,
derivative capped source `10af6d93686d2470f90ef1292183405ce0869e909d791cc4d4b2bd9f9f232e52`,
with `build/libsemu.a` unchanged at `25f1f6d51e213ee4a5d1fd5c764e3c3c240145df815263c23a84fa22e7acda16`).
The idle and post-save pre-admission control pairs and the full post-save
continuation were rerun in parallel and every artifact reproduced the ledger
byte-for-byte: idle control pair `e46aa7e6c57bb90491f84f74aa9cb10f2b2f7eeb1189938351e73acc5ea5b41e`,
post-save pair `6c3e261bc4bb76456ecdee1f506cb42ff7d30d7425cfe5cf09a4b785ce812ad2`,
continuation final `411e3551a95a33aa0fb8afbc756ed9b6f27fe94adf1e7959c83d096d0916c6c8`,
trace `04723096035b034741c0e381aca28187188e185e27301ff4543d689834e5a0aa`,
log `ed0f31b0e5837c58f7320ba752e5bd6233f26c0364914023922fca9325a9e637`,
reset tuple `0x000d2f6e / 4,875,799,883 / 60,779,430,750 ns /
compat_hits=76670`. The three private gates (gps-five, gps-awake,
personal-budget) re-ran green with unchanged pins; the accepted-file list was
respected (`git status` showed only docs/plans). Diagnostic-grant framing is
explicit in E-SAP-GPS-CONTINUATION-239-001. Remaining gaps accepted as
stated: physical GPS behavior beyond the fifth pulse is unobserved and is
now registered as blocked ticket 776; HEIGHT-selection visual acceptance
remains unasserted.
