# Ambiq Apollo4-family MCU

## Public specification

The downloaded Ambiq Apollo4 Plus datasheet describes a Cortex-M4F SoC with
integrated memory, multiple I2C/SPI masters, UARTs, serial-memory interfaces,
display interfaces, and a low-power 2D/2.5D graphics block. The Armv7-M manual
is included as the architectural reference for the CPU execution model.

- [Apollo4 Plus datasheet](../vendor/ambiq-apollo4-plus-datasheet.pdf)
- [Apollo4 Plus product page](https://ambiq.com/product/apollo4-plus/)
- [Armv7-M Architecture Reference Manual](../vendor/armv7-m-architecture-reference-manual.pdf)

These documents describe the silicon family. They do not identify the exact
Suunto package, board revision, clock tree, power rails, or attached devices.

## Emulator-pinned interfaces

The Sapporo firmware evidence pins the following controller locations:

| Controller | Base | IRQ | Evidence |
| --- | ---: | ---: | --- |
| UART1 | `0x4001d000` | 16 | `E-A4-UART-001` |
| IOM0..IOM6 | `0x40050000 + n*0x1000` | 6..12 | `E-A4-IOM-001` |
| MSPI1 | `0x40061000` | 21 | `E-A4-MSPI-001` |
| MSPI2 | `0x40062000` | 22 | `E-A4-MSPI-001` |
| NEMA GPU | `0x40090000` | 28 | `E-NEMA-RING-001` |

These are firmware and platform observations, not values copied from a vendor
datasheet. Unobserved registers remain unsupported by design.

## Watch-specific status

- Sapporo: Apollo4-family platform is confirmed; exact SKU and package remain
  unproven.
- Ulsan: the firmware explicitly requires Apollo4 Plus extended SRAM apertures.
- Wismar: the profile starts from Apollo4 and adds only Wismar-proven address
  space.
- Rostock, Tianjin, and Xiamen: no board-level MCU/package promotion yet.
