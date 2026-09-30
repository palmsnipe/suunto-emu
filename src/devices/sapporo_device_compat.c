#include "sapporo_devices_internal.h"
#include "sapporo_gps_compat.h"
#include <string.h>

static semu_transaction_result gps235_exchange(void *context,
    const uint8_t *request, size_t count, semu_sapporo_cxd5610 *gps,
    semu_error *error)
{
    semu_sapporo_devices *d = context;
    if (request != NULL && count == 6u && memcmp(request, "@VER\r\n", 6u) == 0)
        return semu_sapporo_235_gps_exchange(&d->gps_235_context,
            request, count, gps, error);
    return semu_sapporo_235_gps_reopen_exchange(&d->gps_235_reopen_context,
        request, count, gps, error);
}

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
    semu_layer_state *startup = NULL, *reopen = NULL, *awake = NULL;
    semu_layer_state *gps235 = NULL, *reopen235 = NULL, *awake235 = NULL;
    size_t i;
    if (layers == NULL && count != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "incomplete GPS layer set");
        return SEMU_ERR_ARGUMENT;
    }
    for (i = 0u; i < count; ++i) {
        semu_layer_state **slot = NULL;
        if (layers[i].descriptor == &semu_sapporo_235_gps_layer) slot = &gps235;
        if (layers[i].descriptor == &semu_sapporo_235_gps_reopen_layer) slot = &reopen235;
        if (layers[i].descriptor == &semu_sapporo_235_gps_awake_layer) slot = &awake235;
        if (semu_sapporo_239_gps_is_layer(layers[i].descriptor)) slot = &startup;
        if (semu_sapporo_239_gps_reopen_is_layer(layers[i].descriptor)) slot = &reopen;
        if (semu_sapporo_239_gps_awake_is_layer(layers[i].descriptor)) slot = &awake;
        if (slot == NULL) continue;
        if (*slot != NULL) goto conflict;
        *slot = &layers[i];
    }
    if (reopen != NULL && startup == NULL) goto conflict;
    if (awake != NULL && (startup == NULL || reopen == NULL)) goto conflict;
    if (reopen235 != NULL && gps235 == NULL) goto conflict;
    if (awake235 != NULL && (gps235 == NULL || reopen235 == NULL)) goto conflict;
    /* Ticket 792 continuation: a snapshot load re-runs this bind after
     * apply_layers restored the image's hit counts into the same
     * layer-state instances the contexts already own from machine
     * creation; that re-bind must confirm the owners (the 2.39 branch
     * below has the same state-identity tolerance).  Any other
     * already-bound context stays a conflict. */
    if (d != NULL && d->gps_235_awake_context.state != NULL &&
        (awake235 == NULL || d->gps_235_awake_context.state != awake235))
        goto conflict;
    if (d != NULL && d->gps_235_reopen_context.state != NULL &&
        (reopen235 == NULL || d->gps_235_reopen_context.state != reopen235))
        goto conflict;
    if (d != NULL && ((d->gps_239_context.state != NULL && startup == NULL) ||
        (d->gps_reopen_context.state != NULL && reopen == NULL) ||
        (d->gps_awake_context.state != NULL && awake == NULL))) goto conflict;
    if (gps235 != NULL) {
        int restore235 = d != NULL && d->gps_235_context.state == gps235;
        if (d == NULL || !d->ohr2_profile_235 || logger == NULL ||
            !gps235->enabled ||
            (!restore235 && (gps235->hits != 0u ||
                             d->gps_235_context.state != NULL)) ||
            startup != NULL || reopen != NULL || awake != NULL) goto conflict;
        if (reopen235 != NULL &&
            (!reopen235->enabled ||
             (!restore235 && reopen235->hits != 0u) ||
             (d->gps_235_reopen_context.state != NULL &&
              d->gps_235_reopen_context.state != reopen235)))
            goto conflict;
        if (awake235 != NULL &&
            (!awake235->enabled ||
             (!restore235 && awake235->hits != 0u) ||
             (d->gps_235_awake_context.state != NULL &&
              d->gps_235_awake_context.state != awake235)))
            goto conflict;
        d->gps_235_context.state = gps235;
        d->gps_235_context.logger = logger;
        semu_sapporo_cxd5610_set_exchange(d->gps,
            semu_sapporo_235_gps_exchange, &d->gps_235_context);
        if (reopen235 != NULL) {
            d->gps_235_reopen_context.state = reopen235;
            d->gps_235_reopen_context.logger = logger;
            d->gps_235_reopen_context.startup = &d->gps_235_context;
            semu_sapporo_cxd5610_set_exchange(d->gps, gps235_exchange, d);
        }
        if (awake235 != NULL) {
            d->gps_235_awake_context.state = awake235;
            d->gps_235_awake_context.logger = logger;
            d->gps_235_awake_context.reopen = &d->gps_235_reopen_context;
        }
    } else if (d != NULL && d->gps_235_context.state != NULL) goto conflict;
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
    if (awake != NULL && (!awake->enabled ||
        (awake->descriptor != &semu_sapporo_239_gps_awake_layer &&
         awake->descriptor != &semu_sapporo_239_gps_awake_five_layer &&
         awake->descriptor != &d->gps_awake_context.descriptor) ||
        (awake->descriptor == &d->gps_awake_context.descriptor &&
         awake->descriptor->interventions != &d->gps_awake_context.intervention) ||
        (d->gps_awake_context.state != NULL && d->gps_awake_context.state != awake)))
        goto conflict;
    /* Validate every owner/lifecycle before changing any descriptor/callback. */
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
    if (awake != NULL) {
        semu_status status = semu_sapporo_239_gps_awake_validate(awake, startup, reopen, error);
        if (status != SEMU_OK) return status;
    }
    for (i = 0u; i < semu_scheduler_event_count(d->scheduler); ++i) {
        const semu_scheduled_event_state *event = semu_scheduler_event_get(d->scheduler, i);
        if (event->kind == SEMU_SCHED_EVENT_CXD_AWAKE &&
            (awake == NULL || awake->hits == 0u)) goto conflict;
    }
    if (semu_sapporo_devices_bind_gps_startup_fixture(d, startup, logger, error) != SEMU_OK)
        return error->code;
    if (reopen != NULL) {
        if (semu_sapporo_239_gps_reopen_bind(&d->gps_reopen_context, reopen,
                &d->gps_239_context, logger, error) != SEMU_OK) return error->code;
        semu_sapporo_cxd5610_set_exchange(d->gps, gps_exchange, d);
    }
    if (awake != NULL)
        return semu_sapporo_239_gps_awake_bind(&d->gps_awake_context, awake,
            startup, reopen, logger, error);
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

