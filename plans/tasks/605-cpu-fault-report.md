# 605 — CPU and Refusal Reports

**Status:** blocked
**Phase:** 6
**Dependencies:** 600

## Goal

Produce bounded deterministic reports for CPU faults, unmapped MMIO, refused
device transactions, firmware assertions, and compatibility refusals.

## Execution Budget

One to two model-days; at most three report source/test files.

## Required Reading

`include/semu/{cpu,types,trace}.h`, `src/cpu/armv7m/cpu.c`, device transaction
contracts, and ticket 600 handoff.

## Current Baseline

Stop reasons and one fault instruction are exposed. Reports lack register
snapshots, fault address/access metadata, bounded history, and stable text.

## Allowed Files

`src/core/{report,report_cpu}.c`, `tests/unit/test_fault_report.c`, and additions
to `include/semu/trace.h` reserved by ticket 600's handoff.

## Frozen Interfaces

Reports borrow immutable trace history and CPU inspection state. They never
read guest memory after the fault or mutate stop state.

## Evidence Inputs

Synthetic faults only. If CPU inspection fields required by the report are not
in the Phase 2 frozen contract, request an integration change and stop.

## Implementation

Normalize registers, exception/fault fields, access width/direction/address,
device endpoint, and a bounded preceding history. Redact paths and raw firmware.

## Tests and Commands

`make test TEST_FILTER=fault_report` selects `test_fault_report` and exits 0 for
golden text covering each stop class and history truncation. Run
`make check-lines && make check && make sanitize`.

## Acceptance

Repeated reports are byte-identical and distinguish every requested stop class
without leaking host paths or firmware bytes.

## Forbidden Scope

No debugger, recovery, snapshot, permissive retry, or fault semantic changes.

## Handoff

Report stable fields, redaction rules, goldens, and missing inspection fields.
