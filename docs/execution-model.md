# Deterministic Execution Model

## Time and Scheduling

Virtual time is an unsigned integer count of functional ticks. Normal instructions advance by a documented fixed cost; peripherals schedule future state changes rather than consulting host time. Events are ordered by `(deadline, insertion_sequence)`, making equal-time execution stable across platforms.

Execution is always bounded by an instruction limit, a virtual-time limit, or both. Budget exhaustion is a normal stop reason, not an error. Host wall time, threads, locale, random sources, and filesystem timestamps must not affect guest-visible state.

Callbacks retain their void ABI. A callback that cannot complete a device
operation calls `semu_scheduler_callback_fail` with its diagnostic. The
scheduler owns a copy of the first error until that dispatch returns; later
reports or successful scheduling calls cannot replace it. Invalid reports
(NULL, OK, or an unsupported status code) report `SEMU_ERR_ARGUMENT`.
Reporting outside a callback refuses with `SEMU_ERR_STATE` (a NULL scheduler
returns `SEMU_ERR_ARGUMENT`) without affecting future dispatch.

`run_next` and `advance`, including the one-tick CPU path, return that first
failure without draining remaining due events or advancing to the requested
target. The failed event has been consumed and time remains at its deadline,
unless the callback explicitly reset the scheduler. There is no generic
rollback of callback side effects. CPU tick and sleeping WFI/WFE dispatch
failures halt with `device-refused`, preserving the original diagnostic and
without fabricating an architectural exception. A retired instruction still
costs one instruction; a sleeping dispatch retires none.

Recursive dispatch or time advancement on the same scheduler refuses before
queue/time mutation and reports `SEMU_ERR_STATE` to the outer dispatch.
Scheduling, cancellation and reset from callbacks remain permitted; reset
clears the queue, clock and counters but cannot erase an active first error
or the recursion guard. Destruction during a callback is unsupported. The
failure is cleared after dispatch so explicit subsequent dispatch/reset/reuse
does not inherit it; the guard and diagnostic are transient, not serialized.

CXD awake admission validates that both deadlines fit and queues the rise
before committing transport state or delivering low. At rise, the falling
event receives its fresh ID/sequence at that same historical deadline, before
high is emitted. Failure to schedule it reports the scheduler diagnostic and
emits no high or artificial zero-width pulse. A signal sink's explicit reset
still cancels the pending event; resetting during accepted low reports that
interruption, and resetting during high cancels the already-arranged fall.
Successful pulse width remains 1 ms (E-EMU-CXD-AWAKE-FAILURE-001).

## Reset and Run

Reset is deterministic and proceeds in this order:

1. clear the prior stop record and scheduler queue;
2. reset storage overlays and compatibility hit counts as requested by reset mode;
3. reset board devices and SoC register blocks;
4. reset CPU architectural state and load MSP/PC from the configured vector table;
5. schedule board-defined reset events.

One execution step fetches and executes a complete instruction or enters an architectural fault. Peripheral side effects caused by an instruction complete before same-time scheduled events. Events due at the resulting virtual time are then drained in stable order.

## WFI, WFE, and Interrupts

`WFI` advances directly to the next event capable of changing interrupt state. If no such event exists, execution stops with deadlocked WFI. `WFE` observes the architectural event register; with no event and no future wake source it stops identically. Interrupt eligibility is recalculated after instruction retirement, exception return, and each scheduled event.

Exception entry/return, nested IRQ priority, MSP/PSP selection, privilege, masks, SysTick, PendSV, SVC, and optional FPU stacking are CPU responsibilities. Apollo4 models expose external interrupt lines and priorities without implementing CPU stacking themselves.

## Memory and DMA

All CPU and DMA accesses pass through the same registered address-space API. Accesses declare address, width, direction, and origin. Regions implement little-endian accesses and may reject unsupported widths. DMA validates the complete range before making any transfer, so a refused transfer cannot partially mutate memory.

Validated component bytes are loaded from read-only source files into immutable emulator-owned bases before mapping. Guest program/erase updates a sparse overlay; reads merge overlay and base. Reset policy controls whether a session overlay persists, but source files are never opened writable or modified.

## Reproducibility Record

A deterministic comparison uses the ordered tuple of stop reason, virtual time, instruction count, checkpoint identifiers, frame generations and hashes, compatibility hit counts, and normalized device transcripts. Host paths, pointer values, and wall-clock timestamps are excluded from comparison output.

Trace buffers are bounded. Overflow is explicit and stops or truncates according to selected trace policy; it may never silently change guest behavior.
