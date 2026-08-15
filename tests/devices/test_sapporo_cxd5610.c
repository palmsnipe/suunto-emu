#include "test.h"

#include <string.h>

#include "../../src/devices/sapporo_cxd5610.h"

typedef struct gps_fixture gps_fixture;
struct gps_fixture {
    semu_error error;
    semu_scheduler *scheduler;
    semu_sapporo_cxd5610 *transport;
    semu_serial_endpoint endpoint;
    uint8_t tx_trace[64];
    size_t tx_count;
    uint8_t rx_trace[64];
    size_t rx_count;
    unsigned awake_count;
    int awake_level;
    uint64_t awake_time;
    unsigned exchange_count;
    int reset_on_awake_high;
    int reset_on_receive;
};

static void awake(void *context, unsigned signal, int level)
{
    gps_fixture *fixture = (gps_fixture *)context;
    (void)signal;
    fixture->awake_count++;
    fixture->awake_level = level;
    fixture->awake_time = semu_scheduler_now(fixture->scheduler);
    if (level == 1 && fixture->reset_on_awake_high) {
        fixture->reset_on_awake_high = 0;
        semu_sapporo_cxd5610_reset(fixture->transport);
    }
}

static void receive(void *context, uint8_t value, uint64_t virtual_time_ns)
{
    gps_fixture *fixture = (gps_fixture *)context;
    (void)virtual_time_ns;
    if (fixture->rx_count < sizeof(fixture->rx_trace)) {
        fixture->rx_trace[fixture->rx_count++] = value;
    }
    if (fixture->reset_on_receive) {
        fixture->reset_on_receive = 0;
        semu_sapporo_cxd5610_reset(fixture->transport);
    }
}

static void trace(void *context, semu_sapporo_cxd5610_trace_direction direction,
                  uint8_t value, uint64_t virtual_time_ns)
{
    gps_fixture *fixture = (gps_fixture *)context;
    (void)virtual_time_ns;
    if (direction == SEMU_SAPPORO_CXD5610_TX &&
        fixture->tx_count < sizeof(fixture->tx_trace)) {
        fixture->tx_trace[fixture->tx_count++] = value;
    }
}

static semu_transaction_result exchange(
    void *context, const uint8_t *request, size_t count,
    semu_sapporo_cxd5610 *transport, semu_error *error)
{
    static const uint8_t expected[] = { '@', 'V', 'E', 'R', '\r', '\n' };
    static const uint8_t response[] = {
        '$', 'P', 'S', 'S', '0', '0', '0', '0', '\r', '\n'
    };
    gps_fixture *fixture = (gps_fixture *)context;
    if (count != sizeof(expected) || memcmp(request, expected, count) != 0) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "unexpected CXD5610 request");
        return SEMU_TRANSACTION_REFUSE;
    }
    fixture->exchange_count++;
    return semu_sapporo_cxd5610_inject_rx_after(
               transport, response, sizeof(response), 10u, error) == SEMU_OK
               ? SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE;
}

static int fixture_init(gps_fixture *fixture)
{
    memset(fixture, 0, sizeof(*fixture));
    semu_error_clear(&fixture->error);
    fixture->scheduler = semu_scheduler_create(&fixture->error);
    if (fixture->scheduler == NULL) return 0;
    fixture->transport = semu_sapporo_cxd5610_create(
        fixture->scheduler, awake, fixture, receive, fixture, trace, fixture,
        &fixture->error);
    if (fixture->transport == NULL) return 0;
    fixture->endpoint = semu_sapporo_cxd5610_endpoint(fixture->transport);
    return 1;
}

static void fixture_destroy(gps_fixture *fixture)
{
    semu_sapporo_cxd5610_destroy(fixture->transport);
    semu_scheduler_destroy(fixture->scheduler);
}

static semu_transaction_result send(gps_fixture *fixture, const uint8_t *bytes,
                                    size_t count)
{
    semu_serial_transaction transaction = { 0u, 0u, bytes, count, NULL, 0u };
    return fixture->endpoint.transfer(fixture->endpoint.context, &transaction,
                                      &fixture->error);
}

