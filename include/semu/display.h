#ifndef SEMU_DISPLAY_H
#define SEMU_DISPLAY_H

#include "semu/bus.h"
#include "semu/frame.h"
#include "semu/types.h"

typedef struct semu_surface semu_surface;

typedef semu_transaction_result (*semu_display_backend_submit_fn)(
    void *context, semu_bus *bus, uint32_t command_ring_address,
    uint32_t command_word_count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error);

#define SEMU_DISPLAY_MAX_LISTS 64u
#define SEMU_DISPLAY_LIST_INLINE 1u
typedef struct semu_display_list {
    uint32_t address;
    uint32_t word_count;
    uint32_t flags;
} semu_display_list;

/* One synchronous transaction per context. Prepare stages at most MAX_LISTS
 * contiguous lists in order, including inherited state and per-list frames.
 * flags=0 requests the usual publication; LIST_INLINE stages commands without
 * publishing a frame. Unknown flags refuse. Inline writes participate in the
 * same transaction, including writes following the last published child.
 * Prepare publishes
 * nothing. Refusal leaves committed state unchanged and no new transaction;
 * a conflict preserves the already-pending transaction.
 * A successful prepare (including count zero) must be followed exactly once by
 * commit or abort. Lists/bus are borrowed only during prepare; callback/context
 * remain borrowed until commit/abort. Neither operation may allocate or fail.
 * Commit publishes prepared frames in order; abort changes no committed state.
 * Callbacks borrow immutable frames only for the call. They must not execute or
 * reset the guest, destroy owners, mutate the bus, or commit/abort recursively.
 * Same-context prepare/reset conflicts must refuse before mutation. Pending
 * transactions are transient, not serializable. No WAIT result is supported. */
typedef struct semu_display_backend_ops {
    semu_transaction_result (*prepare)(void *context, semu_bus *bus,
        const semu_display_list *lists, size_t count, uint64_t virtual_time_ns,
        semu_frame_callback callback, void *frame_context, semu_error *error);
    void (*commit)(void *context);
    void (*abort)(void *context);
} semu_display_backend_ops;

/* Persistence for the same caller-owned backend context. The machine copies
 * this table. backend_id is a stable nonzero wire identity. Save allocates a
 * bounded buffer with malloc; ownership passes to the caller (free). Output
 * arguments are unchanged on refusal. Load validates the entire byte image
 * before mutation, allocates nothing, emits no callbacks and fails atomically.
 * Both operations refuse active transactions. No host pointer is serialized.
 * published_frame borrows the last published image, or NULL before publication;
 * it must not expose later unpublished drawing pixels. Restore presentation
 * reuses this frame without advancing generation or guest time. */
typedef struct semu_display_snapshot_ops {
    uint32_t backend_id;
    semu_status (*save)(void *context, uint8_t **data, size_t *size,
        semu_error *error);
    semu_status (*load)(void *context, const uint8_t *data, size_t size,
        semu_error *error);
    const semu_frame *(*published_frame)(void *context);
} semu_display_snapshot_ops;

semu_surface *semu_surface_create(uint32_t width, uint32_t height,
                                  semu_error *error);
void semu_surface_destroy(semu_surface *surface);
void semu_surface_clear(semu_surface *surface, uint16_t rgb565);
semu_status semu_surface_write(semu_surface *surface, uint32_t x, uint32_t y,
                               uint32_t width, uint32_t height,
                               const uint8_t *rgb565_le, uint32_t stride,
                               semu_error *error);
const semu_frame *semu_surface_frame(semu_surface *surface);

#endif
