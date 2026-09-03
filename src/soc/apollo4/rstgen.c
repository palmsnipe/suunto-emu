#include "rstgen.h"

#include <stdlib.h>

enum {
    RSTGEN_CFG = 0u
};

#define RSTGEN_CFG_MASK UINT32_C(0x3)

struct semu_apollo4_rstgen {
    uint32_t cfg;
};

static int cfg_valid(uint32_t value)
{
    return (value & ~RSTGEN_CFG_MASK) == 0u;
}

static semu_status validate_access(void *context, uint32_t offset,
                                   unsigned width, semu_error *error)
{
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset generator context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset != RSTGEN_CFG) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 reset generator supports only 32-bit CFG access");
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

semu_status semu_apollo4_rstgen_read(void *context, uint32_t offset,
                                     unsigned width, uint32_t *value,
                                     semu_error *error)
{
    semu_apollo4_rstgen *rstgen = (semu_apollo4_rstgen *)context;
    semu_status status = validate_access(context, offset, width, error);
    if (status != SEMU_OK)
        return status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset generator read value required");
        return SEMU_ERR_ARGUMENT;
    }
    *value = rstgen->cfg;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_rstgen_write(void *context, uint32_t offset,
                                      unsigned width, uint32_t value,
                                      semu_error *error)
{
    semu_apollo4_rstgen *rstgen = (semu_apollo4_rstgen *)context;
    semu_status status = validate_access(context, offset, width, error);
    if (status != SEMU_OK)
        return status;
    if (!cfg_valid(value)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 reset generator CFG value is unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    rstgen->cfg = value;
    semu_error_clear(error);
    return SEMU_OK;
}

void semu_apollo4_rstgen_reset(void *context)
{
    semu_apollo4_rstgen *rstgen = (semu_apollo4_rstgen *)context;
    if (rstgen != NULL)
        rstgen->cfg = 0u;
}

static const semu_bus_device_ops rstgen_ops = {
    semu_apollo4_rstgen_read,
    semu_apollo4_rstgen_write,
    semu_apollo4_rstgen_reset
};

semu_apollo4_rstgen *semu_apollo4_rstgen_create(
    semu_bus *bus, semu_error *error)
{
    semu_apollo4_rstgen *rstgen;
    semu_status status;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset generator requires a bus");
        return NULL;
    }
    rstgen = (semu_apollo4_rstgen *)calloc(1u, sizeof(*rstgen));
    if (rstgen == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 reset generator");
        return NULL;
    }
    status = semu_bus_map_device(bus, "apollo4.rstgen",
                                 SEMU_APOLLO4_RSTGEN_BASE,
                                 SEMU_APOLLO4_RSTGEN_SIZE, &rstgen_ops,
                                 rstgen, error);
    if (status != SEMU_OK) {
        free(rstgen);
        return NULL;
    }
    semu_error_clear(error);
    return rstgen;
}

void semu_apollo4_rstgen_destroy(semu_apollo4_rstgen *rstgen)
{
    free(rstgen);
}

semu_status semu_apollo4_rstgen_snapshot_write(
    const semu_apollo4_rstgen *rstgen, semu_snapshot_writer *writer,
    semu_error *error)
{
    if (rstgen == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset generator snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    return semu_snapshot_writer_u32(writer, rstgen->cfg, error);
}

semu_status semu_apollo4_rstgen_snapshot_read(
    semu_apollo4_rstgen *rstgen, semu_snapshot_reader *reader,
    semu_error *error)
{
    uint32_t cfg;
    semu_status status;
    if (rstgen == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 reset generator snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_snapshot_reader_u32(reader, &cfg, error);
    if (status != SEMU_OK)
        return status;
    if (!cfg_valid(cfg)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "Apollo4 reset generator snapshot CFG is unreachable");
        return SEMU_ERR_FORMAT;
    }
    rstgen->cfg = cfg;
    semu_error_clear(error);
    return SEMU_OK;
}
