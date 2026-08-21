# Hardware Coverage Matrix

## Status Definitions

- **unknown**: no trustworthy contract has been recovered.
- **traced**: addresses or transactions are captured, but behavior is not modeled.
- **fixture-backed**: a narrow compatibility fixture reproduces a pinned exchange.
- **functional**: modeled behavior passes positive, reset, and refusal tests.
- **verified**: authentic firmware reaches the documented checkpoint and matches repeatable traces or goldens.

Only evidence recorded in `docs/migration-evidence.md` may advance a component beyond unknown. “Implemented” is not a status; use the testable definitions above.

## Current Matrix

| Product / component | Planned evidence | Current status | Verification gate |
| --- | --- | --- | --- |
| ARMv7E-M core | E-CPU-0001..0010, synthetic guests, and E-SAP-0008/0014 runs | verified | CPU families, exceptions, NVIC/SysTick, sleep, DSP, FPU, RTOS guest, refusals, and repeatable authentic checkpoints pass |
| Apollo4 clock/power/reset | E-A4-CLK/PWR/RST-001 and E-SAP-0015/0016 | functional | Narrow evidenced banks and reset ordering pass; native post-`SYSRESETREQ` controller semantics remain unresolved |
| Apollo4 GPIO/timers/STIMER | E-A4-GPIO/TIMER/STIMER-001 and E-SAP-LIVE-0001 | verified | register, IRQ, WFI-wake, button-edge, reset, and refusal tests pass in repeatable runs |
| Apollo4 UART/IOM/MSPI/DMA/MRAM | E-A4-UART/IOM/MSPI/DMA/MRAM-001 and E-SAP-0008 | verified | controller transcripts, atomic bounds/refusals, GPS, flash, and renderer traffic reach repeatable checkpoints |
| Sapporo external flash | E-SAP-FLASH-001 native MSPI2 boundary plus model | verified | ID/status/read, write-enable, page-program, sector-erase, overlay, refusal, and authentic resource traffic pass |
| Sapporo pressure sensor | observed bus transcript | functional | Identity and wrong-address refusal tests pass |
| Sapporo OHR2 | observed transport; optional fixture | fixture-backed | CRC-framed identity and unknown-command refusal tests pass; native startup remains |
| Sapporo LSM6DSL | observed bus transcript | functional | WHO_AM_I and framing tests pass; IRQ/FIFO expansion remains |
| Sapporo wrist magnetometer | E-SAP-TLI493D-001 | functional | identity, configuration, reset, boundary, and refusal tests pass |
| Sapporo haptic PMIC | E-SAP-HAPTIC-001 | functional | command/state, reset, repeatability, snapshot, and refusal tests pass |
| Sapporo ambient light | E-SAP-OPT3007-001 | functional | sample/configuration, reset, boundary, and refusal tests pass |
| Sapporo fuel gauge | E-SAP-MAX17050-001 | functional | startup/status, byte-order, reset, and refusal tests pass |
| Sapporo GPS UART | E-SAP-CXD5610-001 and E-SAP-COMPAT-GPS-001/004 | fixture-backed | bounded startup exchange reaches the authentic GPS checkpoint; unsupported exchanges refuse |
| Sapporo Nema/renderer 240x240 | E-NEMA-*-001, E-NEMA-TSC6A-001, and E-SAP-0002..0004 | verified | three declared private renderer goldens and the observed TSC6A transition forms pass; physical panel remains unresolved |
| Sapporo buttons/backlight | E-SAP-BUTTONS/BACKLIGHT-001 and E-SAP-LIVE-0001 | functional | three-button replay/live input and the first language/profile transitions pass; full OTA-only onboarding remains at the evidenced reset boundary |
| Sapporo 2.33/2.35/2.39 component contracts | SOF extraction manifests and bounded reset/OHR diagnostics (E-SAP-0009/0010/0011/0012/0015/0016) | traced | exact component hashes and vector tables verified; 2.33 has repeatable bounded reset diagnostics, while 2.35/2.39 still need profiles and traces |
| Ulsan platform | future exact profiles/traces | unknown | native display transaction obtained |
| Wismar platform | future exact profiles/traces | unknown | stable reset, then native display |
| Tianjin/Rostock/Xiamen | future firmware and traces | unknown | product-specific gate not yet opened |

## Update Rules

Each status change must cite tests and one or more evidence IDs. A shared device implementation receives a separate row per board when wiring, reset state, or firmware-visible behavior differs. Do not infer coverage from a shared Apollo4 name.
