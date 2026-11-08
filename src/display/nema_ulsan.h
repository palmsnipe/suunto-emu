#ifndef SEMU_DISPLAY_NEMA_ULSAN_H
#define SEMU_DISPLAY_NEMA_ULSAN_H

#include "nema_state.h"

#define NEMA_ULSAN_SIDE 466u
#define NEMA_ULSAN_PIXELS (NEMA_ULSAN_SIDE * NEMA_ULSAN_SIDE)

typedef struct nema_ulsan {
    semu_bus *bus;
    uint32_t target;
    uint8_t dirty[(NEMA_ULSAN_PIXELS + 7u) / 8u];
    unsigned bands, pending_bands;
    unsigned list_x0, list_y0, list_x1, list_y1;
    int list_drew;
} nema_ulsan;

void nema_ulsan_begin(nema_ulsan *panel, semu_bus *bus);
semu_status nema_ulsan_draw(nema_ulsan *panel, semu_bus *bus, uint8_t *pixels,
    const nema_draw_snapshot *draw, semu_error *error);
int nema_ulsan_end_list(nema_ulsan *panel);
semu_status nema_ulsan_validate(const nema_ulsan *panel, semu_error *error);
void nema_ulsan_commit(nema_ulsan *panel, const uint8_t *pixels);

#endif
