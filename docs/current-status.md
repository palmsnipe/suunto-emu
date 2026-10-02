# Current Implementation Status

This file records the implemented baseline without weakening the roadmap
gates. The ticket index remains authoritative for roadmap work; bounded
maintenance follows `AGENTS.md` directly. A roadmap ticket is `done` only when
its full acceptance conditions pass, even if useful pieces of later tickets
already exist.

## Ticket 802 — reject unsupported compressed baseline samples — 2026-10-02

Implemented for integrator review on `d6d1f86`. A known compressed block rewritten
with unsupported auxiliary bits could publish old cached pixels. DRAW_CMD=10 now
checks its complete sampled source footprint before destination writes and
refuses unsupported blocks with a block-index diagnostic. Cache state, renderer
snapshots, frame pixels and publication remain atomic. Unknown blocks outside
the footprint preserve their existing serialized history. No clearing value,
auxiliary decoder or compressed writeback is invented.

E-EMU-NEMA-RESOLVE-REFUSAL-001 records the red-first failures, resulting eight
lifecycle cases, raw verification hashes and unchanged navigation checkpoints.
The decoder predicate and resolve-coordinate calculation are shared with the
validation path. Renderer codec 2, profiles, public API and budgets are unchanged;
the separate semantic mask/quad path remains outside this slice.

Changed files: `nema_backend_draw.c`, `nema_backend_internal.h`,
`nema_tsc6a_sync.c`, `nema_tsc6a_expand.c`, `nema_tsc6a_internal.h`,
`nema_tsc6a_raster.c`, `test_nema_tsc6a_lifecycle.c`, the execution contract,
README, status and evidence. Planning setup adds ticket 802 and its index row;
implementation leaves it `ready` for review.

Commands (logs under `/tmp/semu-nav-20261002/`):

- `make test TEST_FILTER=nema_tsc6a_lifecycle` — 8 tests, first 2 new cases
  fail, then all pass. Covers stale rewrite, cold unknown input, clipping,
  integer/fractional translation and whole-snapshot rollback.
- `make -j4 all sdl`, `make check` — pass, 1,031 PASS records.
  `make sanitize` — 1,026 pass with ASan/UBSan.
- `make check-task-contracts` — 166 tickets validate; `make check-lines`
  passes with advisory warnings; `git diff --check` passes.
- `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-renderer-v2-20261002/codec2-235-1.sems
  sh tools/test_sdl_sapporo_235_navigation_restore.sh` — all six paired
  cases pass, exact logs/snapshots unchanged.
- `make check-sdl` — full 2.22 onboarding/menu, 60-second idle and expected
  finite GPS-cap control pass with unchanged complete transcript pins.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
  make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235`
  — all nine runners pass (ten PASS messages), including paired main-entry
  rendering and snapshot continuation. Optional private `decode-1.bin`
  comparison explicitly skips; synthetic and pinned asset checks run.
- `make check-era` — SKIP, the verified full-flash input is unavailable.
  The shared renderer changed: 2.39 pins remain unverified and may drift;
  ticket 800 owns their re-derivation, with no silent re-pin here.

## Ticket 801 — restored navigation attribution — 2026-10-02

Implemented for integrator review on `10b3cf6`; runtime behavior is unchanged.
The previous “lower repaint stall” was a static widget-pinning prompt reached
by an earlier Middle press. A later Middle press returns to the watchface.
Isolated Upper opens Exercise, Lower opens Widgets, Middle opens the pinning
prompt, and Lower then Middle opens Control Panel. Widget browsing and a
Middle round trip pass twice with complete log/snapshot equality. A one-second
Middle repeat remains on the prompt; a six-second interval returns to the
watchface. No unobserved timing law is inferred.

E-EMU-SAP235-NAVIGATION-002 records all input times, terminal tuples, frame
CRCs, snapshot/log/probe hashes and the cold-window attribution, with existing
E-SAP-BUTTONS-001 and E-EMU-RENDERER-SNAPSHOT-002 as references. The old cold
LOWER and UPPER windows were repeated twice without changing their pins.
Opening Exercise alone works; selecting a sport reaches a separate OHR
configuration request boundary under investigation. GPS/OHR coverage remains
finite. Auxiliary compressed modes and writeback remain unsupported, and
2.39 era validation still needs the verified full-flash fixture (ticket 800).

Changed files: new `tools/test_sdl_sapporo_235_navigation_restore.sh`, corrected
comments/result labels in the existing navigation runner, README, this status,
and the evidence ledger; planning setup adds ticket 801 and its index row.
No firmware bytes, new screenshots, runtime semantics or old goldens change.
Ticket 801 remains `ready` for integrator review.

Verification artifacts: `/tmp/semu-nav-20261002/`. The new six-case runner
passes with `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-renderer-v2-20261002/codec2-235-1.sems
sh tools/test_sdl_sapporo_235_navigation_restore.sh`. Independent bounded
`cold-nav.py` and `navigation.py` probes reproduce the historical and new
windows respectively; all source components validate first. Full commands
and paired hashes are retained in E-EMU-SAP235-NAVIGATION-002.
`make check` passes (1,029 PASS records); `make check-task-contracts` validates
165 tickets, and `make check-lines`, shell syntax and `git diff --check` pass.
Explicit missing-manifest and wrong-snapshot probes refuse as expected.

## Ticket 799 — faithful compressed-frame snapshots — 2026-10-02

Implemented for integrator review on top of `aeed58b`. Renderer codec 2
preserves unresolved GPU strokes, the frame lifecycle flag, and cached
compressed-surface history. Synthetic regressions first reproduced both kinds
of lost state. Invalid images refuse before mutation, and a later machine-load
failure restores the complete previous renderer state. This supersedes the
mid-frame limitation recorded in the earlier GPU-maintenance entry below.

**Recreate old snapshots.** Renderer codec 1 lacks essential continuation state
and now refuses with an explicit recreate-snapshot message. The outer machine
format remains version 2. The renderer image grows by 1,094,412 bytes to
2,246,592 bytes; no rendering law, CPU/device behavior, public API, profile,
compatibility budget, or dependency changed.

The historical 2.22 restore-gate discrepancy is also resolved. A paired
`92b8ac4` control reproduces both old menu/LOWER hashes; the drift is confined
to the compressed shadow introduced by `cb6298b`. Paired codec-2 captures then
prove that only the renderer section's encoding changes relative to the current
model. Guest stop/count/time and published frame pins remain unchanged.
Evidence E-EMU-RENDERER-SNAPSHOT-002 records the complete hashes, paired log
hashes, wire layout and attribution; supporting references are
E-EMU-NEMA-CACHE-LIFECYCLE-001, E-EMU-RENDERER-SNAPSHOT-001,
E-EMU-SAP235-TICKTRAIL-002 and E-EMU-SAP222-SNAPSHOT-AUDIT-001.

| Checkpoint | New snapshot SHA-256 | Preserved frame |
| --- | --- | --- |
| 2.22 menu | `878f93954918f2e924eaba1aceb61b9557692e933507e9ed1d378e54c7eaf72b` | generation 4510 / CRC `040ebb03` |
| 2.22 LOWER | `f6b32641d8b8cd330eaa7beb2d1b07120e2e056af1dede26e4308d4daf9909cb` | generation 4610 / CRC `0cb272ba` |
| 2.35 watchface | `e25c409d8868cd36a6d62c5c9f7da30442c98f9461b19fdd72ac3be1b245c971` | generation 4770 / CRC `500b350f` |
| 2.35 early boot | `cdf9f3ff343d90e7dfa114bb19128d18f52d88ded7053aff127cfa9b4a32aad9` | unchanged boot/continuation transcripts |

Verification, with raw artifacts in `/tmp/semu-renderer-v2-20261002/`:

- `make test TEST_FILTER=renderer_snapshot` — seven tests pass;
  `make test TEST_FILTER=nema_tsc6a_lifecycle` — six pass. New mid-frame and
  cache-history cases failed first; legacy/codec cases also failed against
  the original implementation.
- `make -j4 all sdl` and `make sdl` — pass. `make check` — 1,029 PASS records;
  `make sanitize` — 1,024 tests pass with ASan/UBSan.
- `make check-task-contracts` — 164 tickets validate;
  `make check-lines` — pass with advisory size warnings;
  `git diff --check` and `sh -n` on the three changed runners — pass.
- `python3 /tmp/semu-renderer-v2-20261002/capture.py 222 codec2-222` and
  the corresponding `235 codec2-235` invocation — pass, two identical logs
  and snapshots each; all firmware components validate first. The same 222
  command with `control-222 --emulator
  /tmp/semu-renderer-v2-20261002/control/build/suunto-emu-sdl` reproduces the
  old gate. The hashed `boot235.py`, `restore222.py`, `restore235.py`, and
  `next-control.py` probes also pass their paired bounded derivations.
- `make check-sdl` — pass, full 2.22 onboarding/menu/60-second idle and
  expected finite GPS-cap control.
- `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-renderer-v2-20261002/codec2-222-1.sems
  sh tools/test_sdl_snapshot_restore.sh` — pass, immediate frame and paired
  native LOWER continuation. The independent paired idle logs retain SHA-256
  `91cc708109a2e0405d385fc894674832c58c5457a2845d36c97571d9bd4ab1bf`.
- `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-renderer-v2-20261002/codec2-235-1.sems
  sh tools/test_sdl_sapporo_235_restore.sh` — pass, immediate watchface and
  paired native continuation to `budget / 000e1862 / 9578131227 /
  42000000000 ns`, with zero reset/draw-refusal/compat-refusal events.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
  make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235`
  — all nine runners pass (ten PASS messages). The optional private
  `decode-1.bin` comparison skips; synthetic expansion and pinned source
  asset checks run.
- `make check-era` — explicit skip: the verified 2.39 full-flash fixture is
  unavailable. Codec 2 deliberately moves renderer-containing snapshot
  hashes, so the old era pins are **not accepted for this build**. Ticket 800
  tracks their paired re-derivation and the required 43-script census.

Changed files: the renderer snapshot implementation; `test_renderer_snapshot.c`,
new `test_renderer_snapshot_codec.c`, and `test_nema_tsc6a_lifecycle.c`; the two
SDL restore runners and the 2.35 firmware snapshot runner; README, execution
contract, evidence ledger and this status. Planning setup added tickets 799/800
and their index rows. Implementation leaves 799 `ready` and 800 `blocked`;
review/status promotion and the missing full-flash fixture remain integrator
work. No proprietary artifacts were added to Git.

Remaining GPU limits are unchanged: the auxiliary-plane codec law and surface
writeback are unimplemented; known-to-unknown block rewrites can retain old
pixels. This integration faithfully persists that existing cache policy.
The earlier navigation interpretation is superseded by ticket 801 above;
sport selection still reaches an OHR boundary.
Earlier entries below describe their respective historical baselines.

## Sapporo GPU state hardening — 2026-10-02

Maintenance following the screenshot/status commit `8433225`: fix compressed
surface transaction rollback, propagate baseline-read errors, and invalidate
derived caches on restore. The renderer now stages its frame-lifecycle flag
alongside shadow pixels, including resolve-only submissions. Refusal or abort
preserves committed pixels, registers, lifecycle state, and frame publication.
Baseline synchronization uses a preallocated 172,800-byte scratch buffer and
reads the span once per resolve.

The snapshot regression previously mapped only 4 KiB, so baseline reads silently
failed and it missed the real path. Mapping the full surface exposed lost
strokes when saving between draws and resolve. Codec version 1 has no field for
that lifecycle state. Rejecting such saves also rejected the existing 2.22 menu
checkpoint, so that restriction was not retained. Save admission and encoding
remain unchanged; **mid-frame restore can still lose strokes** and needs codec
integration. The corrected positive regression covers completed-frame restore
with fully mapped RAM. Valid loads discard previous derived caches; invalid
loads preserve them.

Changed implementation: `src/display/nema_backend.c`, `nema_backend_draw.c`,
`nema_backend_internal.h`, `nema_backend_snapshot.c`,
`nema_backend_transaction.c`, and `nema_tsc6a_sync.c`; regression coverage in
`test_nema_tsc6a_lifecycle.c`, `test_nema_backend_atomic.c`, and
`test_renderer_snapshot.c`. Contracts are clarified in `docs/execution-model.md`.
No profile, public header, roadmap status, firmware fixture, or golden was
changed. Evidence: E-EMU-NEMA-CACHE-LIFECYCLE-001, the existing transactional
display contract, E-EMU-RENDERER-SNAPSHOT-001, and E-EMU-SAP235-TICKTRAIL-002.

Remaining GPU work: a renderer codec version that stores the compressed-frame
lifecycle is the smallest required integrator-owned extension for faithful
mid-frame save/restore. The auxiliary-plane codec and surface writeback remain
unimplemented. E-RE-SAP235-TSC6A-001 and E-RE-SAP235-PXB2LOADER-001 still provide
no verified law for bits 75..95; the latter's loader investigation found GPU
descriptor construction, not a software decoder to transplant. An additional
regression found that rewriting a decoded block to an unsupported auxiliary-bit
block retains old pixels. Clearing those pixels preserves the entire 2.35 cold
transcript but changes its snapshot; that correction is deferred to explicit
golden re-derivation rather than silently changing the pin. This maintenance
does not extend compressed-format admission or claim hardware equivalence.

Verification (logs under `/tmp/semu-gpu-state-20261002/`):

- `make test TEST_FILTER=nema_tsc6a_lifecycle` — five tests pass; the retained
  cases failed before their fixes. `make test TEST_FILTER=nema_backend_atomic`
  — nine pass; `make test TEST_FILTER=renderer_snapshot` — four pass.
- `make check` — pass, 1,025 PASS records including quick SDL tests; 162
  ticket contracts validate. `make sanitize` — pass, 1,020 tests with
  ASan/UBSan. `make check-lines` runs within `make check`; only advisory
  size warnings. `git diff --check` — pass.
- `make check-sdl` — pass: live input, 2.22 onboarding/menu, restored idle
  continuation to 60 virtual seconds, and the expected finite GPS-cap control.
  Existing complete transcript pins are unchanged. The earlier attempted
  mid-frame-save refusal failed this gate and was removed before acceptance.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
  make test-firmware TEST_PROFILE=sapporo-2.35.34 TEST_FILTER=sapporo_235`
  — all nine runners pass (ten PASS messages), including paired compressed
  main-entry runs and snapshot continuation. The optional private
  `decode-1.bin` comparison skips because its reference is absent; synthetic
  expansion and the hash-pinned source-asset checks run.
- `sh tools/test_sdl_sapporo_235_restore.sh` — pass including a new cold
  save and two restored continuations. Cold log SHA-256
  `14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae`,
  snapshot SHA-256
  `5f21f7d11a6de4c18e17e61a7256e217508526ea6bcfd09ae46d5e954ca14a5d`,
  watchface generation 4770 / CRC `500b350f`, and continuation checkpoint
  `budget / 000e1862 / 9578131227 / 42000000000 ns` all hold.
- `build/suunto-emu validate --profile sapporo-2.33.16 --firmware
  tests/private/sapporo-2.33.16/firmware.semu`, then `build/suunto-emu run`
  with those same profile/firmware arguments and `--max-instructions
  200000000 --max-time 200000000` — valid, then expected budget exit 3;
  transcript SHA-256
  `5d7c9daf29adff26ac7430542ecafe41f55fb95227a04f761003488dc71b0c0c`
  remains unchanged.
- `make check-era` — SKIP: verified `SEMU_SAPPORO_239_FULL_FLASH` remains
  unavailable. The shared renderer changed, so 2.39 era pins remain unverified
  and may have drifted; no re-pin is made. The known 2.22 historical snapshot
  hash discrepancy below is also not resolved by this maintenance.

## Sapporo review — 2026-10-02

Maintenance scope: reconcile the README with the current evidence, reproduce
native UI captures, and audit the firmware boundaries. Source revision
`05801b4`; no CPU, device, renderer, profile, fixture budget, or regression
golden changed. The six README PNGs have an explicit owner-authorized Git
exception recorded in `AGENTS.md`; raw captures and snapshots remain external.

**Sapporo is usable for selected firmware UI paths, not fully emulated.**
The four private OTA manifests validate all three components. Their coverage
must be assessed separately:

| Version | Current result | What is still missing |
| --- | --- | --- |
| 2.22.60 | Fresh cold onboarding/menu walk and 60-second idle continuation retain their complete transcript pins. The menu restores visibly. | The snapshot-restore gate fails its historical image hash before exercising its continuation. Broader watch functions and unbounded sessions remain unverified. |
| 2.33.16 | Two early-boot runs match E-EMU-SAP233-GAUGE-FIXTURE-001 exactly, with no reset/refusal. | A boot-window pass is not setup, watchface, menu, or long-session acceptance. |
| 2.35.34 | Fresh setup walk reaches the watchface at its scripted quit; the paired restore gate passes. Seconds-hand frames use the corrected per-resolve lifecycle. | The navigation contract still records middle inert, lower repaint stall, and upper navigation followed by an OHR fixture refusal. GPS and OHR support remain finite fixtures. |
| 2.39.20 | Exact profile validation passes. E-SAP239-REPINSWEEP-002 records 43/43 era gates; E-EMU-SAP235-TICKTRAIL-002 records the later snapshot re-pin and green census. | This review cannot repeat the era suite without the verified full-flash input. Bounded GPS/refusal gates do not establish full onboarding, watchface, menus, or a later-version release. |

The [README gallery](../README.md#screenshots) shows five 2.35 phases and a
2.22 menu. Every PNG is an unretouched conversion of a settled SDL frame;
[capture provenance](screenshots/provenance.json) pins each source pixel CRC
and full image/log hashes. Phone instructions are not phone connectivity;
displayed clock and sensor values are not live measurements.

### Fresh verification

Commands run from the repository root; raw logs and snapshots are in the
volatile `/tmp/semu-readme-review-20261002/` workspace. The full capture
commands are in [screenshots/README.md](screenshots/README.md#reproduce).

- `make -j4 all sdl` — pass.
- `make check` — pass, 1,020 PASS records including the five quick SDL cases;
  162 ticket contracts validate. Its firmware walks intentionally skip.
- `build/suunto-emu validate --profile <id> --firmware <manifest>` — pass for
  all four matching manifests under `tests/private/` (2.22.60, 2.33.16,
  2.35.34.18929, and 2.39.20.22297).
- The 2.22 cold capture command, repeated with a fresh snapshot — logs and
  snapshots match byte-for-byte. Log SHA-256
  `2c6910c2d53d0dfd3fa9c00aa046615a3075a725ce33e67876a2706c8a68e0b8`;
  terminal `user / 0800009e / 8491624576 / 38819797929 ns`; menu generation
  4510 / CRC `040ebb03`.
- `SDL_VIDEODRIVER=dummy build/suunto-emu-sdl run --profile sapporo-2.22.60
  --firmware tests/private/sapporo-2.22.60/firmware.semu --layer
  sapporo-2.22-no-device --snapshot-load
  /tmp/semu-readme-review-20261002/222-main.sems --max-instructions
  18000000000 --max-time 60000000000` — expected budget exit 3; idle log
  SHA-256 `91cc708109a2e0405d385fc894674832c58c5457a2845d36c97571d9bd4ab1bf`,
  unchanged. Held menu presents immediately; no reset/refusal.
- `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-readme-review-20261002/222-main.sems
  sh tools/test_sdl_snapshot_restore.sh` — **FAIL**, exit 1, reproduced on the
  second cold snapshot. New image SHA-256
  `1944a15a3fc3647ada151db18535cad8c83854a403ade806f60edc924fbac45a`
  differs from the gate's `87a8dca8…` pin. See
  E-EMU-SAP222-SNAPSHOT-AUDIT-001. No pin was changed.
- `build/suunto-emu run --profile sapporo-2.33.16 --firmware
  tests/private/sapporo-2.33.16/firmware.semu --max-instructions 200000000
  --max-time 200000000` — expected budget exit 3 twice; log SHA-256
  `5d7c9daf29adff26ac7430542ecafe41f55fb95227a04f761003488dc71b0c0c`;
  terminal `budget / 000dbc0a / 48412217 / 339421286 ns`. The scheduler's
  final time jump is reflected in this existing checkpoint.
- The 2.35 cold capture command — pass, log SHA-256
  `14ffff66146dfce6fabbb57ee2f96917431c02061d6c172ffac731b9adaa9aae`;
  terminal `user / 0800009e / 9487528672 / 37414100700 ns`; watchface
  generation 4770 / CRC `500b350f`; snapshot SHA-256
  `5f21f7d11a6de4c18e17e61a7256e217508526ea6bcfd09ae46d5e954ca14a5d`.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu
  make test-firmware TEST_PROFILE=sapporo-2.35.34
  TEST_FILTER=sapporo_235` — pass, all nine selected runners (ten PASS
  messages). The optional private `decode-1.bin` compressed-pixel reference
  is absent and its comparison skips; the synthetic expansion cases,
  hash-pinned source asset, and paired native main-entry gate still run.
- `sh tools/test_sdl_sapporo_235_nav.sh` — pass; all three windows run
  twice with byte-identical transcripts and unchanged pins. This reproduces
  the middle-inert, lower-stall, and upper-navigation/OHR-refusal limits;
  it does not resolve them.
- `SEMU_SDL_TEST_SNAPSHOT=/tmp/semu-readme-review-20261002/235-main.sems
  sh tools/test_sdl_sapporo_235_restore.sh` — pass, paired continuation to
  `budget / 000e1862 / 9578131227 / 42000000000 ns`.
- `SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.39.20.22297/firmware.semu
  make test-firmware TEST_PROFILE=sapporo-2.39.20
  TEST_FILTER=sapporo_239_profile` — pass, one selected runner.
- `make check-era` — **SKIP**, verified `SEMU_SAPPORO_239_FULL_FLASH` absent;
  this is not a new passing 43-script census.
- `python3 tools/export_readme_screenshots.py
  /tmp/semu-readme-review-20261002` — six images exported; a second export
  matches byte-for-byte. PNG decoding reproduces the original RGB bytes.
  Truncated/wrong-size/changed-pixel inputs and an incorrect transcript
  refuse; the bad-transcript invocation creates no output directory.

### Work needed for complete firmware support

1. Attribute the 2.22 snapshot-byte drift with a control build, then have the
   integrator assign/review the required snapshot-golden re-derivation. Do
   not silently re-pin a failing restore gate. Ticket 792 also remains
   `ready` in the index despite the implemented 2.35 codec/restore records;
   its acceptance/status reconciliation belongs to the integrator.
2. Resolve 2.35's lower-button repaint stall and upper-button OHR boundary
   through ticket-710 evidence instances, then validate all three buttons
   across menus and a longer restored session. Ticket 794's observation-only
   navigation goldens preserve these limits; its `done` status does not mean
   complete navigation fidelity.
3. Extend GPS/OHR and sensor coverage from observed laws. No fabricated GPS
   fix, unlimited heartbeat, assertion bypass, or phone connection is
   justified. The project has no physical device; lane observation and the
   authorized hash-pinned offline-RE evidence class govern progress. Ticket
   776 remains deferred under that constraint.
4. Derive the remaining TSC6A auxiliary-bit population and surface writeback
   semantics before claiming general renderer fidelity. E-EMU-SAP235-
   TICKTRAIL-002 leaves 5,441 undecodable resting blocks outside the pinned
   resolve regions and an unmodeled `[resolve, strokes, resolve]` edge.
   Physical-panel equivalence is also unverified.
5. Take 2.33 beyond early boot and validate the full 2.39 user journey with
   its private fixture; complete the later-version release gates 715/718.
   Passing a collection of bounded or refusal tests is insufficient.

Primary references: E-EMU-SAPPORO-BRANCH-GATES-001,
E-EMU-RENDERER-SNAPSHOT-001, E-EMU-SAP233-GAUGE-FIXTURE-001,
E-SAP-0041-EXT3/EXT7, E-EMU-SAP235-GPSRESTORE-001,
E-EMU-SAP235-TICKTRAIL-002, and E-SAP239-REPINSWEEP-002.
Sanitizers were not repeated for this documentation/export-tool change.
No roadmap status, public interface, profile, or runtime behavior changed.

## Implemented Baseline

The dated entries below are cumulative implementation history. Later entries
and the review above supersede older frontier and test-count statements.

### Sapporo 2.35 main entry renders the compressed crosshair — ticket 793, 2026-09-23

`nema_tsc6a_resolve_mask` now accepts exactly one compressed state — the
captured main-entry 60x60 crosshair tuple of E-EMU-SAP235-MAIN-TSC6A-001 —
expanding it in-tree through the E-RE-SAP235-TSC6A-001 law
(`tsc6a_expand_block`) with validate-before-mutate and zero-write refusals;
the function gained a `semu_bus *` parameter (NULL stays fail-closed). On
the private firmware the former refusal→BusFault→reset chain is gone: the
setup-walk trajectory shows the main-entry settled frame (generation 3994,
`crc32=6a446900`) and no compressed refusal anywhere
(E-EMU-SAP235-COMPRESSED-001; runner Section 3, transcript `b2820edf…`;
renderer output byte-identical to an independent law re-composite of the
real capture). Everything else still refuses: any auxiliary-bit block,
every near-miss state, all other compressed shapes.

