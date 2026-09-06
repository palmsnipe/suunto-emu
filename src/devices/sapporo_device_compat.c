#include "sapporo_devices_internal.h"
#include "sapporo_gps_compat.h"
#include <string.h>

static semu_transaction_result gps_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_devices *d = context;
    if (request != NULL && count == 6u && memcmp(request, "@GSR\r\n", 6u) == 0)
        return semu_sapporo_239_gps_reopen_exchange(&d->gps_reopen_context,
            request, count, gps, error);
    return semu_sapporo_239_gps_exchange(&d->gps_239_context,
        request, count, gps, error);
}

semu_status semu_sapporo_devices_bind_gps_layers(
    semu_sapporo_devices *d, semu_layer_state *layers, size_t count,
    semu_logger *logger, semu_error *error)
{
    semu_layer_state *startup = NULL, *reopen = NULL;
    size_t i;
    if (layers == NULL && count != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete GPS layer set");
        return SEMU_ERR_ARGUMENT;
    }
    for (i = 0u; i < count; ++i) {
        semu_layer_state **slot = NULL;
        if (semu_sapporo_239_gps_is_layer(layers[i].descriptor)) slot = &startup;
        if (semu_sapporo_239_gps_reopen_is_layer(layers[i].descriptor)) slot = &reopen;
        if (slot == NULL) continue;
        if (*slot != NULL) goto conflict;
        *slot = &layers[i];
    }
    if (reopen != NULL && startup == NULL) goto conflict;
    if (startup == NULL) return SEMU_OK;
    if (d == NULL || logger == NULL || !d->ohr2_profile_239 || !startup->enabled ||
        (startup->descriptor != &semu_sapporo_239_gps_layer &&
         startup->descriptor != &d->gps_239_context.descriptor) ||
        (d->gps_239_context.state != NULL && d->gps_239_context.state != startup))
        goto conflict;
    if (reopen != NULL && (!reopen->enabled ||
        (reopen->descriptor != &semu_sapporo_239_gps_reopen_layer &&
         reopen->descriptor != &d->gps_reopen_context.descriptor) ||
        (d->gps_reopen_context.state != NULL && d->gps_reopen_context.state != reopen)))
        goto conflict;
    /* Rebinding must validate both owners before changing either callback. */
    if (startup->descriptor->intervention_count != 2u ||
        (startup->descriptor == &d->gps_239_context.descriptor &&
         startup->descriptor->interventions != d->gps_239_context.interventions) ||
        (reopen != NULL && (reopen->descriptor->intervention_count != 2u ||
         (reopen->descriptor == &d->gps_reopen_context.descriptor &&
          reopen->descriptor->interventions != d->gps_reopen_context.interventions))))
        goto conflict;
    if (reopen != NULL && (!semu_sapporo_239_gps_counts_valid(startup->hits,
            startup->descriptor->interventions[0].hits,
            startup->descriptor->interventions[1].hits) ||
        !semu_sapporo_239_gps_counts_valid(reopen->hits,
            reopen->descriptor->interventions[0].hits,
            reopen->descriptor->interventions[1].hits) ||
        (reopen->hits != 0u && startup->hits != 2u))) {
        semu_error_set(error, SEMU_ERR_FORMAT, "2.39 GPS reopen dependency lifecycle invalid");
        return SEMU_ERR_FORMAT;
    }
    if (semu_sapporo_devices_bind_gps_startup_fixture(d, startup, logger, error) != SEMU_OK)
        return error->code;
    if (reopen != NULL) {
        if (semu_sapporo_239_gps_reopen_bind(&d->gps_reopen_context, reopen,
                &d->gps_239_context, logger, error) != SEMU_OK) return error->code;
        semu_sapporo_cxd5610_set_exchange(d->gps, gps_exchange, d);
    }
    return SEMU_OK;
conflict:
    semu_error_set(error, SEMU_ERR_CONFLICT, "GPS layer dependency or ownership conflict");
    return SEMU_ERR_CONFLICT;
}

semu_status semu_sapporo_devices_bind_gps_startup_fixture(
    semu_sapporo_devices *devices, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    semu_status status;
    if (devices == NULL || !devices->ohr2_profile_239) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "GPS fixture requires Sapporo 2.39");
        return SEMU_ERR_CONFLICT;
    }
    status = semu_sapporo_239_gps_bind(&devices->gps_239_context, state, logger, error);
    if (status != SEMU_OK) return status;
    semu_sapporo_cxd5610_set_exchange(devices->gps,
        semu_sapporo_239_gps_exchange, &devices->gps_239_context);
    return SEMU_OK;
}

semu_status semu_sapporo_devices_bind_no_device_fixtures(
    semu_sapporo_devices *devices, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    if (devices == NULL || state == NULL || logger == NULL ||
        !state->enabled) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo compatibility fixture binding is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    devices->fixture_context.state = state;
    devices->fixture_context.logger = logger;
    devices->fixture_context.gps_running_status_armed = 0;
    semu_sapporo_cxd5610_set_exchange(
        devices->gps, semu_sapporo_222_gps_exchange,
        &devices->fixture_context);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_sapporo_devices_apply_compat_hook(
    semu_sapporo_devices *devices, semu_bus *bus, semu_cpu_state *cpu_state,
    semu_layer_state *state, semu_logger *logger, semu_error *error)
{
    uint64_t state_hook_hits;
    semu_status status;
    if (devices == NULL || bus == NULL || cpu_state == NULL || state == NULL ||
        logger == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Sapporo compatibility hook binding is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (!semu_sapporo_devices_compat_hook_pc(cpu_state->r[15])) {
        return SEMU_OK;
    }
    if (semu_sapporo_239_gps_is_layer(state->descriptor)) {
        if (devices->gps_239_context.state != state ||
            devices->gps_239_context.logger != logger) {
            semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS fixture is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_239_gps_startup(&devices->gps_239_context,
            devices->gps, bus, cpu_state, error);
    }
    if (semu_sapporo_239_gps_reopen_is_layer(state->descriptor)) {
        if (devices->gps_reopen_context.state != state ||
            devices->gps_reopen_context.logger != logger) {
            semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS reopen is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_239_gps_reopen_start(&devices->gps_reopen_context,
            devices->gps, bus, cpu_state, error);
    }
    state_hook_hits = state->descriptor != NULL &&
        state->descriptor->interventions != NULL &&
        SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP <
            state->descriptor->intervention_count
        ? state->descriptor->interventions[
              SEMU_SAPPORO_222_IV_GPS_STATE_STARTUP].hits : 0u;
    status = semu_sapporo_222_apply_firmware_hook(bus, cpu_state, state,
                                                   logger, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_sapporo_gps_compat_apply(
        devices->soc != NULL ? devices->soc->uart : NULL, bus, cpu_state,
        devices->gps, &devices->fixture_context, error);
    if (status != SEMU_OK) return status;
    if (state->descriptor != NULL && state->descriptor->interventions != NULL &&
        cpu_state->r[15] == UINT32_C(0x0010f610) && state_hook_hits == 1u &&
        state->descriptor->interventions[
            SEMU_SAPPORO_222_IV_GPS_STARTUP].hits == 0u) {
        return semu_sapporo_222_arm_gps_startup(devices->gps, state, logger,
                                                error);
    }
    return SEMU_OK;
}
