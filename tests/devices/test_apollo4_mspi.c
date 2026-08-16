#include "../../src/soc/apollo4/mspi.h"

#include <string.h>

#include "test.h"

typedef struct fixture fixture;
struct fixture {
    semu_error error;
    semu_bus *bus;
    semu_apollo4_mspi *mspi;
    unsigned irq_count;
    int irq_level;
    semu_transaction_result dma_result;
    semu_transaction_result endpoint_result;
    unsigned dma_count;
    semu_dma_request request;
    unsigned endpoint_count;
};

static void irq(void *context, unsigned number, int level)
{
    fixture *f = (fixture *)context;
    (void)number;
    f->irq_count++;
    f->irq_level = level;
}

static semu_transaction_result transfer(void *context,
                                        semu_serial_transaction *transaction,
                                        semu_error *error)
{
    fixture *f = (fixture *)context;
    (void)transaction;
    (void)error;
    f->endpoint_count++;
    return f->endpoint_result;
}

static semu_transaction_result dma(void *context,
                                   const semu_dma_request *request,
                                   semu_error *error)
{
    fixture *f = (fixture *)context;
    (void)error;
    f->request = *request;
    f->dma_count++;
    if (request->completion != NULL)
        request->completion(request->completion_context, f->dma_result);
    return f->dma_result;
}

static int init_fixture(fixture *f, uint32_t base, unsigned irq_number)
{
    semu_serial_endpoint endpoint = { "mspi-endpoint", transfer, f };
    semu_error_clear(&f->error);
    f->endpoint_result = SEMU_TRANSACTION_OK;
    f->dma_result = SEMU_TRANSACTION_WAIT;
    f->bus = semu_bus_create(&f->error);
    if (f->bus == NULL) return 0;
    f->mspi = semu_apollo4_mspi_create(f->bus, base, irq_number, irq, f,
                                       dma, f, &f->error);
    if (f->mspi == NULL) return 0;
    if (semu_bus_map_ram(f->bus, "sram", 0x10000000u, 0x20000u,
                         &f->error) != SEMU_OK) return 0;
    return semu_apollo4_mspi_attach_endpoint(f->mspi, &endpoint,
                                             &f->error) == SEMU_OK;
}

static void destroy_fixture(fixture *f)
{
    semu_apollo4_mspi_destroy(f->mspi);
    semu_bus_destroy(f->bus);
}

static semu_status read_reg(fixture *f, uint32_t base, uint32_t offset,
                            uint32_t *value)
{
    return semu_bus_read(f->bus, base + offset, 4u, value, &f->error);
}

static semu_status write_reg(fixture *f, uint32_t base, uint32_t offset,
                             uint32_t value)
{
    return semu_bus_write(f->bus, base + offset, 4u, value, &f->error);
}

static void descriptor_and_irq(semu_test_context *context)
{
    fixture f = { 0 };
    uint8_t descriptor[16] = { 0x03u, 0x01u, 0x02u, 0x04u };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI1_BASE,
                                            SEMU_APOLLO4_MSPI1_IRQ));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(f.bus, 0x10000100u, descriptor,
                                   sizeof(descriptor), &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI_INTEN, 0x200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_DESCRIPTOR,
                               0x10000100u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_CONTROL, 3u));
    SEMU_TEST_EQ_U64(context, 1u, f.endpoint_count);
    SEMU_TEST_EQ_U64(context, 1u, f.irq_count);
    SEMU_TEST_EQ_U64(context, 1u, f.irq_level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                              SEMU_APOLLO4_MSPI_INTSTAT, &value));
    SEMU_TEST_EQ_U64(context, 0x200u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI_INTCLR, 0x200u));
    SEMU_TEST_EQ_U64(context, 0, f.irq_level);
    destroy_fixture(&f);
}

static void queue_and_refusal(semu_test_context *context)
{
    fixture f = { 0 };
    uint8_t queue[16] = { 0 };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI1_BASE,
                                            SEMU_APOLLO4_MSPI1_IRQ));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(f.bus, 0x10000200u, queue, sizeof(queue),
                                   &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_QUEUE_ADDRESS,
                               0x10000200u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_QUEUE_COUNT, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_QUEUE_CONTROL,
                               0x13u));
    SEMU_TEST_EQ_U64(context, 1u, f.endpoint_count);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_QUEUE_COUNT, 4u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                              SEMU_APOLLO4_MSPI_INTSTAT, &value));
    SEMU_TEST_EQ_U64(context, 0x40u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_DESCRIPTOR,
                               0x20000000u));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_CONTROL, 3u));
    SEMU_TEST_EQ_U64(context, 1u, f.endpoint_count);
    destroy_fixture(&f);
}

