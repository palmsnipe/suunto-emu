#ifndef SEMU_MACHINE_H
#define SEMU_MACHINE_H

#include "semu/frame.h"
#include "semu/input.h"
#include "semu/log.h"
#include "semu/manifest.h"

typedef struct semu_machine semu_machine;

typedef struct semu_run_limits {
    uint64_t max_instructions;
    uint64_t max_virtual_time_ns;
} semu_run_limits;

typedef struct semu_machine_options {
    const semu_profile *profile;
    const semu_firmware_manifest *firmware;
    const char *const *layers;
    size_t layer_count;
    semu_logger *logger;
    semu_frame_callback frame_callback;
    void *frame_context;
} semu_machine_options;

semu_machine *semu_machine_create(const semu_machine_options *options,
                                  semu_error *error);
void semu_machine_destroy(semu_machine *machine);
semu_status semu_machine_reset(semu_machine *machine, semu_error *error);
semu_stop_reason semu_machine_run(semu_machine *machine,
                                  const semu_run_limits *limits,
                                  semu_error *error);
semu_status semu_machine_input(semu_machine *machine,
                               const semu_input_event *event,
                               semu_error *error);
semu_stop_reason semu_machine_stop_reason(const semu_machine *machine);
uint64_t semu_machine_instructions(const semu_machine *machine);
uint64_t semu_machine_virtual_time(const semu_machine *machine);
uint32_t semu_machine_program_counter(const semu_machine *machine);

#endif
