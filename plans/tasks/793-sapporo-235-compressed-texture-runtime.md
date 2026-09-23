# 793 — Sapporo 2.35 TSC6A Compressed Texture Runtime Integration

**Status:** done
**Phase:** 7
**Dependencies:** 504,513,761,788

## Goal

Render the one observed native compressed TSC6A draw (the 60x60 compass
crosshair refusing at 2.35 main entry) by adding a capture-pinned acceptance
state and a pure deterministic block expansion to the in-tree renderer, so the
main-screen icon draws instead of faulting. Only the E-RE-SAP235-TSC6A-001
observed tuple is authorized; every other compressed state keeps refusing.

## Execution Budget

Two to three model-days. Expected: one new expansion file
(`nema_tsc6a_expand.c`), a validation-branch edit in `nema_tsc6a_raster.c`, one
unit module, one private-firmware runner. No fixture bytes in Git.

## Required Reading

`docs/migration-evidence.md` E-RE-SAP235-TSC6A-001 (block law, acceptance
tuple, ambiguities), E-EMU-SAP235-MAIN-TSC6A-001 (refusal tuple being
replaced), E-NEMA-RGBA4444-001 (observed SRC_OVER blend); `/tmp/sap235-tex17/
INTEGRATION-NOTE.md` and `DERIVATION.md`; `src/display/nema_tsc6a_raster.c`,
`nema_tsc6a.c`, `nema_tsc6a.h`, `nema_tsc6a_internal.h`, `nema_rgba4444.c`
(validate-then-mutate + matrix helpers), `nema_backend_draw.c` (routing),
`tests/unit/test_nema_tsc6a.c`.

## Current Baseline

`nema_tsc6a_resolve_mask()` accepts only the 480x480 semantic shadow (src
stride `0x05a0`) and emits the compressed-source diagnostic for this draw.
The blend helpers (`blend_shadow_pixel`, `blend_unpack_rgb565`,
`tsc6a_snapshot_matrix`, `tsc6a_floor_div_fp16`, `tsc6a_ordered_clip`,
`tsc6a_bounded_sram`) and the RGB565 staging buffer already exist. Code is
extended, not replaced.

## Allowed Files

`src/display/nema_tsc6a_expand.c` (new), `src/display/nema_tsc6a_raster.c`,
`src/display/nema_tsc6a.h` or `nema_tsc6a_internal.h` (declaration only),
`tests/unit/test_nema_tsc6a_expand.c` (new),
`tests/integration/test_firmware_sapporo_235_compressed.sh` (new).
Integrator scope add (2026-09-23, answer to the reported interface gap — the
smallest required integration change, no private-API workaround):
`src/display/nema_tsc6a.h` (add `semu_bus *bus` as the second parameter of
`nema_tsc6a_resolve_mask` — nothing else in this header),
`src/display/nema_backend_draw.c` (one line: pass `draw_context->bus` at the
existing call site), and `tests/unit/test_nema_tsc6a.c` (mechanical call-site
update; shadow cases pass NULL bus; convert the exact-tuple compressed
diagnostic pin into an out-of-tuple refusal pin, e.g. stride 181, keeping the
diagnostic message asserted). No other edits outside the original list.
`Makefile`, registries, profiles and `plans/index.tsv` are integration-owned
and must not be edited by this ticket.

## Frozen Interfaces

Existing 480x480 semantic-shadow acceptance (accepts with NULL bus exactly as
today), the ordered-clip and matrix helpers, the completion (clid, IRQ)
condition, strict refusal for all other states, deterministic checkpoints,
and firmware safety remain. The single `semu_bus *bus` parameter addition to
`nema_tsc6a_resolve_mask` is the authorized interface change of this ticket;
the compressed acceptance state must additionally refuse (zero writes) when
bus is NULL or any bounded read fails. No further public API changes.

## Evidence Inputs

E-RE-SAP235-TSC6A-001 block law and acceptance tuple; the reference expansion
`/tmp/sap235-tex17/decode-1.bin` (SHA-256
`2f30fe186ba7edd5ef39498f4ad69736684a1611ee1f5d581f05b2afba756b2f`, twice
reproduced) for the golden window; refusal control = one bit set in any block
byte 10 (auxiliary region). The capture-pinned tuple and every constant are
from E-EMU-SAP235-MAIN-TSC6A-001; inventing no value.

## Implementation

