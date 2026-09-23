# 794 — Sapporo 2.35 Main-Screen Settling And Button Navigation

**Status:** ready
**Phase:** 7
**Dependencies:** 504,513,761,789,793

## Goal

After ticket 793 the 2.35 guest opens main after Done and the 60x60
compressed crosshair composites into the settled frame
(E-EMU-SAP235-COMPRESSED-001, settled step 25, generation 3994,
`crc32=6a446900`), but ~2.4 s later the guest self-requests a machine reset
at `0xcdf5a` (`reset_count=1`, `instructions=7554756551`,
`virtual_time_ns=32447955715`) and the run then ends at the exhausted
OHR-fixture compat boundary. Determine whether that self-reset is authentic
firmware behavior or the symptom of another unmodeled refusal, keep main
alive (or reproduce the authentic reset with evidence), and drive native
button navigation on the main screen. This is the direct next step of the
"Sapporo firmwares fully work with display and buttons" objective.

## Evidence basis

E-EMU-SAP235-MAIN-TSC6A-001 (captured refusal tuple, pre-change boundary),
E-EMU-SAP235-COMPRESSED-001 (post-change boundary and honest residual),
E-EMU-SAPPORO-BRANCH-GATES-001 / ticket 787–789 button-press mechanism
(native SDL button path already proven past the phone-instructions gate),
E-SAP-0033/E-SAP-0037 (manual-clock control and scheduler context around
`0xcdf5a`-era behavior).

## Implementation

Bounded diagnostic-first work, inside the profile/compat/renderer files
ticket work already allows; no renderer changes unless a new refusal is
captured and evidenced:

1. Trace window (observer builds, volatile) between the main-entry settled
   frame and the `0xcdf5a` reset: name the callsite chain into the reset,
   and whether any bus refusal/unimplemented write/missing fixture response
   precedes it. Pre-change runs faulted into the same `0xcdf5a`; the
   post-change run reaches it without any refusal, so both branches must
   be attributed, not assumed.
2. If a refusal or missing compat response is found: smallest evidenced
   behavior per AGENTS.md (evidence entry first or with the change),
   hit-bounded compat where the fixture is the oracle, never a silent
   ceiling increase without a ledger note.
3. If the reset is authentic (post-wizard production reboot): model the
   settling as the real two-stage boot-to-main and pin the post-reset
   second-boot main-entry behavior; the exhausted-OHR tail then becomes a
   fixture-scope matter for the runner, not a machine defect.
4. Button navigation: extend the setup-walk (or a new named SDL live test)
   with post-settling native M/L/TAP sequences and assert observable
   navigation (settled-frame generation/crc transitions of the main
   screen), never guest-internal guesses.

## Tests and Commands

- New/extended SDL live test under `tools/` following
  `test_sdl_sapporo_235_scroll.sh` conventions (skip without private
  manifest, twice-identical transcripts, derived pins only).
- Section 3 of `tests/integration/test_firmware_sapporo_235_compressed.sh`
  may be superseded by this ticket's runner; re-derivation is allowed only
  with a ledger note recording the old pins.
- `make check`, `make check-lines`, affected era scripts, `check-sdl` plus
  the 2.35 SDL gates (scroll `1d44ea98…`, startup/language, snapshot
  restore) unchanged or re-derived with attribution.

## Out of Scope

Other PXB2 format bytes and the aux-plane (A3) law, 2.33/2.39 work, GPS
deepening, snapshot codecs (792), product release gate (790).

## Handoff

Report the attribution of the `0xcdf5a` reset with lane or in-tree capture
hashes, the changed files, exact commands, and derived navigation pins. Do
not claim main "works" beyond what settled-frame evidence shows.
