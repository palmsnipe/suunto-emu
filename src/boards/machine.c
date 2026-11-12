#include "machine_internal.h"

#include "semu/apollo4.h"
#include "semu/bus.h"
#include "semu/compat.h"
#include "semu/cpu.h"
#include "semu/hash.h"
#include "semu/scheduler.h"
#include "semu/storage.h"
#include "../compat/sapporo_222.h"
#include "../compat/sapporo_233_files.h"
#include "../compat/sapporo_233_firstboot.h"
#include "../compat/sapporo_233_sentinel_guard.h"
#include "../compat/sapporo_233_touch_route.h"
#include "../compat/sapporo_233_ohr.h"
#include "../compat/sapporo_233_gps.h"
#include "../compat/sapporo_233_production.h"
#include "../compat/sapporo_235_production.h"
#include "../compat/sapporo_235_ohr.h"
#include "../compat/sapporo_235_gps.h"
#include "../compat/sapporo_235_gps_reopen.h"
#include "../compat/sapporo_235_gps_awake.h"
#include "../compat/sapporo_239.h"
#include "../compat/sapporo_239_gps.h"
#include "../compat/sapporo_239_gps_reopen.h"
#include "../compat/sapporo_239_gps_awake.h"
#include "../compat/ulsan_244_production.h"
#include "../devices/sapporo_devices.h"
#include "../devices/sapporo_flash.h"
#include "../devices/ulsan_als.h"
#include "../devices/sapporo_info1.h"
#include "../devices/sapporo_nema_gpu.h"
#include "../devices/ulsan_buszero.h"
#include "ulsan_board.h"

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
            strcmp(profile->id, "sapporo-2.35.34") == 0 ||
            strcmp(profile->id, "sapporo-2.39.20") == 0);
}

static int known_ulsan_profile(const semu_profile *profile)
{
    return semu_ulsan_board_accepted(profile->board, profile->id);
}

static int known_wismar_profile(const semu_profile *profile)
{
    return semu_wismar_board_accepted(profile->board, profile->id);
}

/* Forward declaration: map_board dispatches on the board identity. */
static semu_status map_sapporo(semu_machine *machine, semu_error *error);
static semu_status map_wismar_tail(semu_machine *machine,
                                   semu_apollo4_irq_fn irq_sink,
                                   semu_error *error);

/* Board dispatch for the Apollo4 Plus variant (ticket 725): the Ulsan board
 * maps only evidence-backed memory; Sapporo wiring stays isolated. */
static semu_status map_board(semu_machine *machine, semu_error *error)
{
    if (known_ulsan_profile(&machine->profile)) {
        if (strcmp(machine->profile.id, "ulsan-2.44.52") == 0) {
            semu_status status = semu_ulsan_244_board_map(machine->bus,
                &machine->ulsan_244, machine->scheduler, irq_sink, machine,
                error);
            if (status != SEMU_OK)
                return status;
            return semu_ulsan_244_disp_map(machine->bus,
                &machine->ulsan_244.display, irq_sink, machine,
                machine->frame_callback, machine->frame_context, error);
        }
        /* Integrator seam (precedent: map_sapporo passes irq_sink):
         * one line sink and scheduler for every evidence-wired Ulsan
         * interrupt (IOM4 IRQ 10, MSPI1 IRQ 21, TIMER1 comparator
         * IRQ 14/68). Device maps reset and detach the wiring, so the
         * attach follows the map; machine resets preserve it. */
        if (semu_ulsan_board_map(machine->bus, error) != SEMU_OK) {
            return SEMU_ERR_STATE;
        }
        semu_ulsan_board_attach_irq_sink(irq_sink, machine,
                                         machine->scheduler);
        return SEMU_OK;
    }
    /* Wismar reset board (ticket 755, E-WIS-0001): regions only, then the
     * shared Apollo4 SoC blocks, which fail closed on every unobserved
     * transaction (the first one is the TIMER14 edge selector). No Sapporo
     * device, INFO1, or Nema display attach: E-WIS-0001 names no Wismar
     * display/sensor wiring, and the ticket forbids inferred shared wiring. */
    if (known_wismar_profile(&machine->profile)) {
        if (semu_wismar_board_map(machine->bus, error) != SEMU_OK) {
            return SEMU_ERR_STATE;
        }
        return map_wismar_tail(machine, irq_sink, error);
    }
    return map_sapporo(machine, error);
}

