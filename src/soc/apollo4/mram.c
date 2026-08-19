#include "mram.h"

#include <stdlib.h>

enum {
    MRAM_CONTROL = 0x04u,
    MRAM_KEY = 0x08u,
    MRAM_COMMAND = 0x10u,
    MRAM_ADDRESS = 0x14u,
    MRAM_DATA = 0x18u,
    MRAM_LENGTH = 0x1cu,
    MRAM_STATUS = 0x50u,
    MRAM_CONFIGURATION = 0x54u,
    MRAM_READY = 0x70u
};

typedef struct mram_register {
    uint32_t offset;
    uint32_t write_mask;
    uint32_t value;
    uint8_t writable;
} mram_register;

struct semu_apollo4_mram {
    semu_bus *bus;
    mram_register registers[9];
};

static const semu_bus_device_ops mram_ops = {
    semu_apollo4_mram_read,
    semu_apollo4_mram_write,
    semu_apollo4_mram_reset
};

static int register_index(const semu_apollo4_mram *mram, uint32_t offset)
{
    size_t index;
    for (index = 0u; index < sizeof(mram->registers) /
                         sizeof(mram->registers[0]); ++index) {
        if (mram->registers[index].offset == offset) {
            return (int)index;
        }
    }
    return -1;
}

static semu_status validate_access(semu_apollo4_mram *mram,
                                   uint32_t offset, unsigned width,
                                   int *index, semu_error *error)
{
    if (mram == NULL || index == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MRAM context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MRAM supports aligned 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    *index = register_index(mram, offset);
    if (*index < 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MRAM offset 0x%08x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_apollo4_mram *semu_apollo4_mram_create(semu_bus *bus,
                                             semu_error *error)
{
    static const uint32_t offsets[] = {
        MRAM_CONTROL, MRAM_KEY, MRAM_COMMAND, MRAM_ADDRESS, MRAM_DATA,
        MRAM_LENGTH, MRAM_STATUS, MRAM_CONFIGURATION, MRAM_READY
    };
    static const uint32_t masks[] = {
        UINT32_C(0x00000008), UINT32_C(0x000000c3), UINT32_C(0x001058a1),
        UINT32_C(0x00000000), UINT32_C(0x00000000), UINT32_C(0x00000000),
        UINT32_C(0x00000010), UINT32_C(0x00002407), UINT32_C(0x00000000)
    };
    semu_apollo4_mram *mram;
    size_t index;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MRAM requires a bus");
        return NULL;
    }
    mram = (semu_apollo4_mram *)calloc(1u, sizeof(*mram));
    if (mram == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 MRAM controller");
        return NULL;
    }
    mram->bus = bus;
    for (index = 0u; index < sizeof(offsets) / sizeof(offsets[0]); ++index) {
        mram->registers[index].offset = offsets[index];
        mram->registers[index].write_mask = masks[index];
        mram->registers[index].writable = index != 8u;
    }
    if (semu_bus_map_device(bus, "apollo4.mram", SEMU_APOLLO4_MRAM_BASE,
                            SEMU_APOLLO4_MRAM_SIZE, &mram_ops, mram,
                            error) != SEMU_OK) {
        free(mram);
        return NULL;
    }
    semu_error_clear(error);
    return mram;
}

void semu_apollo4_mram_destroy(semu_apollo4_mram *mram)
{
    free(mram);
}

void semu_apollo4_mram_reset(void *context)
{
    semu_apollo4_mram *mram = (semu_apollo4_mram *)context;
    size_t index;
    if (mram == NULL) {
        return;
    }
    for (index = 0u; index < sizeof(mram->registers) /
                         sizeof(mram->registers[0]); ++index) {
        mram->registers[index].value = 0u;
    }
}

semu_status semu_apollo4_mram_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    semu_apollo4_mram *mram = (semu_apollo4_mram *)context;
    int index;
    semu_status status = validate_access(mram, offset, width, &index, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MRAM read value required");
        return SEMU_ERR_ARGUMENT;
    }
    *value = mram->registers[index].value;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_mram_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    semu_apollo4_mram *mram = (semu_apollo4_mram *)context;
    int index;
    semu_status status = validate_access(mram, offset, width, &index, error);

    if (status != SEMU_OK) {
        return status;
    }
    if (mram->registers[index].writable == 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MRAM offset 0x%08x is read-only", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    if ((value & ~mram->registers[index].write_mask) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MRAM value 0x%08x is unsupported at 0x%08x",
                       value, offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    mram->registers[index].value = value;
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_mram_bus_ops(void)
{
    return &mram_ops;
}

semu_status semu_apollo4_mram_snapshot_write(
    const semu_apollo4_mram *mram, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    if (mram == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "MRAM snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < SEMU_ARRAY_LEN(mram->registers); ++index) {
        if (semu_snapshot_writer_u32(writer, mram->registers[index].value,
                                     error) != SEMU_OK)
            return error->code;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_mram_snapshot_read(
    semu_apollo4_mram *mram, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_mram candidate;
    size_t index;
    if (mram == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "MRAM snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *mram;
    for (index = 0u; index < SEMU_ARRAY_LEN(candidate.registers); ++index) {
        if (semu_snapshot_reader_u32(reader,
                                     &candidate.registers[index].value,
                                     error) != SEMU_OK)
            return error->code;
    }
    *mram = candidate;
    return SEMU_OK;
}
