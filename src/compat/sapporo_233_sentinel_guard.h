#ifndef SEMU_SAPPORO_233_SENTINEL_GUARD_H
#define SEMU_SAPPORO_233_SENTINEL_GUARD_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/scheduler.h"

/* E-SAP-0092..E-SAP-0107 class: the 2.33.16 sensor-poll sentinel-insert
 * guard. The guest's periodic sensor-poll pass tail-branches a stub that
 * inserts the global foreign sentinel 0x100331c0 into the object's own
 * ordered list on every pass (E-SAP-0095/0096). The second insert of the
 * already-linked sentinel rewrites the sentinel's +4 field to itself
 * (E-SAP-0092), producing the permanent five-instruction self-ring walk
 * that wedges the poll task at 37.1 s of the staged boot (E-SAP-0098/0099).
 *
 * The guard admits exactly the witnessed corrupting shape and skips that
 * insert by advancing PC to the wrapper continuation 0x000a4e8a:
 *   - PC is the insert entry 0x000bb69c,
 *   - r1 is the foreign sentinel 0x100331c0,
 *   - the sensor list's head-anchor next word (0x10029c50) already points
 *     at that sentinel (the first insert completed),
 *   - the sentinel's own +4 field is not yet self-referencing,
 *   - lr is the wrapper continuation 0x000a4e8a (the Thumb link-register
 *     form 0x000a4e8b; the skip lands on the even continuation PC).
 * Every other shape stays native. An already-present self-ring or an
 * unexpected return address refuses fail-closed (the guest state is not
 * the witnessed pre-corruption shape).
 *
 * Budget policy (E-SAP-0117, the recommended per-virtual-time rate cap):
 * the observed poll cycle consumes 2.26-2.44M skips per virtual second;
 * the guard admits at most 3,000,000 skips per virtual second (headroom),
 * computed from the scheduler's time since the last machine reset (the
 * same epoch over which the layer hit counter resets), and refuses
 * fail-closed if the guest ever cycles faster than observed. The absolute
 * ceiling is rate x the supported 120 s census window; a longer run
 * refuses rather than growing without bound. Hits are logged at the first
 * skip and every 1,000,000 skips (per-hit logging would be unbounded for
 * a high-frequency guard). */
#define SEMU_SAPPORO_233_SENTINEL_INSERT_PC UINT32_C(0x000bb69c)
#define SEMU_SAPPORO_233_SENTINEL_NODE UINT32_C(0x100331c0)
#define SEMU_SAPPORO_233_SENSOR_LIST_HEAD_NEXT UINT32_C(0x10029c50)
#define SEMU_SAPPORO_233_SENTINEL_NEXT UINT32_C(0x100331c4)
#define SEMU_SAPPORO_233_SENTINEL_CONTINUATION UINT32_C(0x000a4e8a)
#define SEMU_SAPPORO_233_SENTINEL_RATE_PER_SECOND UINT32_C(3000000)
#define SEMU_SAPPORO_233_SENTINEL_WINDOW_NS UINT64_C(120000000000)
#define SEMU_SAPPORO_233_SENTINEL_MAX_HITS UINT64_C(360000000)
#define SEMU_SAPPORO_233_SENTINEL_LOG_STRIDE UINT64_C(1000000)

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_scheduler *scheduler;
    semu_error refusal;
} semu_sapporo_233_sentinel_guard_context;

extern const semu_layer_descriptor semu_sapporo_233_sentinel_guard_layer;

/* Binds the layer state and the virtual-time source, and clears the
 * sticky refusal. Called from the machine reset path (every reset) after
 * the layer has been enabled; a mismatch refuses. */
semu_status semu_sapporo_233_sentinel_guard_install(
    semu_sapporo_233_sentinel_guard_context *c, semu_layer_state *state,
    semu_logger *logger, semu_scheduler *scheduler, semu_error *error);

/* Applies the skip at the insert PC. Returns SEMU_OK after handling or
 * when the shape is not the layer's (the native path continues), and an
 * error status on any unsupported shape (the refusal is sticky). */
semu_status semu_sapporo_233_sentinel_guard_apply(
    semu_sapporo_233_sentinel_guard_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error);

#endif
