#include "bus_internal.h"
#include "bus_access.h"

#include <stdlib.h>
#include <string.h>

static uint64_t region_end(const bus_region *region) {
    return (uint64_t)region->base + region->size; }
static int valid_interval(uint32_t base, uint32_t size) {
    return size != 0u && (uint64_t)base + size <= UINT64_C(0x100000000); }
static void refresh_lookup(semu_bus *bus)
{
    size_t index;
    bus->regular_cache[0] = NULL;
    bus->regular_cache[1] = NULL;
    bus->overlay_count = 0u;
    bus->overlay_min_base = UINT32_MAX;
    bus->overlay_max_end = 0u;
    for (index = 0u; index < bus->count; ++index) {
        bus_region *region = &bus->regions[index];
        if (region->overlay == 0u) continue;
        ++bus->overlay_count;
        if (region->base < bus->overlay_min_base)
            bus->overlay_min_base = region->base;
        if (region_end(region) > bus->overlay_max_end)
            bus->overlay_max_end = region_end(region);
    }
}

static semu_status reserve_region(semu_bus *bus, semu_error *error)
{
    bus_region *replacement;
    size_t capacity;

    if (bus->count < bus->capacity) {
        return SEMU_OK;
    }
    capacity = bus->capacity == 0u ? 16u : bus->capacity * 2u;
    if (capacity < bus->capacity || capacity > SIZE_MAX / sizeof(*bus->regions)) {
        semu_error_set(error, SEMU_ERR_NOMEM, "bus region capacity overflow");
        return SEMU_ERR_NOMEM;
    }
    replacement = (bus_region *)realloc(bus->regions,
                                         capacity * sizeof(*bus->regions));
    if (replacement == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot grow bus region table");
        return SEMU_ERR_NOMEM;
    }
    bus->regions = replacement;
    bus->capacity = capacity;
    return SEMU_OK;
}

static semu_status insert_region(semu_bus *bus, bus_region *region,
                                 semu_error *error)
{
    size_t position;
    semu_status status;

    if (bus == NULL || region == NULL || region->name[0] == '\0' ||
        !valid_interval(region->base, region->size)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus region");
        return SEMU_ERR_ARGUMENT;
    }
    for (position = 0u; position < bus->count; ++position) {
        bus_region *other = &bus->regions[position];
        if ((uint64_t)region->base < region_end(other) &&
            (uint64_t)other->base < region_end(region) &&
            region->overlay == 0u && other->overlay == 0u) {
            semu_error_set(error, SEMU_ERR_CONFLICT,
                           "region %s overlaps %s", region->name, other->name);
            return SEMU_ERR_CONFLICT;
        }
    }
    status = reserve_region(bus, error);
    if (status != SEMU_OK) {
        return status;
    }
    position = bus->count;
    while (position > 0u && bus->regions[position - 1u].base > region->base) {
        bus->regions[position] = bus->regions[position - 1u];
        --position;
    }
    bus->regions[position] = *region;
    ++bus->count;
    refresh_lookup(bus);
    semu_error_clear(error);
    return SEMU_OK;
}

static bus_region *find_region_kind(semu_bus *bus, uint32_t address,
                                    size_t size, int include_overlays)
{
    size_t index;
    uint64_t end = (uint64_t)address + size;
    bus_region *overlay = NULL;

    if (end > UINT64_C(0x100000000)) {
        return NULL;
    }
    if (include_overlays != 0 && bus->overlay_count != 0u &&
        address < bus->overlay_max_end && end > bus->overlay_min_base) {
        for (index = 0u; index < bus->count; ++index) {
            bus_region *region = &bus->regions[index];
            if (region->overlay != 0u && address >= region->base &&
                end <= region_end(region)) overlay = region;
        }
    }
    if (overlay != NULL) return overlay;
    {
        bus_region *cached = semu_bus_cached_regular(bus, address, end);
        if (cached != NULL) return cached;
    }
    for (index = 0u; index < bus->count; ++index) {
        bus_region *region = &bus->regions[index];
        if (region->overlay == 0u && address >= region->base &&
            end <= region_end(region)) {
            bus->regular_cache[1] = bus->regular_cache[0];
            bus->regular_cache[0] = region;
            return region;
        }
    }
    return NULL;
}

static bus_region *find_region(semu_bus *bus, uint32_t address, size_t size) {
    return find_region_kind(bus, address, size, 1); }

static bus_region *find_region_below(semu_bus *bus, uint32_t address,
                                     size_t size) {
    return find_region_kind(bus, address, size, 0); }

semu_bus *semu_bus_create(semu_error *error)
{
    semu_bus *bus = (semu_bus *)calloc(1u, sizeof(*bus));
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate bus");
        return NULL;
    }
    semu_error_clear(error);
    return bus;
}

