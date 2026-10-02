# Hardware Coverage Matrix

## Status Definitions

- **unknown**: no trustworthy contract has been recovered.
- **traced**: addresses or transactions are captured, but behavior is not modeled.
- **fixture-backed**: a narrow compatibility fixture reproduces a pinned exchange.
- **functional**: modeled behavior passes positive, reset, and refusal tests.
- **verified**: authentic firmware reaches the documented checkpoint and matches repeatable traces or goldens.

Only evidence recorded in `docs/migration-evidence.md` may advance a component beyond unknown. “Implemented” is not a status; use the testable definitions above.

For the 2026-10-02 firmware-by-firmware review, fresh test results, and the
open 2.22 snapshot-golden discrepancy, see
[current status](current-status.md#sapporo-review--2026-10-02). Verification
below applies only to the named bounded windows, not complete watch behavior.

## Current Matrix

| Product / component | Planned evidence | Current status | Verification gate |
| --- | --- | --- | --- |
| ARMv7E-M core | E-CPU-0001..0010, synthetic guests, and E-SAP-0008/0014 runs | verified | CPU families, exceptions, NVIC/SysTick, sleep, DSP, FPU, RTOS guest, refusals, and repeatable authentic checkpoints pass |
| Apollo4 clock/power/reset | E-A4-CLK/PWR/RST-001 and E-SAP-0015/0016 | functional | Narrow evidenced banks and reset ordering pass; native post-`SYSRESETREQ` controller semantics remain unresolved |
| Apollo4 GPIO/timers/STIMER | E-A4-GPIO/TIMER/STIMER-001 and E-SAP-LIVE-0001 | verified | register, IRQ, WFI-wake, button-edge, reset, and refusal tests pass in repeatable runs |
| Apollo4 UART/IOM/MSPI/DMA/MRAM | E-A4-UART/IOM/MSPI/DMA/MRAM-001 and E-SAP-0008 | verified | controller transcripts, atomic bounds/refusals, GPS, flash, and renderer traffic reach repeatable checkpoints |
| Sapporo external flash | E-SAP-FLASH-001 native MSPI2 boundary plus model | verified | ID/status/read, write-enable, page-program, sector-erase, overlay, refusal, and authentic resource traffic pass |
| Sapporo pressure sensor | observed bus transcript | functional | Identity and wrong-address refusal tests pass |
| Sapporo OHR2 | E-SAP-0041 and EXT3; version-pinned transport fixtures | fixture-backed | Native startup and bounded 2.35 MAIN polling pass; upper-button navigation still reaches an OHR fixture refusal; real heart-rate sensing and arbitrary exchanges remain unsupported |
| Sapporo LSM6DSL | observed bus transcript | functional | WHO_AM_I and framing tests pass; IRQ/FIFO expansion remains |
| Sapporo wrist magnetometer | E-SAP-TLI493D-001 | functional | identity, configuration, reset, boundary, and refusal tests pass |
| Sapporo haptic PMIC | E-SAP-HAPTIC-001 | functional | command/state, reset, repeatability, snapshot, and refusal tests pass |
| Sapporo ambient light | E-SAP-OPT3007-001 | functional | sample/configuration, reset, boundary, and refusal tests pass |
| Sapporo fuel gauge | E-SAP-MAX17050-001 | functional | startup/status, byte-order, reset, and refusal tests pass |
| Sapporo GPS UART | E-SAP-CXD5610-001 and E-SAP-COMPAT-GPS-001/004 | fixture-backed | bounded startup exchange reaches the authentic GPS checkpoint; unsupported exchanges refuse |
| Sapporo Nema/renderer 240x240 | E-NEMA-*-001, E-RE-SAP235-TSC6A-001, E-SAP-0041-EXT7, E-EMU-SAP235-TICKTRAIL-002 | verified (bounded frames) | Native setup/menu/watchface frames and observed compressed-icon/scroll families render; the 2.35 seconds-hand trail is fixed. Auxiliary-bit blocks and multi-resolve writeback remain incomplete; physical-panel equivalence is unverified |
| Sapporo buttons/backlight | E-SAP-BUTTONS/BACKLIGHT-001, E-SAP-LIVE-0001, E-EMU-SAPPORO-BRANCH-GATES-001, E-SAP-0041-EXT3 | functional | Three-button delivery drives 2.22 onboarding and menu selection; 2.35 upper navigation is observed, lower repainting stalls, and middle is inert in the main-screen window. This does not establish complete menu or phone-pairing support |
| Sapporo 2.33.16 | Exact profile and E-EMU-SAP233-GAUGE-FIXTURE-001 | verified (early boot) | The former gauge-driven first fault is absent in the paired bounded boot record; no full UI acceptance |
| Sapporo 2.35.34 | Exact profile, E-SAP-0041-EXT3/EXT7, E-EMU-SAP235-GPSRESTORE-001, E-EMU-SAP235-TICKTRAIL-002 | verified (bounded UI) | Setup through Done, ticking watchface, and paired interactive restore pass with five explicit compatibility layers; navigation and long-session limits remain |
| Sapporo 2.39.20 | Exact profile, E-SAP239-DEEPCLEAN-001, E-SAP239-REPINSWEEP-002, E-EMU-SAP235-TICKTRAIL-002 | verified (bounded gates) | Recorded 43-script era census is green, including expected refusals; full-flash fixture required to reproduce it. Complete setup/watchface/menu release and ongoing GPS remain open |
| Ulsan platform | E-ULS-0001..0006 (SOF extraction manifests, vector tables, two-version cross comparison, bounded reference-lane boundary runs, reset wiring contract) | traced | Exact component/hash/load/vector contracts verified byte-for-byte and strictly parsed; bounded reset boundary reproduced byte-identically on the read-only reference lane; NemaDC IDREG `0x87452365` and 466x466 frame contract cited; in-tree reset profile `ulsan-2.35.36` registered with isolated Apollo4 Plus memory map and a reproduced private bounded run (ticket 725); modeled display/SDIO/crown/touch behavior and device goldens remain absent (tickets 730/740) |
| Wismar platform | future exact profiles/traces | unknown | stable reset, then native display |
| Tianjin/Rostock/Xiamen | future firmware and traces | unknown | product-specific gate not yet opened |

## Update Rules

Each status change must cite tests and one or more evidence IDs. A shared device implementation receives a separate row per board when wiring, reset state, or firmware-visible behavior differs. Do not infer coverage from a shared Apollo4 name.
