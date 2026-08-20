#include "bus_access.h"

semu_status semu_bus_read_u16(semu_bus *bus, uint32_t address,
                              uint32_t *value, semu_error *error)
{
    if (bus != NULL && value != NULL) {
        uint64_t end = (uint64_t)address + 2u;
        if (end <= UINT64_C(0x100000000) &&
            !semu_bus_overlay_may_cover(bus, address, end)) {
            bus_region *region = semu_bus_cached_regular(bus, address, end);
            if (region != NULL && region->kind != REGION_DEVICE) {
                *value = semu_bus_read_little_endian(
                    region->memory + (address - region->base), 2u);
                semu_bus_clear_success(error);
                return SEMU_OK;
            }
        }
    }
    return semu_bus_read(bus, address, 2u, value, error);
}
