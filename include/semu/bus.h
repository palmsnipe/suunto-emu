#ifndef SEMU_BUS_H
#define SEMU_BUS_H

#include "semu/types.h"

typedef struct semu_bus semu_bus;

typedef semu_status (*semu_bus_read_fn)(void *context, uint32_t offset,
                                        unsigned width, uint32_t *value,
                                        semu_error *error);
typedef semu_status (*semu_bus_write_fn)(void *context, uint32_t offset,
                                         unsigned width, uint32_t value,
                                         semu_error *error);
typedef void (*semu_bus_reset_fn)(void *context);

typedef struct semu_bus_device_ops {
    semu_bus_read_fn read;
    semu_bus_write_fn write;
    semu_bus_reset_fn reset;
} semu_bus_device_ops;

semu_bus *semu_bus_create(semu_error *error);
void semu_bus_destroy(semu_bus *bus);
void semu_bus_reset(semu_bus *bus);
semu_status semu_bus_map_ram(semu_bus *bus, const char *name, uint32_t base,
                             uint32_t size, semu_error *error);
semu_status semu_bus_map_rom(semu_bus *bus, const char *name, uint32_t base,
                             const uint8_t *data, uint32_t size,
                             semu_error *error);
semu_status semu_bus_map_device(semu_bus *bus, const char *name, uint32_t base,
                                uint32_t size, const semu_bus_device_ops *ops,
                                void *context, semu_error *error);
semu_status semu_bus_read(semu_bus *bus, uint32_t address, unsigned width,
                          uint32_t *value, semu_error *error);
semu_status semu_bus_write(semu_bus *bus, uint32_t address, unsigned width,
                           uint32_t value, semu_error *error);
semu_status semu_bus_load(semu_bus *bus, uint32_t address, const uint8_t *data,
                          size_t size, semu_error *error);
semu_status semu_bus_copy_out(semu_bus *bus, uint32_t address, uint8_t *data,
                              size_t size, semu_error *error);

#endif
