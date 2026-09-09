# 773 — Sapporo 2.39 Personal Save Suffix Budget Integration

**Status:** ready
**Phase:** 7
**Dependencies:** 729,766,771,772

## Goal

Apply exactly the production file-budget increase measured by ticket 772:
raise the `logical-file` intervention ceiling from 76,599 to 76,667 and the
wbsto layer aggregate from 76,602 to 76,670, so the HEIGHT-boundary personal
save completes in production, and move every gate that pinned the old
exhaustion refusal to the newly evidenced sixth-admission endpoint.

## Execution Budget

One model-day. No other behavior, budget, path, layer or GPS change.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
tickets 766 and 772; E-SAP-COMPAT-PERSONAL-SUFFIX-239-001 and
E-SAP-COMPAT-PERSONAL-239-001; `src/compat/sapporo_239.c`;
`tests/unit/test_sapporo_239_{compat,personal_budget,history_budget,general_budget,activity_budget}.c`;
`tests/integration/sapporo_239_five_probe.c` and
`test_firmware_sapporo_239_gps_five.sh`; the four-pulse
`test_firmware_sapporo_239_{gps_awake,personal_budget}.sh` gates to prove
they are unaffected.

## Current Baseline

All dependencies are done. Production permits 76,599 logical / 76,602
aggregate. Ticket 772 measured the HEIGHT save at exactly 68 operations
(open + 66 full 1,727-byte writes + close, ordinals 76,600–76,667) with the
next independent refusal the sixth GPS-awake admission at `001291cc`,
4,345,171,340 / 37,899,807,613 ns (gps_hits 2,2,5, HEIGHT CRC `cd4c0a99`,
75 frames, final snapshot `41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`).
The five-pulse gate currently pins the old budget refusal at `000920b4` /
4,232,903,136 / 34,413,596,174; the unit budget tests and the personal gate
assert 76,599-class constants and last-ordinal strings.

## Allowed Files

- `src/compat/sapporo_239.c`
- `tests/unit/test_sapporo_239_compat.c`,
  `tests/unit/test_sapporo_239_personal_budget.c`,
  `tests/unit/test_sapporo_239_history_budget.c`,
  `tests/unit/test_sapporo_239_general_budget.c`,
  `tests/unit/test_sapporo_239_activity_budget.c`
- `tests/integration/sapporo_239_five_probe.c`,
  `tests/integration/test_firmware_sapporo_239_gps_five.sh`
- `docs/migration-evidence.md`, `docs/current-status.md`,
  `plans/index.tsv`, `plans/tasks/773-sapporo-239-personal-save-suffix-budget.md`

## Frozen Interfaces

Public headers, `Makefile`, registries, profiles, layer identities, file
adapter ABI, capacities, renderer and all historical four-pulse gates and
checkpoints remain unchanged. Only the two measured constants and their
evidence comment move. The idle five-pulse branch keeps
`4071207676 / 37929735196` and the four-pulse gates keep every pin.

## Evidence Inputs

E-SAP-COMPAT-PERSONAL-SUFFIX-239-001 supplies the 68-operation save, the
ordinals 76,600–76,667, the exact save tuple pair, the repeat/resume protocol
hashes and the new MIDDLE endpoint; E-SAP-GPS-FIFTH-239-002 pins the
unchanged five-pulse inputs and idle refusals; E-SAP-COMPAT-PERSONAL-239-001
retains the four-pulse gates' endpoints. No new measurement belongs here.

## Implementation

Set the `logical-file` intervention maximum to 76,667 and the wbsto layer
maximum to 76,670 with a comment naming E-SAP-COMPAT-PERSONAL-SUFFIX-239-001.
Extend the personal-budget unit test with the measured fourth save (same
68-operation personal size sequence to `settings/personal`) reaching exactly
76,667 before the budget refusal, and raise the LIMIT assertions and the
single last-ordinal log grep in the history-budget test to 76,667 with the
unchanged `+3` aggregate relation. Repin the five-pulse gate MIDDLE branch to
the 772 endpoint (pc `001291cc`, 4,345,171,340 / 37,899,807,613, 75 frames,
CRC `cd4c0a99`, unchanged pixel SHA, new raw and name-normalized snapshot
hashes), require exactly 137 adapter log lines containing the 66 full writes,
the mode-two open and the successful close, and keep the single ordinal-5
awake line. Regenerate new snapshot hashes only through native runs.

## Tests and Commands

`make test TEST_FILTER=sapporo_239` (all unit budget tests), the three
2.39 private gates via `make test-firmware` with the private manifest and
`SEMU_SAPPORO_239_FULL_FLASH` fixture (`TEST_FILTER` values
`sapporo_239_gps_five`, `sapporo_239_gps_awake`,
`sapporo_239_personal_budget`), `make test TEST_FILTER=machine_snapshot`,
`make sanitize`, `make check-task-contracts`, `make check-lines`,
`make check` and `git diff --check`. Wrong firmware/flash/hash inputs must
still fail before execution.

## Acceptance

Production permits exactly the 68 newly measured operations and refuses the
next one with the unchanged budget message; the five-pulse MIDDLE gate
completes the HEIGHT save and refuses the sixth GPS admission at the exact
772 tuple with byte-identical repeats; four-pulse gates, the idle branch, all
historical snapshots and the 2.22.60 gate pass unchanged; sanitizer clean.

## Forbidden Scope

Any ceiling beyond the measured 76,667/76,670, any sixth-pulse permission,
file-path additions, time/GSTP behavior, profile or ABI changes, golden
weakening, or claiming post-HEIGHT navigation is supported.

## Handoff

Implemented in the registration commit. `src/compat/sapporo_239.c` carries
76,667/76,670 with the E-SAP-COMPAT-PERSONAL-SUFFIX-239-001 comment; unit
budget and descriptor tests moved to the new constants and the personal test
replays the measured fourth save to exactly 76,667 before the unchanged
budget refusal. The five-pulse gate MIDDLE branch is repinned from native
production runs: refusal `001291cc / 4,345,171,340 / 37,899,807,613`,
75 frames, CRC `cd4c0a99`, SHA `33339448…`, final snapshot
`41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`
(byte-identical to the 772 diagnostic image), normalized
`65255eb1abe56f8f3ff82e1320dfce40c7671a52e446f15b3cdced37768a83d5`, and the
log assertions now require 137 middle lines with the 66 full writes, the
mode-two open, the successful close and ordinal 76,667. Verified: 51 filtered
unit passes, all three private gates, `make sanitize`, `make check`, 140
task contracts, `make check-lines` (pre-existing warnings only) and
`git diff --check`. Prefix, idle pins, four-pulse gates and the 2.22.60
checkpoint are byte-unchanged. Remaining gaps unchanged from 772: HEIGHT
selection acceptance and the post-save input are unmeasured. Integrator
review should re-run the five-pulse gate and the personal-budget unit test
before flipping 773.
