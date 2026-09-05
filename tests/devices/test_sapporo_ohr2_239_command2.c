#include "sapporo_ohr2_239.h"
#include "sapporo_222.h"
#include "test.h"

#include <string.h>

typedef struct ready_state { unsigned edges; int level; } ready_state;

static void ready(void *context, unsigned signal, int level)
{
    ready_state *state = context;
    (void)signal;
    ++state->edges;
    state->level = level;
}

static semu_transaction_result provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state, const uint8_t request[54],
    uint8_t response[54], semu_error *error)
{
    (void)context;
    return semu_sapporo_239_ohr_body_provider(
        command, sequence, state, request, response, error);
}

static void crc_request(uint8_t request[59])
{
    uint32_t crc = semu_sapporo_ohr2_crc32(request + 1u, 54u);
    size_t i;
    for (i = 0u; i < 4u; ++i) request[55u + i] = (uint8_t)(crc >> (8u * i));
}

static void make_request(uint8_t request[59], uint16_t command)
{
    memset(request, 0xff, 59u);
    request[0] = 0u;
    request[1] = (uint8_t)command;
    request[2] = (uint8_t)(command >> 8u);
    request[3] = 7u;
    request[4] = 0u;
    crc_request(request);
}

static semu_transaction_result transfer(semu_sapporo_ohr2 *device,
    const uint8_t *tx, size_t tx_size, uint8_t *rx, size_t rx_size)
{
    semu_error error;
    semu_serial_endpoint ep = semu_sapporo_ohr2_endpoint(device);
    semu_serial_transaction txn = {
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, tx, tx_size, rx, rx_size
    };
    semu_error_clear(&error);
    return ep.transfer(ep.context, &txn, &error);
}

static void assert_refusal(semu_test_context *context,
    semu_sapporo_ohr2 *device, const uint8_t *tx, size_t tx_size,
    size_t rx_size)
{
    semu_snapshot_writer before, after;
    semu_error error;
    uint8_t output[58], untouched[58];
    semu_error_clear(&error);
    memset(output, 0xa5, sizeof(output));
    memcpy(untouched, output, sizeof(output));
    semu_snapshot_writer_init(&before);
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_ohr2_snapshot_write(device, &before, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        transfer(device, tx, tx_size, output, rx_size));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_ohr2_snapshot_write(device, &after, &error));
    SEMU_TEST_EQ_U64(context, before.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(before.data, after.data, before.size) == 0);
    SEMU_TEST_ASSERT(context, memcmp(output, untouched, sizeof(output)) == 0);
    semu_snapshot_writer_destroy(&before);
    semu_snapshot_writer_destroy(&after);
}

static void enter_main(semu_test_context *context, semu_sapporo_ohr2 *device)
{
    uint8_t request[59];
    make_request(request, SEMU_SAPPORO_OHR2_COMMAND_REBOOT);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        transfer(device, request, sizeof(request), NULL, 0u));
}

