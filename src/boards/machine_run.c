#include "machine_internal.h"

#include "../compat/sapporo_239.h"

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
    semu_log_set_time(machine->logger, virtual_time);
    semu_log_write(machine->logger, SEMU_LOG_WARNING, "cpu",
        "machine-reset-request",
        "pc=0x%08x lr=0x%08x sp=0x%08x r0=0x%08x "
        "r1=0x%08x r2=0x%08x r3=0x%08x xpsr=0x%08x "
        "reset_count=%llu compat_hits=%llu instructions=%llu "
        "virtual_time_ns=%llu",
        (unsigned)state->r[15], (unsigned)state->r[14],
        (unsigned)state->r[13], (unsigned)state->r[0],
        (unsigned)state->r[1], (unsigned)state->r[2],
        (unsigned)state->r[3], (unsigned)state->xpsr,
        (unsigned long long)machine->reset_request_count,
        (unsigned long long)(machine->layer_count != 0u
            ? machine->layers[0].hits : 0u),
        (unsigned long long)state->instructions,
        (unsigned long long)virtual_time);
}

static int apply_compat_hook(semu_machine *machine,
                             const semu_cpu_state *state,
                             semu_error *error)
{
    size_t i;

    for (i = 0u; i < machine->layer_count; ++i) {
        semu_layer_state *layer = &machine->layers[i];
        if (layer->descriptor == &semu_sapporo_222_no_device_layer &&
            semu_sapporo_devices_compat_hook_pc(state->r[15])) {
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
        }
    }
    return 1;
}

semu_stop_reason semu_machine_run(semu_machine *machine,
                                  const semu_run_limits *limits,
                                  semu_error *error)
{
    const semu_cpu_state *state;
    uint64_t current_time;
    uint64_t executed = 0u;
    uint64_t elapsed = 0u;

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
        if (!apply_compat_hook(machine, state, error)) {
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
