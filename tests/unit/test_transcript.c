#include "semu/display.h"
#include "semu/machine.h"
#include "semu/peripheral.h"
#include "test.h"
#include "transcript.h"

#include <string.h>

static semu_transaction_result transfer(void *context,
                                        semu_serial_transaction *transaction,
                                        semu_error *error)
{
    (void)context;
    (void)transaction;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void reset_callback(void *context)
{
    unsigned *calls = (unsigned *)context;
    ++*calls;
}

static void signal_callback(void *context, unsigned signal, int level)
{
    unsigned *value = (unsigned *)context;
    *value = signal * 2u + (level != 0 ? 1u : 0u);
}

static void completion_callback(void *context, semu_transaction_result result)
{
    semu_transaction_result *output =
        (semu_transaction_result *)context;
    *output = result;
}

static semu_transaction_result request_sink(
    void *context, const semu_dma_request *request, semu_error *error)
{
    unsigned *calls = (unsigned *)context;
    (void)error;
    if (request == NULL || request->endpoint == NULL) {
        return SEMU_TRANSACTION_REFUSE;
    }
    ++*calls;
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result display_backend(
    void *context, semu_bus *bus, const semu_display_list *lists,
    size_t count, uint64_t virtual_time_ns,
    semu_frame_callback frame_callback, void *frame_context,
    semu_error *error)
{
    unsigned *calls = (unsigned *)context;
    (void)bus;
    (void)lists;
    (void)count;
    (void)virtual_time_ns;
    (void)frame_callback;
    (void)frame_context;
    (void)error;
    ++*calls;
    return SEMU_TRANSACTION_OK;
}

static void display_finish(void *context) { (void)context; }

static void test_frozen_callbacks_and_options(semu_test_context *context)
{
    unsigned reset_calls = 0u;
    unsigned signal_value = 0u;
    unsigned sink_calls = 0u;
    unsigned backend_calls = 0u;
    semu_transaction_result completion = SEMU_TRANSACTION_REFUSE;
    semu_serial_endpoint endpoint = {"dma", transfer, NULL};
    semu_dma_request request = {
        "iom4", SEMU_DMA_TO_ENDPOINT, 0x1000u, 4u, &endpoint, 1u,
        NULL, completion_callback, &completion
    };
    semu_machine_options options;
    semu_error error;

    memset(&options, 0, sizeof(options));
    semu_error_clear(&error);
    {
        semu_peripheral_reset_fn reset = reset_callback;
        semu_peripheral_signal_fn signal = signal_callback;
        semu_dma_request_sink_fn sink = request_sink;
        const semu_display_backend_ops backend = {display_backend, display_finish, display_finish};
        const semu_display_list list = {0x2000u, 8u};
        reset(&reset_calls);
        signal(&signal_value, 3u, 1);
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         sink(&sink_calls, &request, &error));
        options.display_backend = &backend;
        options.display_backend_context = &backend_calls;
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                         options.display_backend->prepare(
                             options.display_backend_context, NULL, &list,
                             1u, 9u, NULL, NULL, &error));
        options.display_backend->commit(options.display_backend_context);
    }
    request.completion(request.completion_context, SEMU_TRANSACTION_OK);
    SEMU_TEST_EQ_U64(context, 1u, reset_calls);
    SEMU_TEST_EQ_U64(context, 7u, signal_value);
    SEMU_TEST_EQ_U64(context, 1u, sink_calls);
    SEMU_TEST_EQ_U64(context, 1u, backend_calls);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, completion);
    memset(&options, 0, sizeof(options));
    SEMU_TEST_ASSERT(context, options.display_backend == NULL &&
                     options.display_backend_context == NULL);
}

