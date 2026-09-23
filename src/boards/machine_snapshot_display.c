#include "machine_internal.h"
#include <stdlib.h>

semu_status semu_machine_snapshot_write_display(const semu_machine *m,
    semu_snapshot_writer *w, semu_error *e)
{
    uint8_t *data = NULL;
    size_t size = 0u;
    semu_status status;
    if (m->display_backend == NULL)
        return semu_snapshot_writer_u32(w, 0u, e);
    if (m->display_snapshot.backend_id == 0u) {
        semu_error_set(e, SEMU_ERR_UNSUPPORTED, "display backend has no snapshot codec");
        return SEMU_ERR_UNSUPPORTED;
    }
    status = m->display_snapshot.save(m->display_backend_context, &data, &size, e);
    if (status != SEMU_OK) return status;
    if (data == NULL || size == 0u || size > SEMU_SNAPSHOT_MAX_SECTION_SIZE - 4u) {
        free(data);
        semu_error_set(e, SEMU_ERR_FORMAT, "display snapshot codec returned invalid size");
        return SEMU_ERR_FORMAT;
    }
    status = semu_snapshot_writer_u32(w, m->display_snapshot.backend_id, e);
    if (status == SEMU_OK) status = semu_snapshot_writer_bytes(w, data, size, e);
    free(data);
    return status;
}

semu_status semu_machine_snapshot_read_display(semu_machine *m,
    semu_snapshot_reader *r, semu_error *e)
{
    uint32_t id;
    semu_status status = semu_snapshot_reader_u32(r, &id, e);
    if (status != SEMU_OK) return status;
    if (id != m->display_snapshot.backend_id ||
        (id == 0u) != (m->display_backend == NULL)) {
        semu_error_set(e, SEMU_ERR_CONFLICT, "snapshot display backend identity differs");
        return SEMU_ERR_CONFLICT;
    }
    if (id == 0u) return SEMU_OK;
    status = m->display_snapshot.load(m->display_backend_context,
        r->data + r->offset, r->size - r->offset, e);
    if (status == SEMU_OK) r->offset = r->size;
    return status;
}
