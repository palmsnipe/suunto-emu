#include "semu/machine.h"

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/scheduler.h"
#include "../compat/sapporo_222.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct semu_machine {
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_cpu *cpu;
    semu_apollo4 *soc;
    semu_logger *logger;
    semu_stop_reason stop_reason;
    semu_profile profile;
    semu_firmware_manifest firmware;
    semu_layer_state layers[SEMU_MAX_LAYERS];
    size_t layer_count;
};

static int known_sapporo_profile(const semu_profile *profile)
{
    return strcmp(profile->id, "sapporo-2.22.60") == 0 &&
           strcmp(profile->board, "sapporo") == 0;
}

static semu_status map_sapporo(semu_machine *machine, semu_error *error)
{
    if (semu_bus_map_ram(machine->bus, "sapporo.mram", 0x00000000u,
                         0x00200000u, error) != SEMU_OK ||
        semu_bus_map_ram(machine->bus, "sapporo.sram", 0x10000000u,
                         0x00180000u, error) != SEMU_OK ||
        semu_bus_map_ram(machine->bus, "sapporo.external-flash", 0x14000000u,
                         0x02000000u, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    machine->soc = semu_apollo4_create(machine->bus, error);
    return machine->soc != NULL ? SEMU_OK : error->code;
}

static semu_status load_file(semu_bus *bus, const semu_component *component,
                             semu_error *error)
{
    FILE *stream;
    uint8_t *data;
    size_t size = (size_t)component->size;
    semu_status status;
    if ((uint64_t)size != component->size) {
        semu_error_set(error, SEMU_ERR_RANGE, "component %s is too large",
                       component->id);
        return SEMU_ERR_RANGE;
    }
    stream = fopen(component->path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO, "cannot open component %s at %s",
                       component->id, component->path);
        return SEMU_ERR_IO;
    }
    data = (uint8_t *)malloc(size != 0u ? size : 1u);
    if (data == NULL) {
        fclose(stream);
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate component %s",
                       component->id);
        return SEMU_ERR_NOMEM;
    }
    if (size != 0u && fread(data, 1u, size, stream) != size) {
        free(data);
        fclose(stream);
        semu_error_set(error, SEMU_ERR_IO, "cannot read component %s",
                       component->id);
        return SEMU_ERR_IO;
    }
    if (fgetc(stream) != EOF) {
        free(data);
        fclose(stream);
        semu_error_set(error, SEMU_ERR_RANGE, "component %s changed size",
                       component->id);
        return SEMU_ERR_RANGE;
    }
    fclose(stream);
    status = semu_bus_load(bus, component->load_address, data, size, error);
    free(data);
    return status;
}

static semu_status load_components(semu_machine *machine,
                                   const semu_firmware_manifest *firmware,
                                   semu_error *error)
{
    size_t i;
    for (i = 0u; i < firmware->component_count; ++i) {
        semu_status status = load_file(machine->bus, &firmware->components[i], error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    return SEMU_OK;
}

static semu_status enable_layer(semu_machine *machine, const char *id,
                                semu_error *error)
{
    semu_layer_state *state;
    if (machine->layer_count >= SEMU_MAX_LAYERS) {
        semu_error_set(error, SEMU_ERR_RANGE, "too many compatibility layers");
        return SEMU_ERR_RANGE;
    }
    state = &machine->layers[machine->layer_count];
    if (strcmp(id, semu_sapporo_222_no_device_layer.id) != 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "unknown layer %s", id);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (semu_layer_enable(state, &semu_sapporo_222_no_device_layer,
                          machine->profile.id, error) != SEMU_OK) {
        return error->code;
    }
    machine->layer_count++;
    return SEMU_OK;
}

semu_machine *semu_machine_create(const semu_machine_options *options,
                                  semu_error *error)
{
    semu_machine *machine;
    size_t i;
    if (options == NULL || options->profile == NULL || options->firmware == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "machine options are incomplete");
        return NULL;
    }
    if (!known_sapporo_profile(options->profile)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "board profile is not implemented");
        return NULL;
    }
    if (semu_manifest_validate(options->profile, options->firmware, error) != SEMU_OK) {
        return NULL;
    }
    machine = (semu_machine *)calloc(1u, sizeof(*machine));
    if (machine == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate machine");
        return NULL;
    }
    machine->profile = *options->profile;
    machine->firmware = *options->firmware;
    machine->logger = options->logger;
    machine->bus = semu_bus_create(error);
    machine->scheduler = semu_scheduler_create(error);
    if (machine->bus == NULL || machine->scheduler == NULL ||
        map_sapporo(machine, error) != SEMU_OK) {
        semu_machine_destroy(machine);
        return NULL;
    }
    for (i = 0u; i < options->layer_count; ++i) {
        if (enable_layer(machine, options->layers[i], error) != SEMU_OK) {
            semu_machine_destroy(machine);
            return NULL;
        }
    }
    machine->cpu = semu_cpu_create(machine->bus, machine->scheduler, error);
    if (machine->cpu == NULL || semu_machine_reset(machine, error) != SEMU_OK) {
        semu_machine_destroy(machine);
        return NULL;
    }
    return machine;
}

