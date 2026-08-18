#include "sapporo_gps_compat.h"

semu_status semu_sapporo_gps_compat_apply(
    semu_apollo4_uart *uart, semu_bus *bus, semu_cpu_state *cpu_state,
    semu_sapporo_cxd5610 *gps,
    semu_sapporo_222_fixture_context *fixture_context, semu_error *error)
{
    int running_status_trigger;

    if (uart == NULL || bus == NULL || cpu_state == NULL || gps == NULL ||
        fixture_context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "GPS compatibility boundary is incomplete");
        return SEMU_ERR_ARGUMENT;
    }
    if (cpu_state->r[15] == UINT32_C(0x0010fbde)) {
        if (semu_sapporo_222_arm_gps_awake_pulse(
                gps, fixture_context, error) != SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        return SEMU_OK;
    }
    if (fixture_context->gps_running_status_armed) return SEMU_OK;
    running_status_trigger = 0;
    if (cpu_state->r[15] == UINT32_C(0x0010f7c2) &&
        cpu_state->r[0] == 1u &&
        cpu_state->r[5] == cpu_state->r[4] + UINT32_C(0x74) &&
        cpu_state->r[6] == cpu_state->r[4] + UINT32_C(0x1ed) &&
        cpu_state->r[4] >= UINT32_C(0x10000000) &&
        cpu_state->r[4] < UINT32_C(0x10180000)) {
        uint32_t mode_flags = 0u;
        uint32_t uart_object = 0u;
        semu_error read_error;
        semu_error_clear(&read_error);
        if (semu_bus_read(bus, cpu_state->r[5] + 5u, 1u,
                          &mode_flags, &read_error) != SEMU_OK) {
            if (error != NULL) *error = read_error;
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        semu_error_clear(&read_error);
        if (semu_bus_read(bus, cpu_state->r[4] + 0x1a0u, 4u,
                          &uart_object, &read_error) != SEMU_OK) {
            if (error != NULL) *error = read_error;
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        running_status_trigger = mode_flags == 2u && uart_object != 0u;
    }
    if (!running_status_trigger) return SEMU_OK;
    if (semu_sapporo_222_arm_gps_running_status(
            gps, fixture_context, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    return SEMU_OK;
}