Ticket 794's first scope is landed: E-EMU-SAP235-RINGKICK-CPU-INVISIBLE-
001 (lane census `c575c2dc`, replay pair `417c2983`) retired the
refused-kick BusFault — post-Done the two refused DRAW=2 resolve passes
log `gpu/draw-refused` lines with zero resets. Ticket 710 then landed
E-SAP-0041-EXT: the OHR fixture answers thirteen ordered startup plus
MAIN-state post-Done queries (lane byte-pinned responses), step 25 now
SETTLES on the main screen (`generation=3998 crc32=1394c638`), and the
walk reaches `stop=unmapped-access pc=0x001023b0
instructions=8772734885 virtual_time_ns=39969302384` (transcript
`245cab82…`) after command `0x0004` sequence 13 refuses and four guest
self-resets. Command `0x0004` is since admitted, and the CPU wall fell: `0xf20e46e4`
is a valid ADD (T3) `addw r6,lr,#0x4e4` (LR-base guard fix,
red-test-first, E-CPU-0011 with the family-wide census — the latent wall
existed in all six private images). The post-Done `0x0002` result poll
(~955 ms period, lane-verified sequence-independent zero-body answer,
19/19 byte-identical) made enumeration the wrong instrument; the
E-SAP-0041-EXT3 ruling replaced it with a lane-law MAIN-only bounded
poll tail after the pinned 14-entry prefix. The five-layer setup-walk
terminates NATURALLY at its scripted quit (`stop=user`; transcript
`a4a04c53…` twice after the EXT4 display gain, `360c325a…` before it):
the first Sapporo window to run post-Done to a scripted end — 0 resets,
main screen settling steps 24–31. The E-SAP-0041-EXT4 ruling (ticket
794, offline-RE census: accent `0xff55aaff` firmware-native, sole
failing predicate of all 124 resolve refusals) admitted the masked
accent to the tsc6a ACCENT predicate — the accent-tinted main-screen
blit now rasters (frames change from step 25; guest instructions and
virtual time unchanged, pure display gain; RED-first unit test).
Open boundaries: ticket 794 is CLOSED done: the button-navigation goldens
pin MIDDLE inert, LOWER repaint-stall, and UPPER navigating (frame
9b554fd9) on the settled main screen under the E-SAP-BUTTONS-001 golden
policy — the governing objective's "buttons for the navigation" is
demonstrated and twice-pinned; tail cap 16 (~15 s post-Done) as the pinned
headroom for longer scripts. The E-SAP-0041-EXT6 ruling (ticket 793 scope
extension, 2026-09-27) admits the 103-refusal bounce family through the
ticket-788 codec: the eased horizontal bounce now RASTERS (window
draw-refused 103→0, twice byte-identical on the final binary; guest
instructions and virtual time unchanged; two observed one-ULP composer
roundings admitted bit-exactly after a twice-reproduced residual census;
distinct vertical-scroll refusals stay fail-closed — see E-SAP-0041-EXT6).
The 2.39 era
gates are the last firmware-level lane (777 re-derivation in flight).
Ticket 795 is closed: E-EMU-SAP233-GAUGE-FIXTURE-001 aligns the MAX17050
AvgVCell fixture with the current lane table (register 0x19 now 0xC000,
pair 9119ef13…), and the 2.33.16 boot passes its former first-fault moment
`0x000c97f2` fault-free — zero resets and zero refusals in the
200M-instruction/200 ms window (integrator-reproduced pair 5d7c9daf). The
2.22 timing pins moved one compat line (resource-status probe no longer
issued; every frame CRC unchanged) and were re-derived twice with
control-build attribution in that entry. Open items kept honest: the
0x06-early lane-order divergence is unexplained, HFSR escalation-bit
fidelity is open, and 0xF4 stays a refused unobserved selector. The 2.39
era set (same-day ticket-777 audit on a from-pin rebuilt full-flash
fixture): 12 gates pin-held, 25 timing-only re-pin candidates, 0
unexplained failures, 6 environment-blocked re-runs owed, and one
green-to-red flip (`timer_pattern`) awaiting rebuild-bisect — re-pins
tracked in ticket 777, none applied silently. The queued final-binary
classification (E-SAP239-ERA-CLASSIFY-001, 2026-09-27, on 7984ebd)
reproduced the census with zero new classes and established the root
cause of the six environment-blocked scripts: every GPS/settings window
ends at a fail-closed mode-2 refusal of the unmodeled native storage
write `storage/38d123/data.jsn` (pc 0x920b4, 446,660,148 instructions /
2.83 s) — tracked as new ticket 796, goldens NOT weakened into it; the
three GPS snapshot inspectors were fixed to register the 791-era display
snapshot codec, and the 25 timing-only scripts are being re-derived on
this binary with twice-identical runs. The re-pin batch completed the
same day: 7 scripts re-pinned green twice (timer_pattern attributed by
rebuild-bisect to exactly `d8bfba9`, the MAX17050 lane-table fixture
align; park pc unchanged), 18 blocked on three named causes — the 796
storage-JSON wall (13 windows total), choreography redistribution past
pinned caps (4 windows), and a new OHR2 BSL refuse→ok semantics change
(7 windows, ticket 797). Census on 947f4bb: 19 of 43 era scripts green
(12 device-register + 7 re-pinned), 24 red, all reds explained and
tracked; the audit's "25 timing-only" classification is superseded by
the batch record. The 797 attribution stage then corrected the B3
reading with clean-build bisect probes (an earlier build-directory
pollution caveat is recorded in the evidence addendum): no OHR2 device
law ever flipped — the cold session's relocation is attributed to
`cd1de52` (pure instruction-count movement at fixed virtual time) and
the pinned refuse goldens relocate with the session (the device's
refusal capability is intact; the boundary instructions no longer carry
an OHR2 transaction at all), so the seven OHR2-era scripts re-derive
mechanically with no engine change. That re-derivation then completed
the same day (E-SAP239-OHR2-REPIN-001): all seven green twice
byte-identically via first-appearance-cap advancement from a
144-probe ±1 bisection, guards and census pins intact, no cap reaching
the 796 wall; census now 26 of 43 era scripts green. The B2 cap stage
(E-SAP239-B2-CAPSTAGE-001) then re-pinned file_size green twice (cap
414252829, census 506, guards byte-identical) and proved history_budget,
preload1, and logical_files wall-entangled negatives — their asserted
events occur nowhere below the 796 wall — folding them into ticket 796's
unblock list (13→16 windows); census 27 of 43 green, all 16 reds
explained and tracked. The 796 storage law then implemented
(E-SAP239-REPO38D123-001): mode-2 admission of the
`storage/<fnv1-hex>/data.jsn` family (34-byte capacity, 63-slot
append-only-name pool, snapshot codec v2 with v1 byte-exact), moving
the 2.39 wall from 442856246/2176971322 to the next unknown writable
path at `stop=compat-refused pc=0x000920b4
instructions=474153646 virtual_time_ns=2208268722`; pre-wall and
cross-wall snapshot resumes verified equivalent to direct runs; era
re-derivation of the 16 wall-entangled windows follows under 777.
First 777 B3 result (2026-09-27): wbsto_cache re-derived green twice
byte-identically (layer-off sentinel 0x00070378 at the unchanged
72774982/521257564 boundary - E-ULS-0041 class, identical at f413e23;
layer-on advanced checkpoint = the new-wall stop, cold log sha
a1e305b5…; session-cache trigger held, preload-result pinned absent -
verified 0 at f413e23 too; reset/underflow guards held), census 28 of
43 green with the census reproduced twice (census1 == census2
byte-identical, and the red set identical on the f413e23 baseline
binary: the 796 law flipped no script green<->red). Second-wall RE
cracked (E-SAP239-REPO38D123-001 census): the refused write is the
storage family's NESTED form, storage/f8572579/data.jsn =
FNV-1("/dive/surfacetimesnapshot"), with nested sub-key forms visible
in guest RAM; the flat-path crack is a bounded negative for
ac100d90/faed64e2 (pathjoin %s%x runtime inputs). Consequence recorded
on 777: the past-wall windows (history/ongoing/quiet_read/zip_read/
general/personal/widgets/gps_five + the C-probe nav harness) are
blocked behind modeling the nested-key admission law, not behind
re-pinning. `make check-era` on the committed tree reports 28 of 43
Sapporo 2.39 era scripts green (the tool counts FAILs; 15 fail),
consistent with the manual twice-run census. Ticket 798 then landed
the refusal-path capture channel (E-SAP239-REFUSED-PATH-001): every
unknown-path refusal names the validated guest path (codes and
machine-visible behavior unchanged; `grep -F -q` prefix anchors
unaffected, exact-line `detail=` pins and pinned log hashes drift by
the ": <path>" suffix — wbsto_cache re-derived twice to
3808c8ff…, census stays 28/43). The first named capture identifies
the second wall as the NESTED storage form
`storage/2e3fa8d2/b51799fe/data.jsn`; the next admission law must
derive the nested-shape rule from the pinned RE (builder
`0x00198960`/pathjoin `0x00198912` composes it), tracked as the 796
continuation.
Verification: `make check` 998 PASS,
`make sanitize` zero findings, all nine 2.35 firmware runners, `check-sdl`,
the scroll/startup/snapshot SDL gates, and 2.35 era scripts all green with
zero pin drift.

### 2.39 era re-pin sweep on the clean-boot engine — ticket 777, 2026-09-29

With the DEEPCLEAN engine (handle recycling, runtime capacity 1037,
named refusals; E-SAP239-DEEPCLEAN-001) taking the cold boot to the
first-frame stop and past it with zero refusals (twice-verified to 40B
instructions deep; E-SAP239-REPINSWEEP-001), the full 43-script 2.39 era
set was re-derived script-by-script (owner-approved). ALL 43 scripts are
twice green at HEAD 9202a06 (final census E-SAP239-REPINSWEEP-002, 0 red
of 43 twice; tickets 777/796/798 closed): 10 hash-only
re-pins plus full anchor-chain moves for widgets, zip_read, quiet_read,
preload1, history_budget, ongoing, activity_budget, ctimer13_inten,
logical_files, and gps_startup (chain + wall+1 refusal law). The 2.35.34
era stays 0 red of 8; `make check` is 1014 tests 0 failed (193 suites).
Lanes landed since: gps_reopen (6320692 — GSTP window replaced by the
UART-fault-to-SYSRESETREQ machine-reset golden) and gps_awake with
its C inspector (4aeb98b — dense pc census proved the old
0x128926/0x12892e retire pcs unobservable; native-IRQ admission
pinned at the compat-hook stop pc=0x1291ce/0x1296f8 under the +1
law), gps_five with its probe rewritten to prefix/idle/refusal modes
(9202a06), and the general/personal budget pair (bf6a481/cd88574; the
"1.966G machine_create" probe-side concern was retested against the
current build and proven stale-lineage — gone). Dead-trigger asserts are re-scoped with in-script notes and
zero-refusal guards, never silently deleted, and the logical-file
maximum_hits=76670 budget stays untouched.

### Sapporo 2.35 bounce family rasters through the 788 codec — ticket 793 scope extension, 2026-09-27

E-SAP-0041-EXT6: the 103 draw-refused repaints are one census family — a
60x60 format-17 asset at SRAM `0x100a490c` with an eased horizontal bounce,
six bursts at setup-walk steps 25-30, two draw-color variants
(`0xff555555` x89, `0xff000000` x14), 31 left-clipped rects (width 1-46)
and 72 full-width. The offline-RE matrix law (mm00/mm11 = 1.0f,
mm01/mm10 = 0, mm12 = -90 exact, mm02 = 60-rect_x1 within 2^-15) plus two
one-ULP composer roundings observed bit-exactly in a twice-reproduced
residual census (mm11 `0x3f7fffff`, mm12 `0xc2b40001`) admit every bounce
quad: the final-binary window ends with ZERO GPU refusals, stop line and
guest instruction count byte-identical to the pre-788 pin, settled
generations drifting host-side as the bounce consumes GPU frames. The
compressed-era script and the navigation tool re-pin cleanly (both run
their windows twice internally; baseline `c8b69fce…`, lower `ac856e52…`,
upper `be0e2e1c…`; the upper window's navigation frame `9b554fd9` is crc-
identical to the pre-788 capture). A distinct vertically-scrolling family
observed in the upper window (4 draws, different clip/rect/mm12 laws) is
NOT admitted and stays fail-closed as the named residual. Two independent
derivations (implementing agent + integrator) produced identical re-pinned
goldens.

### Sapporo 2.35 sustained GPS-awake cadence — ticket 710 instance-9, 2026-09-23

Two longer read-only lane probes (same E-SAP-0048 script, admission ceilings
16 and 64, caps 120 s and 430 s) reproduce twice as identical censuses. All
64 admissions keep the invariant `state=12 pending=10 flags=1,0,0 config=93`
and the fixed 5500 ms rearm; exhaustion at the observed ceiling reproduces the
known retry/assert path. The `sapporo-2.35-gps-awake` layer budget therefore
grows 8 → 64 with predicates, refusal, reset and unit coverage unchanged; the
65th admission still refuses fail-closed. E-SAP-0049 records scripts, log and
census hashes.

The bounded awake firmware gate now ends `stop=budget` at 70 s with eleven
healthy polls (transcript `17cc9087…`); the old ninth-admission refusal pin is
superseded by observation, not weakened. The SDL scroll gate transcript
re-pins to `1d44ea98…`; the only diff against the pre-change transcript is
three `layer-hit` metadata rows (`maximum=8→64`, `E-SAP-0048→E-SAP-0049`) —
frames, generations, CRC and stop tuple are byte-identical, attributed by a
control build that reproduces the old bytes.

New frontier: paired 26B/400 s cold runs stay cadence-invariant through the
55th admission at 306.7 s virtual time, then a high-rate region at PC
`0x000ccac4` burns about 13B instructions per 13 s of guest time until the
instruction budget ends the run at 328.7 s. No reset/assert/refusal occurs;
the lane census is clean at the 430 s cap. Naming that region is the next
ticket-710 instance; no throttling or clock guessing is authorized.

`make check` passes 989 cases; the focused awake module passes 4/4; the
re-derived awake and scroll gates pass. 2.39 era gates are unaffected by
construction (only the profile-pinned fixture budget changed).

### Sapporo 2.35 compressed TSC6A law derived — ticket 788, 2026-09-23

Under the owner-authorized offline-RE evidence class, the format-17 (TSC6A)
block law blocking 2.35 main entry is derived in E-RE-SAP235-TSC6A-001:
12-byte 4x4 blocks (16 2-bit indices, RGBA4444 endpoints E0/E1, 11-bit
constant alpha, unverified auxiliary bits 75..95 that fail closed), QCP-thirds
color table. Twice-reproduced decode of the refused 60x60 draw matches
independent integrator re-runs byte-for-byte (`2f30fe18…`, a compass crosshair
with exact icon symmetry), and the `PXB2` asset container was confirmed in the
pinned resource partition (asset at `0x9db613`, format byte `0x11`). Recorded
ambiguities A1/A2/A3 stay contained by fail-closed refusals. No runtime code
changed; the renderer integration is ticket 793 (`ready`), which draws this one
observed draw behind the capture-pinned tuple. The main-screen and stable
long-session gaps remain open until 793 lands.

### Renderer snapshot integration — ticket 791, 2026-09-22

Version-2 snapshots now preserve the renderer as well as guest/device state:
inherited registers, working pixels, last published frame/generation and the
TSC6A shadow. Loading publishes the saved frame only after every machine
component succeeds. Refused loads preserve machine and renderer byte-for-byte
and emit no frame. Active transactions and missing/wrong codecs refuse.
Version-1 snapshots explicitly refuse; regenerate them from cold execution.

Two cold 2.22 runs retain the previous stop, instruction/time and frame pins.
Their new snapshots are identical (5,952,483 bytes). Restoring immediately shows
Logbook at generation 4510 / CRC `040ebb03`. Paired LOWER continuation ends at
Media controls, generation 4610 / CRC `0cb272ba`, with identical full machine
snapshots. Idle continuation retains the old 60-second checkpoint; only the
initial restored-frame line changes its log pin. A live native window showed
the menu and accepted interaction through the Logbook page.

SDL accepts any first button after restoration and presents the held image
before it waits for input. The README now includes cold menu capture and direct
interactive restore commands. Evidence, full state/pixel hashes, API/format
ownership and validation are recorded in E-EMU-RENDERER-SNAPSHOT-001.

Validation: the regression failed before the fix and passes afterward; four
renderer cases, all 276 snapshot-selected cases, 989 `make check` cases and
984 sanitizer cases pass. Both full onboarding runs pass, the new cold-capture
restore runner and its supplied-snapshot path pass, and all seven 2.35 firmware
runners plus both paired SDL prefixes retain their existing pins. The 2.22
SDL live-input gate also passes. All 155 task contracts validate.
No ticket status is changed. Remaining gaps include 2.35 device/layer snapshot
codecs, compressed main-menu icons (788), finite GPS fixture budgets and the
existing 2.39 era drift (777/783). This is usable 2.22 menu restoration, not a
claim of full Sapporo function coverage.

### Post-branch firmware regression audit — ticket 789, 2026-09-22

The seven 2.35 firmware runners now pass twice. The 2.22 live-input and both
2.35 SDL prefixes also pass twice, preserving their frame CRCs and all
reset/refusal/layer-budget checks. A control build with only the old F57F
decoder restored passes all ten original gates, attributing the drift to
E-CPU-F57F-001. Ordered device event payloads remain identical before/after.
E-EMU-SAPPORO-BRANCH-GATES-001 records every changed pin and raw-log hash.

The 2.22 onboarding regression now requires native Navigation-to-Logbook
selection, a normal harness exit, and active execution through 60 virtual
seconds. The old fatal halt is no longer treated as completion. Its disabled
manual-time control retains exactly eleven GPS pulses and the expected
compatibility refusal. The complete revised runner passes twice, completing
all eleven ticket runners. Independent cold, idle and disabled-control captures
also match byte-for-byte in pairs. Ticket 789 is ready for integrator review;
its index status remains unchanged. `make check` passes 985 cases,
`make sdl` passes, and 154 task contracts and advisory line checks pass.

Nine runners change (five 2.35 firmware scripts and four SDL scripts), plus
README/status/evidence. All 334 engine/header/profile files remain unchanged;
pressure/production runner pins are unchanged. Sanitizers were not repeated
for shell/documentation-only work; E-CPU-F57F-001's 980 passing cases still
cover this engine. No ticket status is changed and no 2.39 gate is re-pinned.

Snapshot restoration remains incomplete. A paired LOWER replay after restoring
the Logbook-selected snapshot renders Media controls and remains active to
41 seconds, but publication restarts at generation 1 and idle restoration
publishes no initial frame. Required integration: versioned backend snapshot
hooks, atomic machine restore and initial-frame presentation, preserving both
the last published image and current drawing surface, inherited registers
and TSC6A semantic shadow. The public backend interface currently lacks those
hooks; no private workaround is added. The 2.35 compressed-icon/GPS limits,
2.35 device/layer snapshot gaps, and other-profile validation gaps remain.

### Previous Thumb branch correction clears the 2.22 heap loop, 2026-09-22

Bounded CPU maintenance fixes a dispatch collision: `F57F AF87` is a Thumb
conditional branch, but the interpreter treated it as a barrier/no-op. In
2.22 software double addition this made `1.0 + 0.0` return zero, leaving a
script loop counter unchanged and recursively wrapping the same menu object
until allocation failed. E-CPU-F57F-001 records the architecture reference,
paired lane probes, captured increment and before/after regression.

The corrected increment matches Renode in 103 instructions, all 16 general
registers and every byte of 1,441,792 bytes of copied RAM. The old decoder
takes 127 instructions and leaves the counter at zero. No heap sizes,
firmware bytes or compatibility layers were changed. The old test that
incorrectly called three Thumb branches barriers is replaced by exact branch
target/state assertions; real Thumb barrier coverage remains.

Paired cold button walks now finish step 31 at `user / 0800009e /
8500057344 / 38818426902 ns`; their logs and snapshots match byte-for-byte.
Paired idle resumes reach `budget / 000d4a8c / 8807319394 /
60041792981 ns` without the former fatal loop, reset or refusal. A fresh
visual capture shows Navigation selected at step 30 and Logbook selected after the next
button press. This verifies native main-menu rendering and one selection
change; broader functions remain untested. Snapshot resume preserves execution
but publishes no initial frame in this case, so the SDL input gate cannot yet
provide a ready-to-use restored menu.

Verification: `make test TEST_FILTER=cpu_thumb32` passes 34 cases; `make check`
passes 985; `make sanitize` passes 980; `make sdl` passes. The 2.22 live-input
runner fails historical stop pins. Of seven 2.35 era runners, pressure and
production pass; block erase, OHR, GPS startup/reopen/awake fail their first
transcript hash comparison; retained pairs are identical. Ticket 789 owns
fresh paired derivation and attribution; no firmware golden was changed.
The 2.39 era suite was not repeated after this shared CPU correction, so its
pins may have additional drift beyond tickets 777/783. Other profiles were
not revalidated. The 2.35 compressed-texture evidence and ongoing GPS gaps
remain. Existing roadmap statuses are unchanged.

### Previous 2.22 heap investigation, 2026-09-22

E-EMU-SAP222-HEAP-001 narrows the fatal script-engine allocation failure;
it does not fix it. The current build reproduces the historical 14-billion
instruction checkpoint and snapshot byte-for-byte. A paired control omitting
the final scripted Down press still reaches the fatal loop, so that press is
not required for the failure.

At the first final 80-byte allocation failure, 754 live blocks consume 64,932
bytes of pool capacity. Only four smaller blocks remain free; every fitting
class is exhausted. Of the live blocks, 649 were allocated between 35 and 40
virtual seconds. Their retained capacity is 53,820 bytes. These counts are
reconciled against the captured pool layout and free lists, not inferred from
an incomplete free-call trace.

Paired Renode replays match the interpreter's allocator scan and a synthetic
successful allocation exactly. The first garbage-collection retry also
matches: 364,254 instructions, all 16 registers and all 1,441,792 copied RAM
bytes. It releases no pool capacity from this captured state. These are
isolated comparisons using emulator-derived input, not a cold lane proof of
the preceding allocation history. The next diagnostic target is the creation
and retention of the live graph around 35–40 seconds. Heap inflation and
assertion bypass remain unsupported.

This maintenance changes only this status and the evidence ledger. `make
check` passes 985 cases; no runtime, profile, golden or ticket status changes.
Sanitizers and profile era sweeps were not repeated for documentation-only
work. The 2.35 codec evidence blocker and other profile gaps remain.

### Compressed-texture diagnostic maintenance, 2026-09-22

The 2.35 main-entry refusal now identifies `compressed source 60x60 stride
180` and explains that only the 480x480 semantic shadow is modeled. The
acceptance predicate, unsupported status, pixels and publication behavior are
unchanged. The synthetic descriptor regression fails before the change and
passes afterward; paired captured-command replays preserve all 115200 bytes
and publish zero frames. E-EMU-TSC6A-DIAGNOSTIC-001 records the results.

`make check` passes 985 cases, `make sanitize` 980, and the focused NEMA
selection 120. The SDL build passes. Full firmware walks and profile era
sweeps were not repeated for this diagnostic-only change; previous profile
gaps and potential era drift remain. Ticket 788 still lacks a positive codec
reference. Vendor research found documented conversion tools, but no usable
decoder in the inspected public repositories. The requested exception to the
lane-only oracle rule remains pending; no decoder behavior is authorized by
this maintenance change.

### Latest 2.35 boundary — Done, then a compressed-texture refusal

A longer button sequence passes phone pairing without connecting a phone,
reaches the time-zone selector and displays `Done`. In this run 2.35 obtains
a usable time value, so manual date/time entry is unnecessary. Its clock
source has not been traced. Opening main triggers a graphics refusal and reset;
the Done screen does not prove a usable watch.

E-EMU-SAP235-MAIN-TSC6A-001 records paired first-fault traces and complete draw
captures. The source is a compressed 60×60 TSC6A image (format 17, stride 180),
not the supported 480×480 semantic transition surface. A valid CMDRINGSTOP
store receives `unsupported mask resolve state`; HardFault occurs at
`1be85a / 7486616865 / 32533939561 ns`, 79 instructions before reset.

The read-only lane also refuses the captured command in two bounded replays
and preserves every destination byte. It provides no compressed-pixel reference
for this asset. Enlarging the shadow or treating it as uncompressed would
invent pixels. Ticket 788 tracks the missing decoder evidence; no existing
ticket status, runtime behavior or golden changed in this investigation.

Verified frames: Time/date `d13391e9`, time-zone selector `13021279`,
Done `1c1f9064`. All private captures remain outside Git. The five-layer SDL
preview remains available for three-button input. Main rendering, ongoing
GPS, 2.35 snapshots and other profile gaps remain. This turn adds evidence
and planning. `make check` passes 984 cases and `make check-task-contracts`
validates 153 tickets; the previous sanitizer verification remains applicable.


### Previous 2.35 boundary — phone-instructions scroll (ticket 787)

The format 06 fault from E-EMU-SAP235-SCROLL-001 is fixed by a bounded
RGBA4444 strip renderer. E-NEMA-RGBA4444-001 records paired native draw
captures and native, synthetic and refusal lane replays. The interpreter
matches every byte of the native 115,200-byte output (SHA-256
`ae6001a5…95ee05c`) and both synthetic outputs, including skewed fractional
sampling and transparent borders.

Only the observed 240×96 source, shader, white tint, bilinear sampler and
240×240 RGB565 target are accepted. Existing integer software binary32
arithmetic is shared through `include/semu/fpu_math.h`; the CPU arithmetic
implementation and guest FPSCR are unchanged. Source bytes are checked
individually through the memory-only bus API to reject narrow MMIO overlays.
The draw is staged before mutation. Malformed state, missing program writes,
mapping overflow, overlapping source/target and allocation failure refuse
without publishing or modifying committed pixels. The existing observed
black clear still takes precedence over inherited texture state.

Two SDL walks pass LOWER on the phone instructions and match fully:
step 15, generation 1756, CRC `f0ff828c`; endpoint `budget / 000e1862 /
4896065682 / 22000000000 ns`. The screen shows the lower pairing directions
and watch name. `tools/test_sdl_sapporo_235_scroll.sh` pins that bounded
checkpoint; it does not label the walk as completed onboarding. Seven focused
cases cover lane pixels, 33 invalid states, 11 missing-program variants,
memory holes/overlays, later-child rollback/retry, allocation/mapping
atomicity, signed clipping and inherited-texture clear.

A further pair reaches the native `Later` button (step 17, CRC `895a642c`)
and, after selecting it, the phone-connection recommendation (step 18,
CRC `e0c2f63d`). Both finish at 40 seconds with 5,570,776,834 instructions
and no reset/refusal. The next work is navigating this recommendation and
verifying the remaining setup.

The previous GPS fixture ceiling, 2.35 snapshot gaps, 2.22 onboarding
allocation failure and 2.39 era failures remain. The full 2.39 era sweep was
not rerun here; its pins may have drifted after this shared renderer addition.
Ticket 783 owns the existing 30-of-43 failure audit. Physical panel behavior,
phone pairing, ongoing GPS and full watch functionality remain unproven.

