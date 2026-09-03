#include "test.h"

#include <stdint.h>
#include <string.h>

#include "semu/peripheral.h"
#include "../../src/soc/apollo4/iom.h"

typedef struct iom_fixture {
    semu_error error;
    semu_bus *bus;
    semu_scheduler *scheduler;
    semu_apollo4_iom *iom;
} iom_fixture;

typedef struct irq_log {
    unsigned count;
    unsigned irq;
    int level;
} irq_log;

typedef struct dma_log {
    unsigned count;
    semu_dma_direction direction;
    uint32_t guest_address;
    uint32_t count_val;
    uint8_t continuation;
    int transaction_seen;
    uint8_t transaction_address;
    size_t transaction_tx_size;
    uint8_t transaction_tx0;
    semu_transaction_result result;
} dma_log;

typedef struct lsm6_phase_log {
    int command_seen;
    int held_read_seen;
} lsm6_phase_log;

typedef struct selector_log {
    int selector_seen;
} selector_log;

static semu_serial_endpoint test_endpoint;

static void irq_sink(void *context, unsigned irq, int level)
{
    irq_log *log = (irq_log *)context;
    log->count++;
    log->irq = irq;
    log->level = level;
}

static semu_transaction_result dma_sink(void *context,
                                         const semu_dma_request *request,
                                         semu_error *error)
{
    dma_log *log = (dma_log *)context;
    if (log->count < 4u) {
        log->direction = request->direction;
        log->guest_address = request->guest_address;
        log->count_val = request->count;
        log->continuation = request->continuation;
        if (request->transaction != NULL) {
            log->transaction_seen = 1;
            log->transaction_address = request->transaction->address;
            log->transaction_tx_size = request->transaction->tx_size;
            log->transaction_tx0 = request->transaction->tx != NULL &&
                                   request->transaction->tx_size != 0u
                                       ? request->transaction->tx[0u] : 0u;
        }
    }
    log->count++;
    semu_error_clear(error);
    if (request->transaction != NULL && request->endpoint != NULL &&
        request->endpoint->transfer != NULL) {
        semu_transaction_result endpoint_result;
        endpoint_result = request->endpoint->transfer(
            request->endpoint->context, request->transaction, error);
        if (endpoint_result != SEMU_TRANSACTION_OK) {
            return endpoint_result;
        }
    }
    if (log->result == SEMU_TRANSACTION_REFUSE) {
        return SEMU_TRANSACTION_REFUSE;
    }
    if (log->result == SEMU_TRANSACTION_WAIT) {
        return SEMU_TRANSACTION_WAIT;
    }
    if (request->completion != NULL) {
        request->completion(request->completion_context,
                            SEMU_TRANSACTION_OK);
    }
    return SEMU_TRANSACTION_OK;
}

static int fixture_init(iom_fixture *f, irq_log *irq, dma_log *dma)
{
    semu_error_clear(&f->error);
    f->bus = semu_bus_create(&f->error);
    f->scheduler = semu_scheduler_create(&f->error);
    if (f->bus == NULL || f->scheduler == NULL) {
        return 0;
    }
    f->iom = semu_apollo4_iom_create(f->bus, SEMU_APOLLO4_IOM0_BASE,
                                      SEMU_APOLLO4_IOM0_IRQ, irq_sink, irq,
                                      dma ? dma_sink : NULL, dma,
                                      f->scheduler, &f->error);
    return f->iom != NULL;
}

static void fixture_destroy(iom_fixture *f)
{
    semu_apollo4_iom_destroy(f->iom);
    semu_scheduler_destroy(f->scheduler);
    semu_bus_destroy(f->bus);
}

static semu_status rd(iom_fixture *f, uint32_t off, uint32_t *v)
{
    return semu_bus_read(f->bus, SEMU_APOLLO4_IOM0_BASE + off, 4u, v, &f->error);
}

static semu_status wr(iom_fixture *f, uint32_t off, uint32_t v)
{
    return semu_bus_write(f->bus, SEMU_APOLLO4_IOM0_BASE + off, 4u, v, &f->error);
}

static void setup_dma_tx(iom_fixture *f)
{
    test_endpoint.name = "test";
    test_endpoint.transfer = NULL;
    test_endpoint.context = NULL;
    semu_apollo4_iom_attach_endpoint(f->iom, &test_endpoint, &f->error);
    (void)wr(f, 0x21cu, 4u);
    (void)wr(f, 0x220u, 0x10000000u);
    (void)wr(f, 0x218u, 0x03u);
    (void)wr(f, 0x200u, 0xFFFFFFFFu);
}

static void test_reset_values(semu_test_context *context)
{
    iom_fixture f;
    uint32_t v = 0xDEADu;
    irq_log irq = {0u};
    dma_log dma = {0u};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x200u, &v));
    SEMU_TEST_EQ_U64(context, 0u, v);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x204u, &v));
    SEMU_TEST_EQ_U64(context, 0u, v);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x224u, &v));
    SEMU_TEST_EQ_U64(context, 0u, v);
    SEMU_TEST_EQ_U64(context, 0u, irq.count);
    fixture_destroy(&f);
}

