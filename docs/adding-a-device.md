# Adding a Physical Device

## Before Coding

Create an evidence entry with product/version, bus, address or chip select, reset observations, successful transaction bytes, and at least one unknown/refused transaction. Decide whether the behavior is a physical device, controller feature, board-wiring rule, or version-pinned compatibility fixture. Do not put board addresses into a reusable device model.

## Implementation Contract

1. Define the smallest device state and typed bus callback needed by observed traffic.
2. Keep transport framing in the SPI/I2C/UART adapter and physical state in the device.
3. Make reset values explicit and deterministic.
4. Validate address/register, direction, length, state, and payload before mutation.
5. Return `WAIT` only with a scheduled completion/wake path; otherwise return `REFUSE`.
6. Route IRQ or DMA effects through controller interfaces, never by mutating CPU internals.
7. Wire the instance in the board module and document chip select/address and interrupt line.

Unknown commands and invalid state transitions must refuse with a bounded diagnostic. Do not add a wildcard success response, blanket read-as-zero behavior, host-time delay, or random sensor data.

## Tests Required Before Functional Status

- reset and repeated-reset behavior;
- one byte-exact successful transcript for each supported operation;
- wrong address/register/command, wrong length, and wrong-state refusal;
- boundary and DMA-range tests where applicable;
- deterministic IRQ/event timing;
- two-run transcript equality;
- authentic-firmware checkpoint when advancing to verified.

Update `docs/hardware-coverage.md` and `docs/migration-evidence.md` in the same integration change. If a fixture is temporarily needed, define it under `compat/` according to `docs/compatibility-policy.md`; do not hide it in the physical device.

## Review Checklist

The implementation must be C99, warnings-clean, below the line limit, independent of SDL3, and free of copyrighted firmware bytes. Public header changes require an integration ticket. Cite the task ID and evidence IDs in the change description.