Verification: 984 normal checks, 979 sanitizer checks, 119 NEMA cases,
23 FPU cases and 152 task contracts pass. The expanded seven-case RGBA suite
also passes separately under sanitizers. All seven 2.35 private firmware
gates pass. Existing 2.22 and 2.35 SDL input pins are unchanged. The new paired
22-second SDL scroll runner passes on the final build. E-NEMA-RGBA4444-001
records exact commands, transcripts and hashes. Ticket 787 remains ready
for integrator review.

Changed files: `include/semu/fpu_math.h`; CPU private `fpu_softfloat.h`;
display `nema_rgba4444.c/.h`, `nema_backend_draw.c`, `nema_state.c/.h`;
`tests/unit/test_nema_rgba4444.c`; the new optional SDL runner;
README/status/evidence; planning ticket 787 and its index row. The state
validator scope was explicitly added before its edit to retain the lane's
required zero-register presence. No CPU arithmetic, scheduler, GPU framing,
MMIO, profile, old golden, snapshot format or compatibility budget changed.

### Previous 2.35 boundary — bounded GPS awake support (ticket 786)

`--layer sapporo-2.35-gps-awake`, requiring the explicit initial and reopen
layers, adds eight synthetic GPIO24 pulses at the observed native poll.
Each starts after100ms and lasts1ms. Both lane experiments reproduce twice:
eight pulses preserve the normal polling path for60s; zero pulses lead to
GSTP/retry and the GPS assertion. E-SAP-0048 records the provenance and exact
censuses. This is finite bring-up support, not physical GPS cadence or fixes.

The cold interpreter now reaches54.6 virtual seconds, then refuses its ninth
admission at1259fe /1579930110 instructions /54642979249ns. Paired observer
traces show native IRQ handling restoring awake=1 between polls. All six GPS
layer orderings are covered by focused tests; a reversed CLI order also
produces the identical cold transcript. Reset cancels pulse events and clears
bindings; invalid dependencies/state and exhausted budgets remain fail-closed.

The SDL setup window extends through weight and height to phone-pairing
instructions. The generic walk's default LOWER presses merely adjust HEIGHT,
so its31-step voluntary exit is not onboarding completion. A separate
confirmation-button walk verifies the later phone screens. Pressing LOWER on
the phone instructions triggers a graphics-submission fault and native reset
at19.2s (E-EMU-SAP235-SCROLL-001). Paired diagnostics isolate unsupported
source format06; the lane has an exact RGBA4444 pairing-strip implementation
for the next rendering investigation. The awake fixture does not fix that
failure. GPS acquisition,
phone pairing, full onboarding and stable long sessions remain unproven.
The 2.35 snapshot and earlier2.22/2.39 gaps remain open.

Verification: 977 normal checks and972 sanitizer checks pass. Twelve focused
GPS cases (four new,36 refusal variants), thirteen transport cases and151 task
contracts pass. The old two-layer SDL language-menu transcript is unchanged.
All seven 2.35 private firmware scripts pass. Detailed private-run results
and hashes are in E-SAP-0048.

Changed files: new awake compatibility module/header, unit test and private
firmware runner; existing device context/reset/binding and machine
registry/dispatch; README, status, evidence and ticket/index786. No generic
CPU, scheduler, GPIO, UART MMIO, profile or snapshot contract changes.
Ticket786 remains ready pending integrator review.

### Previous 2.35 boundary — GPS reopen and setup controls (ticket 785)

The explicit `sapporo-2.35-gps-reopen` layer requires the startup layer and
adds exactly two delayed synthetic responses: the post-reopen status and an
exact GSR reply. Native parsing reaches states 7,8,9,10,12 with zero retry.
The layer validates its dependency, firmware hashes, driver state and callback;
unknown requests, exhausted budgets and scheduling failures latch a refusal.
Reset cancels pending RX and clears bindings. E-SAP-0047 records the paired
lane/interpreter observations, negative control and implementation checks.

All four 2.35 layers together reach the native Welcome, birth-year and
unit-system screens under SDL button control. Two bounded setup walks match
byte-for-byte, including the unit-system frame at generation1274 / CRC3e13459a.
The next unsupported command is `@GSTP`: the cold run stops at
`001be85a / 1020576082 / 16288236023`, and the input-driven walk at
`001be85a / 3544601348 / 15965172775`, both `compat-refused`. No response,
awake heartbeat, location/time data or instruction patch is invented.

Verification: eight GPS cases (four new, including 43 refusal variants),
thirteen transport cases, 973 normal checks and 968 sanitizer checks pass.
All six 2.35 private firmware scripts pass. Existing initial-only and two-layer
SDL pins remain unchanged; reversed GPS option order gives the identical cold
transcript. 150 task contracts validate. Full onboarding, ongoing GPS,
2.35 snapshots, the 2.22 allocation failure and the 2.39 era audit remain open.
Ticket785 is implemented for review; its status remains unchanged.

Changed files: new `src/compat/sapporo_235_gps_reopen.c/.h`, GPS unit test and
private firmware runner; existing device context/reset/binding files and
machine registry/dispatch; README, status, evidence and planning ticket/index.
No generic CPU, scheduler, UART MMIO, public include API, profile or snapshot
format changed. The prior 2.39 era failures remain; that sweep was not repeated
for this profile-specific addition.

### Previous 2.35 boundary — initial GPS exchange (ticket 784)

`--layer sapporo-2.35-gps-startup` adds exactly two synthetic status lines
through the existing UART RX/IRQ transport: unsolicited startup and the
response to exact `@VER`. It requires the three pinned 2.35 hashes, is disabled
by default, and owns its ordered two-hit budget per machine. The lane positive
and wrong-prefix control each reproduce twice (E-SAP-0046).

Native parsing reaches GPS states 2,14,15 and closes the UART with retry zero.
Two interpreter observer runs match completely. The later reopen arms pending
seven, gets no response, and retries initial startup. The layer refuses that
extra intervention at `001254ec / 996415389 / 14881889213`, before an assertion
or reset. This is the next unsupported lifecycle; no awake pulses, fabricated
position/time or instruction patches are added.

The 500M prefix is `budget / 000ee120 / 500000000 / 3343660033`. Existing
two-layer firmware and SDL pins are retained. With the new layer, the SDL
language menu still settles at generation 79, CRC32 `405422e1`; its time/PC
change because GPS now follows the native success path. Full onboarding,
GPS reopen/ongoing behavior, 2.35 snapshots and the earlier 2.22/2.39 gaps
remain open. Verification details and hashes are in E-SAP-0046.

Verification: four focused GPS cases (including 31 refusal variants), thirteen
CXD transport cases, 969 normal checks and 964 sanitizer checks pass. All five
2.35 private firmware gates pass; the new gate repeats both startup and the
later refusal with exact hashes. Both old and new-layer SDL runs reproduce
their own transcripts. 149 task contracts validate. E-SAP-0047 additionally
recorded a paired lane census for the later ticket785 integration above. Other profiles retain the prior limitations, including
the 30/43 failing 2.39 era scripts from the shared erase audit.

Changed files: `src/compat/sapporo_235_gps.c` and `.h`, existing device
factory/context/binding files, `src/boards/machine.c` and `machine_run.c`,
`tests/unit/test_sapporo_235_gps.c`, the new GPS private firmware runner,
README, this status, evidence ledger, and planning ticket/index 784.

### Previous 2.35 boundary — storage recovery and display (ticket 782)

The previously ignored 64 KiB flash erase (`DC`) now reaches the storage
endpoint. Native 2.35 creates, closes and reopens `logs/entries.bin`; its
read/update handles are 90/a0. The former `LogbookEntryDb.cpp:53` assertion
is passed. With both explicit production and OHR layers, startup renders
“Select language” (240×240 RGB565, generation 6, CRC32 `3bd12ac8`). The native renderer
publishes it through the regular display callback.

E-SAP-0043 distinguishes the unmodified lane's missing erase from the controlled
experiment: sixteen existing 4 KiB erases recover the file in two identical
lane censuses, corroborating the native helper's 64 KiB request and documented
DC opcode. Source firmware and lane files remain unchanged. The controller
now refuses unknown commands while preserving separately observed B9/AB
completion behavior. Aligned storage erases also allocate all needed overlay
pages before mutation, preserving bytes and ownership on allocation failure.

The paired first-visible-frame checkpoint is
`000bdd2a / 813500000 / 3733351422`, `stop=user`. The OHR gate's enabled
500M suffix is deliberately re-derived under ticket 782 to
`000bdc36 / 500000000 / 2719206417`. Its disabled control and the 30M,
80M and 150M production/pressure pins remain byte-identical. All original
no-fixture 2.35 input/idle checkpoints remain unchanged.

Four focused erase/controller cases and the allocation-failure regression
pass. `make check` passes 965 cases; `make sanitize` passes 960. All four
bounded 2.35 firmware scripts pass; 148 task contracts validate. The optional
`sh tools/test_sdl_sapporo_235.sh` repeats the native button sequence through
the language menu, generation 79 / CRC `405422e1`, with an exact transcript.
The existing 2.22 SDL live-input check also passes its exact checkpoint and transcript.
The 2.39 sweep fails 30 of 43 scripts (previously 16): the corrected storage
behavior changes native file paths and checkpoint state. All sixteen prior
failures remain, with fourteen additional failing gates. Ticket 783 tracks
the compatibility/regression audit; no 2.39 pin is changed here.

Remaining boundary: a longer cold observer reaches
`CXD5610GF-driver.cpp:894` at instruction 1022014915, assertion helper 7945e,
then BKPT 79424 at instruction 1022015210. Continuing enters the same class
of downstream list loop as earlier assertions. A longer middle-button
probe opens the language menu with English selected; shorter exploratory
budgets ended during its transition. Full onboarding, GPS startup, snapshots,
the 2.22 allocation failure and
2.39 compatibility/era gaps remain open.

Changed files for this instance: `src/core/storage.c`,
`src/devices/sapporo_flash.c`, `src/soc/apollo4/mspi.c`,
`tests/unit/test_storage_erase_atomic.c`,
`tests/devices/test_sapporo_flash_block_erase.c`,
`tests/integration/test_firmware_sapporo_235_block_erase.sh`,
`tests/integration/test_firmware_sapporo_235_ohr.sh`,
`tools/test_sdl_sapporo_235.sh`, README, this status,
the evidence ledger, and planning ticket/index 782. No public API, profile,
firmware identity or snapshot format changed.

### Previous 2.35 boundary — OHR startup (ticket 781)

`--layer sapporo-2.35-ohr-startup`, together with the production-data layer,
completes the eight E-SAP-0041 startup responses through the existing OHR
transport. Identity strings and zero result bodies are explicitly synthetic;
echo carries the request's ten data bytes. The layer pins all three firmware
hashes, checks command/sequence/state/padding, logs each of eight hits, and
owns its counters per machine. Disabled or unexpected exchanges refuse.
A latched fixture refusal stops with `compat-refused` before a guest reset
can renew the budget; no CPU instruction is patched.

Paired 500M-instruction enabled runs reach
`000bdcfc / 500000000 / 1805389482` with eight responses and no resets.
The 280M disabled control retains the E-SAP-0040 reset and exact log hash.
A longer 1B-instruction observer reaches
`000bdcfc / 1000000000 / 2305389482`, zero frames. The PC is inside a
firmware sorted-list insertion loop after an earlier `LogbookEntryDb.cpp:53`
assertion: opening `logs/entries.bin` returned zero. This storage path is the
next evidence gap.
The lane also reaches a BKPT boundary at `79424` after its startup exchanges;
that boundary is diagnostic evidence, not proof of a usable UI.

Verification: four focused OHR cases, 960 normal-check cases and 955 sanitizer
cases pass. All three bounded 2.35 firmware scripts pass; 146 task contracts
validate. Exact commands, hashes and changed files are in E-SAP-0041.

The production and pressure regression pins remain unchanged. Full UI stability,
2.35 snapshots, the 2.22 allocation failure and the 2.39 era gaps remain open.

### Previous 2.35 boundary — haptic integration (ticket 779)

E-SAP-0040 adds scoped IOM4 haptic startup at address 0x50. The lane's 105
startup transactions reproduce twice: two-byte configuration writes, one-byte
autotune/calibration reads, and five-byte waveform writes. The emulator now
completes these with the lane's four-plus-one chunking and owned reset state.
Unsupported command shapes, registers, DMA state and endpoint switching during
an active command refuse before mutation. Other profiles retain their existing
controllers; 2.35 snapshots remain explicitly unsupported.

The earlier pressure-only wait below is superseded. A paired 150M-instruction
run now reaches `000cd0c8 / 150000000 / 1258826798`, no reset. At instruction
261789155, virtual time 1567178637, firmware resets after OHR command 0010
is refused in BSL state. Paired 280M runs reach
`000a6bc8 / 280000000 / 1590729375` with that one reset. There is no working
2.35 UI claim and the OHR gap needs a separate evidence-scoped instance.

Ticket 779 explicitly re-derives the pressure runner's suffix while retaining
the original 30M prefix and strict log hash/no-reset checks. The 80M production
prefix remains unchanged. See E-SAP-0040 for exact commands and hashes.

Verification: 17 focused IOM4 cases, 956 normal-check cases and 951 sanitizer
cases pass; 145 task contracts validate. Both bounded 2.35 firmware scripts
pass. The broader 2.22 fatal-allocation issue and 2.39 era failures remain
unchanged and are not hidden by these passing focused gates.

### Sapporo stability review — 2026-09-19

The sections below include historical milestones and retired stop tuples;
they must not be read as a claim that all Sapporo versions pass today's
firmware gates. This review against `5980046` reproduced:

| Firmware | Verified boundary | Remaining gap |
| --- | --- | --- |
| 2.22.60 | Short SDL input gate passes at `000bacf4 / 774081920 / 6520939902`; the long walk reaches main-menu CRC `fb8e0155`, generation 4403. | Full onboarding gate fails: an 80-byte script allocation exhausts its retries, enters the fatal handler and eventually loops at `0005a8f8`. |
| 2.33.16 | Validated 100M-instruction run reaches `000a4c78 / 100000000 / 372326454`. | Two firmware reset requests at `000c97f2`; no working UI claim. |
| 2.35.34 | Validated cold boot repeats `000e1862 / 122878688 / 300000000000` with empty traces. | The production-data layer selects normal mode 5; 0x5c/0x5d negative pressure probes now pass without reset. Haptic startup and the explicit OHR layer now complete. Execution then waits in a sorted-list insertion loop without UI; 2.35 snapshots unsupported. |
| 2.39.20 | 27 of 43 era scripts pass; the 16 failures exactly match the retained baseline failure set. | The era gate fails; bounded GPS/UI limitations and unresolved checkpoint differences remain release gates. |

Bounded maintenance fixed three concrete 2.35 safety defects:

- IOM4 DMA now validates every mapped byte and stages source bytes before
  touching FIFO, gauge, interrupts or destination memory. Mapping holes,
  ROM destinations and one-byte device overlays refuse atomically; adjacent
  readable memory regions work. The existing lane-observed invalid-target
  status/IRQ behavior is unchanged (E-EMU-SAP235-DMA-001, E-SAP-0036).
- Snapshot save/restore explicitly refuse live RTC/IOM4 state, which the
  codec does not serialize. Previously a cold snapshot could be accepted
  while omitting that state. This is a refusal guard, not full snapshot
  support (E-EMU-SAP235-SNAPSHOT-001).
- The live RTC is now owned by its SoC. Previously, creating a second
  Sapporo machine cleared the first RTC and replaced its scheduler/IRQ sink.
  Independent clocks/alarms and owned-event cancellation on destruction now
  pass, including scheduler-reset ID reuse (E-EMU-SAP235-RTC-OWNER-001).

The requested file-size policy is advisory in `AGENTS.md`, the checker,
contributor/testing guides and task template. A synthetic 501-line file
warns and exits zero. The README button mapping now matches the unchanged
board wiring: upper/middle/lower = GPIO57/58/59 (E-SAP-BUTTONS-001).

Verification of the runtime fixes:

```sh
make test TEST_FILTER=sapporo_iom4
make test TEST_FILTER=apollo4_snapshot
make test TEST_FILTER=sapporo_rtc
make check
make sanitize
make check-lines
make check-task-contracts
git diff --check
```

The focused checks pass 13 IOM4 cases, three SoC snapshot cases and 11 RTC
cases. At that stage, `make check` passes 939 cases (including five SDL cases),
`make sanitize` passes 934, and 143 task contracts validate. The four DMA
regressions, live-profile snapshot regression and three
RTC ownership regressions fail before their respective fixes. Exact commands,
external log hashes and before/after 300-second firmware checkpoints are
recorded in the evidence entries.
`make test-firmware TEST_PROFILE=sapporo-2.35.34
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.35.34.18929/firmware.semu`
validates the three components but selects no 2.35 shell era script; the
bounded CLI pairs and the private input/RTC test cases provide its actual
runtime coverage. No firmware, checkpoint pixels, or raw private logs are
added to Git, and no expected hash or stop reason is weakened.

The broader review also ran `make check-sdl` with the automatically detected
private 2.22 manifest. Its short input walk passes, but its long completion
walk fails (make exit 2) at `stop=budget pc=0x0005a8f8
instructions=40000000000 virtual_time_ns=69612174532`. The step-30 main-menu
CRC and generation match; the historical `stop=halt` tuple does not. The
disabled-case branch is not reached because the completion check exits first.
This is a reproduced pre-existing failure, not a passing onboarding release
gate. The final `sh tools/test_sdl_live_input.sh` also passes after the RTC
ownership correction. Static inspection of the hash-pinned 2.22 application
confirms that `0005a8f8` branches to itself. The follow-up below traces the
allocation failure that reaches it; no expected stop was changed
(E-EMU-SAPPORO-AUDIT-001, E-EMU-SAP222-PANIC-001).

`SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin make check-era`
finishes with 16 failures out of 43 scripts (make exit 2). The failure names
exactly match the retained E-SAP-ERA-GATES-239-001 census, with no additional failing script.
The full derived list and log hashes are in E-EMU-SAPPORO-AUDIT-001. This is a
single review sweep, not new lane evidence or a claim that the failures are
harmless. Ticket 777 currently allows only six scripts, most already passing;
repair of the full failing census needs an integrator scope update before
re-deriving any pins.

Changed runtime/test files are `src/devices/sapporo_iom4.c`,
`src/devices/sapporo_iom4_internal.h`, `src/devices/sapporo_iom4_regs.c`,
`src/soc/apollo4/apollo4_snapshot.c`,
`tests/devices/test_sapporo_iom4_dma.c`, and
`tests/devices/test_apollo4_snapshot.c`. RTC ownership additionally changes
`src/devices/sapporo_rtc.c`, `src/devices/sapporo_rtc.h`,
`src/soc/apollo4/apollo4.c`, `src/soc/apollo4/apollo4_internal.h`,
`src/soc/apollo4/auxiliary.c`, `src/boards/machine.c`,
`tests/devices/test_sapporo_rtc.c`, and
`tests/devices/test_sapporo_rtc_ownership.c`. Supporting changes are `AGENTS.md`,
`README.md`, `tools/check_source_size.sh`, `docs/contributing.md`,
`docs/testing-strategy.md`, `plans/task-template.md`, this status, and the
evidence ledger. No roadmap status or public interface changes.

Full 2.35 checkpoint integration needs an explicit integration scope for
the two codecs, scheduler ownership validation and deterministic cold/resumed
execution. RTC ownership is now isolated; unrelated device ownership is not
covered by that claim. Ticket 710 remains the route for
the next evidence-backed firmware behavior; ticket 777 tracks era repair.

### Follow-up: 2.35 manufacturing gap and 2.22 panic — 2026-09-19

Ticket 710 instance 8 adds `sapporo-2.35-production-data`, an explicit layer
pinned to all three 2.35 component hashes. Two lane runs per configuration
show that absent manufacturing records select mode 2; the synthetic record
selects normal mode 5. The integrated emitter reproduces the accepted sector
byte for byte. Identity/calibration fields remain synthetic, with no physical
calibration claim. Populated unrelated storage and invalid invocation refuse
before mutation; counters belong to each machine (E-SAP-0038).

The paired firmware regression reaches `000932a8 / 30000000 / 35339893`
without a reset. Extending to 400 ms reaches the next distinct gap: an IOM2
read at address `5c`, command `0f000112`, has no attached verified device.
The firmware requests a fault reset at instruction 73523963, virtual time
396214443; the bounded run ends at `000cc418 / 77309520 / 400000000`.
Both log pairs match exactly. This advances bring-up; there is no usable 2.35
UI claim or change to roadmap status. Pressure-sensor attachment/behavior
needs the next ticket 710 instance, with its own lane evidence.

The private input harness also now rejects an overfull timeline and time
conversion overflow, stops on machine refusal/no progress, and fails on an
invalid or explicitly missing firmware manifest. Before this repair it could
write beyond its event array, hang, or report a malformed fixture as a skip.
Three new regressions pass; existing input checkpoints are unchanged
(E-EMU-SAP235-INPUT-HARNESS-001).

The 2.22 long-walk failure is now traced: an 80-byte script allocation fails
initially and through ten retries, then invokes the fatal handler with
`uncaught error`. The guest asserts at `peScriptEngine.cpp:46`, retires the
BKPT at the historical halt tuple, and returns to the self-loop at `5a8f8`.
Two diagnostic replays are identical. The pool's allocation history still
needs comparison with the lane; no heap expansion, assertion bypass or golden
change has been made (E-EMU-SAP222-PANIC-001).

Verification at that stage: `make check` passes 947 cases
(including five SDL cases), `make sanitize` passes 942, `make check-lines`
passes with advisory warnings, `make check-task-contracts` validates 143
indexed tickets, and `git diff --check` passes. The five new production-layer
cases and the paired 2.35 firmware runner pass. Existing full 2.22 onboarding
and 16-of-43 2.39 era failures remain as recorded above; they are not included
in a claim that the Sapporo release suite is green.

Additional files are `src/compat/sapporo_235_production.c`, its internal header,
`tests/unit/test_sapporo_235_production.c`,
`tests/integration/test_firmware_sapporo_235_production.sh`, the existing
board-layer attachment in `src/boards/machine.c`, and
`tests/devices/test_sapporo_235_input.c`. Exact commands, log hashes and the
derived lane/diagnostic censuses are retained in the evidence entries.

### Follow-up: 2.35 negative pressure probes — 2026-09-19

The lane disproves a positive LPS22 attachment for this profile: it probes
absent sensors at 0x5c and 0x5d, obtains zero identities, and continues with
the existing pressure sensor. E-SAP-0039 records two identical derived
censuses and halted-controller tests. The latter overwrite an A5 sentinel
with zero and produce INTSTAT 442, DMASTAT 2, DMACFG 100 and DMATRIGSTAT 4.

Ticket 778 explicitly scopes the controller integration needed by ticket 710
instance 9. The new module recognizes only the observed one-byte identity
command under the 2.35 IOM2 profile. Wrong command, length, direction, state,
ROM, hole or device overlay refuses before mutation. Other profiles retain
their previous controller behavior. No LPS22 is attached and there is no
arbitrary absent-device fallback. Direct IOM2 snapshots refuse, consistent
with the already unsupported 2.35 machine snapshot.

The former pressure-fault suffix is replaced by a stricter no-reset progress
check: `000e1862 / 73528280 / 494546055`, with one production-layer hit.
The unchanged 30M prefix still matches E-SAP-0038. The new paired three-second
runner reaches `000e1862 / 122589556 / 3000000000`, also without a reset.
The 400-ms requested limit ends at 494546055 ns because a sleeping CPU
advances through a scheduled event; this is the existing run-loop granularity,
not a precise 400-ms checkpoint claim.

A longer observer pair reaches `000e1862 / 203436260 / 30000000000`, no
reset and zero frames. Its eleven IOM2 commands match the lane's pressure and
magnetometer sequence through the 23-byte 0x35 read; subsequent OHR traffic
seen in the lane is not reached. That startup wait is the next diagnostic
boundary. There is still no usable 2.35 UI or completed release-gate claim.

Final checks for this step pass: `make check` has 952 PASS records,
`make sanitize` has 947, `make check-lines` passes with advisory warnings,
`make check-task-contracts` validates 144 tickets, and `git diff --check`
passes. The five pressure regressions and both firmware runners pass. The
full 2.22/2.39 release limitations recorded above remain open.

Changed files for this step: `src/soc/apollo4/iom_sapporo235.c`, `iom.c`,
`iom.h`, `iom_internal.h`, `iom_snapshot.c`, `apollo4.c`,
`tests/devices/test_sapporo_235_pressure.c`, the new
`tests/integration/test_firmware_sapporo_235_pressure.sh`, and the preceding
production runner. Planning adds ticket 778 and its index row; its status
remains ready for review. Public include headers and firmware files are unchanged.

### Shared implementation

- Dependency-free C99 headless build and optional SDL3 build.
- Strict profile and firmware manifests with exact size and SHA-256 checks.
- Fail-closed little-endian memory bus and stable integer-time scheduler.
- Immutable storage bases with sparse session-only program/erase overlays.
- Bounded machine execution, structured stop reasons, and semantic board input.
- An in-tree ARMv7E-M/Thumb-2 interpreter with exception, NVIC, SysTick,
  sleep/wake, DSP, and single-precision FPU coverage; a deterministic synthetic
  RTOS guest exercises nested interrupts, context switching, WFI, and FP state.
- Evidence-scoped Apollo4 clock, power, reset, GPIO, timer, STIMER, UART, IOM,
  MSPI, DMA, and MRAM models with unknown-offset and invalid-shape refusal.
- Sapporo flash, pressure, motion, magnetic, haptic, ambient-light, fuel-gauge,
  GPS, and OHR transports with deterministic fixtures and refusal tests.
- Exact Sapporo 2.22.60 component metadata and an opt-in, hit-bounded synthetic
  manufacturing-state compatibility layer.
- NEMA command framing/state, RGB565/A2LE rasterization, native-renderer frame
  publication, SDL3 presentation, semantic three-button input, and versioned
  input replay.
- Identity-pinned machine snapshots covering CPU, guest RAM, scheduler events,
  Apollo4 controller state, Sapporo device state, flash overlays, NEMA state,
  virtual time, and compatibility counters. Snapshot load is atomic on a
  malformed or incompatible image and keeps firmware/resource files external.
  Save and restore reject unsupported scheduler kinds, unowned scheduler
  entries, detached device-owned events, and mismatched event identities
  before the scheduler queue is committed.