void semu_bus_destroy(semu_bus *bus)
{
    size_t index;
    if (bus == NULL) {
        return;
    }
    for (index = 0u; index < bus->count; ++index) {
        free(bus->regions[index].memory);
    }
    free(bus->regions);
    free(bus);
}

void semu_bus_reset(semu_bus *bus)
{
    size_t index;
    if (bus == NULL) {
        return;
    }
    for (index = 0u; index < bus->count; ++index) {
        bus_region *region = &bus->regions[index];
        if (region->kind == REGION_RAM) {
            (void)memset(region->memory, 0, region->size);
        } else if (region->kind == REGION_DEVICE && region->ops.reset != NULL) {
            region->ops.reset(region->context);
        }
    }
}

static semu_status map_memory(semu_bus *bus, const char *name, uint32_t base,
                              const uint8_t *data, uint32_t size,
                              bus_region_kind kind, semu_error *error)
{
    bus_region region;
    size_t name_length;
    semu_status status;

    if (bus == NULL || name == NULL || !valid_interval(base, size)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid memory mapping");
        return SEMU_ERR_ARGUMENT;
    }
    name_length = strlen(name);
    if (name_length == 0u || name_length >= sizeof(region.name) ||
        (kind == REGION_ROM && data == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid memory mapping data");
        return SEMU_ERR_ARGUMENT;
    }
    (void)memset(&region, 0, sizeof(region));
    (void)memcpy(region.name, name, name_length + 1u);
    region.base = base;
    region.size = size;
    region.kind = kind;
    region.memory = (uint8_t *)malloc(size);
    if (region.memory == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate region %s", name);
        return SEMU_ERR_NOMEM;
    }
    if (kind == REGION_ROM) {
        (void)memcpy(region.memory, data, size);
    } else {
        (void)memset(region.memory, 0, size);
    }
    status = insert_region(bus, &region, error);
    if (status != SEMU_OK) {
        free(region.memory);
    }
    return status;
}

semu_status semu_bus_map_ram(semu_bus *bus, const char *name, uint32_t base,
                             uint32_t size, semu_error *error)
{
    return map_memory(bus, name, base, NULL, size, REGION_RAM, error);
}

semu_status semu_bus_map_rom(semu_bus *bus, const char *name, uint32_t base,
                             const uint8_t *data, uint32_t size,
                             semu_error *error)
{
    return map_memory(bus, name, base, data, size, REGION_ROM, error);
}

static semu_status map_device(semu_bus *bus, const char *name, uint32_t base,
                              uint32_t size, const semu_bus_device_ops *ops,
                              void *context, uint8_t overlay,
                              semu_error *error)
{
    bus_region region;
    size_t name_length;

    if (bus == NULL || name == NULL || ops == NULL ||
        !valid_interval(base, size)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid device mapping");
        return SEMU_ERR_ARGUMENT;
    }
    name_length = strlen(name);
    if (name_length == 0u || name_length >= sizeof(region.name) ||
        (ops->read == NULL && ops->write == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid device operations");
        return SEMU_ERR_ARGUMENT;
    }
    (void)memset(&region, 0, sizeof(region));
    (void)memcpy(region.name, name, name_length + 1u);
    region.base = base;
    region.size = size;
    region.kind = REGION_DEVICE;
    region.ops = *ops;
    region.context = context;
    region.overlay = overlay;
    return insert_region(bus, &region, error);
}

semu_status semu_bus_map_device(semu_bus *bus, const char *name, uint32_t base,
                                uint32_t size, const semu_bus_device_ops *ops,
                                void *context, semu_error *error)
{
    return map_device(bus, name, base, size, ops, context, 0u, error);
}

semu_status semu_bus_map_overlay(semu_bus *bus, const char *name,
                                 uint32_t base, uint32_t size,
                                 const semu_bus_device_ops *ops,
                                 void *context, semu_error *error)
{
    return map_device(bus, name, base, size, ops, context, 1u, error);
}

void semu_bus_unmap_overlay(semu_bus *bus, void *context)
{
    size_t index;

    if (bus == NULL) return;
    index = 0u;
    while (index < bus->count) {
        bus_region *region = &bus->regions[index];
        if (region->overlay != 0u && region->context == context) {
            size_t tail = bus->count - index - 1u;
            if (tail != 0u) {
                memmove(region, region + 1u, tail * sizeof(*region));
            }
            --bus->count;
            continue;
        }
        ++index;
    }
    refresh_lookup(bus);
}

static semu_status read_region(bus_region *region, uint32_t address,
                               unsigned width, uint32_t *value,
                               semu_error *error)
{
    if (region->kind == REGION_DEVICE) {
        if (region->ops.read == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "device %s is not readable", region->name);
            return SEMU_ERR_UNSUPPORTED;
        }
        return region->ops.read(region->context, address - region->base,
                                width, value, error);
    }
    *value = semu_bus_read_little_endian(
        region->memory + (address - region->base), width);
    semu_bus_clear_success(error);
    return SEMU_OK;
}

static semu_status write_region(bus_region *region, uint32_t address,
                                unsigned width, uint32_t value,
                                semu_error *error)
{
    if (region->kind == REGION_ROM) {
        semu_error_set(error, SEMU_ERR_STATE, "write to ROM %s", region->name);
        return SEMU_ERR_STATE;
    }
    if (region->kind == REGION_DEVICE) {
        if (region->ops.write == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "device %s is not writable", region->name);
            return SEMU_ERR_UNSUPPORTED;
        }
        return region->ops.write(region->context, address - region->base,
                                 width, value, error);
    }
    semu_bus_write_little_endian(
        region->memory + (address - region->base), width, value);
    semu_bus_clear_success(error);
    return SEMU_OK;
}

