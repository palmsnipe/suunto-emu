#include "machine_internal.h"

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/scheduler.h"
#include "semu/storage.h"
#include "../compat/sapporo_222.h"
#include "../compat/sapporo_239.h"
#include "../devices/sapporo_devices.h"
#include "../devices/sapporo_flash.h"
#include "../devices/sapporo_info1.h"
#include "../devices/sapporo_nema_gpu.h"

#include <stdlib.h>
#include <string.h>

static void irq_sink(void *context, unsigned irq, int level)
{
    semu_machine *machine = (semu_machine *)context;
    if (machine != NULL && machine->cpu != NULL) {
        semu_cpu_set_irq(machine->cpu, irq, level);
    }
}

static int known_sapporo_profile(const semu_profile *profile)
{
    return strcmp(profile->board, "sapporo") == 0 &&
           (strcmp(profile->id, "sapporo-2.22.60") == 0 ||
            strcmp(profile->id, "sapporo-2.33.16") == 0 ||
            strcmp(profile->id, "sapporo-2.39.20") == 0);
}

static semu_status map_sapporo(semu_machine *machine, semu_error *error)
{
    const semu_component *resources = NULL;
    size_t component_index;

    for (component_index = 0u;
         component_index < machine->firmware.component_count;
         ++component_index) {
        if (strcmp(machine->firmware.components[component_index].role,
                   "resources") == 0) {
            resources = &machine->firmware.components[component_index];
            break;
        }
    }
    if (semu_bus_map_ram(machine->bus, "sapporo.mram", 0x00000000u,
                         0x00200000u, error) != SEMU_OK ||
        semu_bus_map_ram(machine->bus, "sapporo.sram", 0x10000000u,
                         0x00180000u, error) != SEMU_OK ||
        semu_bus_map_ram(machine->bus, "sapporo.external-flash", 0x14000000u,
                         0x02000000u, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_sapporo_info1_map(machine->bus, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (machine->external_flash_path != NULL || resources != NULL) {
        machine->flash_storage = semu_storage_open(
            machine->external_flash_path != NULL ? machine->external_flash_path
                                                 : resources->path,
            UINT64_C(0x02000000), 0xffu, error);
        if (machine->flash_storage == NULL) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
    }
    machine->soc = semu_apollo4_create(machine->bus, error);
    if (machine->soc == NULL) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_apollo4_init(machine->soc, machine->scheduler, irq_sink,
                           machine, error) != SEMU_OK) {
        return error->code;
    }
    machine->devices = semu_sapporo_devices_create(
        machine->scheduler, machine->flash_storage, error);
    if (machine->devices == NULL) {
        return error->code;
    }
    semu_sapporo_devices_set_logger(machine->devices, machine->logger);
    if (semu_sapporo_devices_bind_bus(machine->devices, machine->bus,
                                      error) != SEMU_OK) {
        return error->code;
    }
    if (semu_sapporo_devices_attach(machine->devices, machine->soc,
                                     error) != SEMU_OK) {
        return error->code;
    }
    machine->nema_gpu = semu_nema_gpu_create(machine->bus,
        machine->display_backend_submit, machine->display_backend_context,
        machine->frame_callback, machine->frame_context,
        irq_sink, machine, machine->scheduler, error);
    if (machine->nema_gpu == NULL) {
        return error->code;
    }
    if (semu_nema_gpu_attach(machine->nema_gpu, error) != SEMU_OK) {
        return error->code;
    }
    return SEMU_OK;
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
        if (machine->external_flash_path != NULL &&
            strcmp(firmware->components[i].role, "resources") == 0) continue;
        semu_status status = load_file(machine->bus, &firmware->components[i], error);
        if (status != SEMU_OK) {
            return status;
        }
    }
    if (machine->external_flash_path != NULL) {
        semu_component image = { "external-flash", "", "", 0u, 0u, { 0u } };
        int written = snprintf(image.path, sizeof(image.path), "%s",
                               machine->external_flash_path);
        if (written < 0 || (size_t)written >= sizeof(image.path)) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "full external flash image path is too long");
            return SEMU_ERR_RANGE;
        }
        image.load_address = UINT32_C(0x14000000);
        image.size = UINT64_C(0x02000000);
        return load_file(machine->bus, &image, error);
    }
    return SEMU_OK;
}