- Compatibility snapshot restore now rejects excessive aggregate/per-trigger
  counters, inconsistent totals and sum overflow. Runtime intervention commits
  enforce the aggregate bound. Stale 2.22 aggregate metadata is corrected to
  twenty, matching its unchanged individual allowances. GPS final-fragment
  refusal/WAIT, delayed-RX scheduling errors and the legacy startup response
  retain transport/counter/log state (E-EMU-COMPAT-ATOMIC-001). Valid 2.22
  snapshots and 2.39 logo/halt checkpoints remain byte-identical.

All normal tests use synthetic inputs. The repository contains no firmware
bytes; the six README screenshots are the owner-approved pixel exception
documented above.

## Authentic-Firmware Boundary

On 2026-08-19, user-supplied OTA components matching all three profile hashes
were validated with the named compatibility layer. Two fresh bounded OTA-only
runs produced byte-identical stage logs and reached:

```text
time_ns=0 production-data
378713110 ohr-startup
1011008641 gps-state-startup
1209669280 gps-startup
4806834547 resource-status
5192876953 diap-worker-wake
5192877054 diap-worker-irq
SDL first-frame width=240 height=240 generation=3
stop=user pc=0x0009a3fc instructions=450900000 virtual_time_ns=5333307331
```

The run reached production startup, OHR/GPS startup, the OTA resource-status
boundary, NEMA initialization, and the first native command submission without
a reset, assertion, or device refusal. The NEMA model now consumes the observed
marker-only completion transaction and raises the evidenced CLID/INTERRUPT
completion. The SDL3 frontend receives a non-black 240x240 renderer frame from
the authentic firmware command stream. A2LE sub-LSB rounding remains an
explicit software-renderer approximation; physical-panel completion, panel
wire bytes, and generic factory-runtime behavior remain unsupported.

The renderer also now carries the observed binary32 affine matrix registers
through NEMA snapshots and applies the native TSC6A semantic shadow/resolve
path without decoding private compressed bytes. The observed TSC6A target
triangles, A2LE masks, and 480x480-to-240x240 resolves are accepted
transactionally; unknown programs and geometry still refuse. Two fresh SDL
dummy runs with middle (5.400/5.470 seconds) and lower (8.000/8.070 seconds)
semantic button pulses were byte-identical through the language/profile
transition and reached the bounded budget without renderer refusal. This
exposes the evidenced setup UI in SDL. The three declared Phase 5 renderer
goldens remain exact for their named checkpoints; this does not claim
physical-panel equivalence or bit-identical behavior for every unobserved
A2LE/TSC6A edge. The
`--until middle-language` replay checkpoint still stops at the first
post-input non-black setup frame. Live SDL checkpoints wait for a bounded
350-ms virtual-time quiet window after the last post-input renderer submission,
so an intermediate logo/text transition is not frozen as the interactive frame.
SDL button edges hold active-low for 70 ms of guest time and keep the released
level stable for 70 ms before another press, matching the native debounce
boundary (E-SAP-LIVE-0001). The SDL first-frame diagnostic also includes a
bounded CRC32 of the presented RGB565 bytes; the live checkpoint accepts only
visible pixels from a
new renderer generation, excluding stride padding and stale submissions.

SDL also accepts the `middle-language`, `lower-transition`, and neutral
`setup-next` checkpoints without `--input-replay`. In that live mode Arrow Up,
Return/Enter, and Arrow Down are delivered through the semantic input mapper.
Left clicks in the upper, middle, and lower window thirds use the same path, and
the SDL window requests focus when its first validated frame creates the native
surface. After a quiet settled post-button frame the window pauses for the next
live button edge so the setup UI can be navigated manually. The named checkpoint
button is required only for the first edge; subsequent setup edges accept any of
the three mapped buttons.
Pressing that edge returns control to the guest immediately and rearms the next
settled frame; replay checkpoints retain their deterministic stop behavior.
The optional authentic `check-sdl` flow now queues one SDL Return key-down/up
pair and two successive middle-screen mouse clicks. From the initial validated
frame CRC32 `2a01c517`, it verifies settled setup checkpoints `4979f432`,
`629da47e`, and `d4ed66c7`, then exits through an SDL quit event at the
repeatable checkpoint `pc=0x000bacf4`, `instructions=774081920`,
`virtual_time_ns=6520939902` (E-SAP-ONBOARD-EMU-012). The former 011
checkpoint included a nonfatal haptic timeout from an incorrect register
selector; the existing haptic corrections remove it. Cold live-input checks
also require the complete transcript SHA-256. The neutral
`setup-next` checkpoint is also available for a snapshot-loaded, middle-button
replay continuation; it reports the first visible post-input frame without
naming an unverified screen. Invalid automation configuration is always
checked and fails closed; absent private firmware skips only the authentic run.

The opt-in `SEMU_SDL_LIVE_TEST=setup-walk` diagnostic drives the authentic
2.22.60 onboarding from a fresh boot. With the corrected board mapping (semantic
upper/middle/lower → GPIO57/58/59, matching the verified `sapporo_wiring.c`
table; a prior build had upper/lower inverted to 59/57), steps 1–11 advance on
the middle button through the profile screens, and step 12 settles on the
`Connect with mobile` frame (`261712ad`). From a fresh boot that screen advances
only on the middle button; a middle press then reaches the `Continue the setup
on your phone` handoff (`ea3bc5f8`), and three lower presses settle three more
post-handoff frames: CRC32 `9b58f243`, `eb868d29`, and `ed7eeb7a` (the rendered
`Watch info: SUUNTO 9 PEAK PRO` / "Later" step). MIDDLE on `ed7eeb7a` then
settles the phone-pairing recommendation sequence `74a5e6ab`, `b26dd658`,
`0b93f6c9` ("Connect / Connect Later"), `a797ec30`, and MIDDLE on `a797ec30`
settles `8362b9bc` "Time/date" (`w-tida`, ~23.0 s virtual). MIDDLE on
`8362b9bc` *does* advance the onboarding: it opens `w-ltim`, whose
"Searching for GPS" ring animates every ~1–4 ms of virtual time and therefore
never reports a settled frame, so the frame-stepped walk cannot observe the
advance and no further step-driven press is possible there
(E-SAP-ONBOARD-EMU-010 corrects the earlier phone-gated reading of this
boundary). Every walk without a time-scheduled driver stops byte-stably at
`pc=0x0010fbde`, `stop=compat-refused`, when the background GPS
power-cycling exhausts the eleven-hit `gps-awake-pulse` budget
(E-SAP-COMPAT-GPS-005) at virtual time ~71 s.
The time zone screen `w-ltim` (next view after `w-tida`) puts
`Navigation/State=1`, waits 5 s, then subscribes to `Dev/Time/LocalTime`:
`t >= 1646092800` (2022-03-01Z, unix seconds) routes to the UTC-offset menu and
then `w-done`; otherwise it shows "Search for GPS" whose down button
("Set manually") opens `w-year`. The manual chain `w-year` → `w-mont` →
`w-day` → `w-time` → `w-tset`/`w-done` (which auto-opens `main` after 3 s) is
fully present in the resource. The 2.22.60 application binary has no
standalone clock source: the authoritative path is the `GpsTimeSynchronizer`
worker (NMEA GGA/RMC time-of-day), and no OHR2/phone time-sync command exists
in 2.22.60. Two experiments bound the alternatives: a valid time-bearing
GGA/RMC/EPU NMEA group injected on the `@GSR` running-status exchange makes the
firmware issue repeated deliberate `SYSRESETREQ` writes from `0x000be93e`
(synthetic NMEA time is not usable), and a gated value at RTC `0x40004820`
leaves the onboarding byte-identical (the register does not feed
`Dev/Time/LocalTime`). Onboarding **does complete** standalone through the
firmware's own manual-entry chain, verified deterministically: MIDDLE on
`8362b9bc` opens `w-ltim`; an opt-in time-scheduled LOWER press
(`SEMU_SDL_SETUP_WALK_TIMELINE`, ~30 s virtual) hits "SET MANUALLY" and opens
`w-year`; the walk's POST letters then confirm `w-year` → `w-mont` → `w-day` →
`w-time` (its `next` button toggles hour/minute focus; the second MIDDLE saves
hour+minute+local) → `w-done` "Done" → `main`, with settled CRCs
`5321867e`/`c683e828`/`cd1b0979`/`455b603a`/`53d3f0c1`/`17e1772c`/`578e2601`/
`1c62ab1a` and final main-menu frame `fb8e0155` (Navigation/Logbook/Media
controls); the run then idles and stops at `stop=halt` ~43.8 s virtual (no
phone time source needed, and no `OHR2` time command exists to build one).
Note: a manual time below the `1646092800` gate (e.g. 2022-01-01) still
finishes the wizard, but the next boot routes to `n-sync-rec` instead of
`main`; year ≥ 2023 (or month ≥ Mar) keeps `LocalTime` post-gate
(E-SAP-ONBOARD-EMU-008/009/010).

E-SAP-ONBOARD-EMU-007 recorded that, under the pre-fix board mapping, replayed
upper/middle/lower presses were delivered to the guest's `INPUT_READ1` button
polling yet the 2.22.60 guest submitted zero NEMA command lists on the
`Continue the setup on your phone` screen across all press patterns and a
51-second idle continuation. That "inert button" symptom is now explained by
the inverted upper/lower mapping (semantic lower drove GPIO57, the physical
back pin, instead of GPIO59, the bottom/Skip pin); with the corrected mapping
the same press advances the onboarding, so the conclusion in 007 that the
handoff button path is inert and that the post-handoff view definitions are
missing from the resource is superseded by E-SAP-ONBOARD-EMU-008. The
post-handoff onboarding strings (`Skip` `750d4b9f`, `Time/date` `6a5c2eec`,
`Time zone` `6072e392`, `All done!` `2bb84502`) and their view definitions
(`w-conn-1`, `w-tida`, `w-ltim`, `w-year`, `w-mont`, `w-day`, `w-time`,
`w-done`) are present in the recovered 2.22.60 resource. Skipping phone pairing
now works; the walk advances through the phone-pairing recommendation screens
to the `Time/date` step (`w-tida`, `8362b9bc`), one internal-viewset advance
before the `w-ltim` "Time zone / Search for GPS" screen, and stops at ~73.1 s
when the eleven-hit `gps-awake-pulse` budget (E-SAP-COMPAT-GPS-005) is
exhausted (E-SAP-ONBOARD-EMU-009).

The next native transition is now provenance-pinned as E-SAP-ONBOARD-001: an
English-row selection reaches a native software-rendered `Define your profile`
frame (raw SHA-256 `0930d2cc...`). The capture is comparison evidence only;
its source marks physical NEMAP output and complete watch UI as unavailable, so
the emulator keeps `setup-next` neutral until an equivalent guest contract is
reproduced.

The deterministic emulator comparison is recorded as E-SAP-ONBOARD-EMU-001.
Two identical bounded `setup-next` probes stop at the first visible frame
(generation 2, raw SHA-256 `0096f059...`, CRC32 `bbf3549e`); a longer diagnostic
continuation's last distinct frame is generation 59 (raw SHA-256 `688ca665...`,
CRC32 `d4ed66c7`). Neither matches the native onboarding capture. The external
snapshot and replay are hash-pinned but have no provenance sidecar, so this
remains a diagnostic gap rather than authorization for a screen-specific
checkpoint or pixel golden.

The normal `-O2` runtime now dispatches successful SCS accesses directly to
SysTick, NVIC, or SCB instead of constructing speculative refusal diagnostics,
and it calls the 2.22 compatibility dispatcher only at its exact, hash-pinned
trigger PCs. CPU state and virtual-time accounting are cached across ordinary
one-tick instruction steps, while WFI jumps, reset, and refusal paths still
read the scheduler directly. On the same host, five SDL dummy continuations
from the identity-pinned pre-frame snapshot to the fixed 650,800,000-
instruction limit averaged 5.284 seconds before the accounting changes and
4.700 seconds after them, an 11.1% wall-time reduction. Both paths produced
first-frame CRC32 `4979f432` and stopped at `pc=0x0009a7e4`, 650,800,000
instructions, and 15,707,372,848 ns of virtual time. This is a host performance
measurement; guest execution and virtual time are unchanged.

Fixed-width bus access now uses explicit little-endian 1-, 2-, and 4-byte
operations and bypasses the general region search only when the regular-region
cache proves that no overlay can apply. A five-run paired continuation against
commit `07ad14c` averaged 4.658 seconds before these bus changes and 4.180
seconds after them, a further 10.3% wall-time reduction. All ten runs produced
the same output SHA-1 `c0db13d31ba201deeee2453d328aa4027c990b59` and the same
stop checkpoint above.

The CPU instruction path now uses an internal, exact 16-bit bus fetch that
keeps overlay and fault handling unchanged while avoiding generic-width work
on cached ROM/RAM hits. The regular-region cache stores invalidation-safe
pointers instead of reconstructing them from indexes. A second five-run paired
continuation against commit `44e545b` averaged 4.132 seconds before and 3.880
seconds after these changes, a further 6.1% wall-time reduction, with the same
output SHA-1 and stop checkpoint.

The machine run loop now updates the public logger timestamp field directly
after its existing null check instead of making an out-of-line call after every
instruction. Five paired runs against commit `f17e638` averaged 3.874 seconds
before and 3.786 seconds after this change, a further 2.3% reduction, with the
same output SHA-1 and stop checkpoint.

The reset boundary diagnostic now records the request PC, LR, SP, R0–R3, xPSR,
runtime reset count, compatibility hit total, and virtual time without changing
guest execution. OHR2 also emits a bounded 64-event-per-device-lifetime
transaction/ready trace with command, sequence, BSL/MAIN state, and completion
status. With the exact private Sapporo 2.33.16 package, two fresh bounded runs
are identical (E-SAP-0015): the guest requests `SYSRESETREQ` at `0x000c97f2`
with `compat_hits=0` and then continues to the bounded budget. The exact
2.22.60 no-layer run likewise repeats `SYSRESETREQ` at `0x000be93e` with
`compat_hits=0`; the opt-in layer's OHR trace reaches BSL identity/configure,
MAIN transition, result, and echo exchanges with matching ready edges and no
refusal (E-SAP-0016). These are reproducible emulator observations, not a
later-version behavior fix; a native reset-register or post-reset transaction
trace is still required before changing Apollo4 reset semantics.

The later-Sapporo audit is recorded as E-SAP-0017, and the first executable
2.39 C-emulator boundary is now recorded as E-SAP-0018. The 2.33 MSPI power
change remains a static hotfix candidate without a recovered failing runtime
state, while the exact 2.39 image now has a concrete fail-closed sequence: its
HardFault handler resets after a precise access fault at the missing Apollo4
DSP0 memory-power register `0x40021058`. Fully reverted diagnostic experiments
show that the six-register DSP0/DSP1 enable/status/retention cluster advances
the image to the next distinct initialization boundary. The image writes
watchdog CFG at `0x40024000`, configures Reset/BoD routing at `0x40000000`, then
accesses watchdog INTEN at `0x40024200`. The 2.39 storage/UI probes still
require their native owner events; the power/reset/watchdog result does not
authorize fabricated storage opens. The exact built-in `sapporo-2.39.20`
profile pins all three component
hashes under ticket 706. Ticket 711 now implements the evidenced DSP0/DSP1
memory enable/status/retention registers, including reset and snapshot state;
two byte-identical firmware runs advance the first reset from 11,897,027 ns to
11,897,251 ns. Ticket 712 implements only the evidenced watchdog CFG register,
including reset, snapshot, reserved-bit, selector, offset, and width checks;
two byte-identical runs advance three more instructions to the precise
Reset/BoD fault and first reset at 11,897,254 ns. Ticket 713 implements only
RSTGEN CFG, with a zero reset value, valid bits 0..1, strict access refusal, and
snapshot validation. Two byte-identical post-713 logs (SHA-256
`540b62b500147fffa74f44f5ba4d0f1f02c9fa1c7713512a8834c78700fee7b5`)
advance the first reset to 11,897,266 ns and stop at the next unsupported
watchdog INTEN access. Ticket 714 implements only WDTIEREN at offset `0x200`,
with reset zero, valid bits 0..1, strict access refusal, and snapshot
validation. Two byte-identical post-714 logs (SHA-256
`493e50f23db1149402e2aadb20cbe6108c7e37fec27be6700e23c8821d5c35fa`)
advance the first reset to 11,897,284 ns. The next unsupported access is the
watchdog restart-key write at `0x40024004`. Ticket 716 implements only the
write-only `0xb2` restart command and zero readback, without inventing timer or
expiry behavior. Two byte-identical post-716 logs (SHA-256
`0a092da13d76a589b189bc43a22461bd5e280d68e791927d81dd2193f96958f6`)
advance the first reset to instruction 19,945,598 and virtual time 25,285,493
ns. Ticket 717 adds only deterministic zero-valued CHIPID0/CHIPID1 reads,
matching both PAC reset values and the pinned reference environment, while
keeping writes and all other MCUCTRL identity offsets refused. Two
byte-identical post-717 logs (SHA-256
`3d182aea65869a4414579e79ce5f942610570257b606d5b99b71c3f2674a483a`)
advance the first reset to instruction 19,948,596 and 25,288,491 ns. The next
precise fault is CTIMER auxiliary offset `0xe8` refusing firmware value
`0x3f`. Ticket 719 identifies that address as OUTCFG26 and accepts only the
observed whole-register value while retaining strict refusal and the existing
reset value. Its snapshot validator now accepts the same value without a
format change. Two byte-identical 30,000,000-instruction logs (SHA-256
`8e079f452fdc7f6485d6688746a1db93f0688fe517b01f1ca295ad6db5e8cb23`)
advance the first reset to instruction 24,771,518 and 30,111,413 ns. The next
precise fault is a 32-bit USB CLKCTRL read at `0x400b2000`; firmware PC
`0x000f8c02` reads it before PC `0x000f8c08` writes `0x02000000`. Ticket 721
implements exactly that zero read and trace-matching no-output write. Two
byte-identical 100,000,000-instruction logs (SHA-256
`06e69fa86a9034a491bc7381a4b51b6dea538e9d326d29fb81bba10945d79374`)
advance the first reset to instruction 77,220,237 and 368,259,842 ns. The next
precise fault is CTIMER observed-pattern offset `0x104` refusing value
`0x00012300` at firmware PC `0x000f7d60`. Ticket 722 accepts only that
temporary pattern value and preserves snapshot validation. Two byte-identical
200,000,000-instruction/2,000,000,000 ns budget runs (SHA-256
`96e428de7caf01f866ed3a91193a7e45ff2c37d700a63a9deb9764d8f0506890`)
now contain no reset or unsupported access. They reach the firmware WFI/ISB
idle path and stop at PC `0x000e955a`, instruction 84,856,118, virtual time
6,372,873,793 ns solely because of the configured time budget. Ticket 723
then exercises the production storage path with an explicit, hash-gated
synthetic full-flash fixture. The Sapporo flash endpoint now applies the
reference model's NOR page-program rule (`old & requested`) before calling the
unchanged strict storage API, so requested zero-to-one bits remain clear
instead of causing a bus fault. Two production logs are byte-identical
(SHA-256
`21c415dee3c52ef4f77f0da42c3ea4020e0c7dc0e7092bf3bcab08d2c6661cc5`)
and advance beyond the former `0x0010272a` HardFault to instruction
41,435,661 at 278,677,270 ns. The new independent fail-closed boundary is a
write of `0x00004000` to CTIMER address `0x40008068` at PC `0x000cb882`;
the completed flash DMA is no longer the fault owner (E-SAP-0026). Ticket 724
identifies that write as Timer7 CMP0's write-one-to-clear INTCLR bit and maps
it to the timer model's channel-7 pending/IRQ state. Two complete logs are
byte-identical (SHA-256
`32a5bc1df226ca20da0c94aa90dc121fea14125f45890397f3671d6cb95c0b33`)
and advance 22 instructions and 22 ns. The next precise fault is the same
firmware routine writing `0x00004000` to CTIMER INTEN at `0x40008060`, with
stacked PC `0x000cb854` (E-SAP-0027). Ticket 726 accepts and retains only that
additional INTEN value, preserving the existing IRQ model and snapshot format.
Two complete logs are byte-identical (SHA-256
`5e0d8dd23c863aaa00b44489d9235b4967183d44d26d49f538f094e55e6affe0`)
and advance the first reset to instruction 49,456,422 at 441,085,096 ns. The
next precise fault is IRQ21's handler reading `0x00004000`, ORing Timer0 CMP0
bit zero, and writing combined INTEN value `0x00004001` at PC `0x000f7d7e`
(E-SAP-0028). Ticket 727 accepts only that combined value. Two complete runs
are byte-identical (SHA-256
`db1ba19b3e47306cf304b52f32b86db8dd4aa97f6afc6c64d962f7fdf24e43d8`),
contain no reset, and halt at PC `0x00079e1e`, instruction 72,774,982,
virtual time 521,257,564 ns. The halt follows firmware `BKPT #0`; its caller
identifies `StartupClient.cpp` line 67, so the next boundary is application
startup-state reverse engineering rather than another unsupported MMIO
transaction (E-SAP-0029).

Ticket 728 resolves that application boundary with the opt-in,
exact-hash-pinned `sapporo-2.39-synthetic-wbsto` layer. Read-only firmware
tracing identifies `WbStoPreload` command zero as the owner: four persisted
WbStorage files omitted from the compact OTA fragment return native status 204,
which the provider collapses to 500. The layer validates the exact empty native
session-cache vectors, installs four explicitly synthetic values, and
translates only that final command-zero preload result after revalidating the
cache byte-for-byte. Layer-off behavior remains the E-SAP-0029 halt. Two
layer-on runs are byte-identical (SHA-256
`b1156669803cbd2c09e16599fa3719ff2adeecb493eb3749e20fcec34b8f37c0`)
and advance without reset to the next fail-closed boundary: a native 32-byte
FAT-cache write to unmapped `0x0f676e34` at PC `0x0007038c`, instruction
78,496,951 and virtual time 526,979,533 ns
(E-SAP-COMPAT-WBSTO-239-001). This confirms that the compact OTA fragment is
not a coherent writable filesystem; the address must not be broadly mapped.

Ticket 729 extends that same exact-build layer with an in-memory logical-file
adapter at the firmware's public wrapper ABI. It retains only native bytes
created for eleven observed writable paths, enforces the reference's exact
per-path size ceilings, passes missing read/update opens through to native
storage, and refuses unknown writable paths, modes, handles, ranges, or budget
excess before mutation. Two exact runs and their snapshots are byte-identical;
the firmware creates `settings/sync.txt` (0 bytes), `settings/uiv2.txt` (235),
and `settings/general` (1505) in 118 operations, with no return of the FAT
underflow. A saved checkpoint resumes exactly into the next reset one
instruction later. Exception-frame recovery pins that reset to an IOM2 access
at `0x40052120` from stacked PC `0x0014e8ee`, matching the reference's I2C
probe of address `0x5c` (E-SAP-COMPAT-FILES-239-001). No generic IOM or sensor
response is yet implemented.

Ticket 731 implements that independently evidenced boundary as
E-SAP-LPS22-239-001. The board factory now adds a strict LPS22HB endpoint at
I2C address `0x5c` only for `sapporo-2.39.20`; 2.22 and 2.33 continue to
refuse that address and retain byte-identical device snapshots. The IOM2
adapter forwards the command's high-byte selector only for the newly evidenced
address. Authentic firmware performs the reference's exact fourteen reads and
writes, observes identity `0xb1`, completes both self-clearing CTRL_REG2
commands, and finishes with CTRL_REG1 `0x1e`. Two 79,000,000-instruction runs
and snapshots are byte-identical (log SHA-256
`fa74015d06b9aa988787724e666f4e37c1223c14fc36f996cd773b2a93d61592`,
snapshot SHA-256
`75f0f534bfc9aae60adabd642f0d4fa982146ed9a743abb7e584aa0e1664e290`),
stop at PC `0x000a7b2e` and virtual time 520,829,069 ns, and contain no reset.
Snapshot continuation advances one instruction identically. A longer
exploratory run reaches a distinct firmware reset request at instruction
122,452,650 and virtual time 1,230,991,337 ns; its owner is not inferred here.

That reset is now attributed to an IOM4 haptic selector adapter error rather
than firmware reset policy. Native command `0x22000112` reads autotune register
`0x22` at address `0x50`, but the adapter supplied stale byte `0xa0` from a
fixed DMA-adjacent location. Address-scoped command-selector forwarding makes
the poll succeed without changing the haptic endpoint or compatibility layer.
Two fresh runs stop byte-identically before the next strict boundary at PC
`0x0014e8ea`, instruction 122,457,908, virtual time 1,230,996,595 ns (log
SHA-256 `f1c41ec3d40617174d8cbb299883c69b028a6bfb445b44a0bb7aaf2622915354`,
snapshot `629ba604acfbb1eb1265a755283c6133b9650d45dcf2c44d9439752365e22247`).
The next command is the evidenced haptic calibration read `0x23000112`;
registers `0x23`/`0x24` remain intentionally unsupported pending a bounded
fixture contract (E-SAP-IOM4-HAPTIC-239-001).

Ticket 733 supplies that bounded fixture under E-SAP-HAPTIC-CAL-239-001.
After autotune completes, the strict haptic endpoint now exposes registers
`0x23` and `0x24` as separate, one-byte, read-only zero values. Zero is the
hash-pinned reference endpoint's reset fixture, not a recovered physical
calibration. Writes and multi-byte spans refuse before mutation, and the
fixture adds no writable state or snapshot bytes. Two fresh authentic runs
advance byte-identically to PC `0x000cceb2`, instruction 357,033,113, virtual
time 1,878,381,357 ns (log SHA-256
`3aee5f2f3271f54448ab2ca681908e6dfa766348b4dfbe0e2099add4c7b24ca7`,
snapshot `c287c2c1e256e55c100b083a6db1b35730a646ad9aabeea21600347873a9e95e`)
without reset or a new compatibility hit. The next instruction is a native
word read from GPIO address `0x40010218`; one-instruction continuation takes
the precise fault vector at PC `0x001c0db4`. GPIO offset `0x218` remains
fail-closed pending independent register evidence.

