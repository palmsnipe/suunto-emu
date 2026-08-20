#include "bus_internal.h"

#include "snapshot_io.h"

#include <stdlib.h>
#include <string.h>

static int snapshot_region(const bus_region *region)
{
    /* The XIP image is immutable firmware-backed state. Its session overlay
     * is serialized separately by semu_storage. */
    return region->kind == REGION_RAM && region->overlay == 0u &&
           region->base != UINT32_C(0x14000000);
}

typedef struct snapshot_region_image {
    bus_region *target;
    uint8_t *data;
} snapshot_region_image;

static void free_region_images(snapshot_region_image *images, size_t count)
{
    size_t index;

    if (images == NULL) return;
    for (index = 0u; index < count; ++index) {
        free(images[index].data);
    }
    free(images);
}

semu_status semu_bus_snapshot_write(const semu_bus *bus,
                                    semu_snapshot_writer *writer,
                                    semu_error *error)
{
    size_t index;
    uint32_t count = 0u;

    if (bus == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "bus snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < bus->count; ++index) {
        if (snapshot_region(&bus->regions[index])) ++count;
    }
    if (semu_snapshot_writer_u32(writer, count, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < bus->count; ++index) {
        const bus_region *region = &bus->regions[index];
        if (!snapshot_region(region)) continue;
        if (semu_snapshot_writer_u32(writer, region->base, error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, region->size, error) != SEMU_OK ||
            semu_snapshot_writer_bytes(writer, region->memory, region->size,
                                       error) != SEMU_OK) {
            return error->code;
        }
    }
    return SEMU_OK;
}

semu_status semu_bus_snapshot_read(semu_bus *bus,
                                   semu_snapshot_reader *reader,
                                   semu_error *error)
{
    uint32_t count;
    size_t expected_count = 0u;
    size_t index;
    snapshot_region_image *images;
    if (bus == NULL || reader == NULL ||
        semu_snapshot_reader_u32(reader, &count, error) != SEMU_OK) {
        if (error != NULL && error->code == SEMU_OK)
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "bus snapshot arguments are invalid");
        return error != NULL ? error->code : SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < bus->count; ++index) {
        if (snapshot_region(&bus->regions[index])) ++expected_count;
    }
    if ((size_t)count != expected_count) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "bus snapshot region count does not match machine");
        return SEMU_ERR_CONFLICT;
    }
    if (expected_count > SIZE_MAX / sizeof(*images)) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "bus snapshot region count is too large");
        return SEMU_ERR_RANGE;
    }
    images = expected_count == 0u ? NULL :
        (snapshot_region_image *)calloc(expected_count, sizeof(*images));
    if (expected_count != 0u && images == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate bus snapshot regions");
        return SEMU_ERR_NOMEM;
    }
    for (index = 0u; index < expected_count; ++index) {
        uint32_t base;
        uint32_t size;
        size_t region_index;
        bus_region *region = NULL;
        if (semu_snapshot_reader_u32(reader, &base, error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &size, error) != SEMU_OK)
            return error->code;
        for (region_index = 0u; region_index < bus->count; ++region_index) {
            bus_region *candidate = &bus->regions[region_index];
            if (snapshot_region(candidate) && candidate->base == base) {
                region = candidate;
                break;
            }
        }
        if (region == NULL || region->size != size) {
            semu_error_set(error, SEMU_ERR_CONFLICT,
                           "bus snapshot region does not match machine");
            free_region_images(images, expected_count);
            return SEMU_ERR_CONFLICT;
        }
        for (region_index = 0u; region_index < index; ++region_index) {
            if (images[region_index].target == region) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "duplicate bus snapshot region");
                free_region_images(images, expected_count);
                return SEMU_ERR_FORMAT;
            }
        }
        images[index].target = region;
        images[index].data = (uint8_t *)malloc(size);
        if (images[index].data == NULL) {
            semu_error_set(error, SEMU_ERR_NOMEM,
                           "cannot allocate bus snapshot region data");
            free_region_images(images, expected_count);
            return SEMU_ERR_NOMEM;
        }
        if (semu_snapshot_reader_bytes(reader, images[index].data, size,
                                       error) != SEMU_OK)
        {
            free_region_images(images, expected_count);
            return error->code;
        }
    }
    for (index = 0u; index < expected_count; ++index) {
        memcpy(images[index].target->memory, images[index].data,
               images[index].target->size);
    }
    free_region_images(images, expected_count);
    return SEMU_OK;
}