static void test_command2_packet_and_snapshot(semu_test_context *context)
{
    semu_error error;
    ready_state signal = {0u, 0};
    semu_sapporo_ohr2 *device, *restored;
    semu_snapshot_writer queued, after;
    semu_snapshot_reader reader;
    uint8_t request[59], response[58], second[58], expected[58] = {0};
    uint8_t selector = SEMU_SAPPORO_OHR2_RESPONSE_SELECTOR;
    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready, &signal, provider, NULL, &error);
    restored = semu_sapporo_ohr2_create(NULL, NULL, provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL && restored != NULL);
    enter_main(context, device);
    make_request(request, 2u);
    SEMU_TEST_EQ_U64(context, 0x8cef3ba3u,
        semu_sapporo_ohr2_crc32(request + 1u, 54u));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        transfer(device, request, sizeof(request), NULL, 0u));
    SEMU_TEST_EQ_U64(context, 1u, signal.edges);
    SEMU_TEST_EQ_U64(context, 1u, signal.level);
    assert_refusal(context, device, request, sizeof(request), 0u);
    assert_refusal(context, device, &selector, 1u, 57u);
    assert_refusal(context, device, NULL, 0u, 58u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        transfer(device, &selector, 1u, NULL, 0u));
    semu_snapshot_writer_init(&queued);
    semu_snapshot_writer_init(&after);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_ohr2_snapshot_write(device, &queued, &error));
    semu_snapshot_reader_init(&reader, queued.data, queued.size);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_ohr2_snapshot_read(restored, &reader, &error));
    SEMU_TEST_ASSERT(context, semu_snapshot_reader_done(&reader));
    queued.data[0] = SEMU_SAPPORO_OHR2_BSL;
    semu_snapshot_reader_init(&reader, queued.data, queued.size);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_sapporo_ohr2_snapshot_read(restored, &reader, &error));
    queued.data[0] = SEMU_SAPPORO_OHR2_MAIN;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_sapporo_ohr2_snapshot_write(restored, &after, &error));
    SEMU_TEST_EQ_U64(context, queued.size, after.size);
    SEMU_TEST_ASSERT(context, memcmp(queued.data, after.data, queued.size) == 0);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        transfer(device, NULL, 0u, response, sizeof(response)));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        transfer(restored, NULL, 0u, second, sizeof(second)));
    expected[0] = 2u; expected[2] = 7u;
    expected[54] = 0xf3u; expected[55] = 0xccu;
    expected[56] = 0xffu; expected[57] = 0xf0u;
    SEMU_TEST_ASSERT(context, memcmp(expected, response, sizeof(response)) == 0);
    SEMU_TEST_ASSERT(context, memcmp(response, second, sizeof(response)) == 0);
    SEMU_TEST_EQ_U64(context, 2u, signal.edges);
    SEMU_TEST_EQ_U64(context, 0u, signal.level);
    assert_refusal(context, device, &selector, 1u, 58u);
    semu_sapporo_ohr2_reset(device);
    assert_refusal(context, device, request, sizeof(request), 0u);
    semu_snapshot_writer_destroy(&queued);
    semu_snapshot_writer_destroy(&after);
    semu_sapporo_ohr2_destroy(restored);
    semu_sapporo_ohr2_destroy(device);
}

static void test_command2_refusals(semu_test_context *context)
{
    semu_error error;
    ready_state signal = {0u, 0};
    semu_sapporo_ohr2 *device;
    uint8_t request[59];
    size_t i;
    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready, &signal, provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    make_request(request, 2u);
    assert_refusal(context, device, request, sizeof(request), 0u);
    enter_main(context, device);
    for (i = 5u; i < 55u; ++i) {
        make_request(request, 2u);
        request[i] = 0u;
        crc_request(request);
        assert_refusal(context, device, request, sizeof(request), 0u);
    }
    make_request(request, 2u);
    request[55] ^= 1u;
    assert_refusal(context, device, request, sizeof(request), 0u);
    make_request(request, 5u);
    assert_refusal(context, device, request, sizeof(request), 0u);
    make_request(request, 2u);
    request[3] = 0xffu; request[4] = 0xffu;
    crc_request(request);
    assert_refusal(context, device, request, sizeof(request), 0u);
    SEMU_TEST_EQ_U64(context, 0u, signal.edges);
    semu_sapporo_ohr2_destroy(device);
}

static void test_command2_provider_isolation(semu_test_context *context)
{
    semu_sapporo_ohr2_body_provider_fn providers[] = {
        NULL, semu_sapporo_222_ohr_body_provider
    };
    semu_error error;
    uint8_t request[59];
    size_t i;
    semu_error_clear(&error);
    make_request(request, 2u);
    for (i = 0u; i < SEMU_ARRAY_LEN(providers); ++i) {
        semu_sapporo_ohr2 *device = semu_sapporo_ohr2_create(
            NULL, NULL, providers[i], NULL, &error);
        SEMU_TEST_ASSERT(context, device != NULL);
        enter_main(context, device);
        assert_refusal(context, device, request, sizeof(request), 0u);
        semu_sapporo_ohr2_destroy(device);
    }
}

int main(void)
{
    const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_command2_packet_and_snapshot),
        SEMU_TEST_CASE(test_command2_refusals),
        SEMU_TEST_CASE(test_command2_provider_isolation)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