static semu_status enable_layer(semu_machine *machine, const char *id,
                                semu_error *error)
{
    const semu_layer_descriptor *descriptor;
    char hash_text[SEMU_MAX_COMPONENTS][65];
    const char *hashes[SEMU_MAX_COMPONENTS];
    semu_layer_state *state;
    size_t i;

    if (machine->layer_count >= SEMU_MAX_LAYERS) {
        semu_error_set(error, SEMU_ERR_RANGE, "too many compatibility layers");
        return SEMU_ERR_RANGE;
    }
    state = &machine->layers[machine->layer_count];
    if (strcmp(id, semu_sapporo_222_no_device_layer.id) == 0) {
        descriptor = &semu_sapporo_222_no_device_layer;
    } else if (strcmp(id, semu_sapporo_239_wbsto_layer.id) == 0) {
        descriptor = &semu_sapporo_239_wbsto_layer;
    } else {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "unknown layer %s", id);
        return SEMU_ERR_UNSUPPORTED;
    }
    for (i = 0u; i < machine->firmware.component_count; ++i) {
        semu_sha256_format(machine->firmware.components[i].sha256,
                           hash_text[i]);
        hashes[i] = hash_text[i];
    }
    if (semu_layer_enable_checked(state, descriptor, machine->profile.id,
            hashes, machine->firmware.component_count, error) != SEMU_OK) {
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
    if (options->external_flash_path != NULL &&
        semu_sapporo_flash_validate_image(options->external_flash_path,
                                          error) != SEMU_OK) {
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
    machine->display_backend_submit = options->display_backend_submit;
    machine->display_backend_context = options->display_backend_context;
    machine->frame_callback = options->frame_callback;
    machine->frame_context = options->frame_context;
    machine->external_flash_path = options->external_flash_path;
    machine->input_poll = options->input_poll;
    machine->input_poll_context = options->input_poll_context;
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
        semu_nema_gpu_destroy(machine->nema_gpu);
        semu_sapporo_devices_destroy(machine->devices);
        semu_storage_destroy(machine->flash_storage);
        semu_apollo4_destroy(machine->soc);
        semu_scheduler_destroy(machine->scheduler);
        semu_bus_destroy(machine->bus);
        free(machine);
    }
}

semu_status semu_machine_reset_state_internal(semu_machine *machine,
                                              uint32_t vector_table,
                                              int preserve_ram,
                                              semu_error *error)
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
    if (!preserve_ram) semu_bus_reset(machine->bus);
    semu_apollo4_reset(machine->soc);
    semu_sapporo_devices_reset(machine->devices);
    semu_nema_gpu_reset(machine->nema_gpu);
    if (load_components(machine, &machine->firmware, error) != SEMU_OK) {
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return error->code;
    }
    {
        size_t i;
        for (i = 0u; i < machine->layer_count; ++i) {
            if (semu_layer_enable(&machine->layers[i],
                                  machine->layers[i].descriptor,
                                  machine->profile.id, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_222_no_device_layer &&
                semu_sapporo_222_install_no_device(machine->bus,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_222_no_device_layer &&
                semu_sapporo_devices_bind_no_device_fixtures(
                    machine->devices, &machine->layers[i], machine->logger,
                    error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
        }
    }
    semu_cpu_reset(machine->cpu, vector_table, error);
    machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
    return machine->stop_reason == SEMU_STOP_NONE ? SEMU_OK : error->code;
}

semu_status semu_machine_reset(semu_machine *machine, semu_error *error)
{
    if (machine == NULL || machine->cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "cannot reset null machine");
        return SEMU_ERR_ARGUMENT;
    }
    machine->instruction_epoch = 0u;
    machine->virtual_time_epoch = 0u;
    return semu_machine_reset_state_internal(
        machine, machine->profile.vector_table, 0, error);
}

semu_status semu_machine_input(semu_machine *machine,
                               const semu_input_event *event,
                               semu_error *error)
{
    /* Sapporo physical button wiring (E-SAP-BUTTONS-001,
     * docs/research/native-live-ui-navigation.md): GPIO57 = upper/previous,
     * GPIO58 = middle, GPIO59 = lower/next. Semantic IDs stay spatial, so the
     * semantic LOWER button drives the physical next/skip button (pin 59). */
    static const unsigned pins[] = { 57u, 58u, 59u };
    if (machine == NULL || event == NULL || event->kind != SEMU_INPUT_BUTTON ||
        event->code >= SEMU_ARRAY_LEN(pins)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "unsupported board input");
        return SEMU_ERR_UNSUPPORTED;
    }
    return semu_apollo4_set_gpio_input(machine->soc, pins[event->code],
                                       event->value != 0, error);
}

semu_stop_reason semu_machine_stop_reason(const semu_machine *machine)
{
    return machine != NULL ? machine->stop_reason : SEMU_STOP_USER;
}

uint64_t semu_machine_instructions(const semu_machine *machine)
{
    const semu_cpu_state *state = machine != NULL && machine->cpu != NULL
                                      ? semu_cpu_get_state(machine->cpu) : NULL;
    return state != NULL ? machine->instruction_epoch + state->instructions : 0u;
}

uint64_t semu_machine_virtual_time(const semu_machine *machine)
{
    return machine != NULL && machine->scheduler != NULL
               ? machine->virtual_time_epoch +
                     semu_scheduler_now(machine->scheduler) : 0u;
}

uint32_t semu_machine_program_counter(const semu_machine *machine)
{
    const semu_cpu_state *state = machine != NULL && machine->cpu != NULL
                                      ? semu_cpu_get_state(machine->cpu) : NULL;
    return state != NULL ? state->r[15] : 0u;
}
