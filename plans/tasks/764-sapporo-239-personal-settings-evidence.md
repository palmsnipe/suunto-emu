# 764 — Sapporo 2.39 Personal Settings Persistence Evidence

**Status:** done
**Phase:** 7
**Dependencies:** 729,759,763

## Goal

Measure the complete native personal-settings save at the birth-year boundary,
including native success checks and the next independent refusal. Determine
whether an exact finite allowance suffices or an ABI repair needs evidence.

## Execution Budget

One model-day of bounded native observation and read-only firmware analysis.
No production behavior change.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, `docs/testing-strategy.md`,
`docs/compatibility-policy.md`; ticket 763 and all Allowed Files;
E-SAP-COMPAT-GENERAL-239-001, E-SAP-UI-PERSONAL-239-001;
`include/semu/machine.h`, `include/semu/compat.h`, `include/semu/bus.h`,
`src/compat/sapporo_239.c`, `src/compat/sapporo_239_files.h`,
`src/compat/sapporo_239_files.c`, `src/compat/sapporo_239_files_internal.h`,
`src/compat/sapporo_239_file_hook.c`, `src/compat/layer.c`;
the external observer sources pinned by those evidence entries; pristine
application wrapper `0x000adb1e..0x000adb48`, serializer `0x000d5ff8` and
reachable helpers before interpreting their return/serialization contracts.

## Current Baseline

All dependencies are done. Production `3afb5ff` permits 76,371 logical-file /
76,374 aggregate hits. Three further native middle presses reach BIRTH YEAR
and refuse mode-two `settings/personal` open at PC `0x000920b4`, LR
`0x000adb2f`, 2,953,605,137 instructions / 24,380,651,994 ns.
GPS hits are `2,2,3`; no renderer refusal occurs. The full save is unmeasured.

## Allowed Files

- `docs/migration-evidence.md`, `docs/current-status.md`
- `plans/tasks/764-sapporo-239-personal-settings-evidence.md`
- External temporary observational probes and isolated diagnostic sources;
  no private firmware, pixels, snapshots or modified libraries enter Git.

## Frozen Interfaces

Runtime, public headers, Makefile, profiles, registries, budgets, file paths,
capacities, handles, formats, renderer and goldens remain read-only. Keep all
four explicit layers and the four-pulse GPS bound. No private-state mutation,
counter replenishment, callback invocation or parallel runtime API.

## Evidence Inputs

E-SAP-UI-PERSONAL-239-001 pins production inputs, six actual input edges,
normal-renderer observer and repeated refusal snapshot. General-settings
evidence establishes diagnostic methodology, not this serializer's count.

## Implementation

Repeat the unchanged production boundary and inspect pristine serialization
and success branches. An external diagnostic may use a separately linked,
explicit finite descriptor allowance while forwarding the unchanged adapter
and normal renderer. Pin sources/library/inputs; validate every component
and full flash before execution. Record PC/LR, arguments, returns, offsets,
sizes, ordinals and hashes, never proprietary payloads in Git.

Bound each run by five billion instructions and 35 billion virtual ns, at
most 512 extra logical operations and 5,000 changed frames. Require equivalent
repeats and a mid-save snapshot resume. Observe native close and pending-flag
clear; distinguish failure from mere function return. Preserve the first
independent refusal. Propose only the measured finite production scope.

## Tests and Commands

Compile and run the unchanged production observer from
E-SAP-UI-PERSONAL-239-001 twice; compare trace, final snapshot and pixels.
Record exact bounded diagnostic commands and source/output hashes.
Run `make test TEST_FILTER=sapporo_239`,
`make test TEST_FILTER=machine_snapshot`, `make check-task-contracts`,
`make check-lines`, `make check` and `git diff --check`.
Missing private evidence may skip, but identity mismatches fail before execution.

## Acceptance

Complete successful native save sequence with exact operation count, native
return/flag checks, repeat/resume agreement and next refusal. Explain whether
the unchanged ABI suffices and propose precise separately owned integration
files/tests. A boundary-only trace does not complete this ticket.

## Forbidden Scope

No production allowance change, guessed personal values, extra GPS pulse or
fix/time/GSTP response, assertion bypass, firmware patch, renderer change,
new dependency, index/status update or weakened golden.

## Handoff

Planning baseline only. Personal serialization and further setup remain
unproven; implementation leaves this ticket/index status unchanged.

### Evidence handoff, 2026-09-08