static semu_transaction_result lsm6_held_phase_probe(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    lsm6_phase_log *log = (lsm6_phase_log *)context;

    if (transaction == NULL || transaction->address != 0u ||
        transaction->chip_select != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "IOM0 LSM6 transaction address is invalid");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (transaction->tx_size == 1u && transaction->tx != NULL &&
        transaction->tx[0u] == 0x8fu) {
        log->command_seen = 1;
    } else if (transaction->tx_size == 0u && log->command_seen) {
        log->held_read_seen = 1;
    } else {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "IOM0 LSM6 held-CS transaction shape is invalid");
        return SEMU_TRANSACTION_REFUSE;
    }
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static semu_transaction_result lps22_selector_probe(
    void *context, semu_serial_transaction *transaction, semu_error *error)
{
    selector_log *log = (selector_log *)context;

    if (transaction == NULL || transaction->address != 0x5cu ||
        transaction->tx == NULL || transaction->tx_size != 1u ||
        transaction->tx[0u] != 0x0fu) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "IOM2 LPS22 selector transaction is invalid");
        return SEMU_TRANSACTION_REFUSE;
    }
    log->selector_seen = 1;
    semu_error_clear(error);
    return SEMU_TRANSACTION_OK;
}

static void test_lps22_read_selector(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    selector_log selector = {0};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    test_endpoint.name = "lps22-probe";
    test_endpoint.transfer = lps22_selector_probe;
    test_endpoint.context = &selector;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_attach_endpoint(
                         f.iom, &test_endpoint, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x21cu, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x220u, 0x10001000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x2c4u, 0x5cu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x218u, 0x01u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x0f000112u));
    SEMU_TEST_EQ_U64(context, 1u, selector.selector_seen);
    fixture_destroy(&f);
}

static void test_lsm6_held_read_phase(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    lsm6_phase_log phases = {0};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    test_endpoint.name = "lsm6-probe";
    test_endpoint.transfer = lsm6_held_phase_probe;
    test_endpoint.context = &phases;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_apollo4_iom_attach_endpoint(
                         f.iom, &test_endpoint, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_map_ram(f.bus, "iom-test-sram",
                                      0x10000000u, 0x1000u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_write(f.bus, 0x10000000u, 1u, 0x8fu,
                                    &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x21cu, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x220u, 0x10000000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x2c4u, 0u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x218u, 0x03u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x181u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x220u, 0x10000001u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x218u, 0x01u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x102u));
    SEMU_TEST_EQ_U64(context, 2u, dma.count);
    SEMU_TEST_EQ_U64(context, 1u, phases.command_seen);
    SEMU_TEST_EQ_U64(context, 1u, phases.held_read_seen);
    SEMU_TEST_EQ_U64(context, 0u, dma.transaction_tx_size);
    fixture_destroy(&f);
}

static void test_dma_request_emission(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    setup_dma_tx(&f);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x401u));
    SEMU_TEST_EQ_U64(context, 1u, dma.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_DMA_TO_ENDPOINT, dma.direction);
    SEMU_TEST_EQ_U64(context, 0x10000000u, dma.guest_address);
    SEMU_TEST_EQ_U64(context, 4u, dma.count_val);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x224u, &v));
    SEMU_TEST_EQ_U64(context, 2u, v);
    SEMU_TEST_EQ_U64(context, 1u, irq.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_APOLLO4_IOM0_IRQ, irq.irq);
    fixture_destroy(&f);
}

static void test_p2m_dma_request(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    test_endpoint.name = "test-rx";
    test_endpoint.transfer = NULL;
    test_endpoint.context = NULL;
    semu_apollo4_iom_attach_endpoint(f.iom, &test_endpoint, &f.error);
    (void)wr(&f, 0x21cu, 8u);
    (void)wr(&f, 0x220u, 0x10001000u);
    (void)wr(&f, 0x218u, 0x01u);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x882u));
    SEMU_TEST_EQ_U64(context, 1u, dma.count);
    SEMU_TEST_EQ_U64(context, (uint64_t)SEMU_DMA_FROM_ENDPOINT, dma.direction);
    SEMU_TEST_EQ_U64(context, 0x10001000u, dma.guest_address);
    SEMU_TEST_EQ_U64(context, 8u, dma.count_val);
    SEMU_TEST_EQ_U64(context, 1u, dma.continuation);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x224u, &v));
    SEMU_TEST_EQ_U64(context, 2u, v);
    fixture_destroy(&f);
}

