#ifndef SEMU_BUS_ACCESS_H
#define SEMU_BUS_ACCESS_H

#include "bus_internal.h"

static inline int semu_bus_valid_width(unsigned width)
{
    return width == 1u || width == 2u || width == 4u;
}

static inline void semu_bus_clear_success(semu_error *error)
{
    if (error != NULL) {
        error->code = SEMU_OK;
        error->text[0] = '\0';
    }
}

static inline uint32_t semu_bus_read_little_endian(const uint8_t *data,
                                                    unsigned width)
{
    if (width == 1u) return data[0];
    if (width == 2u) {
        return (uint32_t)data[0] | (uint32_t)data[1] << 8u;
    }
    return (uint32_t)data[0] | (uint32_t)data[1] << 8u |
           (uint32_t)data[2] << 16u | (uint32_t)data[3] << 24u;
}

static inline void semu_bus_write_little_endian(uint8_t *data,
                                                 unsigned width,
                                                 uint32_t value)
{
    data[0] = (uint8_t)value;
    if (width == 1u) return;
    data[1] = (uint8_t)(value >> 8u);
    if (width == 2u) return;
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static inline int semu_bus_overlay_may_cover(const semu_bus *bus,
                                              uint32_t address,
                                              uint64_t end)
{
    return bus->overlay_count != 0u && address < bus->overlay_max_end &&
           end > bus->overlay_min_base;
}

static inline bus_region *semu_bus_cached_regular(semu_bus *bus,
                                                  uint32_t address,
                                                  uint64_t end)
{
    size_t slot;
    for (slot = 0u; slot < 2u; ++slot) {
        bus_region *region = bus->regular_cache[slot];
        if (region != NULL) {
            if (address >= region->base &&
                end <= (uint64_t)region->base + region->size) {
                return region;
            }
        }
    }
    return NULL;
}

#endif
