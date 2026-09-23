#include "sapporo_iom4_internal.h"

/* E-SAP-0040: admission is deliberately narrower than the lane's permissive
 * register bank. Validate the complete staged command before FIFO/IRQ/device
 * state changes. Only this profile's IOM4 owns these registers. */
semu_status haptic_validate(const semu_sapporo_iom4 *m, uint32_t command,
                            const uint8_t *source, semu_error *error)
{
    uint32_t size = (command >> 8) & 0xfffu;
    uint32_t address = m->dma_target;
    int write = command == 0x201u || command == 0x501u;
    int valid = 0;
    if (m->devconf != ADDR_HAPTIC) return SEMU_OK;
    if (m->i2c_en == 0 || m->spi_en != 0 || m->active_cmd != 0u ||
        m->out_count != 0u || m->in_count != 0u || m->dma_total != size ||
        m->dma_cfg != (write ? 0x103u : 0x101u) ||
        !((address >= 0x10000000u && address < 0x10160000u) ||
          (address >= 0x20000000u && address < 0x20400000u) ||
          (write && address >= 0x40000u && address < 0x1c0000u))) {
        goto refuse;
    }
    if (command == 0x201u) {
        switch (source[0]) {
        case 0x09u: case 0x0du: case 0x11u: case 0x12u: case 0x13u:
        case 0x1du: case 0x1eu: case 0x22u: valid = 1; break;
        default: break;
        }
    } else if (command == 0x501u) {
        valid = source[0] == 0x40u;
    } else {
        valid = command == 0x22000112u || command == 0x23000112u ||
                command == 0x24000112u || command == 0x40000412u;
    }
    if (valid) return SEMU_OK;
refuse:
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
        "Sapporo 2.35 haptic command/state is unobserved (0x%08x)",
        (unsigned)command);
    return SEMU_ERR_UNSUPPORTED;
}

void haptic_write(semu_sapporo_iom4 *m, const uint8_t *data, uint32_t n)
{
    uint32_t i;
    if (n == 0u) return;
    m->haptic_selected = data[0];
    for (i = 1u; i < n; ++i)
        m->haptic_registers[m->haptic_selected++] = data[i];
    if (data[0] == 0x22u && n > 1u && (data[1] & 1u) != 0u)
        m->haptic_registers[0x22u] |= 2u;
}

void haptic_read(semu_sapporo_iom4 *m, uint8_t *bytes, uint32_t n)
{
    uint32_t i;
    for (i = 0u; i < n; ++i)
        bytes[i] = m->haptic_registers[m->haptic_selected++];
}
