#include "machine_internal.h"

#include "../compat/sapporo_239.h"
#include "../compat/sapporo_235_ohr.h"
#include "../compat/sapporo_235_gps.h"
#include "../compat/sapporo_235_gps_reopen.h"
#include "../compat/sapporo_235_gps_awake.h"
#include "../compat/sapporo_239_gps.h"
#include "../compat/sapporo_239_gps_reopen.h"
#include "../compat/sapporo_239_gps_awake.h"

#include <string.h>

static semu_status reset_after_request(semu_machine *machine,
                                       semu_error *error)
{
    const semu_cpu_state *state = semu_cpu_get_state(machine->cpu);
    uint64_t now = semu_scheduler_now(machine->scheduler);

    if (state == NULL || UINT64_MAX - machine->instruction_epoch <
            state->instructions || UINT64_MAX - machine->virtual_time_epoch <
            now) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "machine reset accounting overflow");
        return SEMU_ERR_RANGE;
    }
    machine->instruction_epoch += state->instructions;
    machine->virtual_time_epoch += now;
    /* Apollo4 software reset retains SRAM; explicit reset remains cold. */
    return semu_machine_reset_state_internal(
        machine, machine->profile.vector_table, 1, error);
}

static int update_run_accounting(semu_machine *machine, int was_waiting,
                                 semu_status step_status,
                                 uint64_t instruction_delta,
                                 uint64_t *current_time, uint64_t *executed,
                                 uint64_t *elapsed)
{
    uint64_t time_delta = instruction_delta;

    if (was_waiting || step_status != SEMU_OK) {
        uint64_t after_time = semu_scheduler_now(machine->scheduler);
        if (after_time < *current_time) return 0;
        time_delta = after_time - *current_time;
        *current_time = after_time;
    } else {
        if (UINT64_MAX - *current_time < time_delta) return 0;
        *current_time += time_delta;
    }
    if (UINT64_MAX - *executed < instruction_delta ||
        UINT64_MAX - *elapsed < time_delta) {
        return 0;
    }
    *executed += instruction_delta;
    *elapsed += time_delta;
    return 1;
}

static void log_reset_request(semu_machine *machine,
                              const semu_cpu_state *state,
                              uint64_t virtual_time)
{
    uint32_t fault_address = 0u;
    int has_fault = semu_cpu_fault_address(machine->cpu, &fault_address);
    char fault_text[40];
    semu_log_set_time(machine->logger, virtual_time);
    if (has_fault != 0)
        snprintf(fault_text, sizeof(fault_text), " fault_address=0x%08x",
                 (unsigned)fault_address);
    else
        fault_text[0] = '\0';
    semu_log_write(machine->logger, SEMU_LOG_WARNING, "cpu",
        "machine-reset-request",
        "pc=0x%08x lr=0x%08x sp=0x%08x r0=0x%08x "
        "r1=0x%08x r2=0x%08x r3=0x%08x xpsr=0x%08x "
        "reset_count=%llu compat_hits=%llu instructions=%llu "
        "virtual_time_ns=%llu%s",
        (unsigned)state->r[15], (unsigned)state->r[14],
        (unsigned)state->r[13], (unsigned)state->r[0],
        (unsigned)state->r[1], (unsigned)state->r[2],
        (unsigned)state->r[3], (unsigned)state->xpsr,
        (unsigned long long)machine->reset_request_count,
        (unsigned long long)(machine->layer_count != 0u
            ? machine->layers[0].hits : 0u),
        (unsigned long long)state->instructions,
        (unsigned long long)virtual_time, fault_text);
}