Ticket 734 identifies that boundary as GPIO WT1 under
E-SAP-GPIO-WT1-239-001. WT1 is the output-state register for pins 32–63;
firmware operation 1 reads it to query pin 53. The existing WTS/WTC state is
`0x00040000`, making pin 53 low. Only aligned 32-bit WT1 reads are added;
direct WT writes and the other three banks remain fail-closed, and no state or
snapshot byte changes. Two fresh authentic runs advance byte-identically to
PC `0x0014e8ea`, instruction 359,772,704, virtual time 1,881,120,948 ns (log
SHA-256 `48c514ba4504a25122c60e71e2b3966fba9463edc9a3641b4ba3ab5854ff9e02`,
snapshot `20febdf889a8d8baf7d146f6a1f1bcac6182d009b4ad3ed4b4ace4bd9f28d81a`)
without reset or a new compatibility hit. The next instruction submits OHR2
command `0x0010`, sequence zero, while the endpoint is in BSL state; its strict
refusal enters the precise fault vector.

Ticket 735 implements that command under E-SAP-OHR2-BOOT-239-001. The native
2.39 trace defines data byte `0x01` plus forty-nine `0xff` bytes and a reply
whose data is all zero; the same exchange occurs in both BSL and MAIN without
changing state. The strict transport accepts only that exact payload, and the
profile-selected 2.39 device provider supplies the observed reply without a
compatibility layer or hit. OHR reset now also drives its evidenced low ready
level onto GPIO62, so the first reply produces the required low-to-high edge
and is consumed normally. Two exact runs and snapshots are byte-identical at
PC `0x0014e8ea`, instruction 359,790,038, virtual time 1,881,138,282 ns (log
SHA-256 `b8977bf8911cc5435e19afc109205c267e822249c17e19fba3809779d046664e`,
snapshot `b7d1d84e2be435635cc6031b8424ece436b6557d3ba3883c59f92b7550916f86`),
with ready high/request success/ready low/response success, no reset, and the
same 118 compatibility operations. The next instruction submits the already
known identity command zero, sequence one, in BSL; its 2.39 response body is
not yet wired and therefore refuses into the precise fault vector.

Ticket 736 implements that BSL identity body under
E-SAP-OHR2-ID-BSL-239-001. A dedicated 2.39 physical provider requires all
fifty request data bytes to be `0xff`, returns the otherwise-zero body with
`BSL\0` at payload offsets 9..12, retains the ticket-735 boot-mode response,
and refuses every unimplemented 2.39 body without falling through to the 2.22
compatibility fixture. Two exact runs and snapshots are byte-identical at PC
`0x0014e8ea`, instruction 368,947,987, virtual time 1,890,296,231 ns (log
SHA-256 `c5599a2faf3016cdeb85bbb2cd6951f70fad49d6732639bbda861d7f5348c1ed`,
snapshot `2a823cb69c1bdb7463233c553a2e55312c462bca99aa1715246cb1fd3866d690`).
They complete BSL identity, fire-and-forget reboot, and the second boot-mode
exchange in MAIN with no reset and the same 118 logical-file operations. The
next instruction submits MAIN identity command zero, sequence three; that
body remains a strict refusal and enters the precise fault vector.

Ticket 737 implements MAIN identity under E-SAP-OHR2-ID-MAIN-239-001. The
same dedicated 2.39 physical provider validates the all-`0xff` request and
selects the exact otherwise-zero `MAIN\0` body from modeled MAIN state; BSL
identity and boot-mode behavior are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 368,958,374, virtual time
1,890,306,618 ns (log SHA-256
`d82ebc5b061787b8cefad7f64f7b70168858bc8da29adb644cd486211a8bfc22`,
snapshot `352cdedcec47360eb478c6eec3649534025c7373b19c9c35c90e3922549c8a81`).
They complete MAIN identity without reset and preserve the same 118 logical-
file operations. The next instruction submits result command `0x000d`,
sequence four; it remains a strict refusal and enters the fault vector.

Ticket 738 implements MAIN result command `0x000d` under
E-SAP-OHR2-RESULT13-239-001. The dedicated 2.39 physical provider validates
the all-`0xff` request in modeled MAIN state and returns the exact all-zero
body; existing response bodies are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 368,995,288, virtual time
1,890,343,532 ns (log SHA-256
`eb76c862ba97bd1b0f5ae569b62dcfd3544ecf39d06e3b791de22ce57c2f2331`,
snapshot `5359e0cdf8f62514c88b6a90cb381e40c55811a748fcf5b510319268680100f4`).
They complete command 13 without reset and preserve the same 118 logical-file
operations. The next instruction submits result command `0x000e`, sequence
five; it remains a strict refusal and enters the fault vector.

Ticket 739 implements MAIN result command `0x000e` under
E-SAP-OHR2-RESULT14-239-001. The dedicated 2.39 physical provider validates
the all-`0xff` request in modeled MAIN state and returns the exact all-zero
body; existing response bodies are unchanged. Two exact runs and snapshots
are byte-identical at PC `0x0014e8ea`, instruction 369,026,992, virtual time
1,890,375,236 ns (log SHA-256
`8e1e68584a5d869f91c07216add9d6d1befbc36aac26718113359b76298fd1c4`,
snapshot `d7c30abd8ff1744c1644b2730953d45012c547977b24c905873b37fa2533b8e0`).
They complete command 14 without reset and preserve the same 118 logical-file
operations. The next instruction submits echo command `0x0006`, sequence six;
it remains a strict refusal and enters the fault vector.

Ticket 741 implements the exact MAIN echo under E-SAP-OHR2-ECHO-239-001.
The dedicated 2.39 physical provider accepts only the complete native body or
the distinct complete body generated by the exact deterministic guest state,
then returns all fifty data bytes unchanged. Two exact runs and snapshots are
byte-identical at PC `0x0014e8ea`, instruction 369,037,329, virtual time
1,890,385,573 ns (log SHA-256
`2ef900dbf79d08a83e94c2e6d8e642c53b51fa40d3faaca977319ac64f4d59cf`,
snapshot `5168ba1e48997e23553370705d49f4ea0f83c407337576bb3c7a56cacb308686`).
They complete echo without reset and preserve the same 118 logical-file
operations. The next instruction submits command `0x0002`, sequence seven; it
was a strict generic-command refusal at that ticket's boundary.

Ticket 742 integrates command `0x0002` into the shared OHR2 enum/MAIN-state
registry and exact 2.39 provider under E-SAP-OHR2-CMD2-239-001. The complete
all-`ff` request produces the observed all-zero body; BSL, malformed bodies,
and absent/legacy providers still refuse atomically. Packet framing, ready
edges, sequence rules, and snapshot format are unchanged. Two fresh authentic
runs and snapshots match at PC `0x00079e1c`, instruction 393,235,868, virtual
time 1,914,584,112 ns (log SHA-256
`9161895c12da70077ec78fb76bae6062196194a80f1df5b8c9609876fa20b17a`,
snapshot `c36512287d4bf7d5a06762334ba261d984d0259a1466ec83076e73ebb253dcb0`).
Command 2 completes without reset/refusal and execution performs 449 existing
logical-file operations without increasing the compatibility budget. A
one-instruction resume executes the real firmware BKPT and stops at
`pc=0x00079e1e`, instruction 393,235,869, time 1,914,584,113 ns. No normal
frame has been reached. E-SAP-STARTUP-SLEEP-239-001 identifies the next
investigation: `StartupClient.cpp`'s failure path for `sleepln`, command zero,
result 500. The underlying cause is not yet established. This is an evidenced
reference startup response, not a claim of physical OHR measurement support;
ticket status remains integrator-owned.

Ticket 743 traces that sleep failure to a missing logical-file size query,
not bad header data. Native header validation succeeds; public wrapper
`0x00092244` then falls through with a synthetic handle and reports zero
instead of the retained 17,888 bytes. The adapter now returns the actual
length through its existing hash-pinned, opt-in, hit-bounded file operation.
It leaves cursor/data/snapshot format unchanged and refuses stale synthetic
handles and exhausted budgets. No startup-status translation was added.
The firmware's own `sleepln` callback now returns 200. Two fresh runs and
snapshots match at PC `0x00079e1c`, instruction 405,895,301, virtual time
1,927,243,545 ns (log SHA-256
`8139068b549a4e2be4baf57c94bc3b8eff385cb2bbb8efca506a8b30469de0d8`,
snapshot `0fa411dde053a15ef42d1b4ce2bf7282ad1990f88532b059dcf1c14ca9824193`).
The run contains 512 logical-file operations, including sleep size 17,888 and
training size 2,384, without a reset or device refusal. One more instruction
halts at PC `0x00079e1e`, time 1,927,243,546 ns. This distinct StartupClient
failure belongs to `TrainingTss`, command zero, result 500; its file-size
check now succeeds, but the subsequent failure's cause remains unresolved.
No normal frame is claimed (E-SAP-COMPAT-FILE-SIZE-239-001).

Ticket 744 corrects the seek wrapper's success return under
E-SAP-COMPAT-SEEK-239-001. The native ABI returns the requested offset bit
pattern, not a zero status or the computed absolute cursor. The adapter's
existing signed cursor calculation, range checks, state, and hit budget stay
unchanged. TrainingTss now reads all 42 native 56-byte records and returns
startup status 200 without a callback override. Two fresh runs and snapshots
match at PC `0x00079e1c`, instruction 416,256,851, virtual time
1,955,213,393 ns (log SHA-256
`4f8e749ebe80774b968a091dda8086aabeb85229e6dd08b916cb615e24e6497e`,
snapshot `92e7f05339ff218402f0b788a6263dc8173c81fba30d56d4cf8c66c1bab09a16`).
There are 595 logical-file operations, with no reset/device refusal or source
flash change. One-step resume executes BKPT and halts at PC `0x00079e1e`,
instruction 416,256,852, time 1,955,213,394 ns. The new StartupClient failure
is `WbStoPreload`, command one, result 500; the existing synthetic cache layer
handles command zero only. The command-one missing state still needs tracing,
and the current milestone remains short of a normal frame.

Ticket 746 traces that command-one failure to the same four unavailable
persisted records (E-SAP-COMPAT-PRELOAD1-239-001). It adds a separate one-shot
`wbsto-preload1-result` intervention, requiring both prior cache interventions
and byte-exact revalidation of the unchanged synthetic cache. No new values or
file bytes are supplied. Existing per-trigger limits remain unchanged; total
layer capacity becomes 2,674. Older two-/three-counter snapshots restore with
the appended counters at zero; unknown counts still refuse atomically.

The advanced checkpoint is PC `0x000921dc`, instruction 435,333,559, virtual
time 1,974,290,101 ns: log SHA-256
`476cf8603492ff3cfc456a61babd1c0cc7c5347b4bc81a7e8a39594e595f9b71`,
snapshot `27b6e51ee66569d9e68ef56c7608c280cfd4e48f6cfe5d4beb1aaaf68f4fa79f`.
The preload translation fires once at 1,955,180,209 ns. Execution then reaches
the unchanged 2,671-operation logical-file limit during a sleep-record scan,
at cursor 11,480 of 17,888 bytes. The next attempted instruction refuses
without advancing PC/time/count. A real three-counter snapshot from instruction
405,860,000 resumes to the identical new snapshot. The next task is to trace
the repeated scans and establish a justified finite file-operation budget or
identify a native file-contract defect; no budget increase is included here.
No normal frame is claimed. Historical checkpoint tests remain unchanged.

Ticket 747 establishes that the repeated sleep reads are finite native history
queries, not a retry or an adapter ABI defect (E-SAP-COMPAT-HISTORY-239-001).
The firmware performs 42-, 60-, and 42-day windows over 248 records: 35,712
successful 72-byte reads and matching record seeks. Empty histories return
416 normally. The existing file limit is now exactly the measured 75,764
operations to the next independent boundary; the three other one-hit limits,
all paths/capacities and file semantics are unchanged.

The new checkpoint is `stop=budget pc=0x000920b4 instructions=439081594
virtual_time_ns=1978038136`, log SHA-256
`6f47fad1eeb3b6032955b463e2c4ba26310dbf5ddc453ae3f0f350acf15a9348`,
snapshot `3c56bfb5f3f7b541433ca05a3de999c941df3151484a5e080ad09a89b3672ae1`.
The guest has created and read the allowlisted Activity Timeline database.
The next operation is enum-create of `actitmln/ongoing.bin` at wrapper
`0x000920b4`, LR `0x000b944d`; that path remains unknown and is refused
before mutation. Its native schema and bounded storage requirements are the
next reverse-engineering task. No new provider-status translation, persisted
payload, or normal frame is claimed. The ticket-746 pre-refusal log/snapshot
hashes remain exact; its subsequent budget refusal is historical.

Ticket 748 supersedes the ongoing-file refusal using direct native layout
evidence E-SAP-COMPAT-ONGOING-239-001. Only `actitmln/ongoing.bin` is appended,
with the exact 152-byte capacity: 24-byte header, eight padding bytes and
three 40-byte records. Firmware creates every byte, reopens and validates the
header and size, and reads the records without an activity-state repair.
There are 20 ongoing-file operations plus 408 `settings/personal` and 66
`zapp/storage.sbm` operations on already-supported paths. The measured file
ceiling is now 76,258, aggregate 76,261; no diagnostic headroom is retained.
S29F version one accepts only eleven/twelve slots and still emits the old
eleven-slot encoding until the appended file exists. Historical checkpoint
bytes remain unchanged.

The new checkpoint is `stop=budget pc=0x000920b4 instructions=451511675
virtual_time_ns=1990468217`, log SHA-256
`33f75a3051a8487405a4f5221d9db36a806fc54b2cf26fee8ebff5082bf62a7e`,
snapshot `42de6549afe8bef32603a4acd497f2aee0bb41a92a77f59022064c23e194998b`.
Two fresh runs match exactly; the ticket-747 prefix keeps both original
hashes and resumes to the identical new snapshot. The next attempted
instruction refuses open mode nine for `zapp/zwspee01.zip`, LR `0x000843e9`,
without advancing PC/time/count. Native mode semantics and the ZIP
resource path need separate recovery; neither is enabled here. The exact
private test reports no reset/device refusal before this boundary and verifies
immutable source flash. No normal 2.39 frame is claimed.

Ticket 749 lets the exact normalized `zapp/zwspee01.zip` / mode-nine request
execute in firmware (E-SAP-COMPAT-ZIP-READ-239-001). Native disassembly shows
read mode plus quiet open-failure logging; the adapter now leaves CPU, RAM,
files and counters untouched for that pair. The real filesystem returns
handle `0x30`, which firmware later closes. No ZIP slot, fabricated archive,
host overlay, mode translation, hit-budget increase or snapshot change is
introduced. Other unsupported modes and mode-nine paths remain refused.

Two fresh exact runs stop at `stop=budget pc=0x000920b4
instructions=459796107 virtual_time_ns=1998752649`: log SHA-256
`ea04ac89a277fc58cc1c653e59e595f2a40f25d7202afd16b5adc309e6bf2732`,
snapshot `13e104c98a6fdf5a741a15615bf1b77ea53ebe05db39e7bf224cf0818560b5ee`.
The old ticket-748 prefix retains both hashes and resumes to the identical new
snapshot. Logical-file hits remain 76,258. The next attempted instruction
refuses mode nine for `ui/js/config.js`, LR `0x000843e9`, without advancing
PC/time/count. Source flash remains unchanged and no reset/device refusal
occurs before the new boundary. This proves native ZIP open/close and later
startup progress, not archive completeness, installation or a normal frame.

Ticket 751 replaces the ZIP-specific exception with native quiet-read routing
under E-SAP-COMPAT-QUIET-READ-239-001. Exact mode nine passes through only for
validated paths outside the synthetic file table. The twelve table-owned paths
still refuse that mode, absent or present; normal synthetic modes and retained
contents are unchanged. Native firmware owns path resolution, missing-file
errors, content and return values. There is no file substitution, new handle
format, compatibility hit or budget increase.

The firmware opens `ui/js/config.js` with native handle `0x40`, then loads
scripts and styles including `ui/js/fonts.js`. The first traced segment to
600 million instructions records 134 opens over 105 distinct paths. Two fresh
runs match at `stop=budget pc=0x000cb852 instructions=607105617
virtual_time_ns=2146062159`: log SHA-256
`740750cbc6460fe8b9c3b420a5509d992dc0757e50de9102da316df7d21be1ec`,
snapshot `15b5f2076d7107e50b693333d1d19bcd3bd18014b8208c33f02ed8e45676dd19`.
The ticket-749 prefix retains both hashes and resumes to that identical state;
file hits remain 76,258 and source flash is unchanged.

The next instruction writes `0x08004001` to CTIMER INTEN `0x40008060` and
takes the precise fault vector at PC `0x001c0db4`, instruction 607,105,618,
time 2,146,062,160 ns. A longer diagnostic observes the firmware-owned reset
79 instructions later. The added interrupt-enable bit is not supported by
this task. There is no normal 2.39 frame yet; UI resource reads now proceed
natively, and the next independent gap is the timer contract.

Ticket 752 identifies bit 27 as the already-modeled Timer13 CMP1 enable
(E-SAP-CTIMER13-INTEN-239-001) and accepts only the new combined value
`0x08004001`. Snapshot validation accepts the same value without a format
change. IRQ gates, compare deadlines, reset values and compatibility state
are unchanged. Both new regressions fail before and pass after the correction;
all 737 normal and sanitizer cases pass.

Two fresh runs stop identically at `stop=budget pc=0x00079e1c
instructions=608140266 virtual_time_ns=2147096849`, with log SHA-256
`2223de22981528b7cd2df049be68ea2e4022627763da13aab9293ef1fbbf7e16` and
snapshot `74e45df965216d809cf41e090ee0fc56b740affbc6d32aec35413dd65db1aa0c`.
The ticket-751 prefix retains both hashes and resumes to the identical state;
there are no resets/device refusals, file hits stay 76,258 and source flash
is unchanged. The next instruction executes native BKPT and halts at
PC `0x00079e1e`, instruction 608,140,267, time 2,147,096,850 ns.

E-SAP-SERIALIZER-ARRAY-239-001 traces this new failure to
`ChunkSerializer.cpp:38`. Native serialization of the synthetic LID `0xa432`
value reads the JSON bytes at `0x100002b4` as an array count (24,946), then
requests 199,568 bytes from a 16-byte buffer whose cursor is already 12.
The bounds check correctly fails. The synthetic value's full native object/
array ABI needs recovery; this is not permission to enlarge the buffer or
bypass the assertion. No normal 2.39 frame is claimed.

Ticket 753 corrects only that synthetic cache value under
E-SAP-COMPAT-WIDGETS-NATIVE-239-001. Native schema/copy tracing establishes
a twelve-byte empty object instead of JSON: zero scalar, zero array count and
null source pointer. The firmware performs its own copy/pointer relocation
and returns status 200. Entry lengths become twelve, while alignment, arena
extent, other values and every compatibility budget remain unchanged. Both
preload checks validate the complete corrected cache and reject legacy JSON
or any altered payload/length without mutation. Installation logs the new
evidence provenance. All 739 normal and sanitizer tests pass.

The exact 2.39 firmware now publishes its first visible 240x240 Suunto boot
logo (E-SAP-BOOT-LOGO-239-001): generation two, CRC32 `4979f432`, pixel
SHA-256 `3eff811736aa1890e78095f31d88ad95a8a457d41caa0ccb3e527555c8ecf373`.
Two fresh logs/snapshots match at `stop=user pc=0x00093be2
instructions=609300000 virtual_time_ns=2148256583`: log SHA-256
`63eb4997ff645958e70ed0586613762f88ee5e6e699434c1fbae48f0f435528b`,
snapshot `30050924fa4986412226750eb422aaccfca934a485ad7813e349e6b1da8b01a3`.
This satisfies the existing first-nonblack `normal-frame` gate, not settled
setup or interactive operation. Source flash remains immutable, with no
reset/refusal before the frame and unchanged 76,258 file operations.

After the logo, update-open of `actitmln/247.bin` (mode three, LR `0x000b9e0d`)
hits the existing file budget at PC `0x000920b4`, instruction 610,599,945,
time 2,149,556,528 ns when resumed from the logo snapshot. The activity
sequence needs a measured finite budget or a separately evidenced file-contract
correction; no increase is included here. Old JSON-bearing snapshots are not
migrated and must be
regenerated from reset or a pre-install checkpoint. Layer-off and pre-install
execution retain their hashes; historical defective-fixture hashes are not
silently re-pinned.

Ticket 754 measures and enables exactly 21 post-logo activity operations
(E-SAP-COMPAT-ACTIVITY-239-001): nine on `actitmln/247.bin`, twelve on
`actitmln/ongoing.bin`. The firmware updates records and headers, then closes
both files, retaining sizes 46,112 and 152. Only the file ceiling changes to
76,279 (aggregate 76,282); no paths, modes, bytes, status translations,
snapshot format or other budgets change. The new regression fails at the old
ceiling and passes at the new one, including atomic excess refusal. All 740
normal and sanitizer tests pass.

The rendering-backed production continuation reaches
`stop=budget pc=0x00079e1c instructions=932397949 virtual_time_ns=11388431926`:
log SHA-256 `3a625809c79c1fdb8937ac36cd6e912b026ffcdc8fbc80c7ed888680d8bb11a7`,
snapshot `8d9b262474b00c6c0a2b5423ce4100363eb4582a96205eae8d045b70c8ae50a7`.
The next instruction executes native BKPT, halting at PC `0x00079e1e`,
instruction 932,397,950 / 11,388,431,927 ns. A read-only trace with the normal
NEMA backend identifies `CXD5610GF-driver.cpp:910`, LR `0x00128f55`;
the native branch increments a retry byte and asserts at three. The missing
GPS exchange/state transition still needs recovery. No reset or extra file
operation occurs after the 21 updates; no settled or interactive 2.39 UI is
claimed. Existing boot-logo hashes remain the required prefix.

An initial backend-less diagnostic instead reaches mode ten for `wui_dump.bin`
at 639,161,545 instructions. This is not the production CLI boundary: attaching
the CLI's NEMA backend reproduces the GPS halt exactly. Earlier backend-less
diagnostics and rendering-backed checkpoints must not be mixed. The unknown
dump mode/path remains refused; this task does not implement it.

A fresh current-build snapshot/frame-loop baseline was measured on 2026-08-20
with the external Sapporo 2.22 manifest (SHA-256 `ac9b381b...`) and a
450,800,000-instruction checkpoint (snapshot SHA-256
`8767a3f9990f1272c55e55566c7479037e0a2fbe3d5a711efb686feff7026e70`). Three
cold starts to the 450,900,000-instruction `normal-frame` boundary averaged
8.253 seconds of host wall time. Five resumes from that snapshot averaged
0.142 seconds headless and 0.174 seconds through SDL3; every run stopped at
`pc=0x0009a3fc`, virtual time `5333307331`, and every SDL run published
`240x240 generation=1 crc32=4979f432`. The current binary hashes were
headless `c80b8191...`, SDL `1d26a03f...`, and `libsemu.a` `9feafe6d...`.
This is a host-only guardrail; it authorizes no performance or guest-behavior
change by itself.

## Sapporo 2.39 Initial GPS Integration (Ticket 756)

The separately selected `sapporo-2.39-gps-startup` layer now implements the
two evidenced synthetic status lines from E-SAP-COMPAT-GPS-STARTUP-239-001.
It requires the exact profile/component hashes, validates the initial native
driver/UART boundary, and queues startup RX and the exact `@VER\r\n` reply
through the existing delayed UART/IRQ path. Each response has one hit; the
aggregate ceiling is two. No CPU, RAM, native-state or firmware-byte patch is
made by this layer. Instance-owned counters survive the unchanged snapshot
format; reset and restore explicitly bind the fixture. Unsupported state,
commands, repeats, budgets and scheduling failures refuse atomically.

Two fresh two-layer production runs reach native state 15 with retry zero:
`stop=budget pc=0x00128ed8 instructions=393785845 virtual_time_ns=2564070074`.
Log SHA-256 `b5b23c9f6a96d9ecfbf4f17aa4f3b70801d08cd9b9636330d11c08f2e0647123`,
snapshot `bfce8efc3cf6fc28330eb81cf453aad2ff71a4c8f4c9d2102624bae0d937a6c9`.
Both artifacts compare byte-identically. Snapshots before startup, with either
response pending and at native state 14 reproduce the same final snapshot and
log suffix. Completed exchanges also resume deterministically to the later
retry. One-layer images are rejected by two-layer configurations; historical
one-layer logo/activity/halt goldens remain unchanged. All 752 normal and
sanitizer tests and both private GPS/activity gates pass. The ticket index
now marks ticket 756 done after review of implementation commit `fa56b33`.

With only the initial layer selected, this remains initial GPS lifecycle
progress, not full GPS or settled 2.39 UI.
The distinct later reopen sets pending state seven at instruction 672,044,891;
its missing response still times out. The subsequent retry reaches
`0x00128d14` at instruction 908,321,039 / 14,978,258,084 ns and is refused
without another status or hit. This two-layer behavior is preserved.

Read-only follow-up E-SAP-COMPAT-GPS-REOPEN-239-001 now recovers that bounded
exchange. One external delayed status at the post-arm `0x00128e8c` boundary
advances native states 7/8/9 to exact `@GSR\r\n`; one delayed synthetic reply
advances states 10 and 12 with retry zero. Two diagnostic runs are identical,
and wrong-prefix controls still time out. The later liveness-recovery command
`@GSTP\r\n` is the next precise UART refusal, at instruction 940,963,736 /
16,349,008,531 ns. These experiments establish synthetic parser acceptance,
not a physical receiver transcript.

## Sapporo 2.39 Bounded GPS Reopen (Ticket 757)

The explicit `sapporo-2.39-gps-reopen` layer now implements the two evidenced
responses separately from startup. It requires the separately selected initial
GPS layer and all three exact component hashes. At post-arm `0x00128e8c`,
it validates the complete driver/UART spans, state, flags, cached GNS and
completed initial exchange before queuing one status after ten ms. The exact
six-byte `@GSR\r\n` then receives one status after ten ms. No CPU/RAM/native
event/GPIO patch is made. Instance-owned counters, reset and restore binding,
dependency validation and atomic refusals use the unchanged snapshot format.

