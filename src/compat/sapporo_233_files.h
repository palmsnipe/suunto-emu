#ifndef SEMU_SAPPORO_233_FILES_H
#define SEMU_SAPPORO_233_FILES_H

#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"

/* E-SAP-0059/0064/0065/0069 class: the session-only writable logical-file
 * overlay for the 2.33.16 OTA staging. Existing readable files stay on the
 * native storage path; missing files opened for creation are retained in
 * memory using bytes the firmware itself writes. The layer seeds the
 * schema-valid settings/ui.txt (wfn:00 + fuwe:T) that E-SAP-0059 proved
 * clears the SettingsProvider.cpp:228 assert, and implements the native
 * 0xbb204 file-size operation (E-SAP-0069) so the guest's own
 * activity-store length check observes the retained bytes. */
#define SEMU_SAPPORO_233_FILES_OPEN UINT32_C(0x000bb094)
#define SEMU_SAPPORO_233_FILES_EXISTS UINT32_C(0x000bb2b2)
#define SEMU_SAPPORO_233_FILES_CLOSE UINT32_C(0x000bb12c)
#define SEMU_SAPPORO_233_FILES_TELL UINT32_C(0x000bb144)
#define SEMU_SAPPORO_233_FILES_SEEK UINT32_C(0x000bb170)
#define SEMU_SAPPORO_233_FILES_WRITE UINT32_C(0x000bb198)
#define SEMU_SAPPORO_233_FILES_READ UINT32_C(0x000bb1bc)
#define SEMU_SAPPORO_233_FILES_FLUSH UINT32_C(0x000bb1e4)
#define SEMU_SAPPORO_233_FILES_SIZE UINT32_C(0x000bb204)

#define SEMU_SAPPORO_233_FILES_HANDLE_BASE UINT32_C(0x1015f000)
#define SEMU_SAPPORO_233_FILES_HANDLE_STRIDE UINT32_C(0x100)
#define SEMU_SAPPORO_233_FILES_MAX 8u
#define SEMU_SAPPORO_233_FILES_MAX_BYTES 65536u
#define SEMU_SAPPORO_233_FILES_MAX_PATH 96u

typedef struct {
    char path[SEMU_SAPPORO_233_FILES_MAX_PATH];
    uint8_t data[SEMU_SAPPORO_233_FILES_MAX_BYTES];
    uint32_t size;
    uint32_t cursor;
    uint32_t size_hits;
    uint32_t handle;    /* 0 = closed */
    int seeded;
} semu_sapporo_233_file;

typedef struct {
    semu_layer_state *state;
    semu_logger *logger;
    semu_error refusal;
    semu_sapporo_233_file files[SEMU_SAPPORO_233_FILES_MAX];
    uint32_t next_handle;
} semu_sapporo_233_files_context;

extern const semu_layer_descriptor semu_sapporo_233_files_layer;

/* Install the session seed and the layer state. One hit per machine reset;
 * refusal is sticky until the device/machine reset. */
semu_status semu_sapporo_233_files_install(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_error *error);

/* The CPU dispatch (the sapporo_239_file_hook pattern): when the current
 * PC is an overlay operation for an overlay handle, the effect is applied
 * directly to the mutable CPU state (r0/r1 result, PC = LR) and the native
 * call is skipped. Returns SEMU_OK after handling or when the PC is not an
 * overlay operation, and an error status on any unsupported shape. */
semu_status semu_sapporo_233_files_apply(semu_sapporo_233_files_context *c,
    semu_bus *bus, semu_cpu_state *cpu, semu_error *error);

#endif