E-SAP-COMPAT-PERSONAL-239-001 records complete native saves, success checks,
repeat/resume pins and the next independently unsupported file. Production
`3afb5ff` remains unchanged. The evidence implementation changes only this
ticket and `docs/{migration-evidence,current-status}.md`; the separate earlier
planning review accepted 763 and created this row in `plans/index.tsv`.
All private probes/snapshots/pixels remain outside Git in
`/tmp/semu-764-personal.rFx4hQ/`. This ticket/index stays ready for review.

The isolated descriptor has at most 512 extra operations (76,883 logical /
76,886 aggregate), five-billion-instruction / 35-billion-ns absolute limits
and 5,000 changed-frame capacity. No RAM/CPU/counter repair or firmware edit.
Both production repeats preserve the prior trace, final state and pixels.

The first personal save consumes 68 operations, including 66 full writes of
1,727 bytes. Native return is one, close returns one and the pending flag
clears. No ABI fix is indicated. Its midpoint is after 33 writes at
`000af814 / 2953666398 / 24380713255`, snapshot
`68ab2fa1fda8596e1b51a6b3d83950363effb0598f1835506492d20636fad7d8`.
Without further input, two earlier-start runs, two refusal-start runs and
mid-save resume all reach the existing fifth GPS-pulse refusal:
`001291cc / 3152721353 / 32538694863`, final snapshot
`4faf5b8934c80cbadc33a7d6a389dd8f50a26bacdf2ed7208effd7a4abadb3e3`,
birth-year CRC `568bdc7d`. Full log suffix and pixels agree exactly.

Three more native presses at 26/28/30 seconds establish the complete useful
continuation: **228 operations** (two personal saves of 68 each and one
general save of 92), all 222 writes full-length and contiguous, 4,959 total
bytes. Complete earlier-start repeats and mid-save continuations converge to
the visually inspected WEIGHT selector, CRC `a8c9f3d3`, pixel SHA-256
`3820703556359211f629aea5ef013dda45fba092229b8d61e5a10d684c42d585`.
All 1,004 complete-run / 232 mid-save-run renderer submissions succeed.
Final snapshot:
`3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`.
Stop is `compat-refused / 000920b4 / 3885178598 / 30368914377`, LR
`000acb8b`, mode-two `settings/time`, GPS hits `2,2,4`. The path is unknown
despite unused diagnostic headroom. Its native wrapper calls `0x000d5084`;
the file schema/capacity and complete serialization remain unmeasured.

### Reproduction commands and results

All commands run from the repository root with the hash-pinned external
sources retained. Each observer validates every component/full flash before
execution; exit zero means the explicit END capture succeeded, not setup
completion. Pairwise comparisons and the pinned END records are required.

```sh
research_dir=/tmp/semu-764-personal.rFx4hQ
cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/devices /tmp/semu-763.WR6heT/walk-probe.c build/libsemu.a -o "$research_dir/production-probe"
for attempt in a b; do
  "$research_dir/production-probe" "$research_dir/prod-$attempt" /tmp/semu-763.WR6heT/first.mid.sems > "$research_dir/prod-$attempt.trace" 2> "$research_dir/prod-$attempt.log"
done
cmp "$research_dir/prod-a.trace" "$research_dir/prod-b.trace"
cmp "$research_dir/prod-a.final.sems" /tmp/semu-763.WR6heT/walk-a.final.sems
cmp "$research_dir/prod-a.final.sems" "$research_dir/prod-b.final.sems"
cmp "$research_dir/prod-a.rgb565" "$research_dir/prod-b.rgb565"
for observer in personal total next; do
  cc -std=c99 -Wall -Wextra -Werror -pedantic -O2 -Iinclude -Isrc -Isrc/compat -Isrc/devices "$research_dir/$observer-probe.c" "$research_dir/file-observer.c" "$research_dir/sapporo_239_diag.c" build/libsemu.a -o "$research_dir/$observer-probe"
done
for attempt in a b; do
  "$research_dir/personal-probe" "$research_dir/save-$attempt" /tmp/semu-763.WR6heT/walk-a.final.sems > "$research_dir/save-$attempt.trace" 2> "$research_dir/save-$attempt.log"
  "$research_dir/personal-probe" "$research_dir/full-$attempt" /tmp/semu-763.WR6heT/first.mid.sems > "$research_dir/full-$attempt.trace" 2> "$research_dir/full-$attempt.log"
  "$research_dir/total-probe" "$research_dir/total-$attempt" /tmp/semu-763.WR6heT/first.mid.sems > "$research_dir/total-$attempt.trace" 2> "$research_dir/total-$attempt.log"
  "$research_dir/next-probe" "$research_dir/next-$attempt" "$research_dir/save-a.mid.sems" > "$research_dir/next-$attempt.trace" 2> "$research_dir/next-$attempt.log"
done
"$research_dir/personal-probe" "$research_dir/resume" "$research_dir/save-a.mid.sems" > "$research_dir/resume.trace" 2> "$research_dir/resume.log"
for pair in save full total next; do
  cmp "$research_dir/$pair-a.trace" "$research_dir/$pair-b.trace"
  cmp "$research_dir/$pair-a.final.sems" "$research_dir/$pair-b.final.sems"
  cmp "$research_dir/$pair-a.rgb565" "$research_dir/$pair-b.rgb565"
done
cmp "$research_dir/save-a.final.sems" "$research_dir/resume.final.sems"
cmp "$research_dir/save-a.final.sems" "$research_dir/full-a.final.sems"
cmp "$research_dir/total-a.final.sems" "$research_dir/next-a.final.sems"
cmp "$research_dir/total-a.rgb565" "$research_dir/next-a.rgb565"
awk 'match($0,/time_ns=[0-9]+/) {t=substr($0,9,RLENGTH-8); if(t>=24380713255) print}' "$research_dir/total-a.log" > "$research_dir/total-suffix.log"
cmp "$research_dir/total-suffix.log" "$research_dir/next-a.log"
awk 'match($0,/time_ns=[0-9]+/) {t=substr($0,9,RLENGTH-8); if(t>=24380713255) print}' "$research_dir/save-a.log" > "$research_dir/suffix.log"
cmp "$research_dir/suffix.log" "$research_dir/resume.log"
```

