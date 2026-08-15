#include "sapporo_ohr2.h"
#include "test.h"

#include "semu/hash.h"

#include <string.h>

typedef struct ohr_fixture {
    unsigned ready_count;
    int ready_level[8];
    unsigned ready_signal[8];
} ohr_fixture;

static void ready_callback(void *context, unsigned signal, int level)
{
    ohr_fixture *fixture = (ohr_fixture *)context;
    if (fixture->ready_count < 8u) {
        fixture->ready_signal[fixture->ready_count] = signal;
        fixture->ready_level[fixture->ready_count] = level;
    }
    ++fixture->ready_count;
}

static semu_transaction_result body_provider(
    void *context, semu_sapporo_ohr2_command command, uint16_t sequence,
    semu_sapporo_ohr2_state state, const uint8_t request[54],
    uint8_t response[54], semu_error *error)
{
    (void)context;
    (void)sequence;
    (void)error;
    (void)memset(response, 0xa5, 54u);
    if (command == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY) {
        const char *name = state == SEMU_SAPPORO_OHR2_MAIN ? "MAIN" : "BSL";
        (void)memcpy(response + 9u, name, 4u);
        response[13u] = 0u;
    } else if (command == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
        (void)memcpy(response + 4u, request + 4u, 50u);
    }
    return SEMU_TRANSACTION_OK;
}

static void make_request(uint8_t request[59], uint16_t command,
                         uint16_t sequence)
{
    (void)memset(request, 0, 59u);
    request[1u] = (uint8_t)command;
    request[2u] = (uint8_t)(command >> 8u);
    request[3u] = (uint8_t)sequence;
    request[4u] = (uint8_t)(sequence >> 8u);
    request[55u] = 0u;
    request[56u] = 0u;
    request[57u] = 0u;
    request[58u] = 0u;
    {
        uint32_t crc = semu_sapporo_ohr2_crc32(request + 1u, 54u);
        request[55u] = (uint8_t)crc;
        request[56u] = (uint8_t)(crc >> 8u);
        request[57u] = (uint8_t)(crc >> 16u);
        request[58u] = (uint8_t)(crc >> 24u);
    }
}

static semu_transaction_result exchange(semu_serial_endpoint endpoint,
                                         uint8_t request[59], uint8_t response[58],
                                         semu_error *error)
{
    semu_serial_transaction transaction = {
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, 59u, NULL, 0u
    };
    semu_transaction_result result = endpoint.transfer(endpoint.context,
                                                       &transaction, error);
    if (result != SEMU_TRANSACTION_OK) {
        return result;
    }
    transaction.tx = (const uint8_t[]){ SEMU_SAPPORO_OHR2_RESPONSE_SELECTOR };
    transaction.tx_size = 1u;
    result = endpoint.transfer(endpoint.context, &transaction, error);
    if (result != SEMU_TRANSACTION_OK) {
        return result;
    }
    transaction.tx = NULL;
    transaction.tx_size = 0u;
    transaction.rx = response;
    transaction.rx_size = 58u;
    return endpoint.transfer(endpoint.context, &transaction, error);
}

static void test_packet_crc_and_ready(semu_test_context *context)
{
    ohr_fixture fixture = { 0u };
    semu_error error;
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint endpoint;
    uint8_t request[59];
    uint8_t response[58];
    semu_transaction_result result;

    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      body_provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, 0u, 1u);
    SEMU_TEST_EQ_U64(context, 0x5c6f91f0u,
                     semu_sapporo_ohr2_crc32(request + 1u, 54u));
    result = exchange(endpoint, request, response, &error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, result);
    SEMU_TEST_EQ_U64(context, 2u, fixture.ready_count);
    SEMU_TEST_EQ_U64(context, SEMU_SAPPORO_OHR2_READY_SIGNAL,
                     fixture.ready_signal[0]);
    SEMU_TEST_EQ_U64(context, 1u, fixture.ready_level[0]);
    SEMU_TEST_ASSERT(context, response[0] == 0u && response[1] == 0u);
    SEMU_TEST_ASSERT(context, response[2] == 1u && response[3] == 0u);
    SEMU_TEST_ASSERT(context, memcmp(response + 9u, "BSL\0", 4u) == 0);
    SEMU_TEST_ASSERT(context, semu_crc32(0u, response, 54u) ==
                     ((uint32_t)response[54u] |
                      ((uint32_t)response[55u] << 8u) |
                      ((uint32_t)response[56u] << 16u) |
                      ((uint32_t)response[57u] << 24u)));
    SEMU_TEST_EQ_U64(context, 2u, fixture.ready_count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.ready_level[1]);
    semu_sapporo_ohr2_destroy(device);
}

