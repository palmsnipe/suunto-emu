/*
 * Wismar (Suunto Race 2) 2.46.14 reset memory map (ticket 755).
 *
 * Region evidence, all rows of E-WIS-0001 in docs/migration-evidence.md
 * (twice-identical bounded lane traces; the lane platform
 * $FIRMWARE_ROOT/emulator/devices/wismar/wismar-platform.repl, SHA-256
 * ced65de0..., is the observed oracle; no inheritance from Ulsan or
 * Sapporo is claimed):
 *   wismar.code_window    0x00400000 + 0x00300000  resident/application
 *       window; the lane loads component 03 at 0x00414000 and component
 *       04 at 0x00450000 inside it.
 *   wismar.stack_alias    0x20000000 + 0x00400000  SRAM alias; the lane
 *       records guest writes through 0x202d172c and observed stack/buffer
 *       values 0x20003fb0/0x20003fc8/0x20003fe0/0x20035f30 and MAX20360
 *       buffers at 0x2001c210/0x20035dxx. 4 MiB covers the observed window
 *       without claiming a full device map (the alias bound is not
 *       derivable from the package).
 *   wismar.extended_sram  0x10160000 + 0x00060000  Apollo4 Plus memory
 *       class.
 *   wismar.working_memory 0x101c0000 + 0x000a7000  aperture enclosing the
 *       scatter-table zero range.
 *   wismar.external_flash 0x18000000 + 0x02000000  external flash; the
 *       resources component (component 05, expanded 33,292,288 B) loads at
 *       the XIP alias 0x18040000 with the first 0x40000 bytes permanently
 *       absent (external-flash tail, not a factory dump).
 *
 * Start tuple (E-WIS-0001, the lane's audited choice from component 04's
 * vector words, not an independent start-component proof):
 *   vector table 0x00450000, initial SP 0x20004000, Thumb PC 0x006954cd.
 *
 * NVIC contract (E-WIS-0001, directly observed by a write/readback probe):
 * the implemented priority width is four bits (mask 0xf0); writing
 * 0xffffffff to 0xE000E400 reads back 0xF0F0F0F0 and writing 0x12345678
 * to 0xE000E404 reads back 0x10305070. The CPU selector is bound at
 * profile selection and restored to the 0xe0 reset default by every reset.
 *
 * First Wismar-specific transaction (E-WIS-0001): TIMER14 control at
 * 0x40008000+0x3c0 is written 0x00000a10 (function-select bits 4..7 =
 * Edge=1). The shared Apollo4 timer law refuses that selector, and no
 * Wismar timer semantics are authorized yet: the device stays the shared
 * Apollo4 CTIMER block, which fails closed on the edge selector exactly as
 * the ticket requires ("No Timer14 semantics"). The lane's own wrapper
 * (WismarApollo4Timer.cs, SHA-256 0aa513f4...) is the lane workaround, not
 * a firmware fact, and is deliberately not reproduced here.
 *
 * Deliberately unmapped, so accesses fail closed with a bounded diagnostic:
 *   0x18000000..0x1803ffff  the absent first 256 KiB of the external-flash
 *       tail, never synthesized (E-WIS-0001).
 *   0x1a000000 and above    outside the recorded XIP aperture.
 *   every other 0x400xxxxx block  any SoC block not mapped by
 *       semu_apollo4_init is observed-only or absent in the retained
 *       traces; the recorded lane model gaps (GPIO offsets 0x400/0x340,
 *       non-existing peripheral writes at 0x40010470/0x40010464/0x4001045c,
 *       CLKGEN reads at 0x4000400c/0x40004044) are lane absences that
 *       logged warnings without stopping execution and authorize no
 *       in-tree semantics.
 *
 * No semantic input wiring is registered: E-WIS-0001 names no Wismar
 * button, crown, or touch pin, so semu_machine_input refuses every
 * semantic input.
 */

#include "wismar_board.h"

#include "semu/types.h"

#include <string.h>

typedef struct {
    const char *name;
    uint32_t base;
    uint32_t size;
} wismar_region;

static const wismar_region region_table[] = {
    /* The lane platform keeps the upstream Apollo4 MRAM window
     * (mcu_mram @ 0x0, 2 MiB); the Wismar guest's startup dereferences
     * through it (first observed fault address 0x0 at vt ~58.6 ms, past
     * the lane's 0.1 s boundary stop, inside the 1 s BKPT probe window
     * where the lane ran fault-free). */
    { "wismar.mram", 0x00000000u, 0x00200000u },
    { "wismar.code_window", 0x00400000u, 0x00300000u },
    { "wismar.stack_alias", 0x20000000u, 0x00400000u },
    { "wismar.extended_sram", 0x10160000u, 0x00060000u },
    { "wismar.working_memory", 0x101c0000u, 0x000a7000u },
    { "wismar.external_flash", 0x18000000u, 0x02000000u }
};

int semu_wismar_board_accepted(const char *board, const char *profile_id)
{
    return board != NULL && profile_id != NULL &&
           strcmp(board, "wismar") == 0 &&
           strcmp(profile_id, "wismar-2.46.14") == 0;
}

semu_status semu_wismar_board_map(semu_bus *bus, semu_error *error)
{
    size_t index;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "wismar board needs a bus");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < sizeof(region_table) / sizeof(region_table[0]);
         ++index) {
        semu_status status = semu_bus_map_ram(
            bus, region_table[index].name, region_table[index].base,
            region_table[index].size, error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    return SEMU_OK;
}