static int apply_compat_hook(semu_machine *machine,
                             const semu_cpu_state *state,
                             semu_error *error)
{
    size_t i;

    for (i = 0u; i < machine->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        if (layer->descriptor == &semu_sapporo_235_ohr_layer ||
            layer->descriptor == &semu_sapporo_235_gps_layer ||
            layer->descriptor == &semu_sapporo_235_gps_reopen_layer ||
            layer->descriptor == &semu_sapporo_235_gps_awake_layer ||
            (layer->descriptor == &semu_sapporo_222_no_device_layer &&
             semu_sapporo_devices_compat_hook_pc(state->r[15])) ||
            (state->r[15] == SEMU_SAPPORO_239_GPS_PC &&
             semu_sapporo_239_gps_is_layer(layer->descriptor)) ||
            (state->r[15] == SEMU_SAPPORO_239_GPS_REOPEN_PC &&
             semu_sapporo_239_gps_reopen_is_layer(layer->descriptor)) ||
            (state->r[15] == SEMU_SAPPORO_239_GPS_AWAKE_PC &&
             semu_sapporo_239_gps_awake_is_layer(layer->descriptor))) {
            if (semu_sapporo_devices_apply_compat_hook(
                    machine->devices, machine->bus,
                    semu_cpu_get_state_mutable(machine->cpu), layer,
                    machine->logger, error) != SEMU_OK) {
                return 0;
            }
        } else if (layer->descriptor == &semu_sapporo_239_wbsto_layer &&
                   semu_sapporo_239_compat_hook_pc(state->r[15])) {
            if (semu_sapporo_239_apply_wbsto_hook(
                    machine->bus, semu_cpu_get_state_mutable(machine->cpu),
                    layer, machine->logger, error) != SEMU_OK) {
                return 0;
            }
        } else if (layer->descriptor == &semu_sapporo_239_wbsto_layer &&
                   semu_sapporo_239_file_hook_pc(state->r[15])) {
            if (semu_sapporo_239_apply_file_hook(
                    machine->sapporo_239_files, machine->bus,
                    semu_cpu_get_state_mutable(machine->cpu), layer,
                    machine->logger, error) != SEMU_OK) {
                return 0;
            }
        }
    }
    return 1;
}

/* Per-run summary of the compatibility-hook gates.  Every hook the
 * loop can call is provably a no-op unless its gate fires, so the
 * common instruction skips the whole layer dispatch (performance
 * maintenance: the five-layer 2.35 profile otherwise pays four hook
 * calls per guest instruction).  Any firing gate falls back to the
 * unchanged apply_compat_hook path, so behavior is byte-identical. */
typedef struct compat_gates {
    int any;            /* any gate can fire at all */
    int device_235;     /* 2.35 OHR/GPS layers present */
    int bindings_ok;    /* their contexts are bound as the dispatch requires */
    int pred_222;       /* sapporo-2.22-no-device layer present */
    int pred_wbsto;     /* sapporo-2.39-wbsto layer present */
    uint32_t pcs_239[3u];
    unsigned pcs_239_count;
} compat_gates;

static void summarize_compat_gates(semu_machine *machine, compat_gates *gates)
{
    size_t i;
    memset(gates, 0, sizeof(*gates));
    for (i = 0u; i < machine->layer_count; ++i) {
        const semu_layer_descriptor *d = machine->layers[i].descriptor;
        if (d == &semu_sapporo_235_ohr_layer ||
            d == &semu_sapporo_235_gps_layer ||
            d == &semu_sapporo_235_gps_reopen_layer ||
            d == &semu_sapporo_235_gps_awake_layer) {
            gates->device_235 = 1;
        } else if (d == &semu_sapporo_222_no_device_layer) {
            gates->pred_222 = 1;
        } else if (d == &semu_sapporo_239_wbsto_layer) {
            gates->pred_wbsto = 1;
        } else if (semu_sapporo_239_gps_is_layer(d) &&
                   gates->pcs_239_count < 3u) {
            gates->pcs_239[gates->pcs_239_count++] = SEMU_SAPPORO_239_GPS_PC;
        } else if (semu_sapporo_239_gps_reopen_is_layer(d) &&
                   gates->pcs_239_count < 3u) {
            gates->pcs_239[gates->pcs_239_count++] =
                SEMU_SAPPORO_239_GPS_REOPEN_PC;
        } else if (semu_sapporo_239_gps_awake_is_layer(d) &&
                   gates->pcs_239_count < 3u) {
            gates->pcs_239[gates->pcs_239_count++] =
                SEMU_SAPPORO_239_GPS_AWAKE_PC;
        }
    }
    /* A machine reset unbinds the 2.35 device contexts; the dispatch
     * would then refuse on the next instruction, and the gate falls
     * back to exactly that path. */
    gates->bindings_ok = !gates->device_235 ||
        semu_sapporo_devices_235_bindings_valid(machine->devices,
            machine->layers, machine->layer_count, machine->logger);
    gates->any = gates->device_235 || gates->pred_222 || gates->pred_wbsto ||
                 gates->pcs_239_count != 0u;
}