static void test_state_sequence_and_fire_forget(semu_test_context *context)
{
    ohr_fixture fixture = { 0u };
    semu_error error;
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    uint8_t request[59];
    uint8_t response[58];

    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      body_provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, 1u, 1u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    make_request(request, 3u, 2u);
    transaction = (semu_serial_transaction){
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, 59u, NULL, 0u
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    make_request(request, 0u, 2u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    SEMU_TEST_ASSERT(context, memcmp(response + 9u, "MAIN\0", 5u) == 0);
    make_request(request, 1u, 3u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    make_request(request, 6u, 3u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    semu_sapporo_ohr2_destroy(device);
}

static void test_refusals_reset_and_missing_body(semu_test_context *context)
{
    ohr_fixture fixture = { 0u };
    semu_error error;
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    uint8_t request[59];
    uint8_t response[58];

    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      NULL, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, 0u, 1u);
    transaction = (semu_serial_transaction){
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, 59u, NULL, 0u
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.ready_count);
    request[0u] = 1u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    semu_sapporo_ohr2_destroy(device);

    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      body_provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, 0u, 1u);
    transaction.tx = request;
    transaction.tx_size = 59u;
    transaction.rx = NULL;
    transaction.rx_size = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    make_request(request, 0u, 2u);
    transaction.tx_size = 59u;
    transaction.rx = response;
    transaction.rx_size = 58u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    semu_sapporo_ohr2_reset(device);
    SEMU_TEST_EQ_U64(context, 2u, fixture.ready_count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.ready_level[1]);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    semu_sapporo_ohr2_destroy(device);
}

static void test_sequence_refusal_and_probe_reset(semu_test_context *context)
{
    ohr_fixture fixture = { 0u };
    semu_error error;
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    uint8_t request[59];
    uint8_t response[58];

    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      body_provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, 0u, 1u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    transaction = (semu_serial_transaction){
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, 59u, NULL, 0u
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    make_request(request, 0u, 0u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    make_request(request, 1u, 1u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     exchange(endpoint, request, response, &error));
    make_request(request, 1u, 3u);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    semu_sapporo_ohr2_destroy(device);
}

static void test_sequence_overflow_is_atomic(semu_test_context *context)
{
    ohr_fixture fixture = { 0u };
    semu_error error;
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    uint8_t request[59];

    semu_error_clear(&error);
    device = semu_sapporo_ohr2_create(ready_callback, &fixture,
                                      body_provider, NULL, &error);
    SEMU_TEST_ASSERT(context, device != NULL);
    endpoint = semu_sapporo_ohr2_endpoint(device);
    make_request(request, SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, UINT16_MAX);
    transaction = (semu_serial_transaction){
        SEMU_SAPPORO_OHR2_ADDRESS, 0u, request, 59u, NULL, 0u
    };
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.ready_count);
    semu_sapporo_ohr2_destroy(device);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_packet_crc_and_ready),
        SEMU_TEST_CASE(test_state_sequence_and_fire_forget),
        SEMU_TEST_CASE(test_refusals_reset_and_missing_body),
        SEMU_TEST_CASE(test_sequence_refusal_and_probe_reset),
        SEMU_TEST_CASE(test_sequence_overflow_is_atomic)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
