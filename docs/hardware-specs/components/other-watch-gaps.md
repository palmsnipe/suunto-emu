# Other-watch gaps and next evidence targets

## Rostock — Suunto Vertical

Five exact firmware objects are retained in the evidence repository, but no
board-level component map has been promoted. The next useful evidence is a
bounded cold-boot trace that identifies the storage controller, display path,
GNSS, pressure, IMU, OHR, magnetic, light, and battery endpoints independently.

## Tianjin — Suunto Race

One exact rollback firmware object and an external-flash resource tail are
known. The resource tail begins at logical address `0x00040000`; this establishes
a storage boundary, not a component ordering code or full board BOM.

## Ulsan — Suunto Race S

Ulsan is the most complete non-Sapporo lane. Public or firmware-backed
identities include Apollo4 Plus, HSPPAD148A, LSM6DSOX, LIS2MDL sensor-hub
traffic, OHR2 transport, MAX17050-compatible fuel-gauge traffic, a Macronix-
compatible MSPI1 device, SDIO/eMMC host traffic, and NemaDC display setup.
The exact HSPPAD148A public specification and the physical eMMC, OHR, PMIC,
GNSS package, and panel still need separate collection or board evidence.

## Wismar — Suunto Race 2

The native boundary reaches an Apollo4 platform, a MAX20360 power path, and a
32-MiB external-flash address aperture. Sensor endpoints included by current
diagnostic probes are deliberately classified as boundary reuse only.

## Xiamen — Suunto Vertical 2

Xiamen remains an acquisition target. No exact public SOF or board-specific
component identity is promoted. Do not copy the Sapporo or Ulsan inventory into
this lane by analogy.