static int compat_gates_fire(const compat_gates *gates, uint32_t pc)
{
    unsigned i;
    if (gates->pred_222 && semu_sapporo_devices_compat_hook_pc(pc)) return 1;
    if (gates->pred_wbsto && (semu_sapporo_239_compat_hook_pc(pc) ||
                              semu_sapporo_239_file_hook_pc(pc))) return 1;
    for (i = 0u; i < gates->pcs_239_count; ++i)
        if (gates->pcs_239[i] == pc) return 1;
    return 0;
}

semu_stop_reason semu_machine_run(semu_machine *machine,
                                  const semu_run_limits *limits,
                                  semu_error *error)
{
    const semu_cpu_state *state;
    uint64_t current_time;
    uint64_t executed = 0u;
    uint64_t elapsed = 0u;
    compat_gates gates;

    if (machine == NULL || limits == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid run arguments");
        return SEMU_STOP_USER;
    }
    state = semu_cpu_get_state(machine->cpu);
    if (state == NULL) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "machine CPU state is unavailable");
        machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
        return machine->stop_reason;
    }
    summarize_compat_gates(machine, &gates);
    current_time = semu_scheduler_now(machine->scheduler);
    machine->stop_reason = SEMU_STOP_NONE;
    while (machine->stop_reason == SEMU_STOP_NONE) {
        uint64_t before_instructions;
        uint64_t after_instructions;
        uint64_t instruction_delta;
        semu_status step_status;
        int was_waiting;

        if ((limits->max_instructions != 0u &&
             executed >= limits->max_instructions) ||
            (limits->max_virtual_time_ns != 0u &&
             elapsed >= limits->max_virtual_time_ns)) {
            machine->stop_reason = SEMU_STOP_BUDGET;
            break;
        }
        if (machine->input_poll != NULL && (executed & 4095u) == 0u) {
            machine->stop_reason = machine->input_poll(
                machine->input_poll_context, machine, error);
            if (machine->stop_reason != SEMU_STOP_NONE) break;
        }
        if (gates.any &&
            ((gates.device_235 &&
              (!gates.bindings_ok ||
               !semu_sapporo_devices_compat_idle(machine->devices,
                                                 state->r[15], error))) ||
             compat_gates_fire(&gates, state->r[15])) &&
            !apply_compat_hook(machine, state, error)) {
            machine->stop_reason = SEMU_STOP_COMPAT_REFUSED;
            break;
        }
        before_instructions = state->instructions;
        was_waiting = state->waiting_for_interrupt;
        step_status = semu_cpu_step(machine->cpu, error);
        after_instructions = state->instructions;
        if (after_instructions < before_instructions) {
            instruction_delta = UINT64_MAX;
        } else {
            instruction_delta = after_instructions - before_instructions;
        }
        if (after_instructions < before_instructions ||
            !update_run_accounting(machine, was_waiting, step_status,
                instruction_delta, &current_time, &executed, &elapsed)) {
            semu_error_set(error, SEMU_ERR_RANGE,
                           "machine run accounting overflow");
            machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
            continue;
        }
        if (step_status != SEMU_OK) {
            machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
            if (machine->stop_reason == SEMU_STOP_NONE) {
                machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
            }
        } else if (semu_cpu_reset_requested(machine->cpu)) {
            uint64_t virtual_time = machine->virtual_time_epoch + current_time;
            if (machine->reset_request_count == UINT64_MAX) {
                semu_error_set(error, SEMU_ERR_RANGE,
                               "machine reset request count overflow");
                machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
            } else {
                ++machine->reset_request_count;
                log_reset_request(machine, state, virtual_time);
                if (reset_after_request(machine, error) != SEMU_OK) {
                    machine->stop_reason = SEMU_STOP_DEVICE_REFUSED;
                }
                /* The reset unbinds the 2.35 device contexts; the
                 * gates must re-validate them before the fast path
                 * may skip the dispatch again. */
                summarize_compat_gates(machine, &gates);
                current_time = semu_scheduler_now(machine->scheduler);
            }
        } else {
            machine->stop_reason = semu_cpu_stop_reason(machine->cpu);
        }
        if (machine->logger != NULL) {
            machine->logger->virtual_time_ns = current_time;
        }
    }
    return machine->stop_reason;
}
