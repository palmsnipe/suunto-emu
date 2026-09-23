#include "test.h"
#include "sapporo_235_ohr.h"
#include "../../src/boards/machine_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    semu_layer_state layer;
    semu_sapporo_235_ohr_context context;
    semu_logger logger;
    semu_error error;
    FILE *log;
    unsigned edges;
    int ready;
} fixture;

static int setup(fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    if (!f->log) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    f->context.state = &f->layer;
    f->context.logger = &f->logger;
    return semu_layer_enable_checked(&f->layer, &semu_sapporo_235_ohr_layer,
        "sapporo-2.35.34", semu_sapporo_235_ohr_layer.component_hashes,
        3u, &f->error) == SEMU_OK;
}

static void signal_ready(void *context, unsigned signal, int level)
{
    fixture *f = context;
    (void)signal;
    ++f->edges;
    f->ready = level;
}

static void make_body(uint8_t body[54], unsigned command, unsigned sequence)
{
    unsigned i;
    memset(body, 0xff, 54u);
    body[0] = (uint8_t)command; body[1] = 0u;
    body[2] = (uint8_t)sequence; body[3] = 0u;
    if (command == 16u) body[4] = 1u;
    if (command == 6u) for (i = 4u; i < 14u; ++i) body[i] = (uint8_t)i;
}

static void frame(uint8_t packet[59], unsigned command, unsigned sequence)
{
    uint32_t crc;
    unsigned i;
    packet[0] = 0u;
    make_body(packet + 1u, command, sequence);
    crc = semu_sapporo_ohr2_crc32(packet + 1u, 54u);
    for (i = 0u; i < 4u; ++i) packet[55u + i] = (uint8_t)(crc >> (8u * i));
}

static semu_transaction_result exchange(semu_serial_endpoint ep,
    const uint8_t *tx, size_t tx_size, uint8_t *rx, size_t rx_size,
    semu_error *error)
{
    semu_serial_transaction t;
    memset(&t, 0, sizeof(t));
    t.address = 0x10u; t.tx = tx; t.tx_size = tx_size;
    t.rx = rx; t.rx_size = rx_size;
    return ep.transfer(ep.context, &t, error);
}

static void test_sapporo_235_ohr_startup_transport(semu_test_context *c)
{
    const unsigned commands[] = {16u, 0u, 16u, 0u, 13u, 14u, 6u, 2u};
    const uint8_t selector = 0x3cu;
    uint8_t packet[59], response[58], expected[59];
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint ep;
    fixture f;
    unsigned i, j;
    char log[4096];
    SEMU_TEST_ASSERT(c, setup(&f));
    device = semu_sapporo_ohr2_create(signal_ready, &f,
        semu_sapporo_235_ohr_body_provider, &f.context, &f.error);
    SEMU_TEST_ASSERT(c, device != NULL);
    ep = semu_sapporo_ohr2_endpoint(device);
    for (i = 0u; i < 8u; ++i) {
        uint32_t crc;
        if (i == 2u) {
            frame(packet, 3u, 2u);
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
                exchange(ep, packet, 59u, NULL, 0u, &f.error));
            SEMU_TEST_EQ_U64(c, 0u, f.ready);
        }
        frame(packet, commands[i], i);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 1u, f.ready);
        SEMU_TEST_EQ_U64(c, i + 1u, f.layer.hits);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        memset(expected, 0, sizeof(expected));
        expected[0] = (uint8_t)commands[i]; expected[2] = (uint8_t)i;
        if (i == 1u) memcpy(expected + 9u, "BSL", 3u);
        if (i == 3u) memcpy(expected + 9u, "MAIN", 4u);
        if (i == 6u) memcpy(expected + 4u, packet + 5u, 50u);
        crc = semu_sapporo_ohr2_crc32(expected, 54u);
        for (j = 0u; j < 4u; ++j) expected[54u+j] = (uint8_t)(crc >> (8u*j));
        SEMU_TEST_ASSERT(c, memcmp(expected, response, 58u) == 0);
    }
    SEMU_TEST_EQ_U64(c, 16u, f.edges);
    frame(packet, 2u, 8u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 8u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 16u, f.edges);
    SEMU_TEST_ASSERT(c, f.context.refusal.code != SEMU_OK);
    rewind(f.log);
    j = (unsigned)fread(log, 1u, sizeof(log)-1u, f.log); log[j] = '\0';
    SEMU_TEST_ASSERT(c, strstr(log, "hit=8 maximum=8") != NULL);
    SEMU_TEST_ASSERT(c, strstr(log, "command=0002 sequence=7") != NULL);
    semu_sapporo_ohr2_destroy(device);
    fclose(f.log);
}

