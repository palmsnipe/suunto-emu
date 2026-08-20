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
    bus_region *regular_cache[2];
    uint64_t overlay_max_end;
    uint32_t overlay_min_base;
    size_t overlay_count;
};

semu_status semu_bus_map_overlay(semu_bus *bus, const char *name,
                                 uint32_t base, uint32_t size,
                                 const semu_bus_device_ops *ops,
                                 void *context, semu_error *error);
void semu_bus_unmap_overlay(semu_bus *bus, void *context);
semu_status semu_bus_read_below(semu_bus *bus, uint32_t address,
                                unsigned width, uint32_t *value,
                                semu_error *error);
semu_status semu_bus_read_u16(semu_bus *bus, uint32_t address,
                              uint32_t *value, semu_error *error);
semu_status semu_bus_write_below(semu_bus *bus, uint32_t address,
                                 unsigned width, uint32_t value,
                                 semu_error *error);

semu_status semu_bus_snapshot_write(const semu_bus *bus,
                                    semu_snapshot_writer *writer,
                                    semu_error *error);
semu_status semu_bus_snapshot_read(semu_bus *bus,
                                   semu_snapshot_reader *reader,
                                   semu_error *error);

#endif