static void test_serial_round_trip(semu_test_context *context)
{
    uint8_t tx[] = {0x10u, 0x20u};
    uint8_t rx[] = {0xa0u, 0xb0u, 0xc0u};
    semu_serial_transaction transaction = {0x48u, 2u, tx, sizeof(tx),
                                           rx, sizeof(rx)};
    semu_test_transcript expected;
    semu_test_transcript actual;
    semu_test_transcript_mismatch difference;
    semu_error error;
    semu_serial_endpoint endpoint = {"legacy", transfer, NULL};

    semu_test_transcript_init(&expected);
    semu_test_transcript_init(&actual);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     endpoint.transfer(endpoint.context, &transaction, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record_serial(
                         &expected, &transaction, SEMU_TRANSACTION_OK, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &actual, SEMU_TEST_TRANSCRIPT_TX, 0x48u, 2u,
                         tx, sizeof(tx), SEMU_TRANSACTION_OK, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &actual, SEMU_TEST_TRANSCRIPT_RX, 0x48u, 2u,
                         rx, sizeof(rx), SEMU_TRANSACTION_OK, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_MATCH, difference.field);
}

static void test_first_byte_mismatch(semu_test_context *context)
{
    static const uint8_t expected_bytes[] = {0x01u, 0x02u, 0x03u};
    static const uint8_t actual_bytes[] = {0x01u, 0xffu, 0x03u};
    semu_test_transcript expected;
    semu_test_transcript actual;
    semu_test_transcript_mismatch difference;
    semu_error error;

    semu_test_transcript_init(&expected);
    semu_test_transcript_init(&actual);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &expected, SEMU_TEST_TRANSCRIPT_RX, 0x10u, 0u,
                         expected_bytes, sizeof(expected_bytes),
                         SEMU_TRANSACTION_WAIT, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &actual, SEMU_TEST_TRANSCRIPT_RX, 0x10u, 0u,
                         actual_bytes, sizeof(actual_bytes),
                         SEMU_TRANSACTION_WAIT, &error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_BYTES, difference.field);
    SEMU_TEST_EQ_U64(context, 0u, difference.entry_index);
    SEMU_TEST_EQ_U64(context, 1u, difference.byte_index);
    SEMU_TEST_ASSERT(context, strstr(error.text, "entry 0 byte 1") != NULL);
}

static void test_metadata_mismatch(semu_test_context *context)
{
    static const uint8_t bytes[] = {0x55u};
    semu_test_transcript expected;
    semu_test_transcript actual;
    semu_test_transcript_mismatch difference;
    semu_error error;

    semu_test_transcript_init(&expected);
    semu_test_transcript_init(&actual);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &expected, SEMU_TEST_TRANSCRIPT_TX, 0x11u, 2u,
                         bytes, sizeof(bytes), SEMU_TRANSACTION_OK, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_test_transcript_record(
                         &actual, SEMU_TEST_TRANSCRIPT_TX, 0x11u, 2u,
                         bytes, sizeof(bytes), SEMU_TRANSACTION_OK, &error));
    actual.entries[0].direction = SEMU_TEST_TRANSCRIPT_RX;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_DIRECTION, difference.field);
    actual.entries[0].direction = SEMU_TEST_TRANSCRIPT_TX;
    actual.entries[0].address = 0x12u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_ADDRESS, difference.field);
    actual.entries[0].address = 0x11u;
    actual.entries[0].chip_select = 3u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_CHIP_SELECT,
                     difference.field);
    actual.entries[0].chip_select = 2u;
    actual.entries[0].result = SEMU_TRANSACTION_REFUSE;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
                     semu_test_transcript_compare(&expected, &actual,
                                                  &difference, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_RESULT, difference.field);
}

static void test_refusal_and_bounds(semu_test_context *context)
{
    uint8_t bytes[SEMU_TEST_TRANSCRIPT_MAX_BYTES + 1u];
    semu_test_transcript transcript;
    semu_error error;
    size_t index;

    memset(bytes, 0, sizeof(bytes));
    semu_test_transcript_init(&transcript);
    semu_error_clear(&error);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_test_transcript_record(
                         &transcript, SEMU_TEST_TRANSCRIPT_TX, 0u, 0u, bytes,
                         sizeof(bytes), SEMU_TRANSACTION_OK, &error));
    SEMU_TEST_EQ_U64(context, 0u, transcript.count);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
                     semu_test_transcript_record(
                         &transcript, SEMU_TEST_TRANSCRIPT_TX, 0u, 0u, NULL,
                         1u, SEMU_TRANSACTION_OK, &error));
    for (index = 0u; index < SEMU_TEST_TRANSCRIPT_MAX_ENTRIES; ++index) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
                         semu_test_transcript_record(
                             &transcript, SEMU_TEST_TRANSCRIPT_TX, 0u, 0u,
                             NULL, 0u, SEMU_TRANSACTION_REFUSE, &error));
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     semu_test_transcript_record(
                         &transcript, SEMU_TEST_TRANSCRIPT_TX, 0u, 0u, NULL,
                         0u, SEMU_TRANSACTION_REFUSE, &error));
    SEMU_TEST_EQ_U64(context, SEMU_TEST_TRANSCRIPT_MAX_ENTRIES,
                     transcript.count);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_frozen_callbacks_and_options),
        SEMU_TEST_CASE(test_serial_round_trip),
        SEMU_TEST_CASE(test_first_byte_mismatch),
        SEMU_TEST_CASE(test_metadata_mismatch),
        SEMU_TEST_CASE(test_refusal_and_bounds)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