Add `tsc6a_expand_block(const uint8_t blk[12], uint8_t out[16][4])` returning
failure when bits 75..95 are nonzero, implementing the LE bitfield law
(index/endpoint/alpha fields, thirds QCP table, RGBA4444 top-three-nibble ×17,
11-bit alpha ×255/2047). In `nema_tsc6a_resolve_mask()` add a second accepted
predicate matching the exact E-RE-SAP235-TSC6A-001 tuple (src fmt 0x17
sampling 1 stride 180 60x60 bounded SRAM; RGB565 240x240 target stride 480
bounded; span-overlap rejection; pinned codeptr/matmult/matrix identity bits;
ordered clip (0,81)-(240,162); 60x60 axis-aligned quad; tint 0xffffffff;
drawcolor 0xff555555). On acceptance: bounded-read 2700 B, expand all 225
blocks into a fixed buffer with every block validated BEFORE any target write
(any expansion failure ⇒ `SEMU_ERR_UNSUPPORTED`, zero writes), map each target
pixel through the pinned identity matrix with the existing helpers, range-check
into [0,60)x[0,60), and SRC_OVER the texel onto the staged RGB565 via the
existing blend path, committing on the unchanged completion condition.
Identity scale means texel→pixel is 1:1, so nearest is exact here; do not add
a bilinear interpolator.

## Tests and Commands

`make test TEST_FILTER=nema_tsc6a_expand` exits 0 with ≥1 test: synthetic
color/alpha/border blocks and one-bit-set refusal leaving a full 115200 B
destination unchanged run unconditionally; the capture-golden case (the
225-block expansion equal to the twice-reproduced `decode-1.bin` window
byte-for-byte) follows the established env-fixture pattern of
`test_nema_corpus.c`/`test_ulsan_reset.c` — read `SEMU_TSC6A_FIXTURE`, skip
when unset. Private-firmware runner
`tests/integration/test_firmware_sapporo_235_compressed.sh` (TEST_PROFILE
`sapporo-2.35.34`) derives the fixture with `dd` at offset `0x9db613`, length
2700, pins its hash `f311e1ef…`, and runs the golden case. The same runner
executes paired identical 5-layer runs to the main-entry tuple and derives a
new `stop`/frame checkpoint where the icon draws and the former HardFault at
`0x1be85a` is gone; assert no `status=refuse`, no `machine-reset-request`, and
the derived pixels hash twice. `make check`,
`make check-lines`, `make sanitize` pass. State any era-drift impact if shared
blend helpers change; the semantic-shadow unit pins must stay green.

## Acceptance

The 60x60 crosshair renders through the in-tree renderer on private 2.35 main
entry with no compressed-source refusal, expansion is deterministic and
byte-matches the reference window, the aux-bit refusal is atomic, the
480x480 shadow path is unchanged, and no firmware or asset bytes enter Git.

## Forbidden Scope

No blank/guessed pixels, no bilinear/scaling for other matrices, no nonzero
auxiliary acceptance, no permissive MMIO, no asset-specific hardcoding, no
other PXB2 format byte (0x05/0x0f/0x10/0x13/0x19), no TSC4/TSC6 source, no
snapshot, GPS, or era repin work, no fixture-ceiling increase.

## Handoff

Report changed files, exact commands/results, the reference-window hash, the
refusal case, the derived main-entry checkpoint, and any integrator-owned
change needed. Do not claim done if private firmware is unavailable to run the
renderer gate.

## Integrator acceptance (2026-09-23, delegated)

Implemented (implementer) and integrator-verified. `tsc6a_expand_block`
(`src/display/nema_tsc6a_expand.c`) plus the second accepted state of
`nema_tsc6a_resolve_mask` (exact E-EMU-SAP235-MAIN-TSC6A-001 tuple, new
`semu_bus *bus` parameter per the authorized Option A scope add, fail-closed
NULL, validate-before-mutate with zero-write refusals). The bus-parameter
change, caller line, and `test_nema_tsc6a.c` rework (legacy-diagnostic pin
converted to the stride-181 near-miss) were integrator-specified and
integrator-reviewed.

Integrator verification (2026-09-23): `make -j8 all`; `make test
TEST_FILTER=nema_tsc6a` 7/7 including real-bus resolve accept, aux-bit
zero-write atomicity, and the legacy-diagnostic string; full 2.35 firmware
runner set green; `make check` 996 PASS / 0 FAIL and `make sanitize` zero
findings (implementer runs on the identical tree, integrator spot re-runs
green); `check-sdl`, the 2.35 scroll gate (`1d44ea98…`), the SDL
startup/language gate, and snapshot restore unchanged; 2.35 era scripts zero
drift. Runner Section 3 added by the integrator: the common-command
setup-walk main-entry window, paired byte-identical
(`b2820edf…`), shows the main-entry settled frame (step 25, generation 3994,
`crc32=6a446900`), zero compressed-source refusals, and the single residual
self-reset at `0xcdf5a` ending at the documented OHR-fixture boundary —
recorded with the COMPOSITE-MATCH acceptance probe as
E-EMU-SAP235-COMPRESSED-001. The former refusal→fault→reset chain is gone;
the `0xcdf5a` self-reset and main-screen button navigation are tracked as
ticket 794. Status set `done`.
