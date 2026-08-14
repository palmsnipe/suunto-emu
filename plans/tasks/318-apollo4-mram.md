# 318 — Apollo4 MRAM Controller

**Status:** blocked
**Phase:** 3
**Dependencies:** 285, 295, 298

## Goal

Replace the MRAM placeholder with only trace-verified control/status offsets. This supplies the final optional controller block to 320; MRAM array/storage semantics remain deferred.

## Execution Budget

Complete within the stated one-to-three agent-day budget: Replace the generic MRAM register bank with only verified control/status behavior.

## Required Reading

`docs/architecture.md`, `docs/execution-model.md`, the ticket-specific evidence row from 295, and every baseline source named below.

## Current Baseline

`apollo4.c` maps `0x40014000/0x1000` and retains arbitrary values at offsets `0x00/0x04/0x08`. `suunto-sapporo.repl:mram_ctrl` accepts every offset with read-zero/retain semantics, explicitly a placeholder rather than hardware evidence.

## Allowed Files

Only `src/soc/apollo4/{mram.c,mram.h}` and `tests/devices/test_apollo4_mram.c`.

## Frozen Interfaces

Opaque create/destroy/reset and checked bus ops. MRAM control never owns code memory bytes; it reports control/status only. Unknown offset/width/bit refuses without latching.

## Evidence Inputs

`E-A4-MRAM-001` must include a Sapporo 2.22.60 authentic MMIO trace for every implemented offset, reset/read/write mask, and polling result, and cite `suunto-sapporo.repl:mram_ctrl` only as the prior gap. If no trace exists, ticket remains blocked and generic bank must not be promoted.

## Implementation

Implement only evidence-enumerated behavior behind the frozen interface; validate the complete operation before mutation and split any hand-written file approaching 300 lines.

## Tests and Commands

`make test TEST_FILTER=apollo4_mram` runs only its binary and exits 0; evidenced transcript, reset, write mask, wrong width, offset `0x0c`, and repeatability pass. `make check` exits 0.

## Acceptance

Only traced offsets exist; reset/status values match evidence; controller writes cannot alter mapped firmware; the test explicitly proves an unknown offset refuses.

## Forbidden Scope

No MRAM array/storage implementation, permissive retained offsets, fabricated ready status, flash semantics, code patching, or integration/public edit.

## Handoff

Report verified offsets or a blocked/no-evidence result, trace hash, tests, and 320 disposition.
