#include "ulsan_244_gps.h"

#include <string.h>

#define OPEN_PC 0x0012c128u
#define AWAKE_PC 0x0012c628u
#define PERIODIC_PC 0x0012c79cu
#define AWAKE_PIN 24u
#define GPIO_INTERRUPT_ENABLE (SEMU_APOLLO4_GPIO_BASE + 0x2c0u)
#define SRAM_START 0x10000000u
#define SRAM_END 0x10267000u

enum { RESET, AWAIT_VER, VER_DONE, AWAIT_GSR, GSR_DONE, AWAKE };

static const char *const hashes[] = {
    "3ba7a5b312a2fd4fdb4c327270ec66888f3fc03940bf8b5184233dad221369d9",
    "64827b1c4653464e5d15de9b3efd01243193604b24891b651f28f5c6da6d34ef",
    "81f861d4541d5c9f0e6b0cb73c2b3e44197d5abc6f147e1de8005da9af87f524"
};

const semu_layer_descriptor semu_ulsan_244_gps_layer = {
    .id = "ulsan-2.44-gps-startup",
    .kind = SEMU_LAYER_DEVICE_FIXTURE,
    .profile_id = "ulsan-2.44.52",
    .evidence = "E-ULS-0088/E-ULS-0090",
    .component_hashes = hashes,
    .component_hash_count = SEMU_ARRAY_LEN(hashes),
    .maximum_hits = 36u
};

static semu_status fail(semu_ulsan_244_gps *gps, semu_error *error,
                       const char *reason)
{
    if (gps->refusal.code == SEMU_OK)
        semu_error_set(&gps->refusal, SEMU_ERR_STATE, "Race S GPS: %s", reason);
    if (error != NULL) *error = gps->refusal;
    return gps->refusal.code;
}

static semu_status signal(semu_ulsan_244_gps *gps, const char *effect,
                          int awake, semu_error *error)
{
    static const uint8_t status[] = "$PSS0000\r\n";
    semu_error failure;
    semu_status result;

    if (gps->layer == NULL || !gps->layer->enabled ||
        gps->layer->descriptor != &semu_ulsan_244_gps_layer ||
        gps->layer->hits >= semu_ulsan_244_gps_layer.maximum_hits)
        return fail(gps, error, "layer disabled or event budget exhausted");
    if (gps->refusal.code != SEMU_OK)
        return fail(gps, error, "earlier event refused");
    result = awake ? semu_sapporo_cxd5610_pulse_awake_after(
        gps->transport, UINT64_C(100000000), &failure) :
        semu_sapporo_cxd5610_inject_rx_after(gps->transport, status,
            sizeof(status) - 1u, UINT64_C(10000000), &failure);
    if (result != SEMU_OK) {
        gps->refusal = failure;
        if (error != NULL) *error = failure;
        return result;
    }
    return semu_layer_hit(gps->layer, gps->logger, effect, error);
}

