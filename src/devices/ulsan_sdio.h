#ifndef SEMU_DEVICES_ULSAN_SDIO_H
#define SEMU_DEVICES_ULSAN_SDIO_H

#include "semu/bus.h"

typedef struct {
    uint8_t *media;
    size_t media_size;
    uint32_t registers[64];
    size_t transfer_start;
    size_t transfer_size;
    size_t transferred;
    uint8_t pending_write[512];
    int writing;
    int ready;
    int selected;
} semu_ulsan_sdio;

/* The caller owns the mutable session copy for the lifetime of the bus. */
semu_status semu_ulsan_sdio_map(semu_bus *bus, semu_ulsan_sdio *state,
    uint8_t *media, size_t media_size, semu_error *error);
uint8_t *semu_ulsan_sdio_load_media(const char *path, size_t *size,
                                    semu_error *error);

#endif
