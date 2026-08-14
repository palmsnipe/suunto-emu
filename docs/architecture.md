# Emulator Architecture

## Purpose

`suunto-emu` is a standalone, deterministic C99 emulator for Apollo4-era Suunto watches. It must boot authentic, user-supplied firmware without modifying source images and must fail closed when execution reaches behavior that has not been modeled. The first supported target is Sapporo firmware `2.22.60.3383-P` with a native 240x240 RGB565 display and three buttons.

## Dependency Direction

Dependencies point downward; lower layers never include board or frontend headers.

1. `formats/` and small utilities: endian access, hashes, strict manifests.
2. `core/`: memory bus, scheduler, immutable storage overlays, logging, tracing.
3. `cpu/armv7m/`: ARMv7E-M/Thumb-2 state and execution through the core bus.
4. `buses/`: typed SPI, I2C, UART, and storage transactions.
5. `soc/apollo4/`: register blocks, IRQ routing, DMA, timers, and controllers.
6. `devices/`: physical components connected only through typed buses.
7. `boards/`: product wiring, memory map, semantic input translation.
8. `display/`: validated GPU commands, rasterization, and panel composition.
9. `compat/`: explicit, version-pinned interventions around normal hardware paths.
10. `frontends/`: headless CLI and optional SDL3 presentation.

No module may reach into another module's private structures. Cross-module declarations live under `include/semu/`; a task changing them must be an integration task listed in `plans/index.tsv`.

## Ownership and Lifetimes

`semu_machine` is the ownership root. It owns the CPU, address space, scheduler, board, compatibility state, trace sinks, and stop record. Creation either returns a fully initialized machine or no machine. Destruction must accept a partially initialized machine and release all owned resources.

The machine API must provide:

- create/destroy and reset;
- bounded execution by instructions and virtual time;
- board-level semantic input injection;
- frame publication callback registration;
- trace configuration;
- read-only stop-state and execution-state inspection.

Callbacks receive borrowed data valid only for the duration of the call. Device and region contexts are owned by the board or machine, never by the bus registry.

## Stable Cross-Module Contracts

- `semu_stop_reason` includes halt, budget exhausted, deadlocked WFI, unmapped access, unsupported instruction, refused device transaction, firmware assertion, and compatibility refusal.
- Memory regions have a base, nonzero size, context, and width-aware little-endian read/write/reset operations. Registration rejects overflow and overlap.
- Peripheral transactions return `SEMU_TX_OK`, `SEMU_TX_WAIT`, or `SEMU_TX_REFUSE`; refusal carries a diagnostic at the controller/machine boundary.
- Scheduled events use integer virtual time and stable insertion order. Equal-deadline events run FIFO.
- Display frames carry pixel format, width, height, stride, generation, and immutable bytes. RGB565 byte order is explicitly declared by the pixel format.
- Input is semantic (`upper`, `middle`, `lower`, crown, touch, sensor) until translated by a board.
- Storage bases are immutable. Program and erase operations write only to a sparse session overlay.

## Fail-Closed Rules

The emulator stops on overlapping regions, unmapped accesses, unsupported instruction encodings, invalid access widths, out-of-range DMA, unknown device commands, unexpected compatibility triggers, and malformed profiles. There is no global read-as-zero peripheral fallback. A documented register bank may implement explicit placeholder offsets only when each offset and reset value is listed in evidence.

## Extension Boundaries

A new product is a new board/profile pair, even when it shares an Apollo4 SoC. Share CPU, controller, bus, and device behavior only when traces or documentation prove identity at that layer. Version-specific quirks belong in profiles, board glue, or compatibility layers, never in generic CPU or bus code.

