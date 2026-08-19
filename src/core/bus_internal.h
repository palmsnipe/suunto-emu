#ifndef SEMU_BUS_INTERNAL_H
#define SEMU_BUS_INTERNAL_H

#include "semu/bus.h"

#include "snapshot_io.h"

typedef enum bus_region_kind {
    REGION_RAM = 0,
    REGION_ROM,
    REGION_DEVICE
} bus_region_kind;

typedef struct bus_region {
    char name[SEMU_ID_MAX];
    uint32_t base;
    uint32_t size;
    bus_region_kind kind;
    uint8_t *memory;
    semu_bus_device_ops ops;
    void *context;
    uint8_t overlay;
} bus_region;

struct semu_bus {
    bus_region *regions;
    size_t count;
    size_t capacity;
};

semu_status semu_bus_snapshot_write(const semu_bus *bus,
                                    semu_snapshot_writer *writer,
                                    semu_error *error);
semu_status semu_bus_snapshot_read(semu_bus *bus,
                                   semu_snapshot_reader *reader,
                                   semu_error *error);

#endif
