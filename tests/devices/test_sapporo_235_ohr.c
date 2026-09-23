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

static size_t parse_hex(const char *hex, uint8_t *out, size_t max)
{
    size_t i;
    for (i = 0u; i < max && hex[i * 2u] != '\0' && hex[i * 2u + 1u] != '\0'; ++i) {
        unsigned byte = 0u, j;
        for (j = 0u; j < 2u; ++j) {
            const char ch = hex[i * 2u + j];
            unsigned digit;
            if (ch >= '0' && ch <= '9') digit = (unsigned)(ch - '0');
            else if (ch >= 'a' && ch <= 'f') digit = (unsigned)(ch - 'a') + 10u;
            else return 0u;
            byte = (byte << 4u) | digit;
        }
        out[i] = (uint8_t)byte;
    }
    return i;
}

/* E-SAP-0041-EXT: the post-Done MAIN-state query round. The five request
 * frames below are the byte-exact packets an in-tree OHR2 byte census
 * captured from the five-layer 2.35.34 setup-walk at the point where the
 * eight-hit startup budget was exhausted, and each listed response frame was
 * reproduced twice byte-identically by the read-only lane endpoint. Only the
 * eight pinned startup frames plus these five may be answered: the sixth
 * post-Done request (command 0x0004 sequence 13, the first payload-carrying
 * command outside the observed set) must keep refusing. */
static const char *const ext_requests[] = {
    /* command 0x0010 sequence 8 */
    "001000080001ffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffb7e4559f",
    /* command 0x0000 sequence 9 */
    "0000000900ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff4a8afad8",
    /* command 0x000e sequence 10 */
    "000e000a00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff59365f6a",
    /* command 0x0006 sequence 11 */
    "0006000b003ad7e883d50100000000ffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff88f916fe",
    /* command 0x0002 sequence 12 */
    "0002000c00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff5ed33fce",
};

static const char *const ext_responses[] = {
    /* command 0x0010 sequence 8 */
    "1000080000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000771a7cd7",
    /* command 0x0000 sequence 9 */
    "0000090000000000004d41494e00000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000440b375d",
    /* command 0x000e sequence 10 */
    "0e000a0000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000009c14f16",
    /* command 0x0006 sequence 11 */
    "06000b003ad7e883d50100000000ffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffff88f916fe",
    /* command 0x0002 sequence 12 */
    "02000c0000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000e242fb2",
};

static const char *const ext_unknown_request =
    /* command 0x0004 sequence 13 */
    "0004000d00230000000000000000000000000000ffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff276ae026";

static void test_sapporo_235_ohr_post_done_queries(semu_test_context *c)
{
    const unsigned commands[] = {16u, 0u, 16u, 0u, 13u, 14u, 6u, 2u};
    const uint8_t selector = 0x3cu;
    uint8_t packet[59], response[58], expected[59];
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint ep;
    fixture f;
    unsigned i, j;
    char log[8192];
    SEMU_TEST_ASSERT(c, setup(&f));
    device = semu_sapporo_ohr2_create(signal_ready, &f,
        semu_sapporo_235_ohr_body_provider, &f.context, &f.error);
    SEMU_TEST_ASSERT(c, device != NULL);
    ep = semu_sapporo_ohr2_endpoint(device);
    for (i = 0u; i < 8u; ++i) {
        if (i == 2u) {
            frame(packet, 3u, 2u);
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
                exchange(ep, packet, 59u, NULL, 0u, &f.error));
        }
        frame(packet, commands[i], i);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
    }
    SEMU_TEST_EQ_U64(c, 8u, f.layer.hits);
    for (i = 0u; i < 5u; ++i) {
        SEMU_TEST_EQ_U64(c, 59u, parse_hex(ext_requests[i], packet, 59u));
        SEMU_TEST_EQ_U64(c, 58u, parse_hex(ext_responses[i], expected, 58u));
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 1u, f.ready);
        SEMU_TEST_EQ_U64(c, 9u + i, f.layer.hits);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        SEMU_TEST_ASSERT(c, memcmp(expected, response, 58u) == 0);
    }
    SEMU_TEST_EQ_U64(c, 13u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 26u, f.edges);
    rewind(f.log);
    j = (unsigned)fread(log, 1u, sizeof(log) - 1u, f.log); log[j] = '\0';
    SEMU_TEST_ASSERT(c, strstr(log, "hit=13 maximum=13") != NULL);
    SEMU_TEST_ASSERT(c, strstr(log, "command=0002 sequence=12") != NULL);

    /* Refusal case: the next observed post-Done request is outside the
     * pinned round, so it fails closed without consuming budget. */
    SEMU_TEST_EQ_U64(c, 59u, parse_hex(ext_unknown_request, packet, 59u));
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 0u, f.ready);
    SEMU_TEST_EQ_U64(c, 13u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 26u, f.edges);
    /* A known command out of its turn refuses too, again without a hit. */
    frame(packet, 2u, 13u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 0u, f.ready);
    SEMU_TEST_EQ_U64(c, 13u, f.layer.hits);
    semu_sapporo_ohr2_destroy(device);
    fclose(f.log);
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
    SEMU_TEST_ASSERT(c, strstr(log, "hit=8 maximum=13") != NULL);
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
        case 7: f.layer.hits = 13u; break;
        }
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
            semu_sapporo_235_ohr_body_provider(&f.context,
                (semu_sapporo_ohr2_command)command, (uint16_t)sequence,
                state, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, i == 7u ? 13u : 0u, f.layer.hits);
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
        SEMU_TEST_CASE(test_sapporo_235_ohr_post_done_queries),
        SEMU_TEST_CASE(test_sapporo_235_ohr_machine_refusal_and_reset)
    };
    return semu_test_run(cases, sizeof(cases)/sizeof(cases[0]));
}