Two fresh three-layer runs compare byte-identically at native state 12,
retry zero, with exactly two initial and two reopen interventions:
`stop=budget pc=0x00128ed8 instructions=825147087 virtual_time_ns=10875951888`.
Log SHA-256 `f63cab509a2da82a867580bf9eac35b3e764df53d08155bb11c47c6dfa328007`,
snapshot `0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`.
Snapshots before reopen, with either response pending and at native state ten
resume to that identical image and full log suffix. Wrong lifecycle, ownership,
layer sets and order refuse; one-/two-layer images do not migrate implicitly.
The historical private startup/activity gates retain their existing hashes.

Ticket 757 is accepted by separate integrator review of `9f8c121`; the
implementation and exact verification record are in its handoff. The next
unsupported operation is still `@GSTP\r\n`: at instruction 940,963,736 /
16,349,008,531 ns the native precise UART fault has BFAR `0x4001d000` and
stacked PC `0x00171798`, with no extra compatibility hit. Receiver awake/liveness,
GPS time/fix, later commands and a settled 2.39 UI remain unimplemented.

## Sapporo 2.39 Awake Evidence and Integration Prerequisite

Read-only firmware recovery now identifies the cause of that later recovery:
GPIO24's native callback `0x00128926` sets awake byte `0x100588a2`; state twelve
consumes it between polls. E-SAP-COMPAT-GPS-AWAKE-239-001 independently pins
the 2.39 IRQ table, registration, callback and branch rather than transplanting
the 2.22 hook. External pulses through the existing transport/GPIO path, each
100 ms after a successful poll and high for 1 ms, produce the native IRQ and
avoid GSTP. One pulse postpones recovery by one poll; a late pulse does not
avoid the original fault. Four pulses produce four callbacks and five native
state-twelve successes, retry zero, without another UART command.

Two four-pulse diagnostic runs compare byte-identically through the 35-second
guard: instruction 1,315,882,442 / 35,000,617,152 ns, PC `0x000e955e`, both
existing GPS layers still at two hits. Trace SHA-256
`ca3527d679f889242849f6bfe52ed726cc0c817cb329b45630c6bc2a5ad72010`,
log `5bdb32dc4a6a417d5b16d78d4348681c5ee1cedd47e170ca8dd7bc825171a212`.
These are external synthetic-input experiments, not enabled production
behavior, physical receiver cadence, NMEA/time/fix or a settled UI milestone.

Before integration, E-EMU-CXD-AWAKE-FAILURE-001 exposes a transport prerequisite:
failed pulse admission changes bookkeeping and emits a low callback; a failed
falling-edge schedule emits a zero-duration pulse and reports success. A
six-case host-only reproducer covers time/ID/sequence exhaustion at both
stages. The current void scheduler callback interface cannot report that
second-stage failure. Ticket 758 is ready and explicitly owns the minimal
scheduler/device failure contract; ticket 759's four-hit awake fixture is
blocked until 758 is accepted. No production C, profile, counter, snapshot
encoding or runtime dependency changes in this evidence/planning step.

Verification on unchanged `9f8c121`: `make check` passes 759 cases, and the
exact ticket-757 private reopen command passes without skips or re-pinning.
The prior 759-case sanitizer result remains applicable to unchanged C;
sanitizers were not rerun for this documentation/planning-only step.
The ledger records external source/trace hashes and negative controls; the
new ticket contracts specify the remaining implementation gates.
`make check-task-contracts` validates 129 tickets and `git diff --check`
passes. Changed repository files are this status, `docs/migration-evidence.md`,
`plans/index.tsv`, and tickets 757/758/759 only. No private artifact is added.

## Ticket 758 Implementation — Acceptance Pending

The pulse failure integration now validates complete time bounds and schedules
before changing admission state/signals. Falling-edge failure reports through
the new public copied-first-error scheduler contract without emitting high;
CPU tick and WFI/WFE paths stop with the original device-refusal diagnostic.
Reset/reentrancy behavior is documented in `docs/execution-model.md`. The CXD
snapshot codec is split out without changing encoded bytes. No new 2.39 awake
fixture, UART response, profile or layer is enabled.

Nine new cases raise normal and sanitizer coverage to 768 passing cases.
All focused ticket commands, line checks and 129 task contracts pass. The
unchanged private 2.39 reopen gate passes without re-pinning. Clean baseline
and changed 2.22 headless runs through the first awake pulse have identical
logs and snapshots: stop `budget`, PC `0x000d4a8c`, 599774578 instructions /
12027701702 ns; snapshot SHA-256
`9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.

The authentic SDL live-input gate fails its historical stop tuple on both
the clean starting commit `9f8c121` and the changed build, with byte-identical
logs and all expected CRCs. Both stop at PC `0x000bacf4`, 774081920 instructions /
6520939902 ns rather than the E-SAP-ONBOARD-EMU-011 tuple. No golden is changed.
E-EMU-CXD-AWAKE-FAILURE-001 and ticket 758 record the exact commands and hashes.
That was the implementation handoff's unresolved gate. The separate maintenance
investigation below resolves its cause and passes the corrected strict check.
The subsequent integrator review below accepts 758 and makes 759 ready.

## SDL Live-Input Checkpoint Maintenance

E-SAP-ONBOARD-EMU-012 isolates the old checkpoint's extra startup delay to the
shared haptic fixes already committed in `d6b4235` and `df93397`. Clean builds
before those fixes reproduce the old log hash exactly. Observational traces
show command `0x22000112` incorrectly selecting status register `0x08` from
adjacent SRAM, returning zero for 32 reads and exhausting the firmware's
nonfatal autotune timeout. With the correct command selector, autotune returns
complete (`0x03`) and the two calibration reads succeed. Applying only those
two existing corrections to the historical source reproduces the current
complete SDL log byte-for-byte; ticket 758 is not the cause.

Bounded maintenance changes only `tools/test_sdl_live_input.sh`, this status
and the evidence ledger. The gate now pins PC `0x000bacf4`, 774081920
instructions / 6520939902 ns and, for cold boots, log SHA-256
`9ee0637132d115f0136e490c2314db49ef4a792ab0a1eae1d70ed15ff3a9382c`.
All initial/settled CRCs and generations, stop reason, exit code and invalid
configuration checks remain required. There is no runtime, timing, profile,
compatibility, snapshot or release-frame golden change in this maintenance.

Two fresh exact-firmware runs compare byte-identically. The existing authentic
regression fails before the smoke expectation correction and passes afterward:

```sh
build/suunto-emu validate --profile sapporo-2.22.60 --firmware tests/private/sapporo-2.22.60/firmware.semu
SEMU_FIRMWARE_MANIFEST=tests/private/sapporo-2.22.60/firmware.semu sh tools/test_sdl_live_input.sh
make check-sdl
make check
git diff --check
```

Validation and the focused authentic gate exit 0. `make check` passes 768
cases, line checks and 129 task contracts; `git diff --check` is clean.
An external negative harness
appends one entry to an otherwise exact transcript; all existing checks match
but the new cold hash check exits 1. `make check-sdl` without a manifest passes
five SDL cases and configuration refusals, explicitly skipping its firmware
walks; the short authentic walk was run separately above. The longer manual
time-entry/onboarding gate was not revalidated or re-pinned. No new firmware
fixture or GPS command is enabled. Sanitizers are not rerun for this shell/
documentation-only change; the prior 768-case result applies to unchanged C.

## Ticket 758 Integrator Acceptance

Separate review/planning maintenance accepts 758 and makes 759 ready. Only
the index, those two tickets, this status and the evidence ledger change in
this review. No runtime behavior or private fixture is added. Source review
confirms atomic admission, copied first callback failure, CPU tick/sleep
propagation, reset/reentrancy refusal and unchanged successful serialization.

All ticket commands were rerun: focused scheduler/CXD/sleep/snapshot groups
pass 4/13/4/4 cases; `make check-task-contracts` validates 129 tickets;
`make check-lines`, `make check` (768 cases), and `make sanitize` (768 cases)
pass. The exact private 2.39 reopen gate passes without skips and retains its
state-12 snapshot `0d3b67903adff5826de946b56738ce443dffa784410392363e1a7ddc0623c27d`
and precise later GSTP refusal. `make sdl`, 2.22 component validation and the
authentic short live-input gate all pass under E-SAP-ONBOARD-EMU-012.

A fresh bounded 2.22 headless run through its first awake intervention has
the expected budget exit 3 and byte-identical log/snapshot to clean `9f8c121`,
including snapshot `9db6fe24e0b5a6509333aff140ec7a635a031ef68bbac74f87d7ca2c5992e2eb`.
An external deterministic allocator-fault probe passes three additional
cases normally and under ASan/UBSan; E-EMU-CXD-AWAKE-FAILURE-001 records the
source hash and commands. Ticket 758 records the complete acceptance handoff.
The longer manual-entry SDL gate remains a separate audit; no GPS fix/time,
physical receiver cadence or new production awake layer is claimed.

## Ticket 759 — Accepted Bounded GPS Awake Integration

The explicitly selected `sapporo-2.39-gps-awake` layer now supplies exactly
four synthetic GPIO24 pulses through the accepted transport. It requires the
separately selected startup/reopen layers, all exact firmware hashes and both
completed two-hit lifecycles. Each evidenced successful state-twelve poll
admits one pulse after 100 ms, high for 1 ms. Only native GPIO IRQ handling
sets the firmware's awake byte; the hook changes neither CPU nor guest RAM.
R1 scheduling returns are not predicates. VER/GSR providers remain unchanged.

Instance-owned counters, explicit reset/restore binding, atomic refusals and
strict snapshot lifecycle/event validation are covered. The layer codec was
split into `machine_snapshot_layers.c` with unchanged bytes for earlier layer
sets. No implicit snapshot migration or new scheduler/device behavior is added.

Two fresh four-layer runs and all four snapshot-phase resumes match exactly:
`stop=compat-refused pc=0x001291cc instructions=1272353867 virtual_time_ns=32770943068`.
GPS hits are `2,2,4`, retry zero, GPIO24 low and no pending pulse. Log SHA-256
`06698600df74b250f987bf3f23f2c14d65b7cbf2f62c9f5ebd00784ab52d0543`;
snapshot `da5bb8e0d002683719d6079ca496b03884045775d7268f5911b4b8cc36f6997a`.
The private observer verifies four native awake callbacks/STRB results and
the same final image. A repeated fifth-hook attempt changes no snapshot byte
and emits no further hit. This is an intentional evidence-bound refusal.

All 775 normal and sanitizer cases pass, along with 129 task contracts and
line checks. The unchanged private startup/reopen/activity gates preserve
their hashes; the 2.22 headless first-awake log and snapshot remain byte-identical
to baseline. The ticket handoff records exact commands, files and evidence.
Separate integration review on 2026-09-06 found no blocking issues and reran
every ticket command: 775 normal and 775 sanitizer cases, all six focused
groups, 129 task contracts, line checks and all four private gates pass.
The private runs preserve the exact hashes above and the historical gates'
pins. Ticket 759 is now done; only planning/status records changed during
acceptance. Full GPS, fix/time data, physical cadence and a settled 2.39 UI
are still not established.

## Sapporo 2.39 UI Investigation — Renderer Gaps

E-SAP-UI-239-001 measures progress within the accepted four-pulse interval.
Two cold idle runs reach the native `Select language` tile at 5,805,310,840 ns,
CRC `3bd12ac8`, then retain ticket 759's exact final snapshot hash. A normal
middle-button press advances to a language menu; upper/lower controls do not
leave that tile. Cold and resumed middle runs converge to identical final
pixels and machine state. Read-only native tracing confirms button events
2/5/1 and view-open calls without invoking firmware callbacks directly.

The language image is incomplete, not a new correct-frame milestone: 52 of
79 backend submissions refuse during the cold middle-button run. The first
is a fully clipped-out animated A2LE glyph. The shared sampling helper
rejects its offscreen origin; a second synthetic case returns an inverted
intersection. Separately, the GPU drops backend REFUSE and its error, reports
successful MMIO and consumes the ring submission. Small external failing
reproducers now isolate both bugs, including a successful-backend control
and an ASan/UBSan run of the swallowed-refusal case. No runtime fix is made
by this investigation; the passing 775-case suite does not cover them yet.

Two properly held middle clicks advance native view selection and then hit
the existing logical-file ceiling at `settings/general`, mode two,
PC `0x000920b4`, 1,376,488,437 instructions / 14,401,737,146 ns. Two probes
match exactly, GPS hits `2,2,1`. This later boundary follows silently refused
renderer work; it is not authority to raise the file budget. Only this status
and the evidence ledger change; all probes, snapshots and pixels remain
external. `make check` passes 775 tests, line checks and 129 task contracts.
Full sanitizer and SDL suites were not rerun for these documentation changes.

## Shared Sampling Clip Correction

Bounded maintenance under E-SAP-UI-239-001 / E-EMU-SAMPLING-CLIP-001 fixes
the offscreen and inverted-intersection bugs identified above. The shared
helper returns canonical empty bounds for valid disjoint rectangles, checks
layout/endpoint/offset arithmetic, and leaves outputs unchanged on refusal.
Unsigned geometry remains the contract. Changed files for this follow-up:
`src/display/sampling.c`, `src/display/sampling.h`,
`tests/unit/test_sampling_clip.c`, this status and the evidence ledger.

The two original synthetic regressions fail before the fix. Five new cases
now cover them, 1,728 pixel-oracle combinations, malformed-input atomicity,
and all three sampling consumers. All 780 normal and sanitizer tests pass;
27 focused sampling tests, line checks and 129 task contracts pass. The
private 2.39 awake gate and 2.22 short SDL gate preserve their exact pins.

Two corrected cold middle-button runs accept all 79 submissions; a prefix
resume accepts all 76 remaining submissions. They converge to byte-identical
final pixels and machine snapshots. The language list is visually clear,
CRC `6b6aa2dc`; no stale green icon remains. The evidence entry records full
hashes and exact reproduction commands. This is an emulator observation,
not a physical-panel golden. Backend-refusal propagation is still unfixed;
neither file nor GPS budgets are extended, and no integrator change is needed.

## NEMA Atomicity Integration Prerequisite (Ticket 761)

Commit `c7800be` records the accepted GPS-awake work and sampling correction.
The subsequent bounded GPU investigation changes no runtime code.
E-EMU-NEMA-ATOMIC-001 extends the swallowed-refusal reproducer with three
failures using the real backend: refused commands leak inherited register
state; a failed second child leaves the first child's frame published; a
failed second completion admission leaves the first event and consumed ID.
Two normal and one sanitizer run match exactly; success controls pass and
there are no sanitizer findings. The existing 780-case suite still passes
but does not cover these failures yet.

Returning an error alone cannot undo those mutations. The existing public
backend has no prepare/abort boundary, and completion admission is one event
at a time. Following the repository's insufficient-interface rule, runtime
work stops pending the explicit integration in ticket 761. It is ready, with
all dependencies done, and owns the minimal display/scheduler contract,
caller migration and rollback/refusal regressions. It must preserve successful
frame/event order and snapshot bytes; a partial-output error is not sufficient.
This planning step changes only ticket 761, `plans/index.tsv`, this status and
the evidence ledger. `make check` passes 780 cases, line checks and 130 task
contracts. No new firmware execution, full sanitizer rerun or budget change
is claimed; prior runtime checkpoints remain unchanged.

## NEMA Atomic Admission Foundation (Ticket 761, Partial)

E-EMU-NEMA-BATCH-001 records the first implementation slice: scheduler batch
admission validates and reserves all events before consuming any identity;
NEMA completion batches stage entries and preserve complete snapshots on
refusal. Cancel/reset/destroy remove owned callbacks, including protection
against cancelling another owner's reused event ID after scheduler reset.
The completion codec is split without changing its bytes. Existing single
admissions use the same path and preserve successful event ordering.

All 789 normal and sanitizer tests pass. The short 2.22 SDL gate, 2.39 awake
gate, two cold middle-button runs and a 700-million-instruction prefix resume
preserve their exact prior checkpoints. The corrected language frame remains
CRC `6b6aa2dc`; cold traces and final pixels/snapshots are byte-identical.

This is not whole-ring atomicity: the GPU still admits markers individually,
and the backend still lacks staged multi-child publication/inherited-state
rollback. Refusal propagation remains unfinished. Ticket 761 stays incomplete
and its index status is unchanged; no firmware budget or golden is extended.

## NEMA Backend Transaction Foundation (Ticket 761, Partial)

Commit `ce0e529` records the scheduler/completion foundation. The next slice
adds public display prepare/commit/abort operations and an in-tree backend
implementation. It stages inherited registers, pixels, TSC6A shadows and
ordered per-child frame images; commit neither allocates nor fails. The
existing single-list convenience now uses that path. Refused lists preserve
inherited state and return the original detailed error even after diagnostic
saturation. Reset/reentrant submission conflicts refuse before mutation.
The large backend is split into lifecycle/list execution, draw dispatch and
transaction responsibilities; no persistent encoding changes.

E-EMU-NEMA-BACKEND-001 records three regressions failing before implementation,
seven final backend cases, and 796 passing normal/sanitizer tests. The exact
2.22 SDL and 2.39 awake gates pass. Two cold middle runs and a prefix resume
retain every corrected trace/pixel/snapshot pin. The original five-scenario
GPU probe now has two failures rather than three: inherited state is fixed,
but later-child publication and marker admission remain non-atomic because
GPU/machine callers have not yet migrated. Ticket 761 is not complete and its
status remains unchanged. No budget, fixture, physical-panel claim or golden
is extended.

## NEMA GPU Transaction Integration (Ticket 761, Partial)

Commit `0e8900d` records the backend foundation. The continuation migrates
GPU/machine/CLI callers to the public transaction operations: prepare all
children, admit all markers, then commit frames/registers/stop/generation.
Later-child and completion-admission refusals now preserve the GPU snapshot,
emit no frames and allow a corrected same-stop retry. Original errors reach
MMIO; missing backend/scheduler and unexpected callback results refuse.
Reentrant GPU reset/write/snapshot and machine reset are guarded. GPU lifecycle,
submission and snapshot responsibilities are split with unchanged codec bytes.

E-EMU-NEMA-GPU-001 records the two failing pre-migration regressions, six final
GPU cases and the two-instruction synthetic CPU success/fault test. The original
five-scenario probe now reports zero failures on repeated normal and sanitizer
runs. All 803 normal and sanitizer tests pass. Both private firmware gates and two cold/prefix-resumed language-screen
probes preserve the exact earlier pins, including CRC `6b6aa2dc`.

Ticket 761 remains incomplete: strict ring/wrap/odd-tail validation, composed
allocation-failure coverage and the final acceptance audit remain. No ticket
status, golden, budget, firmware data or CPU/bus policy changes.

## NEMA Control Validation and Allocation Coverage (Ticket 761, Partial)

Commit `1733bb8` records the GPU transaction integration. The continuation
corrects non-power-of-two ring wrap arithmetic, checks complete ring-address
arithmetic, validates held-control/marker fields and refuses unsupported GPU
access widths. A malformed control after a valid child changes no GPU snapshot,
frame or event; correcting the same stop executes once.

The firmware gates caught an overstrict draft that accepted only base-wrap
targets. Read-only reverse engineering established that both native 2.22 and
2.39 marker builders also emit a held jump to the immediate continuation word.
E-EMU-NEMA-CONTROL-001 records the exact instruction ranges and first-refusal
trace. The validator now preserves that form without accepting arbitrary jumps.

Deterministic allocator replacement in test-only compilations of production
sources exercises the full MMIO/parser/backend/completion/scheduler path. Every
allocation refusal preserves the existing queue, identities, inherited color,
pixels, generation and GPU snapshot; retries publish both children and complete
both markers in order. No runtime hook or build dependency is introduced.
Ticket 761 remains incomplete pending strict inline/padding/unmatched-tail
validation and final acceptance review; its status/index and goldens are unchanged.

All 809 normal and sanitizer tests pass, along with 130 task contracts and
line checks. Both exact private firmware gates pass after the evidence-backed
control correction. Two cold language runs and the prefix resume preserve
every prior trace/pixel/snapshot hash and the exact endpoint. No newly exposed
refusal was hidden and no acceptance checkpoint was weakened.

## NEMA Unmatched-Tail Refusal (Ticket 761, Partial)

The prior control/allocation work is committed as `611d3c4`. Native research
reconciliation establishes that the apparent rounded tails in early captures
were artifacts of treating CMDSIZE entries as bytes. The framing parser and
backend now refuse odd word counts, including held register tails, before any
callback or renderer staging. They neither ignore the last command nor read
its value beyond the declared list (E-EMU-NEMA-TAIL-001).

Three new regressions fail on the committed baseline and pass with the fix:
backend inherited-color/pixel preservation, later-child GPU/marker refusal and
zero framing callbacks. Complete paired replacements succeed at the same stop.
The native observer finds inline initialization in both pinned firmware
versions; strict inline/padding interpretation and final acceptance review
remain unfinished. This is not ticket completion or a new hardware claim.

All 812 normal and sanitizer tests pass. Both exact private firmware gates,
two cold 2.39 middle-button runs and the 700M-prefix resume pass with unchanged
trace, pixel and snapshot hashes, including frame CRC `6b6aa2dc`. Ticket 761's
handoff records the commands, bounded native syntax inventory and provenance.

## NEMA Inline Ring Transactions (Ticket 761, Partial)

The 2026-09-07 continuation replaces permissive inline-word skipping and the
independent marker scan with one validated ring plan. Unknown inline registers,
prefixes, nonexact NOPs and incomplete pairs refuse. Values resembling ring
opcodes remain values. Inline state and draws now participate in the same
transaction as child lists, without additional child frame publications.
Descriptor flags extend the existing public prepare/commit/abort contract;
no private backend API or persistent encoding is introduced.

E-EMU-NEMA-INLINE-001 records the native paired-stream evidence, the failing
mixed-command regression and corrections to two old malformed synthetic padding
fixtures. Bounds permit 32 children, 64 markers and 64 total child/inline spans.
Wrapped complete runs and atomic plan refusal are tested; a graphics pair split
across the physical ring end remains explicitly unsupported. IRQ-clear semantics
and fragment-program ISA execution are not inferred from initialization writes.
Final malformed-input/lifecycle review remains; ticket status is unchanged.

All 815 normal and sanitizer tests, both private firmware gates, two cold
language-screen probes and the prefix resume pass. Every earlier trace,
frame and snapshot pin is unchanged. Ticket 761's handoff records exact
commands, test counts and the revised read-only observer's source hash.

## NEMA Memory-Only Command Fetches (Ticket 761, Partial)

E-EMU-NEMA-MEMORY-001 identifies device-read side effects during command
validation. Ring, child and direct-backend command fetches now share the
existing bus memory-copy path with explicit little-endian decoding. Device
commands refuse without invoking their callbacks; synthetic RAM/ROM commands
remain supported. Four zero-read regressions fail before the fix and pass
afterward, alongside a ROM success control and corrected RAM retries.

The audit also reproduces read side effects before texture validation,
RGB565 sampling and A2LE sampling refuse. These readers and their tests are
outside ticket 761's Allowed Files, so they have not been changed. Integrator
authorization to extend that scope is required; staged pixels cannot undo
device callbacks. Callback lifecycle acceptance also remains. No ticket status,
bus policy, profile or snapshot format is changed.

All 820 normal and sanitizer tests pass, as do both exact private firmware
gates, the SDL build, task-contract and line checks. Two cold language-screen
runs and the 700-million-prefix resume retain every trace, frame and snapshot
pin. Exact commands, counts, evidence and the scope request are in ticket 761.

## NEMA Memory-Only Texture Reads (Ticket 761, Partial)

The user authorized the texture-reader scope extension on 2026-09-08.
E-EMU-NEMA-TEXTURE-MEMORY-001 promotes the previous external counter probe into
five MMIO refusal regressions, an A2LE output-preservation regression and a
composed later-child texture failure. Validation and RGB565/A2LE reads now use
the existing byte-wide memory-copy contract; a one-byte device overlay cannot
be bypassed with a wider copy. An A2LE read refusal no longer clears RGB fields.
Mapped-memory errors retain their original code/text instead of a generic
validation error. No compatibility hook, bus policy or persistent format changes.

RAM retries and adjacent-ROM RGB565/A2LE/bilinear success controls pass. The
backend refuses a device byte in the second texel after staging an earlier
child draw, retaining pixels, frame count and inherited state. The corrected
transaction retries successfully. Descriptor validation remains a bounded
last-byte probe; actual source bytes are independently checked by the sampler,
and rendering targets stay staged. Callback lifecycle acceptance remains;
the texture scope request is resolved and ticket status is unchanged.

All 828 normal and sanitizer tests pass. Both exact private firmware gates,
SDL build, line and task-contract checks pass; two cold language-screen runs
and the prefix resume retain every prior frame, trace and snapshot hash.
Ticket 761's texture-reader handoff records exact commands and results.

## NEMA Callback Lifecycle (Ticket 761, Accepted)

The previously verified tail/inline/command-memory/texture changes are committed
as `cfce2a1`. The 2026-09-08 continuation fixes the final identified completion
callback lifetime defect (E-EMU-NEMA-CALLBACK-001): dispatch now copies its
notification recipients before making the entry reusable. Scheduling from CLID
cannot redirect INTERRUPT/IRQ, and resetting/cancelling/destroying the standalone
completion owner cannot invalidate the in-flight sequence. Callback contexts
and the scheduler must remain alive; machine/device owner destruction and
recursive scheduler dispatch are not authorized by this contract.

The new regression fails before the fix and passes afterward, including slot
reuse, reset, cancel and owner destruction with live external contexts. It checks
exact recipient/order/value, queued-event cancellation, counters and the next
100-us deadline. All 829 normal and sanitizer tests, both exact private firmware
gates, SDL build, line checks and task contracts pass. Two cold language-screen
runs and the prefix resume retain every prior trace, frame and snapshot hash.

The lifecycle continuation is committed as `2d230a2`. The separate 2026-09-08
planning-only integration pass reviewed all acceptance evidence and accepts
ticket 761, updating its status/index to done. Review reruns of NEMA (112),
scheduler batch (four), and all 829 normal tests pass. No further scope extension
is requested; unsupported command/source/callback cases remain explicit in the
ticket handoff. This is not a claim of new physical GPU or firmware coverage.

## Native General-Settings Boundary (Ticket 762, Evidence Collected)

Two normal-backend, four-layer runs from the pinned 700-million-instruction
prefix reproduce the two held MIDDLE presses with 148 successful submissions
and zero renderer refusals (E-SAP-UI-239-002). Both stop at instruction
1,376,488,437 / 14,401,737,146 ns / PC `0x000920b4`, opening
`settings/general` in mode two; GPS hits remain `2,2,1`. Their traces and all
five saved machine checkpoints match pairwise. No budget or firmware was
changed. The observed native caller tests a pending flag, opens the file,
calls `0x000d5794`, closes the handle, and clears the flag. The runtime stop
precedes the open's success: the complete serialization sequence and required
finite operation count are not yet measured. Ticket 762 scopes that evidence
work before a separate production integration decision.

The 2026-09-08 continuation now measures that complete sequence using an
isolated, bounded diagnostic executable (E-SAP-COMPAT-GENERAL-239-001):
open, 90 successful contiguous writes totalling 1,505 bytes, and close.
The native serializer returns success and clears its pending-save flag.
Exactly 92 additional operations imply a proposed production ceiling of
76,371 logical-file / 76,374 aggregate hits; no ABI change is indicated.
Production remains unchanged at 76,279 / 76,282 until separate integration.

Two pre-screen-prefix runs, two refusal-start runs and a mid-save snapshot
resume agree on final pixels and machine state. The prefix runs accept all
676 submissions and visibly reach `Define your profile` (CRC `405d1af6`).
They stop at the existing fifth GPS-awake refusal, PC `0x001291cc`,
2,363,623,546 instructions / 32,619,070,564 ns, GPS hits `2,2,4`.
No further logical-file hit occurs between save completion and that stop.
This does not reach the watch face or demonstrate post-setup menu navigation.
Ticket 762 records exact repeat/resume hashes, commands and the smallest
proposed production integration scope; its status awaits integrator review.

## Production General-Settings Save (Ticket 763)

The separate integration review accepts ticket 762's evidence; ticket 763
implements exactly its 92-operation allowance. Production now permits 76,371
logical-file / 76,374 aggregate hits. Only the two budget constants and evidence
comment change in runtime code. Other one-hit interventions, firmware hashes,
file semantics/capacities, rendering and the four-pulse GPS limit are unchanged.
The new synthetic regression fails at the native-shaped open before the change
and passes afterward, including all 90 write sizes, exact close at the limit,
mid-write snapshot restore and atomic excess/unknown-operation refusal.

The production observer matches the accepted native profile frame (CRC
`405d1af6`), mid-save snapshot and final machine hash exactly. Further bounded
native MIDDLE presses at requested 20, 22 and 24 seconds reach the birth-year
selector. Both repeats then refuse a mode-two `settings/personal` open at
PC `0x000920b4`, LR `0x000adb2f`, instruction 2,953,605,137 /
24,380,651,994 ns, with GPS hits `2,2,3` and 772/772 renderer submissions
accepted (E-SAP-UI-PERSONAL-239-001). This exposes the next persistence
sequence before GPS exhaustion; it is not evidence for another budget increase.
The watch face and post-setup menu navigation have not yet been reached.

## Personal-Settings Persistence Evidence (Ticket 764)

The user-requested commit `3afb5ff` records ticket 763's general-save
integration. A separate acceptance review marks 763 done. Evidence-only ticket
764 reproduces the production personal-open refusal twice, then measures its
complete native save using an isolated 512-operation diagnostic allowance:
open, 66 full contiguous writes totalling 1,727 bytes, close. The serializer
returns one and firmware clears its pending flag. Two earlier-start runs,
two refusal-start runs and mid-save restore match the exact final machine and
birth-year pixels at the unchanged fifth-GPS-pulse refusal, about 32.54 seconds.
No file ABI correction is indicated (E-SAP-COMPAT-PERSONAL-239-001).

Additional native MIDDLE presses at 26, 28 and 30 seconds reach the weight
selector. Complete repeated navigation and mid-save continuations match
exactly: 1,004 successful renderer submissions, CRC `a8c9f3d3`, and final
snapshot `3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`.
This whole suffix consumes 228 operations: two 68-operation personal saves
and one repeated 92-operation general save. The next refusal is unknown
`settings/time`, mode two, PC `0x000920b4`, LR `0x000acb8b`, instruction
3,885,178,598 / 30,368,914,377 ns, GPS hits `2,2,4`. It refuses despite
remaining diagnostic headroom; schema/capacity/serialization still need recovery.

Ticket 764 originally proposed a separately tested **76,599 / 76,602**
allowance to cover the measured suffix, not the diagnostic 512 extra
operations. The subsequent planning review accepts 764 and instantiates 766.
All firmware artifacts remain external; the evidence adds no time source.

## Production Personal-Settings Saves (Ticket 766)

Production now permits exactly 76,599 logical-file / 76,602 aggregate hits.
Only the two constants and evidence comment change at runtime. A synthetic
regression fails before the change, then passes all 68+68+92 operations,
content checks, mid-save restore and atomic unknown/excess refusals afterward.
File paths, capacities, formats, rendering and GPS bounds remain unchanged.

The new private gate generates the historical general-save midpoint from
cold boot and verifies two full continuations plus a personal-mid-save
restore. Production matches the accepted weight frame CRC `a8c9f3d3` and
final snapshot `3d19f80df2c47a1e98f6bfa8dd0640f0d6cbb38a4d06f933d16f09c2c54245c6`.
The shorter idle branch also repeats/restores exactly to its existing fifth
GPS-pulse refusal. Retrying either refusal leaves the machine snapshot intact.
Wrong component metadata, full flash and starting checkpoints are rejected.
All 831 normal and sanitizer tests, four Sapporo 2.39 private gates, SDL build
and the Sapporo 2.22.60 live-input gate pass. Commit `bca8f7d` records this
work; the separate 2026-09-08 integrator review accepts ticket 766.

The next production boundary remains unknown mode-two `settings/time` at
PC `0x000920b4`, instruction 3,885,178,598 / 30,368,914,377 ns. Read-only
disassembly now recovers the 19-field serializer and its helper calls
(E-SAP-TIME-SCHEMA-239-001), but not the complete dynamic write sequence or
required file capacity. This is not support for that path, an invented clock,
completed setup, the watch face or post-setup menu navigation.

## Native Time-Settings Persistence Evidence (Ticket 767)

The bounded diagnostic measures two complete saves, each open, 22 writes
totalling 349 bytes, close. Both serializers return one and clear the native
pending flag. More importantly, an independent native-path control shows
that a new compatibility file slot is unnecessary for this observed save:
forwarding only the exact mode-two `settings/time` open allows the original
firmware filesystem to write the same bytes into the existing flash overlay.
No additional file hit or fabricated data is needed (E-SAP-TIME-NATIVE-239-001).

The second 349-byte output is absent before execution and appears exactly once
at flash offset `0x00a91a00` afterward. Two refusal-start runs, two earlier
personal-midpoint runs and a mid-native-save restore match final snapshot
`ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`.
The unchanged production loader restores that native snapshot, retains the
stored bytes and repeats its next refusal without mutation. Normal tests
(831), focused tests (46), snapshot tests (four), contract/line checks and
a native-control ASan/UBSan run pass. Source firmware/full flash is unchanged.

The proposed integration was exact-path native routing, not a new slot,
snapshot format, file capacity or budget increase. At the evidence baseline,
production still refused the time open. The control reaches the existing fifth GPS-awake
refusal at `0x001291cc`, 3,960,123,530 instructions / 32,455,738,919 ns,
with weight-frame CRC `a8c9f3d3`. It does not reach a new setup screen or the
watch face. The separate 2026-09-08 integrator review accepts ticket 767.

## Native Time Routing Integration (Ticket 768)

Production now forwards the exact normalized mode-two `settings/time` open
to the original firmware filesystem. No synthetic file slot, hit, return value,
clock, capacity or snapshot format was added. Routing itself leaves CPU, RAM,
file state and logs untouched; unobserved create paths still refuse.

The extended private personal-settings gate generates its historical prefix
from cold boot and repeats/restores through the accepted native time midpoint
`b85eed95839285b520bb560cd1fff13431b837c59b29b60f5d6a12d8b86b58f2`
to final snapshot `ac0a32899f57d7b84733f458d4bc2b246d005b2885554686d20e157b5674193d`.
Independent production observations verify the 349 saved bytes in the flash
overlay and byte-identical loading of the former time-open refusal snapshot.
The historical shorter idle branch, earlier midpoint/log pins, twelve slots,
76,599 logical-file ceiling and four GPS pulses are unchanged.

All 832 normal and sanitizer tests, 47 focused Sapporo 2.39 tests, four snapshot
tests, four Sapporo 2.39 private gates, SDL build and Sapporo 2.22.60 SDL live
input pass. Commit `44ee4d6` records the work; the separate 2026-09-08
integrator review accepts ticket 768.

Read-only inspection identifies the next refusal as the exhausted four-pulse
GPS compatibility fixture: all remaining checked driver predicates match and
no awake pulse is pending (E-SAP-GPS-FIFTH-239-001). This does not establish a
fifth physical pulse or permission for an indefinite heartbeat. The last frame
is still WEIGHT, not a watch face or post-setup menu.

## Fifth GPS Awake Evidence (Ticket 769)

An isolated five-hit diagnostic preserves all production driver predicates
and supplies one additional 100-ms-delayed, 1-ms-wide GPIO24 pulse. Native IRQ
callback `0x00128926` observes awake zero, then its own STRB sets awake one;
the next state-twelve poll succeeds with retry zero. Two runs, pending-rise,
high and completed-pulse restores, and an ASan/UBSan run match final snapshot
`127214e55e966741d3cc3acb5fd5fad50988b3cb4bdadb78788e590b91f8df28`.
They stop before a sixth pulse at `001291cc / 4071207676 / 37929735196`.
Missing-pulse and six-second-late controls instead reach the same precise
UART fault at `4001d000`, stacked PC `00171798` (E-SAP-GPS-FIFTH-239-002).

With that isolated fifth pulse, a normal held MIDDLE click requested at 34 s
advances from WEIGHT to the native HEIGHT selector (170 cm, firmware state,
not supplied user data), CRC `cd4c0a99`. Two runs reproduce all 69 frames and
the final snapshot `00432bcc97bc988da8370e9e2a298a39bdfa86971e5a8000ccd36dafaaf5a286`.
The next refused operation is `settings/personal`, mode two, LR `000adb2f`,
at `000920b4 / 4232903136 / 34413596174`; the 76,599 logical-file ceiling
is unchanged. No new file hit, bytes or capacity is inferred from that open.

At the ticket-769 baseline, production remains at the original four-pulse refusal; it rejects diagnostic
five-hit snapshots rather than silently migrating them. All 832 normal tests,
47 focused cases, four machine-snapshot cases and 137 task contracts pass.
Private inputs are unchanged and all experimental sources/images stay outside
Git. This is finite synthetic-liveness evidence, not physical GPS timing,
setup completion, a watch face or post-setup menu navigation.

## Optional Five-Pulse GPS Integration (Ticket 771)

Commit `15c2a53` records ticket 769's evidence; separate integrator review
accepts 769. Ticket 771 now implements the explicitly selected alternative
`--layer sapporo-2.39-gps-awake-five`, used instead of
`--layer sapporo-2.39-gps-awake`, with the same startup/reopen dependencies
and exact component hashes. No default selection changes. The old four-pulse
identity, logs, snapshots and refusal gates remain unchanged. Selecting both
variants or loading a snapshot from the other variant refuses atomically.
Five total pulses are permitted; no sixth pulse or file-budget increase is added.

Cold native execution generates the new-identity prefix snapshot
`6e67094057d9510874a3766ced6372066fc134c1014840a0908a990266afa6de`.
Repeated idle and MIDDLE branches reproduce ticket 769's native state, including
HEIGHT CRC `cd4c0a99` and the personal-save refusal at
`000920b4 / 4232903136 / 34413596174`. The new final snapshot is
`2224b55ed72f0cac548fa80117787ae5b049f5783f10a8b6730f5ea1a0936467`.
Test-only normalization proves the serialized layer name is the sole difference
from the diagnostic final snapshot; that comparison copy is never executed.
Snapshots for the alternative must be generated through native execution,
not migrated or relabelled from the old variant.

All 836 normal and sanitizer tests, 138 task contracts, focused lifecycle/profile/
snapshot tests, the new private five-pulse repeat/resume gate, both legacy
awake/personal private gates, SDL build and the 2.22.60 live-input check pass.
An authentic ASan/UBSan MIDDLE continuation matches normal logs, trace and final
snapshot byte-for-byte. Private flash is unchanged. The 2026-09-09 separate
integrator review accepts ticket 771, whose implementation is commit
`0826523`. The review reran every ticket command with private evidence present
(focused filters 11/5/4, 836 normal and 836 sanitizer cases, 138 contracts,
SDL build, the authentic 2.22.60 live-input gate at the exact `pc=0x000bacf4,
774081920 / 6520939902` tuple, and all three exact 2.39 private gates), and
reviewed the diff for identity-bound budgets, atomic variant and
cross-identity refusals, and unchanged four-pulse lifecycle predicates. During
the review the deleted synthetic 32 MiB full-flash fixture was rebuilt
byte-identically from read-only evidence: the exact pinned 2.39 component-05
fragment FF-padded to 16 MiB, the pre-existing synthetic manufacturing sector
reproduced from the exact 2.22.60 application and patched at `0x00FFF000`, and
an all-`0xFF` upper 16 MiB; `shasum -a 256` again equals the pinned
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb`. The
fixture remains external, hash-gated and opt-in. Physical GPS timing,
GPS fix/time, the additional personal save, watch face and post-setup menus
remain outside this verified boundary (E-SAP-GPS-FIFTH-239-002).

