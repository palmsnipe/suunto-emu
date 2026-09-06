#include "sapporo_devices_internal.h"
#include "sapporo_gps_compat.h"

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