semu_status semu_sapporo_devices_bind_235_ohr(
    semu_sapporo_devices *devices, semu_layer_state *state,
    semu_logger *logger, semu_error *error)
{
    if (devices == NULL || !devices->ohr2_profile_235 || state == NULL ||
        logger == NULL || !state->enabled || state->hits != 0u ||
        state->descriptor != &semu_sapporo_235_ohr_layer ||
        devices->ohr_235_context.state != NULL) {
        semu_error_set(error, SEMU_ERR_CONFLICT, "invalid 2.35 OHR fixture binding");
        return SEMU_ERR_CONFLICT;
    }
    devices->ohr_235_context.state = state;
    devices->ohr_235_context.logger = logger;
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
    /* Device refusals are observed at the next machine boundary. This is
     * status propagation only: no guest instruction or CPU register changes. */
    if (state->descriptor == &semu_sapporo_235_ohr_layer) {
        if (devices->ohr_235_context.state != state ||
            devices->ohr_235_context.logger != logger) {
            semu_error_set(error, SEMU_ERR_STATE, "2.35 OHR fixture is not bound");
            return SEMU_ERR_STATE;
        }
        if (error != NULL) *error = devices->ohr_235_context.refusal;
        return devices->ohr_235_context.refusal.code;
    }
    if (state->descriptor == &semu_sapporo_235_gps_layer) {
        if (devices->gps_235_context.state != state ||
            devices->gps_235_context.logger != logger) {
            semu_error_set(error, SEMU_ERR_STATE, "2.35 GPS fixture is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_235_gps_startup(&devices->gps_235_context,
            devices->gps, bus, cpu_state, error);
    }
    if (state->descriptor == &semu_sapporo_235_gps_reopen_layer) {
        if (devices->gps_235_reopen_context.state != state ||
            devices->gps_235_reopen_context.logger != logger) {
            semu_error_set(error, SEMU_ERR_STATE, "2.35 GPS reopen fixture is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_235_gps_reopen_start(&devices->gps_235_reopen_context,
            devices->gps, bus, cpu_state, error);
    }
    if (state->descriptor == &semu_sapporo_235_gps_awake_layer) {
        if (devices->gps_235_awake_context.state != state ||
            devices->gps_235_awake_context.logger != logger || devices->soc == NULL) {
            semu_error_set(error, SEMU_ERR_STATE, "2.35 GPS awake fixture is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_235_gps_awake_poll(&devices->gps_235_awake_context,
            devices->gps, bus, cpu_state, error);
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
    if (semu_sapporo_239_gps_awake_is_layer(state->descriptor)) {
        if (devices->gps_awake_context.state != state ||
            devices->gps_awake_context.logger != logger || devices->soc == NULL) {
            semu_error_set(error, SEMU_ERR_STATE, "2.39 GPS awake fixture is not bound");
            return SEMU_ERR_STATE;
        }
        return semu_sapporo_239_gps_awake_poll(&devices->gps_awake_context,
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

/* Ticket maintenance (performance): the machine run loop calls the
 * dispatch above once per guest instruction for every present 2.35
 * device layer.  Both helpers let that loop prove the call is a no-op
 * without entering the dispatch; any doubt keeps the original path. */

int semu_sapporo_devices_235_bindings_valid(
    const semu_sapporo_devices *devices, const semu_layer_state *layers,
    size_t count, const semu_logger *logger)
{
    size_t i;
    if (devices == NULL || layers == NULL) return 0;
    for (i = 0u; i < count; ++i) {
        const semu_layer_descriptor *d = layers[i].descriptor;
        if (d == &semu_sapporo_235_ohr_layer) {
            if (devices->ohr_235_context.state != &layers[i] ||
                devices->ohr_235_context.logger != logger) return 0;
        } else if (d == &semu_sapporo_235_gps_layer) {
            if (devices->gps_235_context.state != &layers[i] ||
                devices->gps_235_context.logger != logger) return 0;
        } else if (d == &semu_sapporo_235_gps_reopen_layer) {
            if (devices->gps_235_reopen_context.state != &layers[i] ||
                devices->gps_235_reopen_context.logger != logger) return 0;
        } else if (d == &semu_sapporo_235_gps_awake_layer) {
            if (devices->gps_235_awake_context.state != &layers[i] ||
                devices->gps_235_awake_context.logger != logger ||
                devices->soc == NULL) return 0;
        }
    }
    return 1;
}

/* Returns 1 when calling semu_sapporo_devices_apply_compat_hook for
 * every present 2.35 device layer at this pc provably returns SEMU_OK
 * without side effects, performing the OHR branch's unconditional
 * status propagation exactly (the pre-validated binding plus a
 * latched-clean refusal copy the zeroed refusal struct into the run
 * error, which is what semu_error_clear produces).  Returns 0 when the
 * caller must run the full dispatch: a latched refusal (the GPS
 * helpers refuse before their pc gate) or a pc the hooks act on. */
int semu_sapporo_devices_compat_idle(const semu_sapporo_devices *devices,
                                     uint32_t pc, semu_error *error)
{
    if (devices == NULL) return 0;
    if (devices->ohr_235_context.state != NULL) {
        if (devices->ohr_235_context.refusal.code != SEMU_OK) return 0;
        semu_error_clear(error);
    }
    if ((devices->gps_235_context.state != NULL &&
         (devices->gps_235_context.refusal.code != SEMU_OK ||
          pc == SEMU_SAPPORO_235_GPS_PC)) ||
        (devices->gps_235_reopen_context.state != NULL &&
         (devices->gps_235_reopen_context.refusal.code != SEMU_OK ||
          pc == SEMU_SAPPORO_235_GPS_REOPEN_PC)) ||
        (devices->gps_235_awake_context.state != NULL &&
         (devices->gps_235_awake_context.refusal.code != SEMU_OK ||
          pc == SEMU_SAPPORO_235_GPS_AWAKE_PC)))
        return 0;
    return 1;
}
