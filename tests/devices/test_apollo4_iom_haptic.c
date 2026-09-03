#include "test.h"

#include <stdint.h>

#include "semu/peripheral.h"
#include "../../src/soc/apollo4/iom.h"

typedef struct probe_log {
    uint8_t expected_address;
    size_t expected_tx_size;
    uint8_t expected_selector;
    unsigned calls;
} probe_log;

static semu_transaction_result probe_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    probe_log *log = (probe_log *)context;

    if (transaction == NULL || transaction->address != log->expected_address ||
        transaction->tx_size != log->expected_tx_size ||
        (log->expected_tx_size != 0u &&
         (transaction->tx == NULL ||
          transaction->tx[0u] != log->expected_selector))) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "unexpected IOM haptic selector transaction");
        return SEMU_TRANSACTION_REFUSE;
    }
    log->calls++;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result dma_sink(
    void *context, const semu_dma_request *request, semu_error *error)
{
    semu_transaction_result result;
    (void)context;

    if (request == NULL || request->transaction == NULL ||
        request->endpoint == NULL || request->endpoint->transfer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "invalid IOM DMA request");
        return SEMU_TRANSACTION_REFUSE;
    }
    result = request->endpoint->transfer(request->endpoint->context,
                                         request->transaction, error);
    if (result == SEMU_TRANSACTION_OK && request->completion != NULL) {
        request->completion(request->completion_context, result);
    }
    return result;
}

static void run_read(semu_test_context *context, uint8_t address,
                     size_t expected_tx_size)
{
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_iom *iom;
    semu_serial_endpoint endpoint;
    probe_log log = { address, expected_tx_size, 0x22u, 0u };

    semu_error_clear(&error);
    bus = semu_bus_create(&error);
    scheduler = semu_scheduler_create(&error);
    SEMU_TEST_ASSERT(context, bus != NULL && scheduler != NULL);
    iom = semu_apollo4_iom_create(
        bus, SEMU_APOLLO4_IOM0_BASE, SEMU_APOLLO4_IOM0_IRQ,
        NULL, NULL, dma_sink, NULL, scheduler, &error);
    SEMU_TEST_ASSERT(context, iom != NULL);
    endpoint = (semu_serial_endpoint){ "selector-probe", probe_transfer, &log };
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_attach_endpoint(iom, &endpoint, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_map_ram(bus, "selector-sram", 0x10000000u,
                                      0x1000u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(bus, 0x10000008u, 1u, 0xa0u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_write(iom, 0x21cu, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_write(iom, 0x220u, 4u,
                                            0x10000010u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_write(iom, 0x2c4u, 4u, address, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_write(iom, 0x218u, 4u, 1u, &error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_write(iom, 0x120u, 4u,
                                            0x22000112u, &error));
    SEMU_TEST_EQ_U64(context, 1u, log.calls);
    semu_apollo4_iom_destroy(iom);
    semu_scheduler_destroy(scheduler);
    semu_bus_destroy(bus);
}

static void test_haptic_command_selector(semu_test_context *context)
{
    run_read(context, 0x50u, 1u);
}

static void test_selector_scope(semu_test_context *context)
{
    run_read(context, 0x51u, 0u);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_haptic_command_selector),
        SEMU_TEST_CASE(test_selector_scope)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
