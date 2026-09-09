# 772 — Sapporo 2.39 Personal Save Suffix Evidence

**Status:** ready
**Phase:** 7
**Dependencies:** 729,766,771

## Goal

Measure the complete native `settings/personal` save triggered at the
WEIGHT → HEIGHT boundary, including native success checks, exact operation
and write counts, and the next independent refusal. Determine whether the
unchanged ABI suffices and propose the exact finite production allowance.

## Execution Budget

One model-day of bounded native observation and read-only firmware analysis.
No production behavior change.

## Required Reading

`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
tickets 764 and 771; E-SAP-COMPAT-PERSONAL-239-001,
E-SAP-GPS-FIFTH-239-002, E-SAP-COMPAT-FILES-239-001;
`src/compat/{sapporo_239.c,sapporo_239_files.c,sapporo_239_files.h,
sapporo_239_files_internal.h,sapporo_239_file_hook.c,layer.c}`;
`include/semu/{machine,compat,manifest,trace}.h`;
`tests/integration/sapporo_239_five_probe.c` and its inherited probe chain;
pristine application wrappers `0x000920b4` (refusal PC), `0x000adb1e..0x000adb48`
and serializer `0x000d5ff8` before interpreting returns or serialization.

## Current Baseline

All dependencies are done. Production (`0826523` plus accepted 766) permits
76,599 logical-file / 76,602 aggregate hits. The accepted five-pulse gate
reaches HEIGHT at MIDDLE `3991602893 / 34088644931` and release
`3995360381 / 34174452535` (CRC `cd4c0a99`) and then refuses mode-two
`settings/personal` at PC `0x000920b4`, LR `0x000adb2f`,
instruction 4,232,903,136 / 34,413,596,174 ns. The full save is unmeasured.
Earlier evidence (764) measured the birth-year save at 68 operations
(66 full 1,727-byte writes) and the 228-operation useful continuation, and
proved the mode-two `settings/time` refusal at LR `0x000acb8b`; the HEIGHT
save suffix and its successor boundary are new.

## Allowed Files

- `docs/migration-evidence.md`, `docs/current-status.md`
- `plans/tasks/772-sapporo-239-personal-save-suffix-evidence.md`
- External temporary observational probes and isolated diagnostic sources;
  no private firmware, pixels, snapshots or modified libraries enter Git.

## Frozen Interfaces

Runtime, public headers, Makefile, profiles, registries, budgets, file paths,
capacities, handles, formats, renderer and goldens remain read-only. Use the
committed `sapporo-2.39-gps-awake-five` layer with its exact dependencies; the
four-pulse layer and every historical gate remain unchanged. No
private-state mutation, counter replenishment, callback invocation or parallel
runtime API.

## Evidence Inputs

E-SAP-GPS-FIFTH-239-002 pins the exact HEIGHT boundary inputs, refusal
tuple, snapshot identities and the immutable full-flash hash.
E-SAP-COMPAT-PERSONAL-239-001 establishes the measurement methodology
(separately linked finite descriptor allowance, bounded limits, repeat and
mid-save agreement) — not this save's counts.

## Implementation

Build an external probe that reuses the unchanged five-pulse input edges and
the unchanged file adapter/normal renderer, linked against a separately
compiled copy of `src/compat/sapporo_239.c` whose file-layer allowance is a
finite diagnostic ceiling (at most 512 operations above production; record
the exact value). Bound each run by six billion instructions and 45 billion
virtual ns. Pin source/library/binary/flash hashes; validate every component
and the full flash before execution.

Require two byte-identical full runs from the pinned five-identity prefix
`6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de`; a
mid-save snapshot resume (exact midpoint PC/instruction/time/snapshot hash);
a refusal-start repeat; and the first independent refusal preserved with its
PC/LR, instruction/time, GPS hit tuple, final snapshot, log-suffix and pixel
hashes. Record per-save operation counts, write sizes/contiguity, native
return, close and pending-flag clear; distinguish failure from return.
Observe every changed renderer frame CRC; never commit proprietary payloads.
Propose only the measured finite production scope with named integration
files/tests owned by a separate integration ticket.

## Tests and Commands

Compile the probe from the pinned tree; record exact commands and source and
output hashes. Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check` and `git diff --check`. Missing private
evidence may skip, but identity mismatches fail before execution.

## Acceptance

Complete successful native HEIGHT save with exact operation count, native
return/flag checks, repeat/resume agreement and the next refusal. State
whether the unchanged ABI suffices and propose the precise allowance. A
boundary-only trace does not complete this ticket.

## Forbidden Scope

No production allowance change, guessed personal values, sixth pulse or
fix/time/GSTP response, assertion bypass, firmware patch, renderer change,
new dependency, index/status update or weakened golden.

## Handoff

Planning baseline only.

