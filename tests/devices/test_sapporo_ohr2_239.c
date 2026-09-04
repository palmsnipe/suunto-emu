#include "sapporo_ohr2_239.h"
#include "test.h"

#include <string.h>

static void make_request_payload(uint8_t payload[54], uint16_t command,
                                 uint16_t sequence, uint8_t fill)
{
    memset(payload, fill, 54u);
    payload[0u] = (uint8_t)command;
    payload[1u] = (uint8_t)(command >> 8u);
    payload[2u] = (uint8_t)sequence;
    payload[3u] = (uint8_t)(sequence >> 8u);
}

static void test_bsl_identity(semu_test_context *context)
{
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_IDENTITY,
                         1u, 0xffu);
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, 1u,
            SEMU_SAPPORO_OHR2_BSL, request, response, &error));
    for (index = 0u; index < sizeof(response); ++index) {
        uint8_t expected = 0u;
        if (index == 9u) expected = 'B';
        if (index == 10u) expected = 'S';
        if (index == 11u) expected = 'L';
        SEMU_TEST_EQ_U64(context, expected, response[index]);
    }
}

static void test_main_identity(semu_test_context *context)
{
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_IDENTITY,
                         3u, 0xffu);
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, 3u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    for (index = 0u; index < sizeof(response); ++index) {
        uint8_t expected = 0u;
        if (index == 9u) expected = 'M';
        if (index == 10u) expected = 'A';
        if (index == 11u) expected = 'I';
        if (index == 12u) expected = 'N';
        SEMU_TEST_EQ_U64(context, expected, response[index]);
    }
}

static void test_result_13(semu_test_context *context)
{
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_RESULT_13,
                         4u, 0xffu);
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_13, 4u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    for (index = 0u; index < sizeof(response); ++index) {
        SEMU_TEST_EQ_U64(context, 0u, response[index]);
    }
}

static void test_result_14(semu_test_context *context)
{
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_RESULT_14,
                         5u, 0xffu);
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_14, 5u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    for (index = 0u; index < sizeof(response); ++index) {
        SEMU_TEST_EQ_U64(context, 0u, response[index]);
    }
}

static void test_identity_refusals_are_atomic(semu_test_context *context)
{
    static const semu_sapporo_ohr2_state states[] = {
        SEMU_SAPPORO_OHR2_BSL, SEMU_SAPPORO_OHR2_MAIN
    };
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t state_index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_IDENTITY,
                         1u, 0xffu);
    request[53u] = 0u;
    for (state_index = 0u; state_index < SEMU_ARRAY_LEN(states);
         ++state_index) {
        memset(response, 0xa5, sizeof(response));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
            semu_sapporo_239_ohr_body_provider(
                SEMU_SAPPORO_OHR2_COMMAND_IDENTITY, 1u,
                states[state_index], request, response, &error));
        SEMU_TEST_EQ_U64(context, 0xa5u, response[0u]);
        SEMU_TEST_EQ_U64(context, 0xa5u, response[53u]);
    }
}

static void test_result_refusals_are_atomic(semu_test_context *context)
{
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_RESULT_13,
                         4u, 0xffu);
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_13, 4u,
            SEMU_SAPPORO_OHR2_BSL, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[0u]);
    request[53u] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_13, 4u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[53u]);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_RESULT_14,
                         5u, 0xffu);
    request[53u] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_14, 5u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[0u]);
    request[53u] = 0xffu;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_RESULT_14, 5u,
            SEMU_SAPPORO_OHR2_BSL, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[53u]);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_ECHO,
                         6u, 0xffu);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_ECHO, 6u,
            SEMU_SAPPORO_OHR2_MAIN, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[0u]);
}

static void test_boot_mode_body_is_retained(semu_test_context *context)
{
    static const semu_sapporo_ohr2_state states[] = {
        SEMU_SAPPORO_OHR2_BSL, SEMU_SAPPORO_OHR2_MAIN
    };
    semu_error error;
    uint8_t request[54];
    uint8_t response[54];
    size_t state_index;
    size_t byte_index;

    semu_error_clear(&error);
    make_request_payload(request, SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE,
                         0u, 0xffu);
    request[4u] = 1u;
    for (state_index = 0u; state_index < SEMU_ARRAY_LEN(states);
         ++state_index) {
        memset(response, 0xa5, sizeof(response));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
            semu_sapporo_239_ohr_body_provider(
                SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 0u,
                states[state_index], request, response, &error));
        for (byte_index = 0u; byte_index < sizeof(response); ++byte_index) {
            SEMU_TEST_EQ_U64(context, 0u, response[byte_index]);
        }
    }
    request[4u] = 2u;
    memset(response, 0xa5, sizeof(response));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_239_ohr_body_provider(
            SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 0u,
            SEMU_SAPPORO_OHR2_BSL, request, response, &error));
    SEMU_TEST_EQ_U64(context, 0xa5u, response[0u]);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_bsl_identity),
        SEMU_TEST_CASE(test_main_identity),
        SEMU_TEST_CASE(test_result_13),
        SEMU_TEST_CASE(test_result_14),
        SEMU_TEST_CASE(test_identity_refusals_are_atomic),
        SEMU_TEST_CASE(test_result_refusals_are_atomic),
        SEMU_TEST_CASE(test_boot_mode_body_is_retained)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
