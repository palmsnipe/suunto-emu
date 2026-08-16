#include "test.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "semu/peripheral.h"
#include "../../src/soc/apollo4/dma.h"

#define SRAM_BASE 0x10000000u
#define SRAM_SIZE 0x00200000u
#define FLASH_BASE 0x00010000u
#define FLASH_SIZE 0x00100000u

typedef struct dma_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_dma *dma;
} dma_fixture;

typedef struct completion_log {
    unsigned count;
    semu_transaction_result result;
} completion_log;

static semu_serial_endpoint test_endpoint;

static semu_transaction_result response_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    (void)context;
    (void)error;
    if (transaction == NULL || transaction->tx_size != 1u ||
        transaction->tx == NULL || transaction->tx[0u] != 0x9fu ||
        transaction->rx == NULL || transaction->rx_size != 3u) {
        return SEMU_TRANSACTION_REFUSE;
    }
    transaction->rx[0u] = 0x20u;
    transaction->rx[1u] = 0xbbu;
    transaction->rx[2u] = 0x19u;
    return SEMU_TRANSACTION_OK;
}

typedef struct write_log {
    unsigned count;
    uint8_t bytes[4];
} write_log;

static semu_transaction_result write_transfer(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    write_log *log = (write_log *)context;
    if (log == NULL || transaction == NULL || transaction->tx == NULL ||
        transaction->tx_size != sizeof(log->bytes) ||
        transaction->rx != NULL || transaction->rx_size != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "DMA write transaction shape is invalid");
        return SEMU_TRANSACTION_REFUSE;
    }
    memcpy(log->bytes, transaction->tx, sizeof(log->bytes));
    log->count++;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void completion_cb(void *context, semu_transaction_result result)
{
    completion_log *log = (completion_log *)context;
    log->count++;
    log->result = result;
}

static int fixture_init(dma_fixture *f)
{
    semu_error_clear(&f->error);
    f->bus = semu_bus_create(&f->error);
    f->scheduler = semu_scheduler_create(&f->error);
    if (f->bus == NULL || f->scheduler == NULL) {
        return 0;
    }
    f->dma = semu_apollo4_dma_create(f->bus, f->scheduler, &f->error);
    if (f->dma == NULL) {
        return 0;
    }
    if (semu_bus_map_ram(f->bus, "sram", SRAM_BASE, SRAM_SIZE,
                         &f->error) != SEMU_OK ||
        semu_bus_map_ram(f->bus, "flash", FLASH_BASE, FLASH_SIZE,
                         &f->error) != SEMU_OK) {
        semu_apollo4_dma_destroy(f->dma);
        semu_scheduler_destroy(f->scheduler);
        semu_bus_destroy(f->bus);
        return 0;
    }
    return 1;
}

static void fixture_destroy(dma_fixture *f)
{
    semu_apollo4_dma_destroy(f->dma);
    semu_scheduler_destroy(f->scheduler);
    semu_bus_destroy(f->bus);
}

static void make_request(semu_dma_request *req, semu_dma_direction dir,
                         uint32_t addr, uint32_t count,
                         completion_log *log)
{
    test_endpoint.name = "test";
    test_endpoint.transfer = NULL;
    test_endpoint.context = NULL;
    req->controller_id = "test";
    req->direction = dir;
    req->guest_address = addr;
    req->count = count;
    req->endpoint = &test_endpoint;
    req->continuation = 0u;
    req->transaction = NULL;
    req->completion = log ? completion_cb : NULL;
    req->completion_context = log;
}

static void test_valid_tx(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 4u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)log.result);
    fixture_destroy(&f);
}

static void test_valid_rx(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_FROM_ENDPOINT, 0x10001000u, 8u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)log.result);
    fixture_destroy(&f);
}

static void test_endpoint_response_reaches_guest(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    semu_serial_endpoint endpoint = {
        "response", response_transfer, NULL
    };
    semu_serial_transaction transaction;
    completion_log log = {0u};
    semu_error err;
    uint8_t command[] = { 0x9fu };
    uint32_t value;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_FROM_ENDPOINT, 0x10001000u, 3u, &log);
    transaction.address = 0u;
    transaction.chip_select = 0u;
    transaction.tx = command;
    transaction.tx_size = sizeof(command);
    transaction.rx = NULL;
    transaction.rx_size = 0u;
    req.endpoint = &endpoint;
    req.transaction = &transaction;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK, log.result);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(f.bus, 0x10001000u, 1u, &value, &err));
    SEMU_TEST_EQ_U64(context, 0x20u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(f.bus, 0x10001001u, 1u, &value, &err));
    SEMU_TEST_EQ_U64(context, 0xbbu, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_read(f.bus, 0x10001002u, 1u, &value, &err));
    SEMU_TEST_EQ_U64(context, 0x19u, value);
    fixture_destroy(&f);
}

