/*
 * Ulsan 2.35.36 reset memory map (ticket 725).
 *
 * Region evidence, all rows of E-ULS-0006 in docs/migration-evidence.md:
 *   mcu_mram, mcu_tcm, shared_sram  reference platform base mapping
 *       (upstream ambiq-apollo4 platform description; Ulsan images load and
 *       execute inside mcu_mram, and the boot SP lies inside mcu_tcm).
 *   extended_sram                   Apollo4 Plus memory class referenced by
 *       the Ulsan binaries (ulsan-platform.repl).
 *   working_memory                  aperture enclosing the scatter-table
 *       zero range [0x101c0000, 0x10266dc8) observed at 0x001d64e8.
 *   xip_tail                        recovered external-flash tail loaded at
 *       MSPI1 XIP base 0x18000000 plus the logical offset 0x00040000 and
 *       extending to the end of the 32 MiB aperture (ulsan-storage.repl).
 *
 * Attached controllers (ticket 730):
 *   0x40010000 GPIO bank     the boot image's pad-setup sequence stores the
 *       PADKEY value 0x73 and PINCFG words through this block while the
 *       application table runs (first precise bus fault observed at
 *       0x40010200 from PC 0x000c22b2, CFSR 0x00008200). The reference lane
 *       resolves the same stores through AmbiqApollo4_GPIO at 0x40010000
 *       (bank IRQs 56..63, upstream ambiq-apollo4 platform description) and
 *       never refuses them; the same in-tree Apollo4 GPIO controller serves
 *       that block for Sapporo. No GPIO interrupt sink is attached: the
 *       Ulsan boot evidence shows pad configuration only, no bank IRQ.
 *   0x40021000 power control  the boot phase reads DEVICE_POWER_STATUS
 *       (bit 20 sampled at 0x00096b66 by the reader called from 0x0009d20a)
 *       and performs enable writes the reference lane logged as fully
 *       unhandled bits; the evidence-bounded device
 *       src/devices/ulsan_pwrctrl.c accepts exactly those observed
 *       transactions and refuses everything else (E-ULS-0008).
 *
 * Deliberately unmapped, so accesses fail closed with a bounded diagnostic:
 *   0x18000000..0x1803ffff  device-specific first 256 KiB, permanently
 *       absent without hardware and never synthesized (E-ULS-0006).
 *   0x1a000000 and above    outside the recorded XIP aperture.
 *   0x07fffffc, 0x08000000  bootrom/logger; the Ulsan reset starts from the
 *       application vector table, not the bootrom.
 *   every other 0x400xxxxx block  CLKGEN, IOM, MSPI1 registers, display
 *       controller and NVIC-adjacent SoC blocks are observed-only in the
 *       reference lane; no register behavior is claimed here yet.
 *
 * No semantic input wiring is registered: the Ulsan reset evidence names no
 * button, crown, or touch pin, so semu_machine_input still refuses every
 * semantic input.
 */

#include "ulsan_board.h"

#include "../devices/ulsan_pwrctrl.h"
#include "../soc/apollo4/gpio.h"
#include "semu/types.h"

#include <string.h>

typedef struct {
    const char *name;
    uint32_t base;
    uint32_t size;
} ulsan_region;

static const ulsan_region region_table[] = {
    { "ulsan.mram", 0x00000000u, 0x00200000u },
    { "ulsan.tcm", 0x10000000u, 0x00060000u },
    { "ulsan.shared_sram", 0x10060000u, 0x00100000u },
    { "ulsan.extended_sram", 0x10160000u, 0x00060000u },
    { "ulsan.working_memory", 0x101c0000u, 0x000a7000u },
    { "ulsan.xip_tail", 0x18040000u, 0x01fc0000u }
};

int semu_ulsan_board_accepted(const char *board, const char *profile_id)
{
    return board != NULL && profile_id != NULL &&
           strcmp(board, "ulsan") == 0 &&
           strcmp(profile_id, "ulsan-2.35.36") == 0;
}

semu_status semu_ulsan_board_map(semu_bus *bus, semu_error *error)
{
    size_t index;

    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "ulsan board needs a bus");
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
    if (semu_apollo4_gpio_create(bus, NULL, NULL, error) == NULL) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    return semu_ulsan_pwrctrl_map(bus, error);
}