## Roadmap Ledger Reconciliation (2026-09-09)

A planning-only integrator pass reconciled the 23 stale `in-progress` roadmap
rows against their recorded evidence. Tickets 670, 731–739, 741–744, 746–749
and 751–754 are now `done`. Each handoff already claimed its complete
acceptance list — fail-before/pass-after regressions, exact command suites
and, where applicable, hash-pinned private runs with two byte-identical logs,
snapshots and snapshot-resume agreement on the immutable full flash
`37134845eeaa0f451048e39bd66d4a9cf937093a5aeaeda00e515934d649c4cb` — and each
outcome is recorded in `docs/migration-evidence.md`
(E-SAP-ONBOARD-EMU-010; E-SAP-LPS22-239-001 through
E-SAP-COMPAT-ACTIVITY-239-001) and consumed by later accepted work. The
"next boundary" sentence in those handoffs records the next ticket's roadmap
gap, not unfinished ticket work; no separate acceptance commit existed for
the batch, so this pass is the formal closure.

Ticket 665 was initially left `in-progress` because its own handoff ends with
"Keep the ticket `in-progress`." and no later record had re-adjudicated that
instruction. The task contracts then proved the row stale: accepted ticket 670
depends on it. Its original rationale — the unresolved phone-time question —
was consumed by the negative result E-SAP-ONBOARD-EMU-009(c) (the 2.22.60
firmware has no phone/OHR2 time source) and the standalone completion chain
E-SAP-ONBOARD-EMU-010, which ticket 670 itself records, so the explicit
integrator decision is now recorded: `done` — trace complete, positive
manual-entry chain and negative phone-time result both fully evidenced.

Also corrected here: ticket 740 is blocked on dependency 730 (Ulsan product
evidence), not on tickets 736/737 review.

## Personal Save Suffix Evidence (Ticket 772)

