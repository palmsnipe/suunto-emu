#ifndef SEMU_NEMA_STATE_INTERNAL_H
#define SEMU_NEMA_STATE_INTERNAL_H
#include "nema_state.h"
#define NEMA_STATE_REGISTER_COUNT 34u
struct nema_state {
    uint32_t values[NEMA_STATE_REGISTER_COUNT];
    uint64_t presence;
    uint32_t list_id;
    size_t snapshot_count;
};
#endif
