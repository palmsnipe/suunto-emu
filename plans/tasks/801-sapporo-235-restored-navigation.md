# 801 — Sapporo 2.35 Restored Navigation Evidence and Regression

**Status:** ready
**Phase:** 7
**Dependencies:** 705,791,793,794

## Goal

Distinguish a static widget-pinning screen from a GPU repaint freeze and pin
individual button actions plus return-to-watchface behavior. The owner assigned
this investigation on 2026-10-02 after ticket 799 restored complete renderer
state. Existing multi-click navigation windows remain historical controls.

## Execution Budget

One bounded evidence and tooling slice. Raw traces and screenshots stay in an
external volatile workspace. No runtime behavior changes.

## Required Reading

README, current status, architecture, execution model, testing strategy,
compatibility policy; E-SAP-BUTTONS-001, E-EMU-RENDERER-SNAPSHOT-002,
E-SAP-0041-EXT3; tools/test_sdl_sapporo_235_nav.sh,
tools/test_sdl_sapporo_235_restore.sh, src/frontends/sdl_live_test.c,
src/frontends/sdl_button_hold.c, src/frontends/live_frame_gate.c,
include/semu/machine.h, src/frontends/sdl_coalesce_glue.c.

## Current Baseline

The old cold LOWER window ends at frame 4131 / CRC 7ef957e9, the static widget
pinning prompt. A normal lower click from the restored watchface reaches Widgets;
Middle enters the pinning prompt. The cold script sends Middle at step 25 before
Lower at step 26. Its label therefore does not describe an isolated lower click.

## Allowed Files

- tools/test_sdl_sapporo_235_navigation_restore.sh (new)
- tools/test_sdl_sapporo_235_nav.sh (comments and result label only)
- README.md
- docs/current-status.md
- docs/migration-evidence.md

Planning setup owns this ticket and its new plans/index.tsv row. Implementation
must leave statuses and the original cold navigation pins unchanged.

## Frozen Interfaces

All C code, public interfaces, runtime semantics, profiles, compatibility
fixtures, snapshot encoding, existing transcript/snapshot expectations.

## Evidence Inputs

Existing button dispatch and snapshot contracts. Twice-identical bounded runs
establish emulator regression observations only. Inspect actual captured pixels;
do not claim physical-device equivalence or infer unsupported input semantics.

## Implementation

Capture independent upper/middle/lower presses, a Middle round trip, widget
browsing, and return from the old static prompt. Preserve the old cold windows
and attribute their input sequence. Add an optional firmware runner that
validates all components and pins paired snapshots and execution checkpoints.
Correct the misleading global stall/inert labels in user-facing documentation.
Record the actual OHR refusal sequence separately for subsequent evidence work.

## Tests and Commands

- Validate the private 2.35 manifest before every run set.
- Repeat each new input window twice with instruction, virtual-time and wall
  bounds; require identical complete logs and snapshots.
- sh tools/test_sdl_sapporo_235_navigation_restore.sh
- sh tools/test_sdl_sapporo_235_nav.sh (or exact paired windows, retaining logs)
- make check-task-contracts, make check-lines, make check
- Shell syntax and git diff --check.

## Acceptance

Paired evidence attributes the old static screen, demonstrates bounded input
responsiveness and returning clock updates, and the new runner retains exact
checkpoints. Missing private data skips; explicit missing or mismatched data
fails. No legacy golden is weakened or silently replaced.

## Forbidden Scope

No guessed GPU fix, extra sensor responses, fixture-budget increase, new texture
law, screenshot Git exception, or 2.39 golden update.

## Handoff

Report exact input windows, terminal tuples, frame and snapshot hashes, raw-log
and probe hashes, files and commands, plus remaining OHR/GPU evidence gaps.
Leave ready for integrator review.
