#ifndef SEMU_MACHINE_INTERNAL_H
#define SEMU_MACHINE_INTERNAL_H

#include "semu/machine.h"

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/scheduler.h"
#include "semu/storage.h"
#include "../compat/sapporo_222.h"
#include "../compat/sapporo_239_files.h"
#include "../devices/sapporo_devices.h"
#include "../devices/sapporo_nema_gpu.h"

struct semu_machine {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu *cpu;
    semu_apollo4 *soc;
    semu_sapporo_devices *devices;
    semu_storage *flash_storage;
    semu_nema_gpu *nema_gpu;
    semu_sapporo_239_files *sapporo_239_files;
    semu_display_backend_submit_fn display_backend_submit;
    void *display_backend_context;
    semu_frame_callback frame_callback;
    void *frame_context;
    const char *external_flash_path;
    semu_machine_input_poll_fn input_poll;
    void *input_poll_context;
    uint64_t instruction_epoch;
    uint64_t virtual_time_epoch;
    /* Diagnostic-only count of guest SYSRESETREQ requests in this runtime. */
    uint64_t reset_request_count;
    semu_logger *logger;
    semu_stop_reason stop_reason;
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_layer_state layers[SEMU_MAX_LAYERS];
    size_t layer_count;
};

semu_status semu_machine_snapshot_manifest_hash(
    const semu_firmware_manifest *firmware,
    char output[SEMU_REPLAY_HASH_HEX_LEN], semu_error *error);
semu_status semu_machine_reset_state_internal(semu_machine *machine,
    uint32_t vector_table, int preserve_ram, semu_error *error);

#endif
