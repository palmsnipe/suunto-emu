# 772 — Sapporo 2.39 Personal Save Suffix Evidence

**Status:** done
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

Evidence complete 2026-09-09; recorded as E-SAP-COMPAT-PERSONAL-SUFFIX-239-001.
Outcome: under a separately linked diagnostic ceiling (77,111/77,114 =
production + 512), the mode-two `settings/personal` save triggered at the
WEIGHT → HEIGHT boundary completes as exactly 68 operations (open handle
`0x10161600`, 66 successful contiguous 1,727-byte writes with the birth-year
save's exact size sequence, close returning one at ordinal 76,667). Serializer
returns one at `0x000adb3c` (4,233,060,376 / 34,413,753,414 ns, pending
`+0x145` = 1); close/flag-clear complete at `0x000adb48` (4,233,060,380 /
34,413,753,418 ns). The unchanged adapter ABI suffices; no ABI correction.
The screen remains HEIGHT (CRC `cd4c0a99`) and the next independent refusal
is the sixth GPS-awake admission, not a file refusal: `001291cc` /
`lr=000d3c35` / 4,345,171,340 / 37,899,807,613 ns with gps_hits 2,2,5. The
file layer is not binding anywhere through the save. Proposed finite
production scope: **76,667 logical / 76,670 aggregate** (+68 measured), to be
applied by a separate integration ticket; the diagnostic 512 margin is not
proposed.

Verification: prefix `6e670940…` cold-regenerated and hash-validated; two
clean repeats byte-identical in trace and adapter log; third instrumented run
identical modulo probe output; mid-save capture after open + 33/66 writes at
`0x000af814` / 4,232,964,397 / 34,413,657,435 ns (snapshot `e343e340897a…`)
resumes to byte-identical final/refused snapshots (`41c65d4626481ddd…`);
refusal-start repeat re-emits the identical refusal atomically; no-input
control reproduces the accepted 771 idle pin `001291cc / 4071207676 /
37929735196` exactly. Full flash validated at
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`; probe
source `e8157f288a4654c7ea9d9b75540d155c038096a2dc7dbd8e43d068b28a1daa8b`,
diagnostic descriptor `31e2c1ed652e312ec11da351ce979494fa67beeef2d19672d0abf09f91074b27`.
Commands: `cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc
-Isrc/devices -Isrc/compat probe.c sapporo_239_diag.c build/libsemu.a` (plus
`-DMID_HITS=76636u -DMID_START=UINT64_C(4232900000)` for the mid variant),
then `./probe <manifest> <full-flash> <start.sems> <start-hash> <out-prefix>
suffix|idle|resume`.

Remaining gaps: whether the HEIGHT selection was accepted and which input
follows the completed save are unmeasured (the sixth-admission refusal bounds
continuation at 37,899,807,613 ns); watch-face activation and post-setup
menus remain unproven. Production ceilings, profiles, registries and goldens
untouched; status remains `ready` for integrator review.


## Integrator Acceptance

2026-09-09 separate integrator review accepts the evidence work. The exact
recorded probe command was rerun independently against the accepted
commits: trace `76c0ffc1ff0bd31e9325e468d446f8dc8b9bd15b42de3027e4fd08f73dff38b7`,
adapter log `a79b6362bfb2299d23087ef60b1e9df7269e053488c427471a4faaa7f138f65b`
and final snapshot `41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`
all matched byte-for-byte, including the 68-operation save, the
`0x000adb3c`/`0x000adb48` save tuples and the sixth-admission refusal
`001291cc / 4345171340 / 37899807613`. Ticket filters selected real suites
(`TEST_FILTER=sapporo_239` 51 passes, `machine_snapshot` 4 passes); 139 task
contracts and `make check` pass; the full-flash pin is unchanged. The review
confirms the measured proposal 76,667 / 76,670 (+68), the unchanged ABI
conclusion, and that the remaining gaps (post-save input, HEIGHT acceptance,
watch face, menus) are recorded as unmeasured. Applying the ceiling belongs to
integration ticket 773; production values, goldens and GPS bounds were
untouched by this ticket.
