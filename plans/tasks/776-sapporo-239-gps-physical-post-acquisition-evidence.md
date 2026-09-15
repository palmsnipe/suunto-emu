# 776 — Sapporo 2.39 GPS Physical Post-Acquisition Evidence

**Status:** deferred
**Phase:** 7
**Dependencies:** 771,774

## Goal

Recover physical observations of what a 2.39-generation device does after
five GPS-awake admissions — the sixth wake onward, GSTP/NMEA backend traffic
during acquisition, and post-save GPS session behavior — so the emulator can
replace the diagnostic-grant continuation (E-SAP-GPS-CONTINUATION-239-001)
with an evidenced production boundary.

## Execution Budget

Blocked externally; once observations exist, one model-day to record the
evidence entries and register the follow-on integration ticket.

## Required Reading

E-SAP-GPS-FIFTH-239-002, E-SAP-GPS-CONTINUATION-239-001,
E-SAP-COMPAT-PERSONAL-SUFFIX-239-001; tickets 766, 771, 772, 773, 774;
`docs/compatibility-policy.md`; `src/compat/sapporo_239_gps_awake.c`.

## Current Baseline

Production admits exactly five GPS-awake pulses and refuses the sixth; that
five-pulse bound is the physical observation on record. Ticket 774 proved
diagnostically that nothing else — no save, no UI change — happens before a
firmware-initiated SCB system reset roughly 23 seconds after a sixth grant,
but a granted sixth wake is not physical evidence. The emulator cannot close
this gap from existing captures.

## Evidence Inputs

None available yet. Required, any sufficient subset:

1. A same-build (2.39.20.x) observation of the GPS wake cadence beyond the
   fifth admission during a real acquisition: pulse count and wall-clock
   spacing, or an explicit abort/retry behavior with timestamps.
2. GSTP/NMEA backend traffic recorded from the device during the same
   window, sufficient to pin command/response byte sequences.
3. Post-HEIGHT-save UI continuation (frame sequence or photographs at the
   same UI steps the emulator reproduces, through the 75th frame and beyond).

## Allowed Files

`docs/migration-evidence.md`, `docs/current-status.md`, `plans/index.tsv`,
`plans/tasks/776-sapporo-239-gps-physical-post-acquisition-evidence.md`,
plus whatever new evidence-gated tickets those observations register.

## Frozen Interfaces

No production ceiling, gate, checkpoint or refusal may move on the strength
of a partial or ambiguous observation; the five-pulse bound and the
sixth-admission refusal pins change only under an explicit integration
ticket backed by unambiguous evidence.

## Implementation

When observations arrive: record them as E-SAP evidence entries with provenance
(device, build, capture method, hashes of any payloads kept outside Git),
state exactly which tuples they pin, then register the integration ticket
that adjusts the awake ceiling or GSTP model to match — never the reverse.

## Tests and Commands

`make check-task-contracts` while planning; the ticket's evidence entries
must cite capture artifacts whose hashes reproduce. No emulator command can
substitute for the missing physical observation.

## Acceptance

Evidence entries exist with provenance and the awake-boundary integration
ticket is registered `ready` with those entries in Evidence Inputs.

## Forbidden Scope

Inferring wake cadence or GSTP bytes from the diagnostic continuation, the
four-pulse era, or other products; inventing fixes, timestamps or responses;
raising ceilings without physical evidence.

## Handoff

Blocked at registration (2026-09-09): the required physical observations do
not exist in the repository or the user evidence roots. This ticket exists
to keep the roadmap honest about why 2.39 continuation waits for new
captures rather than engineering effort.

Constraint update: the user owns no physical Suunto device and will not
produce captures (stated 2026-09-11 session). The three Evidence Inputs
above cannot be satisfied from any available source; no firmware-side
analysis substitutes for them (Forbidden Scope stands). This ticket is
permanently blocked absent third-party observations. The production
boundary stays as evidenced: exactly five GPS-awake admissions, sixth
refused, pinned by the ticket 771/774 regressions.

Integrator decision (2026-09-15, delegated): `blocked` → `deferred`. Under
the standing no-device constraint this is an explicit human decision to stop
pursuing the ticket, not a temporary obstacle; the recorded Evidence Inputs
and the production five-pulse boundary remain exactly as written and are
reactivated only if third-party captures are ever supplied.
