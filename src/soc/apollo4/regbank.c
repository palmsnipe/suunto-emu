#include "apollo4_internal.h"

#include <string.h>

static int find_offset(const semu_regbank *bank, uint32_t offset)
{
    size_t i;
    for (i = 0u; i < bank->count; ++i) {
        if (bank->allowed[i] == offset) {
            return (int)i;
        }
    }
    return -1;
}

semu_status semu_regbank_read(void *context, uint32_t offset, unsigned width,
                              uint32_t *value, semu_error *error)
{
    semu_regbank *bank = (semu_regbank *)context;
    int index;
    if (bank == NULL || value == NULL || width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid register read");
        return SEMU_ERR_ARGUMENT;
    }
    index = find_offset(bank, offset);
    if (index < 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "%s refuses read at offset 0x%08x", bank->name, offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = bank->values[index];
    return SEMU_OK;
}

semu_status semu_regbank_write(void *context, uint32_t offset, unsigned width,
                               uint32_t value, semu_error *error)
{
    semu_regbank *bank = (semu_regbank *)context;
    int index;
    if (bank == NULL || width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid register write");
        return SEMU_ERR_ARGUMENT;
    }
    index = find_offset(bank, offset);
    if (index < 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "%s refuses write at offset 0x%08x", bank->name, offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    bank->values[index] = value;
    return SEMU_OK;
}

void semu_regbank_reset(void *context)
{
    semu_regbank *bank = (semu_regbank *)context;
    if (bank != NULL) {
        memset(bank->values, 0, sizeof(bank->values));
    }
}
