#include "mspi.h"

#include <stdlib.h>
#include <string.h>

#define INTEN SEMU_APOLLO4_MSPI_INTEN
#define INTSTAT SEMU_APOLLO4_MSPI_INTSTAT
#define INTCLR SEMU_APOLLO4_MSPI_INTCLR
#define INTSET SEMU_APOLLO4_MSPI_INTSET
#define M1_CONTROL SEMU_APOLLO4_MSPI1_CONTROL
#define M1_DESCRIPTOR SEMU_APOLLO4_MSPI1_DESCRIPTOR
#define M1_QUEUE_CONTROL SEMU_APOLLO4_MSPI1_QUEUE_CONTROL
#define M1_QUEUE_ADDRESS SEMU_APOLLO4_MSPI1_QUEUE_ADDRESS
#define M1_QUEUE_DEVICE SEMU_APOLLO4_MSPI1_QUEUE_DEVICE
#define M1_QUEUE_COUNT SEMU_APOLLO4_MSPI1_QUEUE_COUNT
#define M2_COMMAND SEMU_APOLLO4_MSPI2_COMMAND
#define M2_ADDRESS SEMU_APOLLO4_MSPI2_ADDRESS
#define M2_DATA SEMU_APOLLO4_MSPI2_DATA
#define M2_DEVICE_CONFIG SEMU_APOLLO4_MSPI2_DEVICE_CONFIG
#define M2_INSTRUCTION SEMU_APOLLO4_MSPI2_INSTRUCTION
#define M2_DMA_CONFIG SEMU_APOLLO4_MSPI2_DMA_CONFIG
#define M2_DMA_STATUS SEMU_APOLLO4_MSPI2_DMA_STATUS
#define M2_DMA_TARGET SEMU_APOLLO4_MSPI2_DMA_TARGET
#define M2_DMA_DEVICE SEMU_APOLLO4_MSPI2_DMA_DEVICE
#define M2_DMA_COUNT SEMU_APOLLO4_MSPI2_DMA_COUNT

#define M1_COMMAND_DONE (UINT32_C(1) << 9)
#define M1_QUEUE_DONE (UINT32_C(1) << 6)
#define M2_COMMAND_DONE UINT32_C(1)
#define M2_DMA_DONE (UINT32_C(1) << 6)
#define M2_DMA_ERROR (UINT32_C(1) << 7)

struct semu_apollo4_mspi {
    semu_bus *bus;
    uint32_t base;
    unsigned irq_number;
    semu_apollo4_mspi_irq_fn irq;
    void *irq_context;
    semu_dma_request_sink_fn dma_sink;
    void *dma_context;
    semu_serial_endpoint endpoint;
    int endpoint_attached;
    uint32_t registers[SEMU_APOLLO4_MSPI_SIZE / 4u];
    uint32_t status;
    uint32_t dma_status;
    int irq_level;
    uint8_t dma_buffer[4100];
    semu_serial_transaction dma_transaction;
};

static const semu_bus_device_ops mspi_ops = {
    semu_apollo4_mspi_read, semu_apollo4_mspi_write,
    semu_apollo4_mspi_reset
};

static int is_mspi1(const semu_apollo4_mspi *mspi)
{
    return mspi->base == SEMU_APOLLO4_MSPI1_BASE;
}

