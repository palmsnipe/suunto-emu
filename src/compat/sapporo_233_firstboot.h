#ifndef SEMU_SAPPORO_233_FIRSTBOOT_H
#define SEMU_SAPPORO_233_FIRSTBOOT_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"

/* E-SAP-0066/0068 class: the 2.33.16 first-boot provider-wave result
 * translations. The shared startup-client result checker at PC 0x119734
 * receives the client object in r0 and the native status in r3; the
 * client's +12 field is the current module's name pointer (one shared
 * SRAM object rotates it per module, so translations are name-based) and
 * +16 is the command byte. A 500 status for an admitted (name, command)
 * pair is translated to 200 by writing r3. Every admitted pair is
 * hit-bounded per E-SAP-0068's observed counts. */
#define SEMU_SAPPORO_233_FIRSTBOOT_PC UINT32_C(0x00119734)

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
} semu_sapporo_233_firstboot_context;

extern const semu_layer_descriptor semu_sapporo_233_firstboot_layer;

/* Applies the translation at the checker PC. Returns SEMU_OK after
 * handling (the status is written into r3 and the native path continues),
 * SEMU_OK when the PC/status does not match the layer, and an error status
 * on any unsupported shape (refusal is sticky). */
semu_status semu_sapporo_233_firstboot_apply(
    semu_sapporo_233_firstboot_context *c, semu_bus *bus,
    semu_cpu_state *cpu, semu_error *error);

#endif