semu_status semu_bus_read(semu_bus *bus, uint32_t address, unsigned width,
                          uint32_t *value, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || value == NULL || !semu_bus_valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus read");
        return SEMU_ERR_ARGUMENT;
    }
    {
        uint64_t end = (uint64_t)address + width;
        if (end <= UINT64_C(0x100000000) &&
            !semu_bus_overlay_may_cover(bus, address, end)) {
            region = semu_bus_cached_regular(bus, address, end);
            if (region != NULL && region->kind != REGION_DEVICE) {
                return read_region(region, address, width, value, error);
            }
        }
    }
    region = find_region(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped read at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    return read_region(region, address, width, value, error);
}

semu_status semu_bus_read_below(semu_bus *bus, uint32_t address,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    bus_region *region;

    if (bus == NULL || value == NULL || !semu_bus_valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus read");
        return SEMU_ERR_ARGUMENT;
    }
    region = find_region_below(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped read at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    return read_region(region, address, width, value, error);
}

semu_status semu_bus_write(semu_bus *bus, uint32_t address, unsigned width,
                           uint32_t value, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || !semu_bus_valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus write");
        return SEMU_ERR_ARGUMENT;
    }
    {
        uint64_t end = (uint64_t)address + width;
        if (end <= UINT64_C(0x100000000) &&
            !semu_bus_overlay_may_cover(bus, address, end)) {
            region = semu_bus_cached_regular(bus, address, end);
            if (region != NULL) {
                return write_region(region, address, width, value, error);
            }
        }
    }
    region = find_region(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped write at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    return write_region(region, address, width, value, error);
}

semu_status semu_bus_write_below(semu_bus *bus, uint32_t address,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    bus_region *region;

    if (bus == NULL || !semu_bus_valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus write");
        return SEMU_ERR_ARGUMENT;
    }
    region = find_region_below(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped write at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    return write_region(region, address, width, value, error);
}

semu_status semu_bus_validate_write(semu_bus *bus, uint32_t address,
                                    unsigned width, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || !semu_bus_valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid write validation");
        return SEMU_ERR_ARGUMENT;
    }
    region = find_region(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "unmapped write at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    if (region->kind == REGION_ROM) {
        semu_error_set(error, SEMU_ERR_STATE, "write to ROM %s", region->name);
        return SEMU_ERR_STATE;
    }
    if (region->kind == REGION_DEVICE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "device %s has no write preflight", region->name);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_bus_load(semu_bus *bus, uint32_t address, const uint8_t *data,
                          size_t size, semu_error *error)
{
    bus_region *region;
    if (bus == NULL || (size != 0u && data == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus load");
        return SEMU_ERR_ARGUMENT;
    }
    if (size == 0u) {
        semu_error_clear(error);
        return SEMU_OK;
    }
    region = find_region(bus, address, size);
    if (region == NULL || region->kind == REGION_DEVICE) {
        semu_error_set(error, SEMU_ERR_RANGE, "load is not within mapped memory");
        return SEMU_ERR_RANGE;
    }
    (void)memcpy(region->memory + (address - region->base), data, size);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_bus_copy_out(semu_bus *bus, uint32_t address, uint8_t *data,
                              size_t size, semu_error *error)
{
    bus_region *region;
    if (bus == NULL || (size != 0u && data == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus copy");
        return SEMU_ERR_ARGUMENT;
    }
    if (size == 0u) {
        semu_error_clear(error);
        return SEMU_OK;
    }
    region = find_region(bus, address, size);
    if (region == NULL || region->kind == REGION_DEVICE) {
        semu_error_set(error, SEMU_ERR_RANGE, "copy is not within mapped memory");
        return SEMU_ERR_RANGE;
    }
    (void)memcpy(data, region->memory + (address - region->base), size);
    semu_error_clear(error);
    return SEMU_OK;
}
