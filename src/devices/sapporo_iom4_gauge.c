#include "sapporo_iom4_gauge.h"

#include <string.h>

void semu_sapporo_iom4_gauge_reset(semu_sapporo_iom4_gauge *g)
{
    memset(g->registers, 0, sizeof(g->registers));
    g->registers[0x06u] = 0x3200u;
    g->registers[0x08u] = 0x1900u;
    g->registers[0x09u] = 0xC000u;
    g->registers[0x19u] = 0xC000u;
    g->reg = 0u;
    g->expecting_register = 1;
    g->read_index = 0u;
    g->write_index = 0u;
}

void semu_sapporo_iom4_gauge_write(semu_sapporo_iom4_gauge *g,
                                   const uint8_t *data, size_t n)
{
    size_t i = 0u;
    if (n == 0u) {
        return;
    }
    if (g->expecting_register != 0) {
        g->reg = data[0];
        g->expecting_register = 0;
        i = 1u;
    }
    for (; i < n; ++i) {
        unsigned index = g->write_index++;
        uint16_t current = g->registers[g->reg];
        if ((index & 1u) == 0u) {
            g->registers[g->reg] = (uint16_t)((current & 0xff00u) | data[i]);
        } else {
            g->registers[g->reg] = (uint16_t)((current & 0x00ffu) |
                                              ((uint16_t)data[i] << 8));
            g->reg = (uint8_t)(g->reg + 1u);
        }
    }
}

void semu_sapporo_iom4_gauge_read(semu_sapporo_iom4_gauge *g, uint8_t *out,
                                  size_t n)
{
    size_t i;
    for (i = 0u; i < n; ++i) {
        uint16_t value = g->registers[g->reg];
        out[i] = (uint8_t)(((g->read_index++ & 1u) == 0u)
                               ? (value & 0xffu) : (unsigned)(value >> 8));
        if ((g->read_index & 1u) == 0u) {
            g->reg = (uint8_t)(g->reg + 1u);
        }
    }
}

void semu_sapporo_iom4_gauge_finish(semu_sapporo_iom4_gauge *g)
{
    g->expecting_register = 1;
    g->read_index = 0u;
    g->write_index = 0u;
}
