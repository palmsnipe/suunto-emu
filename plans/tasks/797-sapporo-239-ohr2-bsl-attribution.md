# 797 — Sapporo 2.39 OHR2 BSL Refusal-Semantics Attribution

**Status:** done
**Phase:** 7
**Dependencies:** 729

## Goal

Re-derive the seven Sapporo 2.39 OHR2-era scripts on the current tree.
The attribution stage is COMPLETE (integrator clean-build bisect,
2026-09-27): no OHR2 device-law flip exists or needs fixing. The pinned
refusal goldens died from two lawful, separately attributed engine
movements; the remaining work is mechanical re-derivation plus an evidence
record of the negative result.

## Attribution Record (integrator, clean builds — supersede the earlier
subagent classification, whose "refuse→ok device flip" premise was wrong)

Build discipline note: the first probe pass reused one build directory
across checkouts and mixed stale objects into `libsemu.a` (duplicate-symbol
link failure exposed it). Every finding below is from `make clean` +
rebuild per commit. The first probe pass ALSO had the wrong window shape
(440M resume instead of the scripts' pinned one-instruction resume), which
is why no pass ever observed a refuse line; the corrected shape was
verified at HEAD.

Probe shape (the scripts' own goldens): cold window
`--until normal-frame --max-instructions <pinned cap>` + snapshot, resume
window = the same run with cap+1 instructions from that snapshot (the
guest's next single instruction), refuse golden expected in that
one-instruction log. The ohr2_boot_mode pinned cold stop is
`pc=0x0014e8ea vt=1881138282`, pinned resume stop
`pc=0x001c0db4 vt=1881138283`.

1. Clean-build probes (cold stop at cap 359790038, ohr2_boot_mode shape):
   `69b1b35` and `0c84673`: pinned-exact (`0x0014e8ea`/`1881138282`).
   `cd1de52`: relocated (`0x000d2084`/`1879581886`).
   `25e8b3d`, `2f22137`, `39da666`: `0x000d2084`/`1879581886`.
   `main` 06e3c2c: `0x000a7ac4`/`1879581886` — same virtual time, pc drift
   only, matching the ohr2_command2 re-pin lineage (E-ULS-0047 precedent,
   integrator-verified twice). The relocation is a pure instruction-count
   movement at fixed virtual time: attributed to `cd1de52` (2.35 bring-up
   containing the CPU branch fix and other global engine work; the 2.39
   era scripts were not run in that handoff — silent drift, the known
   defect class).
2. The RESUME refuse goldens are NOT dead, contrary to the earlier draft
   of this record. The pinned one-instruction resume at HEAD shows the
   guest advancing a single instruction at the boundary with NO OHR2
   transaction at all (ohr2_boot_mode: cold `pc=0x000a7ac4 vt=1879581886`
   → resume `pc=0x000a7ac6 vt=1879581887`, zero transactions; ohr2_echo:
   cold `pc=0x000a7f32 vt=1888829177` → resume `pc=0x000a7f38
   vt=1888829178`, zero transactions). The pinned refuse transactions
   (`0x0000 seq=1 BSL`, `0x0002 seq=7 MAIN`, the 0x000d/0x000e/0x0006
   family) are still real device refusals — the guard greps
   ('command=0x0006.*status=refuse' etc.) prove the device still refuses
   at HEAD — but they now fire at a DIFFERENT guest instruction (the
   session's post-boot path relocated), and the transcripts at HEAD show
   the same transaction sequence completing with `status=ok` at the
   observed moments (e.g. `command=0x0000 sequence=1 state=BSL status=ok
   ready=1` at time_ns=1881138282 and `command=0x0002 sequence=7
   state=MAIN status=ok ready=1` at time_ns=1890385573). So the seven
   scripts' boundary goldens re-pin to the current instruction lineage and
   their refuse-ok golden lines re-pin to the current transcript lines at
   the same positions; the refuse-GUARD greps stay exactly as-is.
3. The Sep-5 file-law series still explains artifact-hash drift at the
   pinned-exact stop (at `69b1b35` clean, the first-window log/snapshot
   hashes differ from the pins while the stop line matches exactly: log
   `0b4da2f95e105568be485357a881fe4ecc0ce7d6f5734a2e44c859767d80a5f5` vs
   pinned `b8977bf8911cc5435e19afc109205c267e822249c17e19fba3809779d046664e`;
   sems `885e517f21b69c5924237d00b8edb4b450c07b82f7c3d03738adae74022bcfad`
   vs pinned `b7d1d84e2be435635cc6031b8424ece436b6557d3ba3883c59f92b7550916f86`)
   — compat intervention lines and session-local state changed without
   moving the guest. That drift affects hashes and census counts, not the
   refuse semantics.
4. OHR-touching commits `1bc1ce8`, `6ae8ce5`, `7c8bb59` (Sep 23):
   EXONERATED — at `39da666` (7c8bb59's parent) the cold stop and vt are
   already identical to the later era; `7c8bb59`'s MAIN-state 0x0004
   admission is a lawful additive device-module change (lane-proven,
   2.35-scoped usage) and did not move the 2.39 session.
5. No device-law change is warranted. The seven scripts re-derive
   mechanically: boundary stop lines to the current lineage, artifact
   hashes to the observed artifacts, golden transcript lines to the
   current transcript at the same positions (the ok/refuse values exactly
   as observed), all guards and window shapes untouched.

## Execution Budget

One agent-day: seven scripts re-derived twice-identically, one evidence
entry for the negative attribution, integrator spot-verify.

## Required Reading

`AGENTS.md`; this ticket's Attribution Record; ticket 777's classification
record; `docs/migration-evidence.md` E-SAP-COMPAT-FILES-239-001,
E-ULS-0047; the seven era scripts.

## Current Baseline

main 06e3c2c: cold ohr2_boot_mode window stops
`stop=budget pc=0x000a7ac4 instructions=359790038 virtual_time_ns=1879581886`
(clean build, rc=3). All seven scripts red in `make check-era`; each fails
at its artifact-hash or boundary goldens per the attribution above.

## Allowed Files

The seven era scripts named in Acceptance (re-derivation only),
`docs/migration-evidence.md`, `docs/current-status.md`, this ticket.
No engine-source changes: the attribution found none are required.
`Makefile`, registries, profiles, and `plans/index.tsv` stay
integration-owned.

## Frozen Interfaces

Era-script refusal-grep guards stay: a window that reaches a real refusal
or machine reset must still fail the script. Re-derived refusal greps must
match the CURRENT transcript's genuine refusals (none in these windows —
record that in-entry per window), never be deleted. All other profiles'
goldens, deterministic checkpoints, firmware safety.

## Evidence Inputs

The clean-build probe log lineage in `/tmp/sap239-era/ohr2probe/` (volatile;
reproduce per the Attribution Record probe shape — inputs: profile
sapporo-2.39.20, manifest firmware.semu, full-flash fixture sha
37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb, layer
sapporo-2.39-synthetic-wbsto). Twice-reproduced requirement applies to the
re-derived artifact hashes.

## Implementation

For each of the seven scripts: FIRST reproduce its OWN window shape
(cold window cap and the pinned one-instruction resume — read each
script's exact caps; do not substitute a long resume window) twice on the
current clean build, capturing both logs and the snapshot. Then re-pin:
artifact hashes, boundary stop lines, per-window transaction/ordinal/
census lines, and the transcript golden lines to the observed current
values at the same golden positions (a golden whose pinned line reported
`status=refuse` is re-pinned to the CURRENT transcript's line at the same
command/sequence — whatever status the current run actually shows; never
hand-edit a status). Keep every guard and window shape. Run the script
twice (rc=0, byte-identical). Record one evidence entry: "no 2.39 OHR2
device-semantics change in 69b1b35..HEAD; cold relocation attributed to
cd1de52; refuse goldens relocate with the session" with the probe census
above.

## Tests and Commands

The seven era scripts twice each with
`SEMU_EMULATOR/TEST_PROFILE/SEMU_FIRMWARE_MANIFEST/SEMU_SAPPORO_239_FULL_FLASH`
set; `make check-era` moving the seven names red→green (19+7=26 of 43);
`make check-task-contracts` if plans change. No engine change → `make
sanitize` not required.

## Acceptance

gpio_wt1, ohr2_boot_mode, ohr2_bsl_identity, ohr2_echo, ohr2_main_identity,
ohr2_result_13, ohr2_result_14 — all exit 0 twice with re-derived pins;
the evidence entry records the negative attribution with the probe census;
no guard deleted.

## Forbidden Scope

No engine-source change (none is warranted by the attribution), no
weakening of refusal-grep guards, no changes to 2.35 OHR fixture goldens,
no touching the B1/B2-blocked scripts.

## Handoff

Integrator-created 2026-09-27 from the 777 re-pin batch's B3 finding;
attribution completed by the integrator the same day with clean-build
probes (build-dir pollution lesson recorded above).

Completion record 2026-09-27: all seven scripts re-derived and green
twice byte-identically. The re-pin used boundary cap advancement to the
first-transaction-appearance instruction (the ±1 bisection record in
/tmp/sap239-era/notes2.md: e.g. gpio_wt1 cold 359772704→362122619, echo
369037329→371387244, resume cap = cold cap+1 always), cold stop
re-pins, artifact-hash re-pins, and each boundary golden moved from the
one-instruction resume log to first.log, re-pinned to the current
transcript line at the same position (refuse→ok as observed; e.g.
'0x0000 sequence=1 state=BSL status=refuse' →
'0x0010 sequence=0 state=BSL status=ok ready=1'). All refusal-GUARD greps
and census pins (118) byte-identical; every window cap moved stays below
the ticket-796 wall at 442856246. Integrator review: value-only diff
confirmed (56 changed lines; the only structural change is each moved
boundary golden's grep target resume.log→first.log, documented here);
integrator independently ran boot_mode, result_14, main_identity,
result_13 twice each (rc=0, byte-identical artifact assertions inside);
the implementation teammate independently ran all seven twice and
corroborated the caps with 144 bisection probes (probe logs hash
byte-identically to the new log pins).