void semu_machine_destroy(semu_machine *machine)
{
    if (machine != NULL) {
        semu_cpu_destroy(machine->cpu);
        semu_apollo4_destroy(machine->soc);
        semu_scheduler_destroy(machine->scheduler);
        semu_bus_destroy(machine->bus);
        free(machine);
    }
}

semu_status semu_machine_reset(semu_machine *machine, semu_error *error)
{
    if (machine == NULL || machine->cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "cannot reset null machine");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_manifest_validate(&machine->profile, &machine->firmware,
                               error) != SEMU_OK) {
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return error != NULL ? error->code : SEMU_ERR_CONFLICT;
    }
    semu_scheduler_reset(machine->scheduler);
    semu_bus_reset(machine->bus);
    semu_apollo4_reset(machine->soc);
    if (load_components(machine, &machine->firmware, error) != SEMU_OK) {
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return error->code;
    }
    {
        size_t i;
        for (i = 0u; i < machine->layer_count; ++i) {
            machine->layers[i].hits = 0u;
            if (machine->layers[i].descriptor == &semu_sapporo_222_no_device_layer &&
                semu_sapporo_222_install_no_device(machine->bus,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
        }
    }
    semu_cpu_reset(machine->cpu, machine->profile.vector_table, error);
    machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
    return machine->stop_reason == SEMU_STOP_NONE ? SEMU_OK : error->code;
}

semu_stop_reason semu_machine_run(semu_machine *machine,
                                  const semu_run_limits *limits,
                                  semu_error *error)
{
    uint64_t initial_instructions;
    uint64_t initial_time;
    if (machine == NULL || limits == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid run arguments");
        return SEMU_STOP_USER;
    }
    initial_instructions = semu_machine_instructions(machine);
    initial_time = semu_scheduler_now(machine->scheduler);
    machine->stop_reason = SEMU_STOP_NONE;
    while (machine->stop_reason == SEMU_STOP_NONE) {
        const semu_cpu_state *state = semu_cpu_get_state(machine->cpu);
        if (limits->max_instructions != 0u &&
            state->instructions - initial_instructions >= limits->max_instructions) {
            machine->stop_reason = SEMU_STOP_BUDGET;
            break;
        }
        if (limits->max_virtual_time_ns != 0u &&
            semu_scheduler_now(machine->scheduler) - initial_time >=
                limits->max_virtual_time_ns) {
            machine->stop_reason = SEMU_STOP_BUDGET;
            break;
        }
        if (semu_cpu_step(machine->cpu, error) != SEMU_OK) {
            machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
            if (machine->stop_reason == SEMU_STOP_NONE) {
                machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
            }
        } else {
            machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
        }
        semu_log_set_time(machine->logger, semu_scheduler_now(machine->scheduler));
    }
    return machine->stop_reason;
}

semu_status semu_machine_input(semu_machine *machine,
                               const semu_input_event *event,
                               semu_error *error)
{
    static const unsigned pins[] = { 57u, 58u, 59u };
    if (machine == NULL || event == NULL || event->kind != SEMU_INPUT_BUTTON ||
        event->code >= SEMU_ARRAY_LEN(pins)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "unsupported board input");
        return SEMU_ERR_UNSUPPORTED;
    }
    return semu_apollo4_set_gpio_input(machine->soc, pins[event->code],
                                       event->value == 0, error);
}

semu_stop_reason semu_machine_stop_reason(const semu_machine *machine)
{
    return machine != NULL ? machine->stop_reason : SEMU_STOP_USER;
}

uint64_t semu_machine_instructions(const semu_machine *machine)
{
    const semu_cpu_state *state = machine != NULL && machine->cpu != NULL
                                      ? semu_cpu_get_state(machine->cpu) : NULL;
    return state != NULL ? state->instructions : 0u;
}

uint64_t semu_machine_virtual_time(const semu_machine *machine)
{
    return machine != NULL && machine->scheduler != NULL
               ? semu_scheduler_now(machine->scheduler) : 0u;
}

uint32_t semu_machine_program_counter(const semu_machine *machine)
{
    const semu_cpu_state *state = machine != NULL && machine->cpu != NULL
                                      ? semu_cpu_get_state(machine->cpu) : NULL;
    return state != NULL ? state->r[15] : 0u;
}
