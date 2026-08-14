# Compatibility Layer Policy

## Purpose

Compatibility layers represent known missing firmware state or incompletely recovered external behavior. They are not a substitute for valid CPU instructions, general peripheral emulation, or permissive defaults. They are disabled unless the user names each layer with `--layer`.

## Allowed Intervention Classes

1. Synthetic persistent records, such as manufacturing or calibration state.
2. Byte-exact device-response fixtures for a proven startup exchange.
3. Narrow firmware hooks or status translations only when the missing state cannot be represented through storage or hardware paths.

Valid Thumb instructions—including `MOV.W r0,sp` and `STMDB` cases previously patched around Renode decoder defects—must execute in the CPU and are forbidden as compatibility hooks.

## Required Declaration

Every layer declares a stable ID, exact firmware component SHA-256 hashes, evidence references, intervention class, synthetic/recovered fields, trigger predicate, allowed hit count, expected effects, and emitted event name. It must also document why a lower-level representation is not currently possible.

Activation fails before reset when required hashes do not match. At runtime, wrong state, an unexpected trigger, or hit-budget exhaustion stops with compatibility refusal.

## Observability

Every intervention emits a structured event containing layer ID, virtual time, trigger, hit ordinal, and effect summary. End-of-run output lists enabled layers and their hit counts, including zero. Determinism tests compare this ordered record.

Compatibility logs must not contain firmware bytes, secrets, or unstable host data. Layer implementations cannot suppress unmapped accesses or unknown transactions outside their declared trigger.

## Lifecycle

Layers are version-pinned and never auto-selected by product family. Evidence improvements should replace a layer with real storage/device behavior. Removal requires retaining a regression test proving the authentic path reaches the same checkpoint. A layer may remain available for historical firmware after later versions no longer need it.

The initial planned layer, `sapporo-2.22-no-device`, may supply synthetic manufacturing state and minimum GPS/OHR startup fixtures only after their field and transcript provenance is entered in `docs/migration-evidence.md`.