static void test_sapporo_235_ohr_atomic_body_refusals(semu_test_context *c)
{
    unsigned i, j;
    for (i = 0u; i < 8u; ++i) {
        fixture f;
        uint8_t body[54], response[54];
        semu_sapporo_ohr2_state state = SEMU_SAPPORO_OHR2_BSL;
        unsigned command = 16u, sequence = 0u;
        SEMU_TEST_ASSERT(c, setup(&f));
        make_body(body, command, sequence);
        memset(response, 0xa5, sizeof(response));
        switch (i) {
        case 0: f.layer.enabled = 0; break;
        case 1: state = SEMU_SAPPORO_OHR2_MAIN; break;
        case 2: sequence = 1u; break;
        case 3: command = 0u; break;
        case 4: body[4] = 2u; break;
        case 5: body[53] = 0u; break;
        case 6: body[1] = 1u; break;
        case 7: f.layer.hits = 8u; break;
        }
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
            semu_sapporo_235_ohr_body_provider(&f.context,
                (semu_sapporo_ohr2_command)command, (uint16_t)sequence,
                state, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, i == 7u ? 8u : 0u, f.layer.hits);
        for (j = 0u; j < 54u; ++j) SEMU_TEST_EQ_U64(c, 0xa5u, response[j]);
        fclose(f.log);
    }
}

static void test_sapporo_235_ohr_pins_and_owners(semu_test_context *c)
{
    fixture a, b;
    const char *hashes[3];
    uint8_t body[54], response[54];
    unsigned i;
    SEMU_TEST_ASSERT(c, setup(&a));
    SEMU_TEST_ASSERT(c, setup(&b));
    make_body(body, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
        semu_sapporo_235_ohr_body_provider(&a.context, 16u, 0u,
            SEMU_SAPPORO_OHR2_BSL, body, response, &a.error));
    SEMU_TEST_EQ_U64(c, 0u, b.layer.hits);
    SEMU_TEST_ASSERT(c, semu_layer_enable_checked(&b.layer,
        &semu_sapporo_235_ohr_layer, "sapporo-2.39.20",
        semu_sapporo_235_ohr_layer.component_hashes, 3u, &b.error) != SEMU_OK);
    for (i = 0u; i < 3u; ++i) hashes[i] = semu_sapporo_235_ohr_layer.component_hashes[i];
    hashes[1] = "wrong";
    SEMU_TEST_ASSERT(c, semu_layer_enable_checked(&b.layer,
        &semu_sapporo_235_ohr_layer, "sapporo-2.35.34", hashes, 3u, &b.error) != SEMU_OK);
    SEMU_TEST_EQ_U64(c, 0u, b.layer.enabled);
    SEMU_TEST_EQ_U64(c, 1u, a.layer.hits);
    fclose(a.log); fclose(b.log);
}

static void test_sapporo_235_ohr_machine_refusal_and_reset(semu_test_context *c)
{
    semu_machine machine;
    semu_run_limits limits = {1u, 1u};
    semu_serial_endpoint ep;
    const semu_serial_endpoint *mux;
    fixture f;
    uint8_t packet[59];
    SEMU_TEST_ASSERT(c, setup(&f));
    memset(&machine, 0, sizeof(machine));
    machine.logger = &f.logger;
    machine.layers[0] = f.layer; machine.layer_count = 1u;
    machine.bus = semu_bus_create(&f.error);
    machine.scheduler = semu_scheduler_create(&f.error);
    machine.cpu = semu_cpu_create(machine.bus, machine.scheduler, &f.error);
    machine.devices = semu_sapporo_devices_create(machine.scheduler, NULL, &f.error);
    SEMU_TEST_ASSERT(c, machine.bus && machine.scheduler && machine.cpu && machine.devices);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_select_profile(
        machine.devices, "sapporo-2.35.34", &f.error));
    mux = semu_sapporo_devices_iom_endpoint(machine.devices, 2u);
    SEMU_TEST_ASSERT(c, mux != NULL); ep = *mux;
    frame(packet, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error));
    SEMU_TEST_ASSERT(c, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error) != SEMU_OK);
    /* A valid-CRC but wrong startup sequence latches compatibility refusal. */
    frame(packet, 16u, 1u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_STOP_COMPAT_REFUSED,
        semu_machine_run(&machine, &limits, &f.error));
    SEMU_TEST_EQ_U64(c, 0u, semu_cpu_get_state(machine.cpu)->instructions);
    SEMU_TEST_EQ_U64(c, 0u, machine.reset_request_count);
    SEMU_TEST_EQ_U64(c, 0u, machine.layers[0].hits);
    semu_sapporo_devices_reset(machine.devices);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error));
    frame(packet, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 1u, machine.layers[0].hits);
    semu_sapporo_devices_destroy(machine.devices);
    semu_cpu_destroy(machine.cpu);
    semu_scheduler_destroy(machine.scheduler);
    semu_bus_destroy(machine.bus); fclose(f.log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_ohr_startup_transport),
        SEMU_TEST_CASE(test_sapporo_235_ohr_atomic_body_refusals),
        SEMU_TEST_CASE(test_sapporo_235_ohr_pins_and_owners),
        SEMU_TEST_CASE(test_sapporo_235_ohr_machine_refusal_and_reset)
    };
    return semu_test_run(cases, sizeof(cases)/sizeof(cases[0]));
}
