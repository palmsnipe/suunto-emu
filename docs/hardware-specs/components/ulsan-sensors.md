# Ulsan-specific sensors and power silicon

Ulsan is the first independent family for which the firmware evidence names a
different pressure sensor and IMU. The public documents below describe the
parts; the bus connections and sensor-hub details come from the retained Ulsan
profile.

| Function | Part | Public facts | Ulsan observation |
| --- | --- | --- | --- |
| Pressure | Alps Alpine HSPPAD148A | Waterproof absolute pressure sensor, I2C, 30–3200 kPa, 1.7–3.6 V | IOM4 I2C address `0x28`; identity and ready behavior modeled in `UlsanHsppad148A.cs` |
| IMU | ST LSM6DSOX | 3-axis accelerometer + 3-axis gyroscope, SPI/I2C/I3C, 9-KB FIFO, ±2/4/8/16 g and ±125/250/500/1000/2000 dps | IOM0 SPI; native sensor-hub configuration is observed |
| Magnetometer | ST LIS2MDL | 3-axis magnetometer, ±50 gauss, I2C/SPI, 1.71–3.6 V | Connected through LSM6DSOX sensor hub at observed address `0x3d` and sub-address `0x4f` |
| Power / charger / haptic candidate | Analog Devices / Maxim MAX20360 | Wearable PMIC with charger, fuel gauge, regulators, LED sinks, and ERM/LRA driver | Not promoted as Ulsan identity; Wismar native startup is the family with the explicit MAX20360 boundary |

Public documents:

- [HSPPAD148A datasheet](../vendor/alps-hsppad148a-datasheet.pdf)
- [HSPPAD148A product page](https://tech.alpsalpine.com/e/products/detail/HSPPAD148A/)
- [LSM6DSOX datasheet](https://www.st.com/resource/en/datasheet/lsm6dsox.pdf)
- [LSM6DSOX product page](https://www.st.com/en/mems-and-sensors/lsm6dsox.html)
- [LIS2MDL datasheet](https://www.st.com/resource/en/datasheet/lis2mdl.pdf)
- [LIS2MDL product page](https://www.st.com/en/mems-and-sensors/lis2mdl.html)
- [MAX20360 product page](https://www.analog.com/en/products/max20360.html)
- [MAX20360 datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/max20360.pdf)

The MAX20360 row is deliberately a public-spec candidate, not a Ulsan BOM
claim. Wismar's native startup is the current evidence that promotes that part
number for a watch family.
