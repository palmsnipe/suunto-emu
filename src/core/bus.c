#include "semu/bus.h"

#include <stdlib.h>
#include <string.h>

typedef enum region_kind {
    REGION_RAM = 0,
    REGION_ROM,
    REGION_DEVICE
} region_kind;

typedef struct bus_region {
    char name[SEMU_ID_MAX];
    uint32_t base;
    uint32_t size;
    region_kind kind;
    uint8_t *memory;
    semu_bus_device_ops ops;
    void *context;
} bus_region;

struct semu_bus {
    bus_region *regions;
    size_t count;
    size_t capacity;
};

static uint64_t region_end(const bus_region *region)
{
    return (uint64_t)region->base + region->size;
}

static int valid_interval(uint32_t base, uint32_t size)
{
    return size != 0u && (uint64_t)base + size <= UINT64_C(0x100000000);
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
            (uint64_t)other->base < region_end(region)) {
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
    semu_error_clear(error);
    return SEMU_OK;
}

static bus_region *find_region(semu_bus *bus, uint32_t address, size_t size)
{
    size_t index;
    uint64_t end = (uint64_t)address + size;

    if (end > UINT64_C(0x100000000)) {
        return NULL;
    }
    for (index = 0u; index < bus->count; ++index) {
        bus_region *region = &bus->regions[index];
        if (address >= region->base && end <= region_end(region)) {
            return region;
        }
        if (region->base > address) {
            break;
        }
    }
    return NULL;
}

static int valid_width(unsigned width)
{
    return width == 1u || width == 2u || width == 4u;
}

static uint32_t read_little_endian(const uint8_t *data, unsigned width)
{
    uint32_t value = 0u;
    unsigned index;
    for (index = 0u; index < width; ++index) {
        value |= (uint32_t)data[index] << (index * 8u);
    }
    return value;
}

static void write_little_endian(uint8_t *data, unsigned width, uint32_t value)
{
    unsigned index;
    for (index = 0u; index < width; ++index) {
        data[index] = (uint8_t)(value >> (index * 8u));
    }
}

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
                              region_kind kind, semu_error *error)
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

semu_status semu_bus_map_device(semu_bus *bus, const char *name, uint32_t base,
                                uint32_t size, const semu_bus_device_ops *ops,
                                void *context, semu_error *error)
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
    return insert_region(bus, &region, error);
}

semu_status semu_bus_read(semu_bus *bus, uint32_t address, unsigned width,
                          uint32_t *value, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || value == NULL || !valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus read");
        return SEMU_ERR_ARGUMENT;
    }
    region = find_region(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped read at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
    if (region->kind == REGION_DEVICE) {
        if (region->ops.read == NULL) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "device %s is not readable", region->name);
            return SEMU_ERR_UNSUPPORTED;
        }
        return region->ops.read(region->context, address - region->base,
                                width, value, error);
    }
    *value = read_little_endian(region->memory + (address - region->base), width);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_bus_write(semu_bus *bus, uint32_t address, unsigned width,
                           uint32_t value, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || !valid_width(width)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid bus write");
        return SEMU_ERR_ARGUMENT;
    }
    region = find_region(bus, address, width);
    if (region == NULL) {
        semu_error_set(error, SEMU_ERR_RANGE, "unmapped write at 0x%08x", address);
        return SEMU_ERR_RANGE;
    }
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
    write_little_endian(region->memory + (address - region->base), width, value);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_bus_validate_write(semu_bus *bus, uint32_t address,
                                    unsigned width, semu_error *error)
{
    bus_region *region;

    if (bus == NULL || !valid_width(width)) {
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