static semu_status valid_access(semu_apollo4_mspi *mspi, uint32_t offset,
                                unsigned width, semu_error *error)
{
    if (mspi == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 MSPI context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u || offset >= SEMU_APOLLO4_MSPI_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI access is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static int known_offset(const semu_apollo4_mspi *mspi, uint32_t offset)
{
    if (offset == INTEN || offset == INTSTAT || offset == INTCLR ||
        offset == INTSET) return 1;
    if (is_mspi1(mspi)) {
        /* E-A4-MSPI-001: Renode's Sapporo MSPI1 wrapper retains the complete
           aligned register window.  The native setup touches product-specific
           slots before the DIAP4 command registers, so rejecting unlisted
           offsets here faults the firmware before the observed display path. */
        return 1;
    }
    /* The Renode MSPI2 model retains the aligned device-setup slots from
       0x80 through 0x9c; only 0x94 has transfer semantics here. */
    if (offset >= 0x80u && offset <= 0x9cu) return 1;
    if (offset >= 0x30u && offset <= 0x44u) return 1;
    if (offset == 0x20u) return 1;
    if (offset >= 0x100u && offset <= 0x11cu) return 1;
    return offset == M2_COMMAND || offset == M2_ADDRESS || offset == M2_DATA ||
           offset == M2_INSTRUCTION || offset == M2_DMA_CONFIG ||
           offset == M2_DMA_STATUS || offset == M2_DMA_TARGET ||
           offset == M2_DMA_DEVICE || offset == M2_DMA_COUNT;
}

static uint32_t *reg(semu_apollo4_mspi *mspi, uint32_t offset)
{
    return &mspi->registers[offset / 4u];
}

static void update_irq(semu_apollo4_mspi *mspi)
{
    int level = (mspi->status & *reg(mspi, INTEN)) != 0u;
    if (level != mspi->irq_level && mspi->irq != NULL) {
        mspi->irq(mspi->irq_context, mspi->irq_number, level);
    }
    mspi->irq_level = level;
}

static semu_status endpoint_transfer(semu_apollo4_mspi *mspi,
                                     uint8_t *tx, size_t tx_size,
                                     uint32_t device_address,
                                     semu_error *error)
{
    semu_serial_transaction transaction;
    semu_transaction_result result;
    if (!mspi->endpoint_attached || mspi->endpoint.transfer == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "Apollo4 MSPI endpoint is detached");
        return SEMU_ERR_STATE;
    }
    /* MSPI2's device-address register is 32-bit even though the observed
       command frame carries only the low 24 address bits.  The flash
       endpoint receives the high byte through this existing transaction
       metadata field; IOM endpoints continue to use it as their bus address. */
    transaction.address = (uint8_t)(device_address >> 24u);
    transaction.chip_select = 0u;
    transaction.tx = tx;
    transaction.tx_size = tx_size;
    transaction.rx = NULL;
    transaction.rx_size = 0u;
    result = mspi->endpoint.transfer(mspi->endpoint.context, &transaction,
                                     error);
    if (result == SEMU_TRANSACTION_OK) return SEMU_OK;
    if (result == SEMU_TRANSACTION_WAIT) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 MSPI endpoint is waiting");
        return SEMU_ERR_STATE;
    }
    if (error != NULL && error->code == SEMU_OK) {
        semu_error_set(error, SEMU_ERR_STATE, "Apollo4 MSPI endpoint refused");
    }
    return SEMU_ERR_STATE;
}

static semu_status complete_descriptor(semu_apollo4_mspi *mspi,
                                       uint32_t descriptor, uint32_t mask,
                                       semu_error *error)
{
    uint8_t bytes[16];
    semu_status status = semu_bus_copy_out(mspi->bus, descriptor, bytes,
                                           sizeof(bytes), error);
    if (status != SEMU_OK) return status;
    status = endpoint_transfer(mspi, bytes, sizeof(bytes), 0u, error);
    if (status != SEMU_OK) return status;
    mspi->status |= mask;
    update_irq(mspi);
    return SEMU_OK;
}

