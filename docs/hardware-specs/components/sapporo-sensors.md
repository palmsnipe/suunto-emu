# Sapporo sensor and power components

The following are the strongest Sapporo component identities. Bus addresses
and register values come from authentic-firmware boundary traces in
`plans/evidence/phase3-5.tsv`; sample values in the emulator are deterministic
fixtures and are not physical measurements.

| Function | Component | Observed connection | Public material |
| --- | --- | --- | --- |
| Pressure | Alps Alpine HSPPAD143 family | IOM2 I2C `0x48`; identity `0x49`; variant `0xe0` | [HSPPAD143A PDF](../vendor/alps-hsppad143a-datasheet.pdf), [HSPPAD143A page](https://tech.alpsalpine.com/j/products/detail/HSPPAD143A/) |
| Inertial | ST LSM6DSL | IOM0 SPI chip-select 0; `WHO_AM_I=0x6a` | [LSM6DSL datasheet](https://www.st.com/resource/en/datasheet/lsm6dsl.pdf), [product page](https://www.st.com/en/mems-and-sensors/lsm6dsl.html) |
| Magnetic | Infineon TLI493D-W2BW family | IOM2 I2C `0x35` | [TLI493D PDF](../vendor/infineon-tli493d-w2bw-datasheet.pdf) |
| Ambient light | TI OPT3007 | IOM3 I2C `0x45` | [OPT3007 PDF](../vendor/ti-opt3007-datasheet.pdf), [product page](https://www.ti.com/product/OPT3007) |
| Battery gauge | MAX17050 | IOM4 I2C `0x36` | [MAX17050 family PDF](https://www.analog.com/media/en/technical-documentation/data-sheets/MAX17047-MAX17050.pdf), [product page](https://www.analog.com/en/products/max17050.html) |
| Optical HR | OHR2 transport | IOM2 address `0x10`; ready GPIO62 | No public silicon identity found |
| Haptic | Haptic/PMIC endpoint | IOM4 address `0x50` | Exact IC identity not found |
| Board power | Observed endpoint | IOM4 address `0x28` | Exact IC identity not found |

## Important identity limits

`HSPPAD143`, `CXD5610`, and `TLI493D-W2BW` are family-level names where the
firmware does not prove every suffix. The emulator should keep the family
identity and observed protocol separate from a guessed ordering code.

Ulsan-specific parts are documented separately in
[`ulsan-sensors.md`](ulsan-sensors.md); in particular, Ulsan uses HSPPAD148A
and LSM6DSOX rather than the Sapporo pressure/IMU identities.