static void mspi2_dma_request(semu_test_context *context)
{
    fixture f = { 0 };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI2_BASE,
                                            SEMU_APOLLO4_MSPI2_IRQ));
    f.dma_result = SEMU_TRANSACTION_OK;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI_INTEN, 0x40u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DEVICE_CONFIG,
                               0x12340000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                              SEMU_APOLLO4_MSPI2_DEVICE_CONFIG, &value));
    SEMU_TEST_EQ_U64(context, 0x12340000u, value);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_TARGET,
                               0x10001000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_COUNT, 8u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_CONFIG,
                               0x17u));
    SEMU_TEST_EQ_U64(context, 1u, f.dma_count);
    SEMU_TEST_EQ_U64(context, SEMU_DMA_TO_ENDPOINT, f.request.direction);
    SEMU_TEST_EQ_U64(context, 1u, f.request.continuation);
    SEMU_TEST_EQ_U64(context, 0x10001000u, f.request.guest_address);
    SEMU_TEST_EQ_U64(context, 8u, f.request.count);
    SEMU_TEST_EQ_U64(context, 1u, f.irq_level);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                              SEMU_APOLLO4_MSPI2_DMA_STATUS,
                              &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    destroy_fixture(&f);
}

static void refusal_is_atomic(semu_test_context *context)
{
    fixture f = { 0 };
    uint32_t value = 99u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI2_BASE,
                                            SEMU_APOLLO4_MSPI2_IRQ));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_CONFIG,
                               0x1u));
    SEMU_TEST_EQ_U64(context, 0u, f.dma_count);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     read_reg(&f, SEMU_APOLLO4_MSPI2_BASE, 0x300u, &value));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_UNSUPPORTED,
                     semu_bus_write(f.bus, SEMU_APOLLO4_MSPI2_BASE,
                                    2u, 1u, &f.error));
    destroy_fixture(&f);
}

static void mspi2_full_device_address(semu_test_context *context)
{
    fixture f = { 0 };
    uint32_t value = 0u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI2_BASE,
                                            SEMU_APOLLO4_MSPI2_IRQ));
    f.dma_result = SEMU_TRANSACTION_OK;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_INSTRUCTION,
                               0x000c0000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_DEVICE,
                               0x01000020u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_TARGET,
                               0x10001000u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_COUNT, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DMA_CONFIG, 0x13u));
    SEMU_TEST_ASSERT(context, f.request.transaction != NULL);
    SEMU_TEST_EQ_U64(context, 1u, f.request.transaction->address);
    SEMU_TEST_EQ_U64(context, 0x0cu, f.request.transaction->tx[0u]);
    SEMU_TEST_EQ_U64(context, 0x00u, f.request.transaction->tx[1u]);
    SEMU_TEST_EQ_U64(context, 0x00u, f.request.transaction->tx[2u]);
    SEMU_TEST_EQ_U64(context, 0x20u, f.request.transaction->tx[3u]);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                              SEMU_APOLLO4_MSPI2_DMA_STATUS, &value));
    SEMU_TEST_EQ_U64(context, 2u, value);
    destroy_fixture(&f);
}

static void endpoint_refusal_is_atomic(semu_test_context *context)
{
    fixture f = { 0 };
    uint8_t descriptor[16] = { 0 };
    uint32_t value = 99u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI1_BASE,
                                            SEMU_APOLLO4_MSPI1_IRQ));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     semu_bus_load(f.bus, 0x10000300u, descriptor,
                                   sizeof(descriptor), &f.error));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_DESCRIPTOR,
                               0x10000300u));
    f.endpoint_result = SEMU_TRANSACTION_WAIT;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_CONTROL, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                              SEMU_APOLLO4_MSPI1_CONTROL, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_WAIT, f.endpoint_result);
    SEMU_TEST_EQ_U64(context, 0u, f.irq_level);
    f.endpoint_result = SEMU_TRANSACTION_REFUSE;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_STATE,
                     write_reg(&f, SEMU_APOLLO4_MSPI1_BASE,
                               SEMU_APOLLO4_MSPI1_CONTROL, 3u));
    SEMU_TEST_EQ_U64(context, 0u, f.irq_level);
    destroy_fixture(&f);
}

static void mspi2_command_and_reset(semu_test_context *context)
{
    fixture f = { 0 };
    uint32_t value = 99u;
    SEMU_TEST_ASSERT(context, init_fixture(&f, SEMU_APOLLO4_MSPI2_BASE,
                                            SEMU_APOLLO4_MSPI2_IRQ));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI_INTEN, 1u));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_DATA, 0x9fu));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     write_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                               SEMU_APOLLO4_MSPI2_COMMAND, 0xc1u));
    SEMU_TEST_EQ_U64(context, 1u, f.endpoint_count);
    SEMU_TEST_EQ_U64(context, 1u, f.irq_level);
    semu_bus_reset(f.bus);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
                     read_reg(&f, SEMU_APOLLO4_MSPI2_BASE,
                              SEMU_APOLLO4_MSPI_INTSTAT, &value));
    SEMU_TEST_EQ_U64(context, 0u, value);
    SEMU_TEST_EQ_U64(context, 0u, f.irq_level);
    destroy_fixture(&f);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(descriptor_and_irq), SEMU_TEST_CASE(queue_and_refusal),
        SEMU_TEST_CASE(mspi2_dma_request),
        SEMU_TEST_CASE(mspi2_full_device_address),
        SEMU_TEST_CASE(refusal_is_atomic),
        SEMU_TEST_CASE(endpoint_refusal_is_atomic),
        SEMU_TEST_CASE(mspi2_command_and_reset)
    };
    return semu_test_run(cases, SEMU_ARRAY_LEN(cases));
}
