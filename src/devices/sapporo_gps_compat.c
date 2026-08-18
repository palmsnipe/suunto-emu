#include "sapporo_gps_compat.h"

/* E-SAP-COMPAT-GPS-004: the OTA-only run reaches the same second-open UART
 * register boundary as the native run, but the firmware's allocator-backed
 * wrapper is not reconstructible without the factory runtime.  Reproduce the
 * observed UART1 writes and its small event object before resuming at the
 * following firmware branch.
 */
static semu_status gps_uart_reopen_boundary(
    semu_apollo4_uart *uart, semu_bus *bus, uint32_t driver,
    semu_error *error)
{
    static const struct {
        uint32_t offset;
        uint32_t value;
    } writes[] = {
        { SEMU_APOLLO4_UART_CONTROL, 0u },
        { SEMU_APOLLO4_UART_CONTROL, 0x8u },
        { SEMU_APOLLO4_UART_CONTROL, 0x18u },
        { SEMU_APOLLO4_UART_CONTROL, 0x18u },
        { SEMU_APOLLO4_UART_CONTROL, 0x18u },
        { SEMU_APOLLO4_UART_CONTROL, 0x18u },
        { SEMU_APOLLO4_UART_INTEGER_BAUD, 1u },
        { SEMU_APOLLO4_UART_FRACTIONAL_BAUD, 0x28u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_LINE_CONTROL, 0x70u },
        { SEMU_APOLLO4_UART_FIFO_LEVEL, 0u },
        { SEMU_APOLLO4_UART_FIFO_LEVEL, 0u },
        { SEMU_APOLLO4_UART_CONTROL, 0x19u },
        { SEMU_APOLLO4_UART_CONTROL, 0x219u },
        { SEMU_APOLLO4_UART_CONTROL, 0x319u },
        { SEMU_APOLLO4_UART_INTERRUPT_MASK, 0x51u }
    };
    static const uint32_t hardware = UINT32_C(0x1004fbcc);
    static const uint32_t table = UINT32_C(0x1004efd8);
    static const uint32_t hardware_tag = UINT32_C(0x01ea9e06);
    static const uint32_t rx_callback = UINT32_C(0x0010f2df);
    static const uint32_t object_address = UINT32_C(0x1017ff00);
    static const uint32_t wait_address = UINT32_C(0x1017ff40);
    uint32_t object;
    uint32_t offset;
    size_t index;

    if (uart == NULL || bus == NULL) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "GPS UART reopen boundary has no attached UART");
        return SEMU_ERR_STATE;
    }
    for (index = 0u; index < sizeof(writes) / sizeof(writes[0]); ++index) {
        semu_status status = semu_apollo4_uart_write(
            uart, writes[index].offset, 4u, writes[index].value, error);
        if (status != SEMU_OK) return status;
    }
    object = object_address;
    for (offset = 0u; offset <= 0x44u; offset += 4u) {
        if (semu_bus_write(bus, wait_address + offset, 4u, 0u, error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
    }
    /* The firmware immediately inserts a timed wait into the list embedded at
       wait_address + 0x24.  Its observed intrusive-list routine starts at
       the sentinel (list + 0x08), so an empty list must be self-linked and
       carry the maximum deadline. */
    if (semu_bus_write(bus, wait_address + 0x24u, 4u, 0u, error) != SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x2cu, 4u, UINT32_MAX, error) !=
            SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x30u, 4u,
                       wait_address + 0x2cu, error) != SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x34u, 4u,
                       wait_address + 0x2cu, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_bus_write(bus, wait_address + 0x00u, 4u, wait_address, error) !=
            SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x04u, 4u, wait_address, error) !=
            SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x08u, 4u, wait_address, error) !=
            SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x0cu, 4u, wait_address, error) !=
            SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x3cu, 4u, 1u, error) != SEMU_OK ||
        semu_bus_write(bus, wait_address + 0x44u, 4u, UINT32_MAX, error) !=
            SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (semu_bus_write(bus, object + 0x00u, 4u, 1u, error) != SEMU_OK ||
        semu_bus_write(bus, object + 0x04u, 4u, rx_callback, error) != SEMU_OK ||
        semu_bus_write(bus, object + 0x08u, 4u, 0u, error) != SEMU_OK ||
        semu_bus_write(bus, object + 0x0cu, 4u, hardware, error) != SEMU_OK ||
        semu_bus_write(bus, object + 0x10u, 4u, wait_address, error) !=
            SEMU_OK ||
        semu_bus_write(bus, object + 0x14u, 4u, 0u, error) != SEMU_OK ||
        semu_bus_write(bus, table + 0x04u, 4u, object, error) != SEMU_OK ||
        semu_bus_write(bus, hardware + 0x00u, 4u, hardware_tag, error) != SEMU_OK ||
        semu_bus_write(bus, hardware + 0x24u, 4u, 1u, error) != SEMU_OK ||
        semu_bus_write(bus, driver + 0x1a0u, 4u, object, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

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
    running_status_trigger =
        cpu_state->r[15] == UINT32_C(0x0010f7c2);
    if (cpu_state->r[15] == UINT32_C(0x0010f7b8) &&
        cpu_state->r[0] == cpu_state->r[4] &&
        cpu_state->r[5] == cpu_state->r[4] + UINT32_C(0x74) &&
        cpu_state->r[6] == cpu_state->r[4] + UINT32_C(0x1ed) &&
        cpu_state->r[4] >= UINT32_C(0x10000000) &&
        cpu_state->r[4] < UINT32_C(0x10180000)) {
        uint32_t mode_flags = 0u;
        semu_error read_error;
        semu_error_clear(&read_error);
        if (semu_bus_read(bus, cpu_state->r[5] + 5u, 1u,
                          &mode_flags, &read_error) != SEMU_OK) {
            if (error != NULL) *error = read_error;
            return error != NULL ? error->code : SEMU_ERR_RANGE;
        }
        running_status_trigger = mode_flags == 2u;
    }
    if (!running_status_trigger) return SEMU_OK;
    if (semu_sapporo_222_arm_gps_running_status(
            gps, fixture_context, error) != SEMU_OK) {
        return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    if (cpu_state->r[15] == UINT32_C(0x0010f7b8)) {
        if (semu_bus_write(bus, cpu_state->r[4] + 0x1a0u, 4u, 0u, error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        if (gps_uart_reopen_boundary(uart, bus, cpu_state->r[4], error) !=
            SEMU_OK) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        cpu_state->r[15] = UINT32_C(0x0010f7bc);
    }
    return SEMU_OK;
}