Ticket 772 measured the complete native HEIGHT-boundary personal save with an
external probe and a separately linked diagnostic ceiling of 77,111 logical /
77,114 aggregate (production + 512; not a proposal), starting from the cold
five-identity prefix. The save that production refuses at `000920b4` completes
as exactly 68 operations: mode-two open, 66 successful contiguous writes
totalling 1,727 bytes (the birth-year save's exact size sequence), close
returning one at ordinal 76,667. The serializer returns one at `0x000adb3c`
(4,233,060,376 / 34,413,753,414 ns) with object `0x10035500` pending byte
`+0x145` still one; close and flag clear finish at `0x000adb48` four
instructions later. The unchanged adapter ABI suffices. The screen stays on
HEIGHT (CRC `cd4c0a99`) and execution reaches the next independent boundary —
the sixth GPS-awake admission refusal at `001291cc`, instruction
4,345,171,340 / 37,899,807,613 ns with gps_hits 2,2,5 — not a file refusal;
the file layer is not the binding limit anywhere through the save. Two clean
repeats, an instrumented third run, a mid-save resume (after open and 33/66
writes, snapshot `e343e340…`), an atomic refusal-start repeat and the
unperturbed 771 idle control all agree byte-for-byte on final snapshot
`41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`. The
measured finite production scope is 76,667 logical / 76,670 aggregate (+68),
to be applied only by a separate integration ticket. Whether the HEIGHT
selection is accepted and which input follows remain unmeasured
(E-SAP-COMPAT-PERSONAL-SUFFIX-239-001).

## Personal Save Suffix Budget Integration (Ticket 773)

Ticket 773 applied the ticket 772 measurement to production.
`src/compat/sapporo_239.c` now permits 76,667 logical-file operations and
76,670 aggregate wbsto hits (exactly the measured +68 HEIGHT save; the
comment names E-SAP-COMPAT-PERSONAL-SUFFIX-239-001). Under the production
ceilings the five-pulse MIDDLE branch completes the personal save — mode-two
open `0x10161600`, 66 full 1,727-byte writes, successful close at ordinal
76,667 — and refuses the sixth GPS admission at `001291cc`,
4,345,171,340 instructions / 37,899,807,613 ns with 75 frames, CRC
`cd4c0a99`, SHA `33339448…` and final snapshot
`41c65d4626481ddd0da99babfd48aff7c2d930e1864180b2eb64364f060043fa`,
byte-identical to the 772 diagnostic image; the name-normalized companion is
`65255eb1abe56f8f3ff82e1320dfce40c7671a52e446f15b3cdced37768a83d5`. The
five-pulse gate now requires the 137-line middle log with the 66 writes, the
open and the close, and keeps every refusal, repeat, resume and wrong-input
assertion; its cold prefix, idle pins `d5244833…`/`127214e5…`, the
four-pulse awake and personal gates, all historical checkpoints and the
2.22.60 gate are unchanged (each of those runs stops at a GPS or path
refusal before ordinal 76,600). The personal-budget unit test replays a
fourth measured save to exactly 76,667 and then the unchanged budget refusal;
history, general and activity budget tests and the descriptor pins moved to
76,667/76,670 with the unchanged `+3` aggregate relation. All 51 filtered
unit passes, all three private gates, `make sanitize`, `make check` and 140
task contracts pass. Post-save continuation was subsequently measured by
ticket 774 (E-SAP-GPS-CONTINUATION-239-001).

## Post-Save Continuation Evidence (Ticket 774)

Ticket 774 measured the continuation past the completed HEIGHT save with a
diagnostic-only raise of the five-pulse awake ceilings (5→25, two constants
in a separately compiled copy; production untouched). Time-capped control
runs prove the production and diagnostic builds byte-identical at the last
instruction before the sixth admission on both branches (idle
`e46aa7e6…`, post-save `6c3e261b…`, log pairs equal), so pulses one
through five and every 773 tuple are undisturbed. With the sixth wake
granted, neither branch saves again or changes the UI: the firmware issues
a genuine SCB AIRCR VECTKEY|SYSRESETREQ system-reset request at PC
`0x000d2f6e` (idle 4,536,286,836 / 60,828,679,535 ns, `compat_hits=76602`;
post-save 4,875,799,883 / 60,779,430,750 ns, `compat_hits=76670`, frames
frozen at 75/CRC `cd4c0a99`), the emulator models the reset internally per
E-CPU-0006, and each post-reset machine halts at `0x00079e1e`
(finals `bd5004c0…` idle, `411e3551…` post-save) with repeats and
post-grant mid-continuation resumes byte-identical. Sixth-and-later wakes
remain diagnostic grants; production keeps five admissions and the
unchanged sixth-admission refusal.

## GPS Spinner Flicker And Onboarding Walk Repin (Maintenance)

The 2.22.60 `w-ltim` "Searching for GPS" ring strobed in the SDL live view.
The animation publishes every guest redraw tick as a burst of
publish-flagged transactions — measured 660 distinct composite frames in a
10-second window with a 0.36 ms median intra-burst gap and up to 84 partial
composites per ~29 ms tick — and the viewer showed them all, while a physical
panel latches only at frame boundaries. The SDL frontend now feeds its visual
sinks (texture and PPM dump) through a pure presentation-side coalescer
(`src/display/present_coalesce.c`) that withholds a publish while the next
one arrives within 10 ms of virtual time and releases the burst-final
composite otherwise, flushing at stop. The live checkpoint, setup-walk,
frame gate, and counters still observe every raw publish: the completion
control walk emits byte-identical live-test lines, the spinner-window
capture drops 660→255 frames (one per redraw tick), and the main-face frame
`fb8e0155` still lands via the stop flush. The threshold is tunable with
`SEMU_SDL_PRESENT_COALESCE_NS` (0 restores per-publish presentation).
Covered by `tests/unit/test_present_coalesce.c` (burst release, borrow
durability across slots, time discontinuity, fail-closed refusals).

That control walk also exposed that `make check-sdl` firmware walks had been
silently red without the manifest env (they skip when unset, and `make check`
does not include `check-sdl`): the ticket 670 walk's generations,
timeline-press virtual time, disabled-case baseline generation, and halt
totals had drifted. Bisection over `cdfc953..HEAD` identified accepted
`d6b4235` (IOM haptic-selector 0x50 offset-byte transfer, ticket 732) as the
first commit to move device timing — exactly the haptic-correction audit
anticipated in the open-actions list. All ten settled screen CRCs
(`8362b9bc`, `5321867e`, `c683e828`, `cd1b0979`, `455b603a`, `53d3f0c1`,
`17e1772c`, `578e2601`, `1c62ab1a`, main face `fb8e0155`), halt PC
`0x000727ca`, and the `gps-awake-pulse` refusal trigger are byte-identical,
so `tools/test_sdl_onboarding_completion.sh` re-derived only derived
counters, records the provenance inline, and additionally pins the
main-face settled step 30. `make check-sdl` is green again with the manifest.

Silent-skip hardening: `make check` now includes `check-sdl-quick` (SDL
build, input test, dummy-driver smoke, parser-refusal cases; loud banners
when SDL3 is absent or firmware walks are skipped), and both firmware
scripts auto-detect `tests/private/sapporo-2.22.60/firmware.semu`, so a
missing manifest is now a visible decision rather than a silent pass.

Walk-clock performance: sampling the pinned setup-walk showed wall time
dominated by two helpers, not the interpreter. `semu_crc32` moved from a
bit-by-bit loop to the equivalent 256-entry reflected-CRC-32 XOR table
(identical function; all frame/snapshot/manifest hash pins hold), and
`semu_storage_program` now caches the page lookup across ascending bytes
of one operation instead of walking the page list twice per byte. Full
walk wall time 278 s → 229 s (-17.6%) with the SDL live-test output
stream (settled steps, generations, timeline press, halt tuple
`instructions=14178200857 virtual_time_ns=43790375389`) byte-identical.
The remaining profile is interpreter-dominated; further gains there are
roadmap-scale, not maintenance.

Capture sweep verdict: the coalesced onboarding capture was audited frame
by frame — GPS window shows only the legitimate green/cyan arc gradient
(no swapped-channel pixels), and the pure-black frames at screen
transitions (CRC `2a01c517`, twelve in the raw stream) each dwell 16–28 ms
of virtual time (one 509 ms language-transition hold), i.e. they are
genuine firmware clear states held across a full frame boundary, exactly
what a physical panel latches; no defect found.

## Next Actionable Work

Phases 0–6 and the first-target functional milestone are complete. The Phase 7
behavior/release templates remain blocked until their product-specific evidence
is instantiated; independent product evidence inventories no longer wait on
another product's release. Bounded maintenance may proceed under `AGENTS.md`
without manufacturing a roadmap row. The practical work queue is:

- Ticket 761 is accepted. Preserve its native rendering and historical checkpoint
  pins; no private backend API or repinning is needed.
- Tickets 762–764, 766–769, and 771 are accepted. The measured fifth pulse is
  not permission for a sixth or an indefinite heartbeat; explicit selection,
  the existing four-pulse layer and historical refusals remain preserved.
  Ticket 772 has measured the complete HEIGHT-boundary personal save (68
  operations, E-SAP-COMPAT-PERSONAL-SUFFIX-239-001), accepted 2026-09-09, and
  integration ticket 773 applied exactly the measured 76,667 logical /
  76,670 aggregate ceiling with its repinned gates and was accepted
  2026-09-09. Evidence ticket 774 measured the post-save continuation under
  a bounded diagnostic GPS-awake raise: the firmware neither saves again nor
  changes the UI, then issues its own SCB system reset roughly 23 seconds
  later (E-SAP-GPS-CONTINUATION-239-001) and was accepted by integrator
  review on 2026-09-09 with every recorded control reproduced byte-for-byte
  from rebuilt probes. Further 2.39 continuation now requires physical evidence of
  GPS behavior beyond the fifth pulse (or a GSTP-time contract), not another
  ceiling raise; ticket 774 integrates nothing, and that evidence gap is
  registered as blocked ticket 776 naming the exact required captures. Subsequent profile
  choices, watch-face activation and menu navigation remain
  the functional goal, not an already-completed milestone. Preserve the four-pulse GPS bound,
  normal NEMA backend and layer sets. No GSTP response, invented GPS fix/time,
  indefinite heartbeat or assertion bypass is authorized by this observation.
- Recover a native provenance sidecar and an equivalent settled command/text
  contract for E-SAP-ONBOARD-001 before adding a screen-specific emulator
  checkpoint; until then keep `setup-next` neutral. This remains an SDL
  renderer milestone, not a physical-panel claim.
- The skip-phone-pairing transition now works via the corrected LOWER/GPIO59
  mapping (E-SAP-ONBOARD-EMU-008). The onboarding now completes standalone,
  end-to-end, to `main` (E-SAP-ONBOARD-EMU-010): MIDDLE on `w-tida`
  (`8362b9bc`, ~23 s) opens `w-ltim` ("Searching for GPS"); after its 5 s
  subscribe fires with the pre-gate `LocalTime`, the "SET MANUALLY" button
  (LOWER) appears at ~28.5 s; a time-scheduled LOWER (new opt-in
  `SEMU_SDL_SETUP_WALK_TIMELINE`) then drives the manual chain
  `w-year` → `w-mont` → `w-day` → `w-time` → `w-done` ("Done") → `main`
  (settled CRCs through `1c62ab1a`, final main-menu frame `fb8e0155`, stop
  `halt` at ~43.8 s virtual). No phone/OHR2 time source is involved: the 2.22.60
  firmware has none (009(c)). The deterministic completion sequence is committed
  under ticket 670 with a regression (`tools/test_sdl_onboarding_completion.sh`,
   wired into `make check-sdl`); the canonical sequence writes a post-gate time
   (2023-01-01), so the next boot routes to `main` instead of `n-sync-rec`.
- The short firmware-gated SDL live-input check now enforces the corrected
  haptic startup sequence under E-SAP-ONBOARD-EMU-012. Preserve its exact
  frames, stop and cold-log hash. Audit the longer manual-entry onboarding
  gate separately against the same haptic correction; do not assume its
  historical time/generation assertions remain valid or weaken them blindly.
  (Audit completed: see "GPS Spinner Flicker And Onboarding Walk Repin";
  screens, halt PC and refusal trigger held, derived counters re-derived.)
- Preserve the pinned snapshot/frame-loop baseline before any performance
  change: rerun the cold and resumed probes, requiring the exact stop, virtual
  time, and SDL CRC32 while retaining deterministic guest behavior.

The Sapporo UI helper invalidates its cached checkpoint when the selected
headless or SDL executable is newer than the snapshot and verifies a sidecar
containing the manifest, selected binary, library, helper, profile, layer, and
capture-boundary hashes. It retains the explicit refresh switch for copied or
otherwise ambiguous artifacts.

The blocked Phase 7 templates must not be treated as permission to infer later
product wiring, storage, display, or input behavior. Missing evidence remains a
refusal until a read-only native package or trace supplies the exact contract.

## Lane-Oracle Governance Update and Ulsan Post-Assert Frontier (2026-09-15)

Standing-constraint reconciliation, applied under delegated integrator
authority. The repository has no physical device and will not acquire one;
the read-only Renode lane is therefore recorded as the sole machine oracle in
`AGENTS.md` ("Lane Oracle and No-Device Constraint"), together with the probe
retention rule (volatile logs, SHA-256 cited in every evidence entry) and the
opt-in era-gate verification rule. `deferred` was added to the ticket status
vocabulary (`AGENTS.md`, `plans/task-template.md`,
`tools/check_task_contracts.sh`) for tickets whose evidence can never arrive
under this constraint.

Index changes: ticket 776 is `deferred` (its three required physical captures
are unreachable without a device; the five-pulse production boundary stays as
evidenced); ticket 710 is `ready` per its own 2026-09-11 audit note (the
2.35 trace material is on disk and the remaining work is offline
RE derivation, not a missing observation); new ticket 777 (ready) tracks the
re-derivation of the six drifted Sapporo 2.39 opt-in era scripts plus a
`make check-era` target — the drift class discovered during E-ULS-0041, where
`file_seek`, `file_size`, `ohr2_command2`, `ctimer13_inten`, `logical_files`
and `wbsto_cache` were proven to fail September pins independently of the
engine semantics change (bisect attribution in E-ULS-0041).

Ulsan frontier state after E-ULS-0046: BKPT #0 executes as a no-op per
the lane and ARMv7-M (halt-era expectations fully migrated, goldens
re-pinned with cited evidence). The engine's STTMR law changed from the
E-ULS-0014 per-read advance counter to the lane's live virtual-time
counter, so the frontier and wake-era claims were re-derived: both Ulsan
epochs now stop at the 4e9 ns virtual-time cap after a wake-overflow WFI
jump (wrapped TIMER0 re-arm 0xffffffbe, about 2^32 timer ticks) parks the
epoch at virtual time 262,143,351,559,124 ns with 16,667,327 instructions
executed, zero refusals (pass 0 = pass 1: pc 0x000dabcc, SP 0x10029e40,
LR 0x0009760b, XPSR 0x61000000, reproduced byte-identically, E-ULS-0046).
Under the same law and frontier the per-pass wake census (probe72x, run
twice byte-identically) is 5 IRQ14 wakes with computed TIMER0 re-arms
0x20/0x3f/0x20/0x65/0x20 and a final 0xffffffbe wrap 66 ns after a wake -
the first wake-era gap the real-tick law exposed; the wake-reprogram
overflow is the next ticket-730 instance. The E-ULS-0042-era claims (the
4,000,000,000-instruction frontier at pc 0x000b359c/0x000b3598; exact
IRQ26/IRQ45 match, +10 IRQ37 pairs, IRQ30/84 over-fire at 2.25x, and no
tree source for the lane's three IRQ18 pulses) measured the
pre-adoption per-read-advance engine and need a post-adoption re-run;
that census re-derivation is tracked as ticket-730 work.

Census close (2026-09-16, ticket 730 wake-overflow census, E-ULS-0047): the
wake-PARK/wake-reprogram overflow is closed as bounded and self-sustaining -
the guest wakes once at the wrapped 0xffffffbe arm (one full 2^32-tick count
cycle later at the calibrated 61,035 ns TIMER0 tick), runs one bounded
BASEPRI-ladder catch-up of 127,926,510 iterations (383,777,306 ns of pure CPU
at 0x000c320c-0x000c3228), rewrites CMP0 0x20 via the wake tail, recomputes a
second negative arm 0xffffcfa9 that fires one further count cycle later, and
repeats - a deterministic ~262,143 s virtual-time wake cadence, not a
livelock and not a refused stop; the recorded next stop is the second ladder
in flight at the 700M-instruction walk bound. The boot-epoch divergence is
confirmed at 0.5 ms lane granularity (the lane arms 0x20/0x34/0x20/0x64/0x20
exactly like the tree and never arms a 32-bit-negative compare), and the root
is the core CPU cadence law (1 ns charged per executing instruction) versus
the lane's real-time-derived virtual time inside CPU-bound code - every
byte-exact wake observation is already implemented in-tree, so no ticket-730
controller change is authorized by this census. Adopting or formally
accepting the instruction-cost cadence law is core-scope, era-wide integrator
decision territory. The remaining ticket-730 census queue is unchanged:
IRQ18/line-2, IRQ37 +10 pairs, the panel-era census, and the post-adoption
steady-era IRQ census re-run.

Census close + adoption (2026-09-16, ticket 730 IRQ18/line-2 instance,
E-ULS-0048): the three lane IRQ18 pulses are the RTC one-second alarm.
The lane wiring is `rtc: Timers.AmbiqApollo4_RTC @ sysbus 0x40004800 ->
nvic@2` (bundled ambiq-apollo4.repl lines 142-143; the class is
Suunto-fork-only, so its source is unavailable and behavior came from
lane logs plus tree probes). The retained log lp34b was re-derived twice
(lp50) and the timestamp-stripped IRQ18/rtc stream is three-way
byte-identical; its census shows the guest sets the clock once, arms the
alarm once via the boot stores 0x200=1 and 0x208=1 ("First alarm set to:
epoch+1 s, alarm repeat interval: Second"), and the model itself repeats
every second; the line drops at the NVIC acknowledge with NO RTC MMIO
involved. Tree scratch probe77 v1 exposed the guest service (0x208
rewrite at pc 0x0009bf1e, clock dance + `+0x20=0x100` + `+0x24=20230101`
stores at PCs 0x0009be38-0x0009beb6, then the E-ULS-0036 seqlock reads;
no status-register access in 1.85M+ re-service iterations with the line
held high), so the E-ULS-0036 counter-word write refusal was superseded
by the observed service stores and the model uses the E-ULS-0035
momentary-pulse convention (61,035 ns). v2 (twice x two passes
byte-identical) and probe79 against the committed engine (twice
byte-identical, zero refusals) confirmed: exact 1-second IRQ2 cadence
from the boot pair-store anchor, each service consuming exactly the
61,035-instruction wake-service invariant so the drop lands at service
end like the lane's across-acknowledge shape, the boot era byte-identical
through the 4th wake pair up to inst 16667327 (= the old E-ULS-0046
frontier instruction - the alarm simply outran the overflow park), and
the TIMER0 overflow re-arm persisting after every service: the alarm
gives the tree guest its per-second wakes but does NOT fix the TIMER0
steady-era cadence (that stays the E-ULS-0047 core cadence-law
attribution). The committed engine now parks at alarm occurrences (4e9
budget: inst 16853480 pc 0x000dabcc vt 4012595271 = anchor + 4x1e9), the
12 device-test frontier blocks were re-pinned with old pins cited, no
era script exists for the ULSAN profile (era drift surface zero), and
`make check`/`make check-lines`/`make sanitize` are green. The remaining
ticket-730 census queue is: IRQ37 +10 pairs, the panel-era census, and
the post-adoption steady-era IRQ census re-run.

Census close (2026-09-16, ticket 730 post-adoption IRQ census re-run,
E-ULS-0049): the IRQ37 +10-pair item and the steady-era re-run item are
closed on the already-pinned twice-byte-identical artifacts (lane lp50
per-IRQ stream re-derived twice, sha 2abca39c...; tree committed-engine
census p79). The lane's 2184 IRQ37 ack pairs are NOT a steady cadence:
2159 fall in the wake1-to-wake2 interval, 20 in interval 21, 5 in
interval 65 - the E-ULS-0031 persistence lifecycle as three discrete
flush bursts. The steady wake profile is {IRQ30:3, IRQ84:3} every
interval plus one IRQ26 doorbell in 22 of 113 intervals; IRQ45 occurs
only in intervals 28-30 (panel-era scope). The committed engine shows
zero line-21 edges with zero refusals across 16.9M instructions and 4
alarm wakes: the guest's alarm wake re-arms 0x228 with 0x20, recomputes
0xffffffbf and WFI-s to the next alarm without ever entering the
MSPI1/IOM4/display work, so every post-boot lane edge stream sits behind
the E-ULS-0047 negative-compare park. The E-ULS-0042 +10-pair micro-
divergence belonged to the retired read-advance engine era (E-ULS-0046)
and is void under the current engine; the census names no engine-seam
gap, so no ticket-730 src change is authorized. The remaining ticket-730
census queue is the panel-era census (including the IRQ45 intervals
28-30 cluster); the E-ULS-0047 cadence-law disposition stays
integrator-owned.

Census close (2026-09-16, ticket 730 panel-era census, E-ULS-0050): the
panel era is nine display-PLAY frame launches on IRQ45
(`Apollo4DisplayController @ 0x400A0000 -> nvic@29`; the class source is
readable and fully documents the law: PLAY +0x00 raises VSYNC bit 4 of
+0xF8 and the line, a +0xF8 write without bit 4 clears it, +0xF4 =
0x87452365, +0xEC = 0x77, Size 0x9000, TraceWrites off in this profile),
filtered twice byte-identically as 9T/9F/9 acks in wake intervals
28/29/30 (5/3/1) with 5 arm lines starting at interval 28.
`src/devices/ulsan_disp.c` matches the class law and was edge-verified
18-vs-18 (E-ULS-0039/0042); p79 shows zero line-29 edges because the era
is two downstream of the persistence flush, both behind the E-ULS-0047
park. MSPI2 is not loaded in the Ulsan lane profile at all (External IRQ
38 zero occurrences) and the loaded SDIO endpoint has no IRQ wiring and
zero activity - both tree fail-closed holes are lane-consistent. The
ticket-730 census queue is now fully closed: wake-overflow (E-ULS-0047),
IRQ18 (E-ULS-0048), IRQ37 + steady-era re-run (E-ULS-0049), panel era
(E-ULS-0050). No census named an engine-seam gap; the only remaining
gate to the flush/panel eras is the integrator-owned E-ULS-0047
cadence-law disposition. A comment-only defect was noted for the
integrator: the `src/boards/ulsan_board.c` header still lists the
display controller and MSPI1 as unmapped. (fixed in the follow-up maintenance commit).

Sapporo 2.35.34 profile dispatch (2026-09-16, ticket 705 2.35 dispatch
authorized by the E-SAP-0030 next-instance order, recorded as
E-SAP-0031): `profiles/sapporo/2.35.34` is registered (board registry,
CLI listing/selection via the new included `src/frontends/cli_profiles.c`,
and the non-2.39 device group in
`src/devices/sapporo_devices.c` - the 2.33 device snapshot applies per
E-SAP-0018/E-SAP-0030), validated against the private bundle
(`validate` rc=0, three components), and its bounded reset run reached
the recorded stop twice byte-identically: `stop=budget pc=0x000e1862
instructions=89405875 virtual_time_ns=36373760383` (log pair sha256
`87ea1ca8...`). That park PC is exactly the reference-lane startup idle
WFI park of E-SAP-0030, the boot passed every lane-missing component
(RSTGEN, PWRCTRL PWREN, the resource map) without a single refusal, and
unlike 2.33 no SYSRESETREQ cycle occurs (the trace pair is empty
twice). The contract fixture `[bounded_traces]` is now `status=observed`.
The park's wake source is unidentified - identifying it is the first
candidate gap for a ticket 710 implementation instance against this
profile. Focused test `sapporo_profile_235` green; existing profile
code paths untouched, so no era pin can drift.

Ticket 710 instance-2 census: the Sapporo 2.35.34 park wake source is now
identified as the Apollo4 one-second RTC alarm (E-SAP-0032). A lane probe
pair (scratchpad resc re-including `sapporo-2.35.resc` with the upstream
`AmbiqApollo4_RTC` replaced by an access-logging peripheral, 10 s runs)
captured two byte-identical 13-transaction RTC init blocks per run:
`+0x00` control writes 0 and 0xE, `+0x30` write 0, the E-ULS-0048 alarm
stores `+0x208=1` and `+0x200=1`, then `+0x20/+0x24` counter reads that
never advance - the guest then WFI-parks waiting for IRQ 2 (`rtc ->
nvic@2` in the upstream platform repl), which the lane class never
asserts. The in-tree stub in `src/soc/apollo4/auxiliary.c` accepts
exactly the observed offsets, answers zero, and raises nothing, so the
tree swallows the same arm and parks at the same PC (E-SAP-0031). The
RTC replacement (E-ULS-0048 law for `sapporo-2.35.34` only; stubs kept
byte-for-byte for 2.22/2.33/2.39) needs a profile-selection seam on
`semu_apollo4` that `src/boards/machine.c` could use - `auxiliary.c` and
the SoC create path are outside ticket 710's Allowed Files, so per the
contract's stop rule the census ships with the smallest integration ask
instead of a private workaround; a follow-up 710 instance attaches the
law and re-records the bounded stop.

Ticket 710 instance-3 (integrator-authorized seam from E-SAP-0032): the
Sapporo 2.35.34 park now wakes (E-SAP-0033). `semu_apollo4_select_profile`
(new in `include/semu/apollo4.h`) enables the live RTC block from the new
`src/devices/sapporo_rtc.c` for `sapporo-2.35.34` only - the E-ULS-0048
law (framework stores, BCD-hundredths counter at +0x20, the observed
`0x200=1 && 0x208=1` pair arming a one-second IRQ-2 alarm with the
61,035 ns acknowledge pulse and occurrence-scheduled repeat); the 2.22/
2.33/2.39 stubs and Ulsan paths are dispatch-guarded and byte-for-byte.
Recorded post-wake stop (twice byte-identical, CLI pair):
`stop=budget pc=0x000ccb1a instructions=100000000
virtual_time_ns=1033322780` (log sha256 `d8029b6b...`); the trace pair
(`0628133a...`) carries the wake's follow-on native boundary: the boot's
footer-validation AIRCR reset at `pc=0x000cdf5a`
(`r0=0x05fa0004`, `virtual_time_ns=1011860573`, `reset_count=1`) - the
same boundary the lane 2.35 resc registers its reset macro for, the
2.35 analogue of 2.33's startup SYSRESETREQ (E-SAP-0015). The contract
fixture `[bounded_traces]` is re-pinned to this stop (E-SAP-0031's park
stop stays on record in the ledger). Focused
`make test TEST_FILTER=sapporo_rtc` 7/7 green twice; `make check-lines`,
`make check`, `make sanitize`, and the full-flash era gate
(`SEMU_SAPPORO_239_FULL_FLASH=/tmp/sapporo-239-full-flash-exact.bin`,
sha256 `37134845...`) green. Next observation: the next distinct stop
past the AIRCR boot cycle is the following 710 instance's census.

Ticket 710 instance-4 (E-SAP-0034): the attached 2.35.34 alarm survived
its first software reset. Extending the E-SAP-0033 budgets showed the
boot cycle dying in a permanent second park
(`stop=budget pc=0x000e1862 instructions=169620897
virtual_time_ns=37382080363`, pair-identical): the engine's
SRAM-retaining software-reset path zeroes the scheduler - dropping the
pending alarm repeat without any device bus reset - while the guest's
new boot re-arms and the module's sticky pending flag vetoed the
re-arm. The module now tracks the scheduler-clock high-water mark; an
arm-time regression proves the flush (the clock is otherwise
monotonic), the stale state falls, and the re-arm takes - public
scheduler API only, no engine change. Post-fix census (pairs
byte-identical): 500M ends `pc=0x000a6c58 vt=5461709873` with 8 reset
requests, 1000M ends `pc=0x000a7036 vt=10646636623` with 18 - a stable
~1.01186 s boot-wake-reset cycle with ~21.3 ms alternate second
resets, no refusals; the first-boot record and the 100M-budget stop
are byte-preserved. `make test TEST_FILTER=sapporo_rtc` 8/8 green
twice (new `test_software_reset_flush_rearms`); check-lines/check/
sanitize green; full-flash era gate red set identical to the
pre-change baseline (the 16 known drift-red scripts, integrator's
era re-derivation queue). Next observation: cycle escape (the footer
validation apparently never passes without the full-flash preload the
2.39 era uses) is the next 710 instance's census.

Ticket 710 instance-5 (E-SAP-0035): full RTC register-law rewrite, cycle
escape. The in-tree guest-access census (pair-identical; 30 accesses to
the first reset) proved the E-SAP-0034 loop fault-driven: the single
refusing access was the post-wake counter restore `W20=0x100`. The
module now mirrors the lane `Timers.AmbiqApollo4_RTC` law end to end
(upstream source plus the twice-identical rb3 and law probe pairs):
CTRL stores bits 4:0; WRTC gates whole counter writes; the CNTL/CNTU
pair commits the epoch clock with WriteBusy/CTERR flow; fields store raw
validated hex through the lane's hex-compare BCD (`0x00252500` and
`0x003F3F3F` probe-pinned); cold counters read `0`/`0x14700101`;
IRQ2 = Enable && Status until InterruptClear; the cadence comes from RPT
with the lane's true-unit repeats, first-occurrence search, and
`Limit == Value` firing; unmodelled in-window offsets read 0 and writes
drop; the window ends at 0x210 and non-4 widths refuse. Boot escapes:
100M/30s parks at `pc=0x000e1862 instructions=89441522
vt=30000000000`, 100M/300s reaches `pc=0x000a7022 vt=85159975078`, and
1B/300s keeps the guest through two full ~120.148 s intervals (AIRCR
helper at `t=120150672935`/`t=240298801495`, period 120148128560 ns)
before `pc=0x000e1862 instructions=299057602 vt=300310661417`; every run
pair byte-identical. The E-SAP-0032/0033/0034 conventions (61035 ns
pulse, fixed +1s arm delay, refuse-unobserved-in-window, cycle pins) are
retired as an evidence-driven law correction; the fixture
`[bounded_traces]` carries the re-pins and the retirement note. Tests
8/8 twice (module split with `src/devices/sapporo_rtc_time.c`, module at
the 500-line cap), check-lines/check/sanitize green; full-flash era gate
red set identical to the pre-change baseline (the 16 known drift-red
scripts, integrator's era re-derivation queue). Next observation: the
~120.148 s AIRCR cadence is guest logic beyond RTC - its blocker census
belongs to the following instance.

Ticket 710 instance-6 (E-SAP-0036): IOM4 lane mirror at `0x40054000`, the
second fault driver of the boot removed. What instance-5 left as the next
census turned out to be a fault and not a policy: the 2.35 startup path
takes a BusFault (`cfsr=0x8200`) on its very first IOM4 command write
`0x38000212` (read, device `0x28`, offset 3, size 2) because the shared
E-A4-IOM-001 law refuses that window, and the fault handler re-enters the
guest AIRCR helper every ~120.148 s. The new engine `src/devices/sapporo_iom4.c`
with its register access handlers in `src/devices/sapporo_iom4_regs.c` and
the gauge device in `src/devices/sapporo_iom4_gauge.c` mirrors the lane
`Miscellaneous.SapporoApollo4Iom4` end to end behind the
`strcmp(profile_id, "sapporo-2.35.34") == 0` gate in `apollo4.c` (IRQ 10):
two 8-word rings at `0x100`-`0x114` with the reconstructing size word and
the split thresholds `thr_read = value & 0x3f` and `thr_write = (value >> 8) & 0x3f`,
the command at `0x120` with its raw type and its status word at `0x12c` as
`(active & 0x1f) | (cmdstat << 5) | (size_left << 8)`, the interrupts at
`0x200`-`0x20c`, the DMA engine at `0x210`-`0x248` with its target gate, and
the device-config gate at `0x2c4` that admits the observed `0x28` device and
the `0x36` MAX17050 gauge with its probe-pinned register map. Overflow sets
bit 3 and discards, underflow returns zero and sets bit 2, and both return
before the threshold is evaluated, so the threshold is updated only on a
`0x104` write, after a push, and after a pop; an invalid target raises
`DMA_ERR | CMD` while completion bails out unloaded, and an unregistered
device records `Error << 5` yet still completes the command. Six lane probe
scripts, each executed at least twice byte-identically from the read-only
lane, pinned the eighteen readouts (census hashes and the excluded
host-side canaries in `docs/migration-evidence.md` E-SAP-0036). Boot after
the mirror, all pairs re-tested on the final tree: 100M/30s parks at
`pc=0x000e1862 instructions=89441684 vt=30000000000`, 100M/300s reaches the
instruction budget at `pc=0x000a6c06 vt=85159974844`, 1B/125s now runs the
whole former window to the time budget at `instructions=104283521`, and
1B/300s ends at `instructions=122878688 vt=300000000000` - no
`machine-reset-request` any more, and the `--trace` logs of both 300 s runs
are empty, so the guest provokes no refusal and no warning anywhere in the
window. Tests: nine cases in `tests/devices/test_sapporo_iom4.c`, four
successful and five covering the error and refusal paths, byte-identical
twice; `tests/devices/test_sapporo_rtc.c` re-pinned its three-pass record
(89441684/85778971/85778979, uniformly +162 because the command write no
longer takes the fault vector) and stays 8/8; the seam moved to
`src/soc/apollo4/iom_live235.c` so that `iom.c` stays at 497 lines and the
shared IOM law keeps its byte-for-byte behaviour while `live235` is NULL.
check-lines, `make check` (927 passes, exit 0), and `make sanitize` (exit 0)
are green, and the full-flash era gate ran all 43 scripts with a red set
diff-identical to the 16-name drift baseline - no silent era drift. Next
observation: beyond the WFI park at `0x000e1862` the boot advances only to
122878688 instructions in 300 s of virtual time, so what the guest waits for
there is the following instance's census.

Ticket 710 instance-7 (E-SAP-0037): input-driven phase records beyond the
WFI park, the census delivered. Driving the machine through the public API
with the 4096-instruction input poll and a pinned eight-press timeline
(lower, middle, upper at 20/21/22 s, 30/31/32 s, lower and middle at
60/61 s, 300 ms holds, sixteen queue entries), slice-driven at 200000
instructions and 100000000 ns per call: the no-timeline control reproduces
the E-SAP-0036 300 s record byte-identically (927 slices, the observer is
neutral), while under the timeline the park share collapses from 336/927 to
25/2023 - presses do release the park. At the 400M/300s caps the run ends
`stop=budget pc=0x000bdc10 instructions=400000000 vt=30891199362` with
eight entries delivered, and at the 1B/300s caps `pc=0x000a72cc
instructions=1000000000 vt=49836877598` with twelve; the busy regime runs
at 1 ns per instruction against the alarm-driven park's 1-second 1e9-ns
steps. The eighteen counters the control never reaches are named from the
pristine disassembly: the drain at 0xa6bea serves the one-deep depth
counter at struct offset `+0x74` (pushed at 0xa6bb6, popped at 0xa6c04,
loop while non-zero) and its sibling at `+0x4`, the event bitmap at
`+0x54` takes the OR of `1 << index`, the callback table is indexed at
`0x14` bytes per entry, the queue-emptiness predicate at 0xa7042 answers
through `+0x00`, `+0x60`, and the head/capacity pair `+0x38`/`+0x4c`, the
64-bit now-stamp commits to `+0x6c`/`+0x70` with carry at 0xa754e, an
exclusive LDREX/STREX add runs at 0xa7b32, a `cpsid i` seqlock section at
0xccb1a-0xccb2e, the wake path tests SCR bit 4 and the low-power timer bit
20 at 0xe1826, and the cold frame-decode site 0x197a70 takes one hit. The
`--input-replay` record keeps its own 100000-instruction chunk
quantization (`instructions=135757218 vt=300000000000`, +12878530 over the
control) - a different pump-boundary configuration of the same deterministic
engine, both quantizations recorded. The logger census at `SEMU_LOG_TRACE`
holds zero records in both runs, matching the empty `--trace` logs: the
2.35 boot provokes no refusal and no warning anywhere. Tests: new module
`tests/devices/test_sapporo_235_input.c`, four cases - the parser's
grammar refusals (missing separator, unknown button letter, empty value,
junk number, wrong separator; the trailing comma tolerated) and the three
machine records re-taken through the public API - green 4/4 twice
byte-identically; no engine edits, so check-lines, `make check`,
`make sanitize`, and the full-flash era gate at the 16-name drift baseline
stand as recorded in instance-6. Next observations: the 60/61 s press pair
lies beyond the 1B-cap frontier of the slice census, and the
`0x0800009e`-style bootrom-vector fetch-alias counters await naming.
