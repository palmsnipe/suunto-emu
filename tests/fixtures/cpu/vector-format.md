# CPU Instruction Vector Contract

This is a textual contract for dependency-free C99 test tables. It is not a
runtime parser, a generated opcode table, or a claim that the current CPU
implementation supports every row a future ticket may add.

## Canonical row

Each vector fixes one bounded execution and has these fields, in this order:

```text
vector_id | evidence_id:section | instruction_bytes | initial_pc | initial_xpsr | initial_registers | initial_memory | max_steps | expected_status | expected_stop | final_pc | final_xpsr | final_registers | final_memory | expected_exception
```

The field meanings are:

| Field | Contract |
| --- | --- |
| `vector_id` | Stable, repository-local name. It must not contain a firmware address or source-image bytes. |
| `evidence_id:section` | One or more ledger IDs with exact section locations. `E-CPU-0001` is valid only for its two named regressions. |
| `instruction_bytes` | Non-empty, space-separated two-digit hexadecimal bytes in guest memory order. The row must contain the complete bounded program, including any instruction used to stop the vector. |
| `initial_pc` | Architectural aligned PC at the first byte of the program. A reset vector’s Thumb bit is consumed before this field is recorded. |
| `initial_xpsr` | Complete raw xPSR value, including the Thumb bit and exception number. |
| `initial_registers` | Sparse assignments for `r0`–`r15`, `msp`, `psp`, `primask`, `basepri`, `faultmask`, `control`, `fpscr`, and `s0`–`s31`. Unlisted values come from the named C99 fixture baseline. |
| `initial_memory` | Sparse `address=byte` assignments for data or vector-table bytes not already in `instruction_bytes`; addresses and values are hexadecimal. |
| `max_steps` | Positive instruction/exception-retirement bound. A test must not continue after this bound. |
| `expected_status` | Exact `semu_status` result of the final step, such as `SEMU_OK` or `SEMU_ERR_UNSUPPORTED`. |
| `expected_stop` | Exact `SEMU_STOP_*` value after the bounded run, or `none` when execution has not stopped. |
| `final_pc`, `final_xpsr`, `final_registers`, `final_memory` | Exact post-run state. Sparse final maps compare every listed value and require every unlisted register/byte to remain unchanged from the initial state. |
| `expected_exception` | `none`, `architectural:<name>/<number>`, or `emulator-refusal:<reason>`. A refusal is not silently converted into an architectural exception. |

## Byte and state rules

- A halfword at address `A` is stored as low byte then high byte. A 32-bit
  Thumb instruction lists its first halfword followed by its second halfword,
  each in that little-endian order. For example, `EA4F 000D` is
  `4f ea 0d 00` in memory order.
- A word in `initial_memory` or `final_memory` is represented as four byte
  assignments, low address first. No host-endian integer dump is valid.
- A vector’s program bytes are immutable input. Memory assertions cover only
  mapped guest bytes; an omitted byte is required to be unchanged, including
  bytes outside the touched range.
- Omitted initial state is the fixture baseline. Omitted final state is an
  unchanged-state assertion. A refusal vector must validate the complete state,
  memory, instruction count, status, and stop reason before accepting the
  refusal.
- `expected_exception` distinguishes an architectural exception supported by
  the cited reference from an emulator refusal for an unsupported or malformed
  encoding. A valid instruction must never be represented as a compatibility
  hook.
- Every vector has an explicit bound. A vector that needs host time, random
  input, a writable source image, or an unbounded loop is invalid.

## Coverage states

Coverage rows use exactly these states:

- `missing`: no pinned vector and no accepted implementation for the named
  behavior. This is the default for unlisted forms.
- `vector-only`: a pinned positive or refusal vector exists, but no passing
  implementation test has accepted it.
- `implemented`: the exact behavior has a passing in-tree test, but a complete
  family corpus or shared-vector execution contract is not yet complete.
- `verified`: the exact vector is executed by the reusable fixture, has the
  required positive/refusal counterpart, passes twice with identical state, and
  cites one of `E-CPU-0001` through `E-CPU-0005`.

Ticket 180 seeds only exact behaviors already exercised by
`tests/unit/test_cpu.c`. Those rows are `implemented`, not `verified`, because
the current tests still embed state directly and no shared vector runner
exists. No row is a family-completeness claim.

## Seed examples

These examples name the two existing `E-CPU-0001` regressions and the existing
unsupported-instruction refusal. They specify the contract future ticket 190
can move into reusable C99 tables; they do not add a runtime fixture here.

```text
cpu.mov-w-sp | E-CPU-0001:A7.7.77 MOV (register) | 4f ea 0d 00 00 be | 0x100 | 0x01000000 | r13=0x800,r15=0x100 | - | 1 | SEMU_OK | none | 0x104 | 0x01000000 | r0=0x800,r13=0x800,r15=0x104 | - | none
cpu.stmdb-sp | E-CPU-0001:A5.3.5/A7.7 STMDB | 2d e9 f8 41 00 be | 0x100 | 0x01000000 | r3=0x1003,r4=0x1004,r5=0x1005,r6=0x1006,r7=0x1007,r8=0x1008,r13=0x800,r14=0xfeed0001,r15=0x100 | - | 1 | SEMU_OK | none | 0x104 | 0x01000000 | r13=0x7e4,r15=0x104 | 0x7e4=03 10 00 00;0x7e8=04 10 00 00;0x7ec=05 10 00 00;0x7f0=06 10 00 00;0x7f4=07 10 00 00;0x7f8=08 10 00 00;0x7fc=01 00 ed fe | none
cpu.udf-refusal | E-CPU-0002:A5.2.6/A7.7.194 UDF | 00 de | 0x100 | 0x01000000 | r13=0x800,r15=0x100 | - | 1 | SEMU_ERR_UNSUPPORTED | SEMU_STOP_UNSUPPORTED_INSTRUCTION | 0x100 | 0x01000000 | - | - | emulator-refusal:unsupported-instruction
```

The `BKPT` bytes in the first two examples are outside the bounded execution
because `max_steps` is one. They make the examples complete programs without
weakening the exact one-step expectations.
