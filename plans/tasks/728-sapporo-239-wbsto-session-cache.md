# 728 — Sapporo 2.39 Synthetic WbStorage Session Cache

**Status:** done
**Phase:** 7
**Dependencies:** 706, 727

## Goal

Provide an explicit, exact-build compatibility layer for the four missing
Sapporo `2.39.20.22297-P` WbStorage values and the resulting bounded
`WbStoPreload` failure, then stop at the next fail-closed boundary.

## Execution Budget

One model-day for the evidence contract, native-cache fixture, integration,
strict unit tests, and two authentic-firmware runs.

## Required Reading

Tickets 706 and 727; E-SAP-0011 and E-SAP-0029;
`docs/{architecture,execution-model,testing-strategy,compatibility-policy}.md`;
`include/semu/{bus,compat,machine,manifest}.h`;
`src/compat/{layer,sapporo_222}.*`;
`src/boards/{machine,machine_run,machine_snapshot}.c`; the exact 2.39 profile
and profile test; and the hash-pinned read-only firmware research named below.

## Current Baseline

After the combined CTIMER INTEN fix, the exact production run reaches firmware
`BKPT #0` through `StartupClient.cpp:67`. Read-only tracing attributes the
failed request to `WbStoPreload` command zero: all four native cache lookups
return 204 and the provider reports 500. The compact public OTA resource
fragment omits the earlier personalized filesystem bytes, so neither a native
record nor a coherent writable FAT image is available.

## Allowed Files

- `src/compat/sapporo_239.c`, `src/compat/sapporo_239.h`
- `src/boards/machine.c`, `src/boards/machine_run.c`
- `profiles/sapporo/2.39.20/profile.semu`
- `tests/unit/test_sapporo_239_compat.c`
- `tests/unit/test_sapporo_profile_239.c`
- `tests/integration/test_firmware_sapporo_239_wbsto_cache.sh`
- `docs/current-status.md`, `docs/migration-evidence.md`
- `plans/index.tsv`
- `plans/tasks/728-sapporo-239-wbsto-session-cache.md`

## Frozen Interfaces

Keep the public compatibility, machine, profile, manifest, bus, and snapshot
formats unchanged. The layer is disabled by default, exact-profile and
component-hash pinned, session-local, intervention-hit-bounded, and logged.
Existing 2.22 compatibility behavior and all layer-off 2.39 checkpoints remain
unchanged.

## Evidence Inputs

E-SAP-COMPAT-WBSTO-239-001 must pin the exact startup client/provider state,
four local IDs/descriptors/paths, native cache context and entry layout,
synthetic payload classification, decoder result, and why lower-level storage
is unavailable. It cites the read-only 2.39 startup, cache-identity, cache-map,
value-ABI, and synthetic-runtime research plus the disposable hook source by
SHA-256. E-SAP-0011 supplies the three exact component hashes.

## Implementation

Declare `sapporo-2.39-synthetic-wbsto` in the exact profile. At the native
`WbStoManager` command-zero success callback only, validate the complete empty
cache context and all destination bytes before mutation, then add four native
20-byte index entries and their aligned byte values to the existing bounded
session vectors. Advance only the native vector lengths/end pointer. The four
file-backed preload reads still return their observed native 204 statuses; at
the later exact `WbStoPreload` command-zero callback, translate only its final
500 result to 200 after revalidating the installed cache byte-for-byte. Do not
patch control flow, instructions, individual read results, FAT, or source
images. Any target-state mismatch refuses compatibility without partial
mutation.

## Tests and Commands

`make test TEST_FILTER=sapporo_239_compat`, `make test
TEST_FILTER=sapporo_profile_239`, `make test-firmware
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
TEST_PROFILE=sapporo-2.39.20 TEST_FILTER=sapporo_239_wbsto_cache`,
`make check-lines`, `make check`, and `make sanitize
TEST_FILTER=sapporo_239_compat` must pass. Unit coverage includes disabled,
correct/wrong hash, unrelated callback, wrong native context with no mutation,
exact entry/payload layout, missing-cache translation refusal, per-intervention
budgets, and deterministic event logging.
The private runner requires the exact external fixture hash, two byte-identical
runs, unchanged source flash, exactly one hit for each of the two interventions,
and a checkpoint beyond `StartupClient.cpp:67` or the next explicit refusal.

## Acceptance

The layer-off production checkpoint remains the E-SAP-0029 firmware halt. With
the layer selected, the final preload result is translated only after exact
cache validation; the four entries remain in the native session vectors for
the later lookup/decoder path proven by the reference trace, without further
status replacement. Runs repeat byte-identically and advance to a later
frame/checkpoint or a newly identified fail-closed boundary.

## Forbidden Scope

No recovered-default claim, provisioned-watch claim, guessed `data.jsn` or FAT
bytes, individual-read or non-`WbStoPreload` status translation, instruction
patch, auto-enable, wildcard hash/profile, logical writable-file adapter,
watch-face archive, renderer, device, CPU, MMIO, snapshot-format, firmware-byte,
or unrelated change.

## Handoff

Implemented `sapporo-2.39-synthetic-wbsto`, pinned to profile
`sapporo-2.39.20` and the E-SAP-0011 resident/application/resources hashes.
At callback PC `0x00124844`, the first intervention requires client
`0x10025634`, `WbStoManager` provider `0x001c0ec8`, command zero, status
200, and the exact empty 40-byte cache context at `0x1000021c`. It installs
four 20-byte native entries and 40 aligned arena bytes only after full-range
validation. The second intervention requires `WbStoPreload` provider
`0x001c0ed8`, command zero, native status 500, and a byte-exact recheck of the
installed session cache before changing R3 to 200. Each intervention has a
one-hit budget and stable event.

The values `zwwatc01`, `0`, `{"arrayData":[]}`, and `yellow` are
synthetic session-only values, never recovered defaults or persisted bytes.
The layer is disabled by default, listed only by the exact profile, and all
runtime layer activation now validates descriptor component hashes.

Focused compatibility/profile tests, the exact private firmware runner,
`make check-lines`, `make check`, and focused ASan/UBSan all pass. The
private runner retained the layer-off E-SAP-0029 halt and produced two
byte-identical layer-on logs (SHA-256
`b1156669803cbd2c09e16599fa3719ff2adeecb493eb3749e20fcec34b8f37c0`)
with no reset. The new stop is `unmapped-access` at PC `0x0007038c`,
instruction 78,496,951, virtual time 526,979,533 ns, writing 32 bytes to
`0x0f676e34`. This is the native FAT-cache underflow caused by the compact
OTA fragment's incoherent writable view. A separate exact-build logical
writable-file adapter is required; broad mapping, FAT patching, and invented
persisted contents remain unsupported.