static void test_split_request_and_delayed_rx(semu_test_context *context)
{
    static const uint8_t first[] = { '@', 'V', 'E' };
    static const uint8_t second[] = { 'R', '\r', '\n' };
    static const uint8_t expected_rx[] = {
        '$', 'P', 'S', 'S', '0', '0', '0', '0', '\r', '\n'
    };
    gps_fixture fixture;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    semu_sapporo_cxd5610_set_exchange(fixture.transport, exchange, &fixture);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     send(&fixture, first, sizeof(first)));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     send(&fixture, second, sizeof(second)));
    SEMU_TEST_EQ_U64(context, 1u, fixture.exchange_count);
    SEMU_TEST_EQ_U64(context, 6u, fixture.tx_count);
    SEMU_TEST_EQ_U64(context, 0u, fixture.rx_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 9u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.rx_count);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 1u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, sizeof(expected_rx), fixture.rx_count);
    SEMU_TEST_ASSERT(context, memcmp(expected_rx, fixture.rx_trace,
                                     sizeof(expected_rx)) == 0);
    fixture_destroy(&fixture);
}

static void test_refusals_are_bounded(semu_test_context *context)
{
    static const uint8_t overflow[] = "@123456789";
    static const uint8_t unknown[] = { 'A', '\r', '\n' };
    static const uint8_t valid[] = { '@', 'V', 'E', 'R', '\r', '\n' };
    gps_fixture fixture;
    semu_serial_transaction wrong = { 1u, 0u, valid, sizeof(valid), NULL, 0u };
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     send(&fixture, overflow, sizeof(overflow) - 1u));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     send(&fixture, unknown, sizeof(unknown)));
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
                     fixture.endpoint.transfer(fixture.endpoint.context,
                                                &wrong, &fixture.error));
    SEMU_TEST_EQ_U64(context, 0u, fixture.exchange_count);
    semu_sapporo_cxd5610_set_exchange(fixture.transport, exchange, &fixture);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     send(&fixture, valid, sizeof(valid)));
    fixture_destroy(&fixture);
}

static void test_awake_pulse_and_reset(semu_test_context *context)
{
    gps_fixture fixture;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_cxd5610_pulse_awake_after(
                         fixture.transport, 100000000u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     semu_sapporo_cxd5610_pulse_awake_after(
                         fixture.transport, 1u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 99999999u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0, fixture.awake_level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 1u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 1, fixture.awake_level);
    SEMU_TEST_EQ_U64(context, 100000000u, fixture.awake_time);
    semu_sapporo_cxd5610_reset(fixture.transport);
    SEMU_TEST_EQ_U64(context, 0, fixture.awake_level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 1000000u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0, fixture.awake_level);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    fixture_destroy(&fixture);
}

static void test_reset_reentrancy_cancels_current_event(
    semu_test_context *context)
{
    static const uint8_t request[] = { '@', 'V', 'E', 'R', '\r', '\n' };
    gps_fixture fixture;
    SEMU_TEST_ASSERT(context, fixture_init(&fixture));
    fixture.reset_on_awake_high = 1;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_sapporo_cxd5610_pulse_awake_after(
                         fixture.transport, 10u, &fixture.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 10u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 0, fixture.awake_level);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    semu_sapporo_cxd5610_set_exchange(fixture.transport, exchange, &fixture);
    fixture.reset_on_receive = 1;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
                     send(&fixture, request, sizeof(request)));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_scheduler_advance(fixture.scheduler, 10u,
                                             &fixture.error));
    SEMU_TEST_EQ_U64(context, 1u, fixture.rx_count);
    SEMU_TEST_ASSERT(context, !semu_scheduler_has_events(fixture.scheduler));
    fixture_destroy(&fixture);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_split_request_and_delayed_rx),
        SEMU_TEST_CASE(test_refusals_are_bounded),
        SEMU_TEST_CASE(test_awake_pulse_and_reset),
        SEMU_TEST_CASE(test_reset_reentrancy_cancels_current_event)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
