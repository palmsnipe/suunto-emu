#ifndef SEMU_SAPPORO_239_COMPAT_H
#define SEMU_SAPPORO_239_COMPAT_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"

extern const semu_layer_descriptor semu_sapporo_239_wbsto_layer;

enum {
    SEMU_SAPPORO_239_IV_WBSTO_SESSION_CACHE = 0u,
    SEMU_SAPPORO_239_IV_WBSTO_PRELOAD_RESULT,
    SEMU_SAPPORO_239_IV_LOGICAL_FILE,
    SEMU_SAPPORO_239_IV_WBSTO_PRELOAD1_RESULT,
    SEMU_SAPPORO_239_IV_COUNT
};

static inline int semu_sapporo_239_compat_hook_pc(uint32_t pc)
{
    return pc == UINT32_C(0x00124844);
}

semu_status semu_sapporo_239_apply_wbsto_hook(
    semu_bus *bus, semu_cpu_state *cpu_state, semu_layer_state *state,
    semu_logger *logger, semu_error *error);

#endif
