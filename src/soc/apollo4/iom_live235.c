#include "iom_internal.h"

#include "../../devices/sapporo_iom4.h"

#include <stddef.h>

/*
 * E-SAP-0036: IOM 4 lives twice in this emulator.  Under the profile gate
 * in src/soc/apollo4/apollo4.c -- sapporo-2.35.34, and no other profile --
 * the window at 0x40054000 is served by the lane mirror in
 * src/devices/sapporo_iom4.c, which observes every readout of the 2.35
 * SapporoApollo4IOMaster lane verbatim, as reproduced by the byte-identical
 * probe pairs /tmp/sap235/iom4law{,,2,3,4,5,6}.resc (sha256 5a2f3974..,
 * ea5feadd..).  Without the gate -- every other profile and every cold
 * boot -- IOM 4 stays on the E-A4-IOM-001 shared law of the companion
 * iom.c, and all five hooks there keep their byte for byte behaviour: the
 * mirror owns only the transactions the guest addresses at IOM4 (command
 * FIFO, interrupts, and DMA - all as the lane's IOMaster rules prescribe),
 * and nothing the shared IOM law does not.  This translation unit holds
 * the seam itself so that iom.c stays within the hard 500-line limit;
 * iom.c reaches it from reset_state, which forwards a reset through here,
 * and from attach_endpoint, whose mirror case is decided here, while its
 * read and write are dispatched directly to the mirror's register access
 * handlers when the mirror owns the page.
 */

void semu_apollo4_iom_set_live235(semu_apollo4_iom *iom,
                                 struct semu_sapporo_iom4 *live)
{
    if (iom != NULL) {
        iom->live235 = live;
    }
}

int semu_apollo4_iom_live_owns(const semu_apollo4_iom *iom)
{
    return iom != NULL && iom->live235 != NULL;
}

void semu_apollo4_iom_live_reset(semu_apollo4_iom *iom)
{
    if (!semu_apollo4_iom_live_owns(iom)) {
        return;
    }
    /*
     * The mirror owns the 2.35 window, its own IRQ fall, and its endpoint
     * states; the shared fields are still zeroed by the caller afterwards,
     * which keeps the shared snapshot layout untouched.
     */
    semu_sapporo_iom4_reset(iom->live235);
}
