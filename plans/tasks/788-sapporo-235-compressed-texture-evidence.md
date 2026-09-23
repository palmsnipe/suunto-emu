# 788 — Sapporo 2.35 Compressed Texture Evidence

**Status:** done
**Phase:** 7
**Dependencies:** 504,513,761

## Goal

Establish authoritative decoding evidence for the native compressed TSC6A
image preventing 2.35 from opening main after Done. Define a later runtime
integration ticket only when the decoder behavior can be specified concretely.

## Execution Budget

One captured native draw, paired reference probes and a bounded codec census.
Use the pinned captured draw instead of repeating complete cold onboarding.

## Required Reading

`AGENTS.md`, `docs/architecture.md`, `docs/execution-model.md`,
`docs/testing-strategy.md`, `docs/compatibility-policy.md`;
E-NEMA-TSC6A-001, E-EMU-SAP235-MAIN-TSC6A-001; tickets 504/513/761;
`src/display/nema_backend_draw.c`, `nema_tsc6a.c`, `nema_tsc6a_raster.c`,
`nema_tsc6a_internal.h`; the exact lane model, research note and volatile
probe sources identified by the evidence entry.

## Current Baseline

Native setup reaches Time/date, time-zone selection and Done. A 60x60
format 17 source with stride 180 refuses at main entry, causing HardFault and
reset. Two diagnostics and two full draw captures match exactly. The lane also
refuses that exact draw and changes no pixels. Its 480x480 semantic shadow
cannot decode compressed assets. The datasheet identifies the format only.

## Allowed Files

- `docs/migration-evidence.md`
- `docs/current-status.md`
- This ticket

Planning explicitly adds this ticket and its `plans/index.tsv` row. No runtime,
public header, registry, profile, golden or firmware changes. Raw private
probes, XML, pixels and logs stay outside the tree.

## Frozen Interfaces

Strict renderer refusal, existing semantic shadow, physical-panel boundary,
CPU fault path, deterministic checkpoints and firmware safety remain.
The read-only Renode lane remains the sole machine oracle under AGENTS.md.

## Evidence Inputs

E-EMU-SAP235-MAIN-TSC6A-001 pins the descriptors, affine coefficients, source
and destination hashes, fault tuple and paired lane refusal. Ambiq's Apollo4
Plus datasheet section 21.3.4.18 identifies the six-bit compressed format.
Neither the semantic shadow nor the diagnostic TSC4 nibble reader provides
compressed-TSC6A pixel evidence.

## Implementation

No emulator implementation while the positive reference is missing. Identify
an authoritative source compatible with the standing oracle constraint. Derive
compressed bounds, block/addressing rules, channel/alpha decoding, rounding
and filtering. Replay captured native and independent synthetic success/refusal
controls twice, retaining hashes and a derived census. A reference refusal
must not be turned into a success oracle.

## Tests and Commands

Replay `lane-{1,2}.resc` from the evidence entry, with zero guest instructions
and 1 ms virtual caps; compare normalized logs and full output bytes.
`make check-task-contracts` validates planning changes; `make check` applies
when commands/contracts change. A later runtime ticket must name focused
atomic decoder/renderer tests, `make check`, `make sanitize`, paired authentic
setup/main runs and affected era audits.

## Acceptance

A permitted positive reference supplies exact compressed pixels for the 60x60
draw and synthetic color/alpha/border cases, plus bounded refusal controls.
The derived algorithm and bounds permit deterministic C99 implementation
without firmware assets or runtime helpers. Current refusals do not satisfy
acceptance.

## Forbidden Scope

No blank substitute, stretched semantic shadow, guessed nibble layout,
permissive MMIO, asset-specific pixels, predecoded firmware caches, assertion
bypass, CPU hook, fixture ceiling increase or physical-device acquisition.
Do not change the read-only lane to make a hypothesis its own oracle. An
exception to the oracle constraint requires an explicit user or integrator
decision; this ticket grants none.

## Handoff

Planning-only, 2026-09-22. All listed dependencies are done; positive compressed
texture evidence is missing. Report a permissible reference before defining
runtime scope. Main rendering, GPS, persistence and other profile gaps remain;
the overall goal is incomplete.

## Integrator decision (2026-09-23, delegated)

The project owner authorized an additional evidence class for behavior the
lane cannot observe or refuses to model: offline reverse-engineering of the
hash-pinned firmware and resource partitions (AGENTS.md Lane Oracle decision,
2026-09-23). Status set `ready`: the format-17 codec derivation proceeds
through static RE of the pinned 2.35 firmware and resource partition, recorded
as a new evidence entry with input/tool hashes and a twice-reproduced census.


## Integrator acceptance (2026-09-23, delegated)

E-RE-SAP235-TSC6A-001 records the offline-RE derivation under the owner-
authorized evidence class: inputs and tooling hashed, the block law stated, and
a twice-reproduced decode census (`2f30fe18…`, integrator independently
re-run) plus the `PXB2` container corroboration (asset at `0x9db613`, hash
`f311e1ef…`, format byte 0x11). Ambiguities A1/A2/A3 are recorded and
contained: the expansion fails closed on any nonzero auxiliary bits. The
acceptance condition — a permitted positive reference giving exact compressed
pixels for the 60x60 draw plus synthetic controls and bounded refusals — is
satisfied. Status set `done`. Runtime work is defined and tracked as ticket
793; this evidence ticket adds no runtime code.