static semu_status map_wismar_tail(semu_machine *machine,
                                   semu_apollo4_irq_fn irq_sink,
                                   semu_error *error)
{
    /* The shared Apollo4 SoC block set (ctimer at 0x40008000, SystemTimer,
     * UART1, IOM/MSPI, MRAM, GPIO, clock/power/reset controllers) is the
     * Apollo4 address-space contract; every unobserved register refuses.
     * The Wismar guest's first Wismar-specific transaction, the TIMER14
     * edge-mode selector 0x00000a10 at 0x40008000+0x3c0, therefore stops
     * the run as a device refusal (E-WIS-0001; the lane's own edge-compat
     * wrapper is a lane workaround, not firmware behavior, and is
     * deliberately not reproduced). */
    machine->soc = semu_apollo4_create(machine->bus, error);
    if (machine->soc == NULL) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_apollo4_init(machine->soc, machine->scheduler, irq_sink,
                           machine, error) != SEMU_OK) {
        return error->code;
    }
    return semu_apollo4_select_profile(machine->soc, machine->profile.id,
                                       error);
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
    if (semu_sapporo_devices_select_profile(
            machine->devices, machine->profile.id, error) != SEMU_OK) {
        return error->code;
    }
    if (semu_apollo4_select_profile(machine->soc, machine->profile.id,
                                    error) != SEMU_OK) {
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
        machine->display_backend, machine->display_backend_context,
        machine->frame_callback, machine->frame_context,
        irq_sink, machine, machine->scheduler, error);
    if (machine->nema_gpu == NULL) {
        return error->code;
    }
    if (semu_nema_gpu_attach(machine->nema_gpu, error) != SEMU_OK) {
        return error->code;
    }
    semu_nema_gpu_set_logger(machine->nema_gpu, machine->logger);
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
    if (strcmp(id, semu_ulsan_244_production_layer.id) == 0 ||
        strcmp(id, semu_ulsan_244_emmc_layer.id) == 0 ||
        strcmp(id, semu_ulsan_244_gps_layer.id) == 0) {
        if (strcmp(id, semu_ulsan_244_production_layer.id) == 0)
            descriptor = &semu_ulsan_244_production_layer;
        else if (strcmp(id, semu_ulsan_244_emmc_layer.id) == 0)
            descriptor = &semu_ulsan_244_emmc_layer;
        else
            descriptor = &semu_ulsan_244_gps_layer;
        for (i = 0u; i < machine->layer_count; ++i) {
            if (machine->layers[i].descriptor == descriptor) {
                semu_error_set(error, SEMU_ERR_CONFLICT, "duplicate Ulsan layer %s", id);
                return SEMU_ERR_CONFLICT;
            }
        }
    } else if (strcmp(id, semu_sapporo_222_no_device_layer.id) == 0) {
        descriptor = &semu_sapporo_222_no_device_layer;
    } else if (strcmp(id, semu_sapporo_233_files_layer.id) == 0) {
        descriptor = &semu_sapporo_233_files_layer;
    } else if (strcmp(id, semu_sapporo_233_firstboot_layer.id) == 0) {
        descriptor = &semu_sapporo_233_firstboot_layer;
    } else if (strcmp(id, semu_sapporo_233_touch_route_layer.id) == 0) {
        descriptor = &semu_sapporo_233_touch_route_layer;
    } else if (strcmp(id, semu_sapporo_233_sentinel_guard_layer.id) == 0) {
        descriptor = &semu_sapporo_233_sentinel_guard_layer;
    } else if (strcmp(id, semu_sapporo_233_ohr_layer.id) == 0) {
        descriptor = &semu_sapporo_233_ohr_layer;
    } else if (strcmp(id, semu_sapporo_233_gps_layer.id) == 0) {
        descriptor = &semu_sapporo_233_gps_layer;
    } else if (strcmp(id, semu_sapporo_233_production_layer.id) == 0) {
        descriptor = &semu_sapporo_233_production_layer;
    } else if (strcmp(id, semu_sapporo_235_production_layer.id) == 0) {
        descriptor = &semu_sapporo_235_production_layer;
    } else if (strcmp(id, semu_sapporo_235_ohr_layer.id) == 0) {
        descriptor = &semu_sapporo_235_ohr_layer;
    } else if (strcmp(id, semu_sapporo_235_gps_layer.id) == 0) {
        descriptor = &semu_sapporo_235_gps_layer;
    } else if (strcmp(id, semu_sapporo_235_gps_awake_layer.id) == 0) {
        descriptor = &semu_sapporo_235_gps_awake_layer;
    } else if (strcmp(id, semu_sapporo_235_gps_reopen_layer.id) == 0) {
        descriptor = &semu_sapporo_235_gps_reopen_layer;
    } else if (strcmp(id, semu_sapporo_239_wbsto_layer.id) == 0) {
        descriptor = &semu_sapporo_239_wbsto_layer;
    } else if (strcmp(id, semu_sapporo_239_gps_layer.id) == 0) {
        descriptor = &semu_sapporo_239_gps_layer;
    } else if (strcmp(id, semu_sapporo_239_gps_reopen_layer.id) == 0) {
        descriptor = &semu_sapporo_239_gps_reopen_layer;
    } else if (strcmp(id, semu_sapporo_239_gps_awake_layer.id) == 0) {
        descriptor = &semu_sapporo_239_gps_awake_layer;
    } else if (strcmp(id, semu_sapporo_239_gps_awake_five_layer.id) == 0) {
        descriptor = &semu_sapporo_239_gps_awake_five_layer;
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

static semu_status bind_ulsan_startup_iom(semu_machine *machine,
    semu_apollo4_iom **iom, uint32_t base, unsigned irq,
    const semu_serial_endpoint *endpoint, semu_error *error)
{
    *iom = semu_apollo4_iom_create(machine->bus, base, irq, irq_sink, machine,
        semu_apollo4_dma_execute, machine->ulsan_244.iom2.dma, machine->scheduler, error);
    if (*iom == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_apollo4_iom_select_race_s(*iom);
    return endpoint == NULL ? SEMU_OK : semu_apollo4_iom_attach_endpoint(*iom, endpoint, error);
}

static semu_transaction_result ulsan_iom3_transfer(void *context,
    semu_serial_transaction *transaction, semu_error *error)
{
    if (transaction->address == 0x24u)
        return semu_ulsan_touch_transfer(context, transaction, error);
    return semu_ulsan_als_transfer(NULL, transaction, error);
}

static semu_status bind_ulsan_gps(semu_machine *machine, semu_error *error)
{
    semu_ulsan_244_board *board = &machine->ulsan_244;
    size_t i;
    semu_layer_state *gps = NULL;
    int production = 0;

    for (i = 0u; i < machine->layer_count; ++i) {
        if (machine->layers[i].descriptor == &semu_ulsan_244_gps_layer)
            gps = &machine->layers[i];
        if (machine->layers[i].descriptor == &semu_ulsan_244_production_layer)
            production = 1;
    }
    if (gps == NULL) return SEMU_OK;
    if (!production || machine->ulsan_244.emmc_media == NULL) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Ulsan GPS requires the production-data and eMMC layers");
        return SEMU_ERR_CONFLICT;
    }
    board->touch.gpio = board->gpio;
    board->crown.gpio = board->gpio;
    /* The reference lane attaches the actuator for the full UI boot. */
    board->iom4.actuator_enabled = 1;
    semu_apollo4_gpio_enable_race_s_inputs(board->gpio);
    board->iom3_endpoint = (semu_serial_endpoint){
        "ulsan.iom3", ulsan_iom3_transfer, &board->touch};
    board->crown_endpoint = (semu_serial_endpoint){
        "ulsan.crown", semu_ulsan_crown_transfer, &board->crown};
    if (bind_ulsan_startup_iom(machine, &board->iom1, SEMU_APOLLO4_IOM1_BASE,
            SEMU_APOLLO4_IOM1_IRQ, NULL, error) != SEMU_OK ||
        bind_ulsan_startup_iom(machine, &board->iom3, SEMU_APOLLO4_IOM3_BASE,
            SEMU_APOLLO4_IOM3_IRQ, &board->iom3_endpoint, error) != SEMU_OK ||
        bind_ulsan_startup_iom(machine, &board->iom6, SEMU_APOLLO4_IOM6_BASE,
            SEMU_APOLLO4_IOM6_IRQ, &board->crown_endpoint, error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    if (semu_ulsan_244_iom2_enable_startup(&board->iom2, board->gpio, gps,
            machine->logger, error) != SEMU_OK)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    return semu_ulsan_244_gps_bind(&board->gps, machine->bus,
        board->gpio, machine->scheduler, irq_sink, machine,
        gps, machine->logger, error);
}

static semu_status bind_ulsan_emmc(semu_machine *machine, const char *path,
                                   semu_error *error)
{
    size_t i, size;
    int enabled = 0;
    semu_status status;

    for (i = 0u; i < machine->layer_count; ++i)
        if (machine->layers[i].descriptor == &semu_ulsan_244_emmc_layer)
            enabled = 1;
    if (enabled != (path != NULL)) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Ulsan eMMC requires both the named layer and --ulsan-emmc");
        return SEMU_ERR_CONFLICT;
    }
    if (!enabled)
        return SEMU_OK;
    machine->ulsan_244.emmc_media = semu_ulsan_sdio_load_media(path, &size, error);
    if (machine->ulsan_244.emmc_media == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    status = semu_ulsan_sdio_map(machine->bus, &machine->ulsan_244.sdio,
        machine->ulsan_244.emmc_media, size, error);
    if (status == SEMU_OK)
        status = semu_ulsan_244_sd_pad_map(machine->bus, error);
    if (status == SEMU_OK)
        status = semu_ulsan_244_iom2_map(machine->bus, &machine->ulsan_244.iom2,
            machine->scheduler, irq_sink, machine, error);
    if (status == SEMU_OK)
        status = semu_ulsan_244_motion_map(machine->bus, &machine->ulsan_244.motion,
            machine->ulsan_244.iom2.dma, machine->scheduler, irq_sink, machine, error);
    if (status != SEMU_OK)
        return status;
    machine->race_s_backend = semu_nema_backend_create_race_s(error);
    if (machine->race_s_backend == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    machine->nema_gpu = semu_nema_gpu_create(machine->bus,
        &semu_nema_backend_ops, machine->race_s_backend,
        machine->frame_callback, machine->frame_context,
        irq_sink, machine, machine->scheduler, error);
    if (machine->nema_gpu == NULL)
        return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_nema_gpu_select_race_s(machine->nema_gpu);
    semu_nema_gpu_set_logger(machine->nema_gpu, machine->logger);
    return semu_nema_gpu_attach(machine->nema_gpu, error);
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
    if (options->display_snapshot != NULL &&
        (options->display_backend == NULL ||
         options->display_snapshot->backend_id == 0u ||
         options->display_snapshot->save == NULL ||
         options->display_snapshot->load == NULL ||
         options->display_snapshot->published_frame == NULL)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "display snapshot codec is incomplete");
        return NULL;
    }
    if (!known_sapporo_profile(options->profile) &&
        !known_ulsan_profile(options->profile) &&
        !known_wismar_profile(options->profile)) {
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
    machine->display_backend = options->display_backend;
    machine->display_backend_context = options->display_backend_context;
    if (options->display_snapshot != NULL)
        machine->display_snapshot = *options->display_snapshot;
    machine->frame_callback = options->frame_callback;
    machine->frame_context = options->frame_context;
    machine->external_flash_path = options->external_flash_path;
    machine->input_poll = options->input_poll;
    machine->input_poll_context = options->input_poll_context;
    machine->sapporo_239_files = semu_sapporo_239_files_create(error);
    machine->bus = semu_bus_create(error);
    machine->scheduler = semu_scheduler_create(error);
    if (machine->sapporo_239_files == NULL || machine->bus == NULL ||
        machine->scheduler == NULL ||
        map_board(machine, error) != SEMU_OK) {
        semu_machine_destroy(machine);
        return NULL;
    }
    for (i = 0u; i < options->layer_count; ++i) {
        if (enable_layer(machine, options->layers[i], error) != SEMU_OK) {
            semu_machine_destroy(machine);
            return NULL;
        }
    }
    if (bind_ulsan_emmc(machine, options->ulsan_emmc_path, error) != SEMU_OK ||
        bind_ulsan_gps(machine, error) != SEMU_OK) {
        semu_machine_destroy(machine);
        return NULL;
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
        semu_ulsan_244_gps_destroy(&machine->ulsan_244.gps);
        semu_cpu_destroy(machine->cpu);
        semu_nema_gpu_destroy(machine->nema_gpu);
        semu_nema_backend_destroy(machine->race_s_backend);
        semu_sapporo_devices_destroy(machine->devices);
        semu_storage_destroy(machine->flash_storage);
        semu_apollo4_destroy(machine->soc);
        semu_apollo4_iom_destroy(machine->ulsan_244.motion.controller);
        semu_apollo4_iom_destroy(machine->ulsan_244.iom1);
        semu_apollo4_iom_destroy(machine->ulsan_244.iom3);
        semu_apollo4_iom_destroy(machine->ulsan_244.iom6);
        semu_ulsan_244_iom2_destroy(&machine->ulsan_244.iom2);
        semu_scheduler_destroy(machine->scheduler);
        semu_bus_destroy(machine->bus);
        semu_apollo4_gpio_destroy(machine->ulsan_244.gpio);
        semu_ulsan_disp_destroy_state(&machine->ulsan_244.display);
        free(machine->ulsan_244.emmc_media);
        semu_sapporo_239_files_destroy(machine->sapporo_239_files);
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
    if (semu_nema_gpu_busy(machine->nema_gpu)) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "machine: reset during GPU submission");
        return SEMU_ERR_CONFLICT;
    }
    if (semu_manifest_validate(&machine->profile, &machine->firmware,
                               error) != SEMU_OK) {
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return error != NULL ? error->code : SEMU_ERR_CONFLICT;
    }
    if (preserve_ram && strcmp(machine->profile.id, "ulsan-2.44.52") == 0) {
        semu_ulsan_rtc_reset_state(&machine->ulsan_244.rtc);
        semu_ulsan_timer0_reset_state(&machine->ulsan_244.timer);
    }
    semu_scheduler_reset(machine->scheduler);
    if (!preserve_ram) {
        semu_bus_reset(machine->bus);
        semu_sapporo_239_files_reset(machine->sapporo_239_files);
        /* Session-only 2.33 staging: a cold reset re-seeds the overlay. */
        memset(&machine->sapporo_233_files, 0,
               sizeof(machine->sapporo_233_files));
        memset(&machine->sapporo_233_firstboot, 0,
               sizeof(machine->sapporo_233_firstboot));
        memset(&machine->sapporo_233_sentinel_guard, 0,
               sizeof(machine->sapporo_233_sentinel_guard));
        memset(&machine->sapporo_233_touch_route, 0,
               sizeof(machine->sapporo_233_touch_route));
    }
    semu_apollo4_reset(machine->soc);
    semu_sapporo_devices_reset(machine->devices);
    semu_ulsan_244_gps_reset(&machine->ulsan_244.gps);
    semu_apollo4_iom_reset(machine->ulsan_244.iom1);
    semu_apollo4_iom_reset(machine->ulsan_244.iom3);
    semu_apollo4_iom_reset(machine->ulsan_244.iom6);
    semu_ulsan_crown_reset(&machine->ulsan_244.crown);
    semu_ulsan_touch_reset(&machine->ulsan_244.touch);
    semu_ulsan_244_iom2_reset(&machine->ulsan_244.iom2);
    if (machine->ulsan_244.motion.controller != NULL)
        semu_ulsan_244_motion_reset(&machine->ulsan_244.motion);
    semu_nema_gpu_reset(machine->nema_gpu);
    if (machine->race_s_backend != NULL)
        (void)semu_nema_backend_reset(machine->race_s_backend);
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
            if (machine->layers[i].descriptor == &semu_sapporo_235_production_layer &&
                semu_sapporo_235_install_production(machine->bus,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_ulsan_244_production_layer &&
                semu_ulsan_244_install_production(machine->bus, &machine->layers[i],
                    machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_ulsan_244_emmc_layer &&
                semu_layer_hit(&machine->layers[i], machine->logger,
                    "trigger=emmc-mount classification=synthetic verified session media",
                    error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_files_layer) {
                machine->sapporo_233_files.state = &machine->layers[i];
                machine->sapporo_233_files.logger = machine->logger;
                if (semu_sapporo_233_files_install(&machine->sapporo_233_files,
                        machine->bus, error) != SEMU_OK) {
                    machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                    return error->code;
                }
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_production_layer &&
                semu_sapporo_233_install_production(machine->bus,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_firstboot_layer) {
                machine->sapporo_233_firstboot.state = &machine->layers[i];
                machine->sapporo_233_firstboot.logger = machine->logger;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_touch_route_layer) {
                machine->sapporo_233_touch_route.state = &machine->layers[i];
                machine->sapporo_233_touch_route.logger = machine->logger;
                if (semu_sapporo_233_touch_route_install(
                        &machine->sapporo_233_touch_route, machine->bus,
                        error) != SEMU_OK) {
                    machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                    return error->code;
                }
                if (semu_sapporo_devices_bind_233_touch_route(
                        machine->devices, &machine->sapporo_233_touch_route,
                        &machine->layers[i], machine->logger, error) != SEMU_OK) {
                    machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                    return error->code;
                }
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_sentinel_guard_layer &&
                semu_sapporo_233_sentinel_guard_install(
                    &machine->sapporo_233_sentinel_guard,
                    &machine->layers[i], machine->logger, machine->scheduler,
                    error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_gps_layer &&
                semu_sapporo_devices_bind_233_gps(machine->devices,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_233_ohr_layer &&
                semu_sapporo_devices_bind_233_ohr(machine->devices,
                    &machine->layers[i], machine->logger, error) != SEMU_OK) {
                machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
                return error->code;
            }
            if (machine->layers[i].descriptor == &semu_sapporo_235_ohr_layer &&
                semu_sapporo_devices_bind_235_ohr(machine->devices,
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
    if (semu_sapporo_devices_bind_gps_layers(machine->devices, machine->layers,
            machine->layer_count, machine->logger, error) != SEMU_OK) {
        machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
        return error->code;
    }
    semu_cpu_reset(machine->cpu, vector_table, error);
    /* E-WIS-0001: the Wismar NVIC implements four priority bits (writing
     * 0xffffffff to 0xE000E400 read back 0xF0F0F0F0, twice byte-identically).
     * The selector is re-bound after every CPU reset because the reset
     * restores the three-bit 0xe0 default; the seam sits where both the
     * CPU and the selected profile are in scope. The bind call validates
     * the mask and refuses anything but 0xe0/0xf0. */
    if (machine->stop_reason == SEMU_STOP_NONE &&
        strcmp(machine->profile.id, "wismar-2.46.14") == 0 &&
        !semu_cpu_bind_priority_mask(machine->cpu, UINT8_C(0xf0), error)) {
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return error->code;
    }
    machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
    return machine->stop_reason == SEMU_STOP_NONE ? SEMU_OK : error->code;
}

semu_status semu_machine_reset(semu_machine *machine, semu_error *error)
{
    if (machine == NULL || machine->cpu == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "cannot reset null machine");
        return SEMU_ERR_ARGUMENT;
    }
    if (semu_nema_gpu_busy(machine->nema_gpu)) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "machine: reset during GPU submission");
        return SEMU_ERR_CONFLICT;
    }
    machine->instruction_epoch = 0u;
    machine->virtual_time_epoch = 0u;
    return semu_machine_reset_state_internal(
        machine, machine->profile.vector_table, 0, error);
}

static semu_status ulsan_pointer_input(semu_ulsan_244_board *board,
    const semu_input_event *event, semu_error *error)
{
    unsigned pin = event->kind == SEMU_INPUT_TOUCH ? 100u : 104u;
    if (!semu_apollo4_gpio_falling_input_armed(board->gpio, pin)) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "Race S input IRQ is not armed");
        return SEMU_ERR_CONFLICT;
    }
    if (event->kind == SEMU_INPUT_TOUCH)
        return semu_ulsan_touch_point(&board->touch, event->x, event->y,
                                     event->value, error);
    return semu_ulsan_crown_rotate(&board->crown, event->value, error);
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
    if (machine == NULL || event == NULL) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED, "unsupported board input");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (known_ulsan_profile(&machine->profile)) {
        semu_ulsan_244_board *board = &machine->ulsan_244;
        if (board->iom6 != NULL) {
            if (event->kind == SEMU_INPUT_CROWN_ROTATE || event->kind == SEMU_INPUT_TOUCH)
                return ulsan_pointer_input(board, event, error);
            if (event->kind == SEMU_INPUT_BUTTON &&
                event->code < SEMU_ARRAY_LEN(pins))
                return semu_apollo4_gpio_set_input(board->gpio, pins[event->code],
                                                   event->value != 0, error);
        }
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "ulsan board input is not wired");
        return SEMU_ERR_UNSUPPORTED;
    }
    /* E-WIS-0001 names no Wismar button, crown, or touch pin, so every
     * semantic input refuses instead of reaching the Sapporo GPIO wiring. */
    if (known_wismar_profile(&machine->profile)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "wismar board input is not wired");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (event->kind != SEMU_INPUT_BUTTON || event->code >= SEMU_ARRAY_LEN(pins)) {
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