All observer runs and comparisons pass without skipped private evidence.
Metadata audits check each admitted ordinal increments once, successful
status, every cursor and full write return: 68/1,727 for the first save and
228/4,959 with three opens/closes for the full continuation. Pristine
disassembly uses `arm-none-eabi-objdump -D -b binary -m arm -M force-thumb
--adjust-vma=0x40000 --start-address=START --stop-address=END` on
`tests/private/sapporo-2.39.20.22297/application.raw`, at the exact wrapper,
serializer and helper ranges in E-SAP-COMPAT-PERSONAL-239-001. The ledger
records every source, library, trace, snapshot and pixel hash.

`make test TEST_FILTER=sapporo_239` passes 45 cases;
`make test TEST_FILTER=machine_snapshot` passes four;
`make check` passes all 830 cases; `make check-lines` passes with existing
review warnings; `make check-task-contracts` validates 133 tickets;
`git diff --check` passes. Focused/check logs are in the research directory.
The pre-commit general-budget regression also passes one case. Runtime C is
unchanged after the commit: sanitizers and historical firmware gates were
reviewed from the committed 763 handoff, not represented as rerun here.
The full-flash SHA-256 remains unchanged after all experiments.

### Proposed separate integration and remaining gaps

Instantiate a personal-settings continuation budget integration after review,
depending on accepted 764, 729 and 759. Exact proposed production ceiling:
**76,599 logical-file / 76,602 aggregate**, not the diagnostic allowance.
Suggested Allowed Files:

- `src/compat/sapporo_239.c`: only two constants and evidence comment.
- Existing compatibility/history/activity/general budget unit tests: update
  current-ceiling assertions and separate saturation, retaining all measured
  historical sequences and checkpoint pins.
- New `tests/unit/test_sapporo_239_personal_budget.c`: synthetic existing
  personal/general files, exact 68+68+92 suffix, native-sized returns,
  mid-save round trip, final close at the exact limit, atomic excess refusal
  including CPU/RAM/files/handles/counters/log, unknown path/mode refusal.
- New private-only personal observer and firmware shell gate: normal renderer,
  exact identities, twelve native input edges, repeat/mid-save resume, both
  idle GPS and navigated time-path endpoints, malformed-input rejection.
- `docs/current-status.md`, `docs/migration-evidence.md` and its new ticket.

No public header, Makefile, registry, profile, file ABI/capacity, snapshot codec,
renderer or GPS change is needed for those 228 already-supported operations.
Require failing-before regression, `make check`, `make sanitize`, line/contracts
and exact historical general/activity/GPS/2.22 live-input gates with no repin.
Keep `settings/time` refused pending independent schema/capacity evidence;
adding a slot requires separately owned file/snapshot integration. Watch-face
activation, accepting weight, completing setup and post-setup menus remain
unproven. Evidence acceptance has passing observations; status is unchanged.

### Integrator acceptance, 2026-09-08

Separate planning review accepts the complete 68+68+92 sequence, native
success checks, repeated full/idle endpoints, mid-save continuations and
exact pins in E-SAP-COMPAT-PERSONAL-239-001. All dependencies are done.
No acceptance condition is waived. Ticket 766 instantiates the proposed
228-operation production integration; no time-file or GPS extension is
included in this acceptance. Implementation did not change its own status.
