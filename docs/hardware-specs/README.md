# Suunto watch hardware and public specifications

This directory is the evidence-scoped hardware inventory for the watch
families relevant to the emulator. It records what is known from firmware,
Renode boundary traces, public vendor documentation, and explicit unknowns. It
is not a claim that every listed identity has been confirmed by a physical
teardown.

Inventory date: 2026-08-18.

## Watch families

The retained firmware archive identifies six families and their firmware
codenames:

| Product family | Codename | Hardware inventory status |
| --- | --- | --- |
| Suunto 9 Peak Pro | Sapporo | Detailed sensor, bus, and storage inventory |
| Suunto Vertical | Rostock | Family identity known; board BOM unresolved |
| Suunto Race | Tianjin | Family identity and external-flash boundary known; board BOM unresolved |
| Suunto Race S | Ulsan | Apollo4 Plus platform and several device identities observed |
| Suunto Race 2 | Wismar | Apollo4 platform and power/storage boundary observed; board BOM partly unresolved |
| Suunto Vertical 2 | Xiamen | Acquisition target; no exact public hardware inventory yet |

The machine-readable list in [`watch-components.tsv`](watch-components.tsv) is
authoritative. A row marked `confirmed` means the part name or interface is
pinned by firmware evidence; it does not mean that a physical package or board
revision was inspected. `platform-only` means the memory map or controller is
known but the attached board part is not. `boundary-reuse-only` means a
diagnostic profile reused an endpoint and must not be promoted to a BOM claim.

## Public documents

The [`sources.tsv`](sources.tsv) file records the official URL, retrieval date,
local cache name, and SHA-256 for every document successfully downloaded.
Downloaded PDFs live under `vendor/` for local investigation but are ignored
by Git because they are vendor-copyrighted and may be large. The source URLs,
hashes, and notes are committed so the cache can be rebuilt.

The component notes intentionally distinguish vendor specifications from
Suunto-specific observations. The latter are linked to the read-only evidence
repository at `../suunto-firmware`; no firmware or proprietary frame pixels are
copied into this repository.

## Current conclusions

- Sapporo is the only watch family with a detailed, emulator-backed device
  inventory in this repository.
- Ulsan has the strongest independent second-family evidence: Apollo4 Plus,
  HSPPAD148A, LSM6DSOX, LIS2MDL sensor-hub traffic, OHR2 transport,
  MAX17050-compatible fuel-gauge traffic, SDIO/eMMC host traffic, and a
  Macronix-compatible external-flash identity.
- Wismar's current probes reach a MAX20360 power path and external-flash
  boundary, but the reused sensor endpoints are not enough to establish its
  physical BOM.
- The Sapporo and Ulsan display controllers are observed, while the physical
  panel controller and wire protocol remain unresolved. NEMA renderer output
  must not be described as physical-panel evidence.
- Sony's public CXD5610 datasheet download requires a registration form. The
  public product page is retained as the available specification source; the
  private download is not bypassed.
