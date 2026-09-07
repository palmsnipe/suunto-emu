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

`semu_scheduler_schedule_batch` admits up to 64 event requests atomically.
All callbacks, deadlines, count arithmetic, IDs/sequences and required storage
are validated/reserved before insertion. Failure preserves queue contents,
clock, identity counters and caller output IDs. Success assigns IDs and
sequences in input order and preserves deadline/FIFO ordering, exactly like
consecutive single-event scheduling; no callback runs during admission.
Single-event scheduling delegates to this path. Empty batches are no-ops.
The existing tagged event identity and persistent encoding are unchanged.

NEMA completion batch admission stages the complete entry group, removes
duplicate/already-pending list IDs, and calls the scheduler once before
committing device entries/counters. Reset/cancel/destruction remove only
events whose ID, callback and context still belong to that completion owner;
scheduler-reset ID reuse cannot cancel an unrelated event. The scheduler
must outlive its active completion owner. Restored completion state is rebound
to the target scheduler before its events are restored; this borrowed binding
is transient and is not serialized. GPU submissions use one batch for all
completion markers, after all child lists prepare and before publication.

A dispatched NEMA completion copies its accepted callback tuple before
retiring the reusable entry, then delivers CLID, INTERRUPT=1 and IRQ28 from
that copy. Rescheduling cannot redirect the remaining notifications. Cancel,
reset or destruction removes owned queued work but does not revoke the
in-flight sequence (E-EMU-NEMA-CALLBACK-001). Callback contexts and the
scheduler must remain alive through the whole sequence; this does not permit
destroying their machine/device owners. Scheduler recursion restrictions and
the frame-callback restrictions below remain unchanged. No notification tuple
or dispatch-local state is serialized.

The display contract now also has bounded prepare/commit/abort operations.
The NEMA backend prepares up to 64 ordered spans in instance-owned staging:
inherited registers/counters, RGB565 pixels, lazy-cloned TSC6A shadows and
per-list publication images. Prepare publishes nothing. Failure/abort retains
committed state; bounded diagnostic records may change, but cannot replace
the original draw/bus error. The single-list convenience delegates to this
same transaction, so refused lists no longer leak inherited registers.
Descriptor flags distinguish ordinary publication from inline commands without
publication; unknown flags refuse, including on an empty descriptor.

Commit allocates nothing and cannot fail: it installs staged state and
publishes the saved images in order, with unchanged per-callback generation
increments. With no callback, pixels still commit but generation does not
advance. Empty lists publish nothing. Inputs are borrowed only during prepare;
frame callback/context stay borrowed until commit/abort. Callback frames are
immutable and borrowed for that call. Reentrant prepare and reset refuse;
callbacks must not execute/reset the guest, destroy owners, mutate the bus,
or recursively commit/abort. Backend reset returns CONFLICT while a transaction
is active; destruction outside callbacks releases any pending staging.
Transient staging is not encoded in machine snapshots. Machine options and
GPU construction use the public operations table, copied at creation; the
single-list callback remains only a backend convenience entry point.

An active CMDRINGSTOP write builds one validated ring plan containing ordered
inline spans, child lists and completion markers. It prepares all command spans
in one backend transaction, then atomically admits all
markers. Only after successful admission does it commit frames and consume
the stop pointer/generation. Refusal preserves those values and the queue,
publishes no frame/IRQ and returns the original error through MMIO. Unexpected
WAIT/invalid callback results and missing backend configuration refuse with
a bounded fallback diagnostic; corrected retries of the same stop execute.
Successful repeated stops remain no-ops. Marker-only work retains its delayed
completion; direct-list work retains its immediate IRQ and bootstrap has none.
Reentrant GPU writes/reset/snapshot and machine reset refuse before mutation.
Guest execution, bus mutation, owner destruction and scheduler dispatch/reset
from a frame callback remain unsupported, as required by display.h.

Control framing checks the complete ring address span before reading, computes
wrap distances without unsigned underflow, and validates all marker/held-control
fields before callbacks. Held CMDADDR/CMDSIZE records require the configured
capacity and either the ring base or the immediate continuation target; both
forms are evidenced by native builders (E-EMU-NEMA-CONTROL-001). Byte/halfword
GPU accesses refuse without changing registers or read outputs. Production-path
allocation tests cover parser records, staged frames and scheduler growth,
including inherited-state preservation and corrected same-stop retry.

Child and backend lists require complete register/value pairs. Odd word counts
refuse before callbacks or renderer staging; held tails are not an exception.
The old rounded-tail theory was superseded by the native CMDSIZE entry-count
correction (E-EMU-NEMA-TAIL-001). No missing value is read outside the list.
Ring and list words are fetched as explicit little-endian mapped-memory copies,
not device reads. Device-backed commands refuse with the original bus error
without invoking MMIO callbacks (E-EMU-NEMA-MEMORY-001). Texture validation and
RGB565/A2LE sampling now also use memory-only byte copies, preserving one-byte
overlay checks and adjacent mapped-memory access. Failed samples leave their
whole texel/alpha output unchanged; bilinear taps remain staged. The descriptor
validator checks arithmetic and last-byte addressability, not the whole range:
each actually sampled byte is checked separately, with original bus errors
retained. Rendering callers stage target pixels until their samples succeed
(E-EMU-NEMA-TEXTURE-MEMORY-001); the backend stages whole child submissions.

Inline framing uses the same known-register map as child execution. Values
cannot be interpreted as opcodes; only exact NOP padding is accepted. The
old separate marker scan and whole-ring wrapped fallback are removed. A plan
contains at most 32 children, 64 completion markers and 64 total command spans;
32 children interleaved with 32 inline runs are supported. Refusal leaves the
caller-owned plan unchanged. Existing child-only framing callbacks share this
parser but retain their child-only purpose (E-EMU-NEMA-INLINE-001).

Inline runs stage inherited state and pixels without extra frame generations.
This includes inline writes after the last child: final working pixels commit
after ordered child callbacks. Direct graphics-only submissions publish once
at their final span and retain their immediate IRQ. INTERRUPT=0 is accepted
as non-requesting control, not an inferred IRQ-clear operation. Initialization
therefore adds no frame or IRQ. Complete inline runs can wrap as separate
contiguous spans; a graphics pair split across the physical ring end refuses.
Ticket 761's recorded callback-lifecycle and transaction acceptance cases pass;
integrator review remains separate from implementation and status updates.
Arbitrary held jumps and fragment-processor ISA execution are not supported; no physical timing or
unobserved command behavior is inferred from the transaction tests.

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
