# Migration Evidence Ledger

## Purpose and Format

This ledger records facts migrated from `suunto-firmware`, firmware traces, documentation, and synthetic experiments. It contains summaries and hashes, never copyrighted firmware bytes. Each implementation and hardware status change cites stable evidence IDs from this file.

Each future entry must contain: ID, date, source kind and location, product/firmware hashes, observation, confidence, affected modules, validation test, and unresolved questions. Local-only source locations must be described symbolically, such as `$FIRMWARE_ROOT`, rather than with a user path.

## Seed Evidence

| ID | Source | Product/version | Observation | Confidence / validation |
| --- | --- | --- | --- | --- |
| E-SAP-0001 | project target contract | Sapporo `2.22.60.3383-P` | Initial display is 240x240 RGB565 and board has three physical buttons. | Target assumption; validate in private integration tests. |
| E-SAP-0002 | known native frame | Sapporo `2.22.60.3383-P` | Normal frame is 115200 bytes, SHA-256 `8503ffbde124e35f914b09eea858ffcda2fc4bc453d3f9c7eb3e88388621d9cc`. | Golden; reproduce twice. |
| E-SAP-0003 | known native frame | Sapporo `2.22.60.3383-P` | Middle-button language frame SHA-256 is `68a4126a8f908e9dd7c5703982c6fe141e6cc89cc383ee0d9d6d502eeadcb2d9`. | Golden; reproduce through semantic input. |
| E-SAP-0004 | known native frame | Sapporo `2.22.60.3383-P` | Lower-button transition SHA-256 is `dcec235c8b450c96356b27b49306026ab9d14e7626714cdacb8bf3737a623ad3`. | Golden; reproduce through semantic input. |
| E-CPU-0001 | prior Renode workaround report | ARMv7E-M firmware paths | `MOV.W r0,sp` and `STMDB` are valid instructions and must be CPU regressions, not compatibility hooks. | Validate with architectural instruction vectors. |
| E-COMPAT-0001 | recovered native record builder and research notes | Sapporo `2.22.60.3383-P` | Missing manufacturing state is represented by an opt-in, session-local `sapporo-2.22-no-device` layer with ProductionData, ACCR, ACCC, MAGN, and HLAT records. | Fixture-backed; generated from the firmware's CRC table and covered by provenance and hit-budget checks. |
| E-SAP-0005 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Resident component loads at `0x00019000`, size 71504, SHA-256 `a409b088a061c2fe61689c8f39a79b2c35ed0059cd66987646e0195e47a2f522`. | Exact profile metadata; checked before mapping. |
| E-SAP-0006 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Application loads at `0x00040000`, size 1493234, SHA-256 `c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc`. | Exact profile metadata; checked before mapping. |
| E-SAP-0007 | validated SOF extraction manifest | Sapporo `2.22.60.3383-P` | Resource component maps at `0x14000000`, size 16519168, SHA-256 `ec2a4b1c472844ac6ff9cc575cb9383c29a74302107af3619f9a08fdf6abcaf1`. | Exact profile metadata; checked before mapping. |
| E-SAP-0008 | bounded local authentic-firmware run | Sapporo `2.22.60.3383-P` | With `sapporo-2.22-no-device`, two 10,000,000-instruction runs end identically at the default-handler loop at `PC=0x001a2434`, virtual time 10,000,000 ns, with one declared layer hit. | Reproducible bring-up boundary, not a startup or display gate; next work must identify the preceding architectural fault or missing controller behavior. |

## Migration Procedure

Inspect `suunto-firmware` behavior and traces without copying large Renode classes literally. Reduce each fact to a register contract, bus transcript, memory-map entry, or algorithm; cite its source file/symbol or trace hash; add focused positive and refusal tests; then update the corresponding matrix status.

Guesses remain labeled hypotheses and belong in compatibility work until authentic traffic validates them. Evidence that conflicts with an existing entry creates a new entry and explains the superseded assumption; do not silently rewrite history.