static semu_status complete_queue(semu_apollo4_mspi *mspi, semu_error *error)
{
    uint32_t address = *reg(mspi, M1_QUEUE_ADDRESS);
    uint32_t count = *reg(mspi, M1_QUEUE_COUNT);
    uint8_t bytes[16];
    semu_status status;
    if (count != 3u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI queue count is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    status = semu_bus_copy_out(mspi->bus, address, bytes, sizeof(bytes), error);
    if (status != SEMU_OK) return status;
    status = endpoint_transfer(mspi, bytes, sizeof(bytes), 0u, error);
    if (status != SEMU_OK) return status;
    mspi->status |= M1_QUEUE_DONE;
    update_irq(mspi);
    return SEMU_OK;
}

static void dma_complete(void *context, semu_transaction_result result)
{
    semu_apollo4_mspi *mspi = (semu_apollo4_mspi *)context;
    if (mspi == NULL) return;
    if (result == SEMU_TRANSACTION_OK) {
        mspi->dma_status = 1u << 1;
        mspi->status |= M2_DMA_DONE;
    } else if (result == SEMU_TRANSACTION_REFUSE) {
        mspi->dma_status = 1u << 2;
        mspi->status |= M2_DMA_ERROR;
    }
    update_irq(mspi);
}

static semu_status emit_dma(semu_apollo4_mspi *mspi, uint32_t value,
                             semu_error *error)
{
    semu_dma_request request;
    uint32_t target = *reg(mspi, M2_DMA_TARGET);
    uint32_t count = *reg(mspi, M2_DMA_COUNT) & UINT32_C(0xfff);
    semu_transaction_result result;
    if (value != 0x13u && value != 0x17u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI DMA control is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (mspi->dma_sink == NULL || !mspi->endpoint_attached || count == 0u ||
        UINT32_MAX - target < count) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 MSPI DMA request is not ready");
        return SEMU_ERR_STATE;
    }
    request.controller_id = is_mspi1(mspi) ? "mspi1" : "mspi2";
    request.direction = (value & 4u) != 0u ? SEMU_DMA_TO_ENDPOINT
                                          : SEMU_DMA_FROM_ENDPOINT;
    request.guest_address = target;
    request.count = count;
    request.endpoint = &mspi->endpoint;
    request.continuation = (uint8_t)((value & 0x10u) != 0u);
    request.transaction = NULL;
    request.completion = dma_complete;
    request.completion_context = mspi;
    {
        uint32_t instruction = *reg(mspi, M2_INSTRUCTION);
        uint8_t command = request.direction == SEMU_DMA_FROM_ENDPOINT ?
                              (uint8_t)((instruction >> 16u) & 0xffu) :
                              (uint8_t)(instruction & 0xffu);
        uint32_t address = *reg(mspi, M2_DMA_DEVICE);

        if (request.direction == SEMU_DMA_FROM_ENDPOINT) {
            mspi->dma_buffer[0u] = command;
            if (command == 0x0cu) {
                mspi->dma_buffer[1u] = (uint8_t)(address >> 16u);
                mspi->dma_buffer[2u] = (uint8_t)(address >> 8u);
                mspi->dma_buffer[3u] = (uint8_t)address;
                mspi->dma_transaction.tx_size = 4u;
            } else {
                mspi->dma_transaction.tx_size = 1u;
            }
            mspi->dma_transaction.address = (uint8_t)(address >> 24u);
            mspi->dma_transaction.chip_select = 0u;
            mspi->dma_transaction.tx = mspi->dma_buffer;
            mspi->dma_transaction.rx = NULL;
            mspi->dma_transaction.rx_size = 0u;
            request.transaction = &mspi->dma_transaction;
        } else if (command == 0x12u) {
            if (count > sizeof(mspi->dma_buffer) - 4u ||
                semu_bus_copy_out(mspi->bus, target, mspi->dma_buffer + 4u,
                                  count, error) != SEMU_OK) {
                return SEMU_ERR_RANGE;
            }
            mspi->dma_buffer[0u] = command;
            mspi->dma_buffer[1u] = (uint8_t)(address >> 16u);
            mspi->dma_buffer[2u] = (uint8_t)(address >> 8u);
            mspi->dma_buffer[3u] = (uint8_t)address;
            mspi->dma_transaction.address = (uint8_t)(address >> 24u);
            mspi->dma_transaction.chip_select = 0u;
            mspi->dma_transaction.tx = mspi->dma_buffer;
            mspi->dma_transaction.tx_size = (size_t)count + 4u;
            mspi->dma_transaction.rx = NULL;
            mspi->dma_transaction.rx_size = 0u;
            request.transaction = &mspi->dma_transaction;
        }
    }
    result = mspi->dma_sink(mspi->dma_context, &request, error);
    if (result == SEMU_TRANSACTION_REFUSE) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 MSPI DMA request was refused");
        return SEMU_ERR_STATE;
    }
    *reg(mspi, M2_DMA_CONFIG) = value & ~UINT32_C(0x3);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_apollo4_mspi *semu_apollo4_mspi_create(
    semu_bus *bus, uint32_t base, unsigned irq,
    semu_apollo4_mspi_irq_fn irq_sink, void *irq_context,
    semu_dma_request_sink_fn dma_sink, void *dma_context, semu_error *error)
{
    semu_apollo4_mspi *mspi;
    if (bus == NULL || (base != SEMU_APOLLO4_MSPI1_BASE &&
                        base != SEMU_APOLLO4_MSPI2_BASE) ||
        (base == SEMU_APOLLO4_MSPI1_BASE && irq != SEMU_APOLLO4_MSPI1_IRQ) ||
        (base == SEMU_APOLLO4_MSPI2_BASE && irq != SEMU_APOLLO4_MSPI2_IRQ)) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid Apollo4 MSPI base or IRQ");
        return NULL;
    }
    mspi = (semu_apollo4_mspi *)calloc(1u, sizeof(*mspi));
    if (mspi == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4 MSPI");
        return NULL;
    }
    mspi->bus = bus;
    mspi->base = base;
    mspi->irq_number = irq;
    mspi->irq = irq_sink;
    mspi->irq_context = irq_context;
    mspi->dma_sink = dma_sink;
    mspi->dma_context = dma_context;
    if (semu_bus_map_device(bus, "apollo4.mspi", base,
                            SEMU_APOLLO4_MSPI_SIZE, &mspi_ops, mspi,
                            error) != SEMU_OK) {
        free(mspi);
        return NULL;
    }
    semu_error_clear(error);
    return mspi;
}

void semu_apollo4_mspi_destroy(semu_apollo4_mspi *mspi)
{
    free(mspi);
}

