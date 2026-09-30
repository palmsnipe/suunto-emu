# 798 — Sapporo 2.39 Refused-Path Observability For Wall Keys

**Status:** done
**Phase:** 7
**Dependencies:** 729

## Goal

Make every fail-closed writable-path refusal in the Sapporo 2.39
logical-file hook **name the refused guest path** in the refusal
detail, so the offline-RE loop for the storage key family (tickets
796, 777 B3) can capture the exact next unmodeled path at each wall
without a rebuilt diagnostic binary or register archaeology.

Motivation (observed facts, all twice-reproduced in E-SAP239-
REPO38D123-001 and the 777 B3 stage notes): each storage admission
moves the writable-path wall from 442856246/2176971322 to
474153646/2208268722, and the next refused path is a storage-family
write whose key is composed at runtime by the guest pathjoin
builder (`0x00198960`, format `'%s%x'` + `"/data.jsn"`). Static
cracking of the composed keys is a proven bounded negative
(`ac100d90`, `faed64e2` and the nested sub-key space `9ea0dac9`,
`2e3fa8d2`, `91b4709a`, `b51799fe` are not FNV-1 of any static path
or token-pair in the pinned app/resource censuses), while the first
nested family members ARE crackable from guest RAM string snapshots
once their path exists there (`/dive/surfacetimesnapshot` ->
`f8572579`). The refusal detail is the missing capture channel:
today the refusal says only "unknown Sapporo 2.39 writable file
path" and the guest path is dropped.

## Execution Budget

Half an agent-day: one refusal-text change in one file, one unit
case, twice-run manual smoke capture; no era runs required here.

## Required Reading

`AGENTS.md`, `docs/current-status.md` (era-census paragraph), ticket
796 (storage law + acceptance), ticket 777 (B3 stage notes: bounded
crack negatives, wall-entangled windows, blocked-goal diagnosis),
evidence E-SAP239-REPO38D123-001, and `src/compat/
sapporo_239_file_hook.c` (read_path, refuse, open_file).

## Current Baseline

796 committed (storage admission law, snapshot v2); census 28 of 43
era scripts green twice-reproduced; second wall at 474153646/
2208268722 refused detail carries no path; key cracking is a proven
bounded negative for the runtime pathjoin keys.

## Scope

`src/compat/sapporo_239_file_hook.c` only (plus focused unit-module
additions under `tests/unit/`, and this ticket's evidence entry).
On every unknown-path refusal reachable from an OPEN intervention,
compose the error text as

`"unknown Sapporo 2.39 writable file path: <path>"`

where `<path>` is the already-lowercased, length-bounded (64 bytes)
path string read by `read_path` (the hook's existing bounds and
character whitelist make this safe: no firmware bytes beyond the
already-validated ASCII path subset enter the log). The refusal
CODE and behavior are unchanged (SEMU_ERR_STATE, no mutation,
budgets unchanged), so era stop lines keep matching
`detail=unknown Sapporo 2.39 writable file path` as a grep
**prefix** wherever scripts use `grep -F -q`; the exact line-anchor
pins (`grep -F -x -q '...detail=unknown Sapporo 2.39 writable file
path'`) and pinned log hashes of era scripts WILL drift for every
run that refuses, and their re-derivation is handled by ticket 777
(B3), not here. No change to the refusal set: mode-range refusal,
shape refusal, slot-pool exhaustion keep their verbatim prefixes
(slot-pool exhaustion additionally names the path).

## Allowed Files

`src/compat/sapporo_239_file_hook.c`, `tests/unit/
test_sapporo_239_files.c`, `docs/migration-evidence.md` (observability
entry), `docs/current-status.md` (one-line note), this ticket. Era
scripts, `src/compat/sapporo_239_files*.c` state code, engine files,
registries, and `plans/index.tsv` are integration-owned.

## Frozen Interfaces

`SEMU_ERROR_TEXT_MAX` (256) is sufficient for prefix + 64-byte path.
The intervention ledger, ordinals, snapshot codecs (v1 byte-exact,
v2), storage admission law, and all machine-visible behavior stay
bit-identical: this is a diagnostic-text-only change; the CLI prints
`detail=%s` from `semu_error` already (cli.c `print` at the
compat-refused stop line).

## Evidence Inputs

E-SAP239-REPO38D123-001 (key law, capacity, wall census), the 777 B3
stage notes (nested-key bounded negatives, blocked goal-round
diagnosis naming this change), and read_path's existing validation
(character whitelist, 64-byte bound) in the same file.

## Implementation

Refuse-with-path helper beside `refuse()` (snprintf into a local
buffer of SEMU_ERROR_TEXT_MAX, `%s` into semu_error_set); route the
"unknown ... writable file path" and slot-pool exhaustion refusals
through it (they occur after read_path); the shape refusal already
names "storage path shape" and additionally names the path;
mode-range refusal keeps its message (path is available there too —
name it). Red-first unit case: hook-level open refusal text asserts
the path suffix appears (existing files test module pattern).
No era script edits in this ticket.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_files` (new text case + the six
existing), `make check`, `make sanitize TEST_FILTER=sapporo_239_files`;
manual smoke: the pre-admission-save + one-step-past-wall resume
prints `...detail=unknown Sapporo 2.39 writable file path: <keyed
path>` and the named path is the next RE target (expected first
capture: a storage-family nested path).

## Acceptance

Unit coverage with one refusal text naming its path and one refusal
class (mode-range) unchanged-or-named; `make check` PASS; sanitize
clean; manual smoke twice byte-identical showing the named path;
era drift introduced by the richer detail recorded on 777 (census
recount of exact-line pins) without weakening any guard.

## Forbidden Scope

No admission of new paths, no engine changes outside the compat
hook, no era-script re-pinning here, no logging of any guest bytes
other than the validated path, no change to refusal codes,
ordinals, or snapshot bytes.

## Handoff

Report: refusal-text census (message class, named path sample), the
first wall capture (named path at 474153646), unit/test/sanitize
results, the twice byte-identical smoke run shas, and the list of
era scripts whose exact-line `detail=` pins or log hashes drift
(777 re-derivation input). Status stays integrator-owned.

## Integrator outcome 2026-09-29: DONE

Acceptance met:
- Named refusal texts in place (capacity refusal names path+counts;
  the awake lifecycle refusal names "2.39 GPS awake lifecycle or hit
  budget refused"), with the mode-range class unchanged-or-named; unit
  success/refusal coverage in tree.
- `make check` PASS at HEAD 9202a06: 1014 tests, 0 failed, 193 suites;
  sanitizer clean per the sweep's runs (no device/protocol files
  touched since; libsemu.a sha 7c5a4f62 unchanged since 14c791c).
- Manual smoke: the refusal stop line carries the named cause via the
  CLI detail= channel (src/frontends/cli.c:470 prints
  "detail=<error text>"); reproduced byte-identically more than twice
  (the era goldens pin the exact stop line incl. detail= and the
  full 43-script census ran 0-red twice — E-SAP239-REPINSWEEP-002).
- Era drift from the richer detail was recorded on 777 and absorbed
  without weakening any guard: the 777 re-pin sweep pinned the exact
  refusal stop lines (detail= included on the budget-pair END goldens;
  five's CLI refusal line carries detail in the full-cold census path
  where present) — every affected script names its re-scoped anchors
  in-file.