static void test_endpoint_write_receives_guest_bytes(
    semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    semu_serial_endpoint endpoint;
    semu_serial_transaction transaction;
    completion_log completion = {0u};
    write_log writes = {0u};
    semu_error err;
    const uint8_t bytes[] = { 0x7fu, 0x01u, 0xd0u, 0xf0u };

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    endpoint.name = "write";
    endpoint.transfer = write_transfer;
    endpoint.context = &writes;
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10001000u,
                 sizeof(bytes), &completion);
    memset(&transaction, 0, sizeof(transaction));
    transaction.tx = bytes;
    transaction.tx_size = sizeof(bytes);
    req.endpoint = &endpoint;
    req.transaction = &transaction;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, completion.count);
    SEMU_TEST_EQ_U64(context, 1u, writes.count);
    SEMU_TEST_EQ_U64(context, 0x7fu, writes.bytes[0u]);
    SEMU_TEST_EQ_U64(context, 0xf0u, writes.bytes[3u]);
    fixture_destroy(&f);
}

static void test_valid_flash_tx(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x00010000u, 2u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    fixture_destroy(&f);
}

static void test_zero_count_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 0u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_REFUSE,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_overflow_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0xFFFFFFFFu, 2u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_REFUSE,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_cross_region_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10266FFFu, 2u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_REFUSE,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_flash_rx_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_FROM_ENDPOINT, 0x00010000u, 2u, &log);
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_REFUSE,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_null_endpoint_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 4u, &log);
    req.endpoint = NULL;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_REFUSE,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_completion_exactly_once(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 1u, &log);
    semu_error_clear(&err);
    (void)semu_apollo4_dma_execute(f.dma, &req, &err);
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    fixture_destroy(&f);
}

static void test_no_partial_write_on_refusal(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10266FFFu, 4u, &log);
    semu_error_clear(&err);
    (void)semu_apollo4_dma_execute(f.dma, &req, &err);
    SEMU_TEST_EQ_U64(context, 0u, log.count);
    fixture_destroy(&f);
}

static void test_continuation(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 4u, &log);
    req.continuation = 1u;
    semu_error_clear(&err);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_TRANSACTION_OK,
                     (uint64_t)semu_apollo4_dma_execute(f.dma, &req, &err));
    SEMU_TEST_EQ_U64(context, 1u, log.count);
    fixture_destroy(&f);
}

static void test_two_run_equality(semu_test_context *context)
{
    dma_fixture f;
    semu_dma_request req;
    completion_log log1 = {0u}, log2 = {0u};
    semu_error err;

    SEMU_TEST_ASSERT(context, fixture_init(&f));
    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 4u, &log1);
    semu_error_clear(&err);
    (void)semu_apollo4_dma_execute(f.dma, &req, &err);

    make_request(&req, SEMU_DMA_TO_ENDPOINT, 0x10000000u, 4u, &log2);
    semu_error_clear(&err);
    (void)semu_apollo4_dma_execute(f.dma, &req, &err);

    SEMU_TEST_EQ_U64(context, 1u, log1.count);
    SEMU_TEST_EQ_U64(context, 1u, log2.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)log1.result, (uint64_t)log2.result);
    fixture_destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_valid_tx),
        SEMU_TEST_CASE(test_valid_rx),
        SEMU_TEST_CASE(test_endpoint_response_reaches_guest),
        SEMU_TEST_CASE(test_endpoint_write_receives_guest_bytes),
        SEMU_TEST_CASE(test_valid_flash_tx),
        SEMU_TEST_CASE(test_zero_count_refusal),
        SEMU_TEST_CASE(test_overflow_refusal),
        SEMU_TEST_CASE(test_cross_region_refusal),
        SEMU_TEST_CASE(test_flash_rx_refusal),
        SEMU_TEST_CASE(test_null_endpoint_refusal),
        SEMU_TEST_CASE(test_completion_exactly_once),
        SEMU_TEST_CASE(test_no_partial_write_on_refusal),
        SEMU_TEST_CASE(test_continuation),
        SEMU_TEST_CASE(test_two_run_equality)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