static void test_irq_status_and_clear(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    setup_dma_tx(&f);
    (void)wr(&f, 0x200u, 0xFFFFFFFFu);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x120u, 0x401u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x204u, &v));
    SEMU_TEST_EQ_U64(context, ((UINT32_C(1) << 10) | UINT32_C(1)), v);
    SEMU_TEST_EQ_U64(context, 1u, irq.count);
    SEMU_TEST_EQ_U64(context, 1u, irq.level);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x208u, 0xFFFFFFFFu));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x204u, &v));
    SEMU_TEST_EQ_U64(context, 0u, v);
    SEMU_TEST_EQ_U64(context, 2u, irq.count);
    SEMU_TEST_EQ_U64(context, 0u, irq.level);
    fixture_destroy(&f);
}

static void test_endpoint_refusal(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    (void)wr(&f, 0x21cu, 4u);
    (void)wr(&f, 0x220u, 0x10000000u);
    (void)wr(&f, 0x218u, 0x03u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, wr(&f, 0x120u, 0x401u));
    SEMU_TEST_EQ_U64(context, 0u, dma.count);
    fixture_destroy(&f);
}

static void test_dma_refusal(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v = 0u;

    dma.result = SEMU_TRANSACTION_REFUSE;
    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    setup_dma_tx(&f);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE, wr(&f, 0x120u, 0x401u));
    SEMU_TEST_EQ_U64(context, 1u, dma.count);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x224u, &v));
    SEMU_TEST_EQ_U64(context, 4u, v);
    fixture_destroy(&f);
}

static void test_zero_count_refusal(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    test_endpoint.name = "test";
    test_endpoint.transfer = NULL;
    test_endpoint.context = NULL;
    semu_apollo4_iom_attach_endpoint(f.iom, &test_endpoint, &f.error);
    (void)wr(&f, 0x21cu, 0u);
    (void)wr(&f, 0x220u, 0x10000000u);
    (void)wr(&f, 0x218u, 0x03u);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE, wr(&f, 0x120u, 0x401u));
    SEMU_TEST_EQ_U64(context, 0u, dma.count);
    fixture_destroy(&f);
}

static void test_invalid_command_op(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    setup_dma_tx(&f);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, wr(&f, 0x120u, 0x403u));
    SEMU_TEST_EQ_U64(context, 0u, dma.count);
    fixture_destroy(&f);
}

static void test_unknown_offset_refusal(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(f.bus, SEMU_APOLLO4_IOM0_BASE + 0x00u,
                                   4u, &v, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(f.bus, SEMU_APOLLO4_IOM0_BASE + 0x00u,
                                    4u, 0u, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(f.bus, SEMU_APOLLO4_IOM0_BASE + 0x100u,
                                   4u, &v, &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_read(f.bus, SEMU_APOLLO4_IOM0_BASE + 0x200u,
                                   2u, &v, &f.error));
    fixture_destroy(&f);
}

static void test_observed_inner_registers(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t value = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x11cu, &value));
    SEMU_TEST_EQ_U64(context, 0xE20u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x104u, 0x1010u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x104u, &value));
    SEMU_TEST_EQ_U64(context, 0x1010u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x118u, 0x1D0E1301u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x118u, &value));
    SEMU_TEST_EQ_U64(context, 0x1D0E1301u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, wr(&f, 0x11cu, 0x10u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x11cu, &value));
    SEMU_TEST_EQ_U64(context, 0xE30u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x248u, &value));
    SEMU_TEST_EQ_U64(context, 4u, value);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED, wr(&f, 0x248u, 0u));
    fixture_destroy(&f);
}

static void test_reset_and_repeatability(semu_test_context *context)
{
    iom_fixture f;
    irq_log irq = {0u};
    dma_log dma = {0u};
    uint32_t v1 = 0u, v2 = 0u;

    SEMU_TEST_ASSERT(context, fixture_init(&f, &irq, &dma));
    setup_dma_tx(&f);
    (void)wr(&f, 0x200u, 0xFFFFFFFFu);
    (void)wr(&f, 0x120u, 0x401u);
    (void)rd(&f, 0x224u, &v1);
    semu_apollo4_iom_reset(f.iom);
    SEMU_TEST_EQ_U64(context, SEMU_OK, rd(&f, 0x224u, &v2));
    SEMU_TEST_EQ_U64(context, 0u, v2);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(f.scheduler));
    fixture_destroy(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_reset_values),
        SEMU_TEST_CASE(test_dma_request_emission),
        SEMU_TEST_CASE(test_p2m_dma_request),
        SEMU_TEST_CASE(test_lsm6_held_read_phase),
        SEMU_TEST_CASE(test_lps22_read_selector),
        SEMU_TEST_CASE(test_irq_status_and_clear),
        SEMU_TEST_CASE(test_endpoint_refusal),
        SEMU_TEST_CASE(test_dma_refusal),
        SEMU_TEST_CASE(test_zero_count_refusal),
        SEMU_TEST_CASE(test_invalid_command_op),
        SEMU_TEST_CASE(test_unknown_offset_refusal),
        SEMU_TEST_CASE(test_observed_inner_registers),
        SEMU_TEST_CASE(test_reset_and_repeatability)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
