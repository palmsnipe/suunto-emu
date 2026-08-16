# Hardware Coverage Matrix

## Status Definitions

- **unknown**: no trustworthy contract has been recovered.
- **traced**: addresses or transactions are captured, but behavior is not modeled.
- **fixture-backed**: a narrow compatibility fixture reproduces a pinned exchange.
- **functional**: modeled behavior passes positive, reset, and refusal tests.
- **verified**: authentic firmware reaches the documented checkpoint and matches repeatable traces or goldens.

Only evidence recorded in `docs/migration-evidence.md` may advance a component beyond unknown. “Implemented” is not a status; use the testable definitions above.

## Initial Matrix

| Product / component | Planned evidence | Current status | Verification gate |
| --- | --- | --- | --- |
| ARMv7E-M core | architecture vectors and synthetic guests | traced | Core Thumb subset and two translator regressions pass; RTOS exceptions/FPU remain gated |
| Apollo4 clock/power/reset | firmware traces and prior emulator behavior | traced | Narrow clock/power register banks pass refusal tests; native reset transcript remains |
| Apollo4 GPIO/timers/STIMER | firmware traces | unknown | register, IRQ, and WFI-wake tests pass |
| Apollo4 UART/IOM/MSPI/DMA/MRAM | firmware traces | unknown | controller transcript and bounds tests pass |
| Sapporo external flash | native MSPI2 boundary plus model | functional | ID/status/read, write-enable, page-program, sector-erase, overlay, and refusal tests pass |
| Sapporo pressure sensor | observed bus transcript | functional | Identity and wrong-address refusal tests pass |
| Sapporo OHR2 | observed transport; optional fixture | fixture-backed | CRC-framed identity and unknown-command refusal tests pass; native startup remains |
| Sapporo LSM6DSL | observed bus transcript | functional | WHO_AM_I and framing tests pass; IRQ/FIFO expansion remains |
| Sapporo wrist magnetometer | observed bus transcript | unknown | identity/config/refusal tests pass |
| Sapporo haptic PMIC | observed bus transcript | unknown | command/state tests pass |
| Sapporo ambient light | observed bus transcript | unknown | sample/config tests pass |
| Sapporo fuel gauge | observed bus transcript | unknown | startup/status tests pass |
| Sapporo GPS UART | observed transcript; optional fixture | unknown | bounded startup exchange passes |
| Sapporo Nema/panel 240x240 | command traces and frame hashes | unknown | all three private frame goldens pass |
| Sapporo buttons/backlight | board traces | unknown | three-button input and panel-state tests pass |
| Sapporo 2.33/2.35/2.39 component contracts | SOF extraction manifests (E-SAP-0009/0010/0011/0012) | traced | exact component hashes and vector tables verified; bounded emulator traces blocked (no profiles) |
| Ulsan platform | future exact profiles/traces | unknown | native display transaction obtained |
| Wismar platform | future exact profiles/traces | unknown | stable reset, then native display |
| Tianjin/Rostock/Xiamen | future firmware and traces | unknown | product-specific gate not yet opened |

## Update Rules

Each status change must cite tests and one or more evidence IDs. A shared device implementation receives a separate row per board when wiring, reset state, or firmware-visible behavior differs. Do not infer coverage from a shared Apollo4 name.