void semu_apollo4_mspi_reset(void *context)
{
    semu_apollo4_mspi *mspi = (semu_apollo4_mspi *)context;
    if (mspi == NULL) return;
    memset(mspi->registers, 0, sizeof(mspi->registers));
    mspi->status = 0u;
    mspi->dma_status = 0u;
    if (mspi->irq_level != 0 && mspi->irq != NULL)
        mspi->irq(mspi->irq_context, mspi->irq_number, 0);
    mspi->irq_level = 0;
}

semu_status semu_apollo4_mspi_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    semu_apollo4_mspi *mspi = (semu_apollo4_mspi *)context;
    semu_status access_status = valid_access(mspi, offset, width, error);
    if (access_status != SEMU_OK) return access_status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 MSPI read value required");
        return SEMU_ERR_ARGUMENT;
    }
    if (!known_offset(mspi, offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI offset 0x%03x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset == INTSTAT) {
        *value = mspi->status;
    }
    else if (offset == M2_DMA_STATUS && !is_mspi1(mspi)) *value = mspi->dma_status;
    else *value = *reg(mspi, offset);
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_mspi_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    semu_apollo4_mspi *mspi = (semu_apollo4_mspi *)context;
    semu_status status;
    semu_status access_status = valid_access(mspi, offset, width, error);
    if (access_status != SEMU_OK) return access_status;
    if (!known_offset(mspi, offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI offset 0x%03x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (offset == INTEN) {
        *reg(mspi, offset) = value;
        update_irq(mspi);
        return SEMU_OK;
    }
    if (offset == INTCLR) {
        mspi->status &= ~value;
        update_irq(mspi);
        return SEMU_OK;
    }
    if (offset == INTSET) {
        mspi->status |= value;
        update_irq(mspi);
        return SEMU_OK;
    }
    if (!is_mspi1(mspi) && offset == M2_DMA_STATUS) {
        mspi->dma_status &= ~value;
        semu_error_clear(error);
        return SEMU_OK;
    }
    if (is_mspi1(mspi) && offset == M1_QUEUE_COUNT && value != 3u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 MSPI queue count is not evidenced");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (!is_mspi1(mspi) && offset == M2_DMA_CONFIG && value != 0u)
        return emit_dma(mspi, value, error);
    if (is_mspi1(mspi) && offset == M1_CONTROL && value == 3u) {
        status = complete_descriptor(mspi, *reg(mspi, M1_DESCRIPTOR),
                                     M1_COMMAND_DONE, error);
        if (status != SEMU_OK) return status;
        *reg(mspi, offset) = value;
    } else if (is_mspi1(mspi) && offset == M1_QUEUE_CONTROL && value == 0x13u) {
        status = complete_queue(mspi, error);
        if (status != SEMU_OK) return status;
        *reg(mspi, offset) = value;
    } else if (!is_mspi1(mspi) && offset == M2_COMMAND) {
        uint8_t command = (uint8_t)(*reg(mspi, M2_DATA) & 0xffu);
        uint8_t frame[4];
        uint32_t address = 0u;
        size_t frame_size = 1u;

        if (value != 0xc1u && value != 0xe1u) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 MSPI2 command control is not evidenced");
            return SEMU_ERR_UNSUPPORTED;
        }
        frame[0u] = command;
        if (command == 0x21u) {
            address = *reg(mspi, M2_ADDRESS);
            frame[1u] = (uint8_t)(address >> 16u);
            frame[2u] = (uint8_t)(address >> 8u);
            frame[3u] = (uint8_t)address;
            frame_size = sizeof(frame);
        }
        /* E-A4-MSPI-001 records command 0x35 as a controller completion
           boundary; it does not issue a flash transaction. */
        if (command == 0x06u || command == 0x21u) {
            status = endpoint_transfer(mspi, frame, frame_size, address, error);
            if (status != SEMU_OK) return status;
        }
        *reg(mspi, offset) = value;
        mspi->status |= M2_COMMAND_DONE;
        update_irq(mspi);
    } else {
        *reg(mspi, offset) = value;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_mspi_bus_ops(void)
{
    return &mspi_ops;
}

semu_status semu_apollo4_mspi_attach_endpoint(
    semu_apollo4_mspi *mspi, const semu_serial_endpoint *endpoint,
    semu_error *error)
{
    if (mspi == NULL || endpoint == NULL || endpoint->transfer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "invalid Apollo4 MSPI endpoint");
        return SEMU_ERR_ARGUMENT;
    }
    if (mspi->endpoint_attached) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 MSPI endpoint is already attached");
        return SEMU_ERR_STATE;
    }
    mspi->endpoint = *endpoint;
    mspi->endpoint_attached = 1;
    semu_error_clear(error);
    return SEMU_OK;
}