static semu_transaction_result exchange(void *context, const uint8_t *request,
    size_t count, semu_sapporo_cxd5610 *transport, semu_error *error)
{
    semu_ulsan_244_gps *gps = context;
    const char *effect;
    (void)transport;

    if (gps->stage == AWAIT_VER && count == 6u && memcmp(request, "@VER\r\n", 6u) == 0)
        effect = "trigger=ver-status synthetic-rx delay-ns=10000000";
    else if (gps->stage == AWAIT_GSR && count == 6u && memcmp(request, "@GSR\r\n", 6u) == 0)
        effect = "trigger=gsr-status synthetic-rx delay-ns=10000000";
    else {
        (void)fail(gps, error, "unexpected UART request or lifecycle");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (signal(gps, effect, 0, error) != SEMU_OK) return SEMU_TRANSACTION_REFUSE;
    gps->stage = gps->stage == AWAIT_VER ? VER_DONE : GSR_DONE;
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result transmit(void *context, uint8_t value, semu_error *error)
{
    semu_ulsan_244_gps *gps = context;
    semu_serial_endpoint endpoint = semu_sapporo_cxd5610_endpoint(gps->transport);
    semu_serial_transaction t = {0u, 0u, &value, 1u, NULL, 0u};
    return endpoint.transfer(endpoint.context, &t, error);
}

static void receive(void *context, uint8_t value, uint64_t now)
{
    semu_ulsan_244_gps *gps = context;
    semu_error error;
    (void)now;
    if (semu_apollo4_uart_schedule_rx(gps->uart, 0u, &value, 1u, &error) != SEMU_OK)
        (void)semu_scheduler_callback_fail(gps->scheduler, &error);
}

static void awake_pin(void *context, unsigned signal_index, int level)
{
    semu_ulsan_244_gps *gps = context;
    semu_error error;
    (void)signal_index;
    if (semu_apollo4_gpio_set_input(gps->gpio, AWAKE_PIN, level, &error) != SEMU_OK)
        (void)semu_scheduler_callback_fail(gps->scheduler, &error);
}

semu_status semu_ulsan_244_gps_bind(semu_ulsan_244_gps *gps, semu_bus *bus,
    semu_apollo4_gpio *gpio, semu_scheduler *scheduler, semu_apollo4_uart_irq_fn irq,
    void *irq_context, semu_layer_state *layer, semu_logger *logger, semu_error *error)
{
    const semu_apollo4_uart_endpoint endpoint = {"race-s-cxd5610", transmit, gps};
    semu_status result;

    gps->gpio = gpio;
    gps->layer = layer;
    gps->logger = logger;
    gps->scheduler = scheduler;
    gps->uart = semu_apollo4_uart_create(scheduler, error);
    if (gps->uart == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
    gps->transport = semu_sapporo_cxd5610_create(scheduler, awake_pin, gps,
        receive, gps, NULL, NULL, error);
    if (gps->transport == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
    semu_sapporo_cxd5610_set_exchange(gps->transport, exchange, gps);
    result = semu_apollo4_uart_attach_endpoint(gps->uart, &endpoint, error);
    if (result == SEMU_OK)
        result = semu_apollo4_uart_set_irq_sink(gps->uart, irq, irq_context, error);
    if (result == SEMU_OK)
        result = semu_apollo4_gpio_set_irq_sink(gpio, irq, irq_context, error);
    if (result != SEMU_OK) return result;
    return semu_bus_map_device(bus, "ulsan.uart1", SEMU_APOLLO4_UART1_BASE, 0x1000u,
                               semu_apollo4_uart_bus_ops(), gps->uart, error);
}

int semu_ulsan_244_gps_hook_pc(uint32_t pc)
{
    return pc == OPEN_PC || pc == AWAKE_PC || pc == PERIODIC_PC;
}

semu_status semu_ulsan_244_gps_observe(semu_ulsan_244_gps *gps, semu_bus *bus,
    const semu_cpu_state *cpu, semu_error *error)
{
    const uint32_t pc = cpu->r[15];
    semu_status status;

    if (pc == OPEN_PC) {
        uint32_t driver = cpu->r[4];
        uint32_t uart;
        uint8_t state;
        if (driver < SRAM_START || driver > SRAM_END - 0x6a3u ||
            (driver & 3u) != 0u ||
            semu_bus_read(bus, driver + 0x650u, 4u, &uart, error) != SEMU_OK ||
            semu_bus_copy_out(bus, driver + 0x6a2u, &state, 1u, error) != SEMU_OK ||
            uart < SRAM_START || uart >= SRAM_END || (uart & 3u) != 0u ||
            !((state == 0u && gps->stage == RESET) || (state == 19u && gps->stage == VER_DONE)))
            return fail(gps, error, "native UART open state refused");
        status = signal(gps, state == 0u ?
            "trigger=first-open-status synthetic-rx delay-ns=10000000" :
            "trigger=reopen-status synthetic-rx delay-ns=10000000", 0, error);
        if (status == SEMU_OK) gps->stage = state == 0u ? AWAIT_VER : AWAIT_GSR;
        return status;
    }
    if ((pc == AWAKE_PC && gps->stage == GSR_DONE) ||
        (pc == PERIODIC_PC && gps->stage == AWAKE)) {
        uint32_t config, enabled;
        if (semu_bus_read(bus, SEMU_APOLLO4_GPIO_BASE + 4u * AWAKE_PIN, 4u,
                          &config, error) != SEMU_OK || config != 0x93u ||
            semu_bus_read(bus, GPIO_INTERRUPT_ENABLE, 4u, &enabled, error) != SEMU_OK ||
            (enabled & (UINT32_C(1) << AWAKE_PIN)) == 0u)
            return fail(gps, error, "GPIO24 rising interrupt is not enabled");
        status = signal(gps, "trigger=gps-awake synthetic-gpio24 delay-ns=100000000", 1, error);
        if (status == SEMU_OK) gps->stage = AWAKE;
        return status;
    }
    return fail(gps, error, "unexpected awake lifecycle");
}

void semu_ulsan_244_gps_reset(semu_ulsan_244_gps *gps)
{
    semu_apollo4_uart_reset(gps->uart);
    semu_sapporo_cxd5610_reset(gps->transport);
    semu_error_clear(&gps->refusal);
    gps->stage = RESET;
}

void semu_ulsan_244_gps_destroy(semu_ulsan_244_gps *gps)
{
    semu_sapporo_cxd5610_destroy(gps->transport);
    semu_apollo4_uart_destroy(gps->uart);
}
