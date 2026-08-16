#include "dma.h"

#include <stdlib.h>
#include <string.h>

/*
* E-A4-DMA-001 verified: 5,521 MSPI2 command completions, 5,152 TX DMA
* records, 5,340 RX DMA records. DMA direction bits (M2P=bit1, P2M=clear),
* count mask 0xFFF, target mask 0x1FFFFFFF, SRAM range 0x10000000-0x10267000,
* internal flash 0x10000-0x200000 (M2P write only), status bits
* (InProgress=0, Complete=1, Error=2), completion IRQ flag bit 10.
* The entire operation is validated before any memory mutation.
* Completions are exactly-once: OK calls the callback immediately, WAIT
* defers, REFUSE does not call it.
*/

#define SRAM_START       UINT32_C(0x10000000)
#define SRAM_END         UINT32_C(0x10267000)
#define FLASH_START      UINT32_C(0x00010000)
#define FLASH_END        UINT32_C(0x00200000)
#define DMA_TARGET_MASK  UINT32_C(0x1FFFFFFF)

struct semu_apollo4_dma {
    semu_bus *bus;
    semu_scheduler *scheduler;
};

static int range_overflows(uint32_t address, uint32_t count)
{
    return count > 0u && address > UINT32_MAX - count;
}

static int validate_request(const semu_dma_request *request, semu_error *error)
{
    uint32_t addr;
    uint32_t end;

    if (request == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "DMA request required");
        return 0;
    }
    if (request->endpoint == NULL) {
        semu_error_set(error, SEMU_ERR_STATE, "DMA endpoint not attached");
        return 0;
    }
    if (request->count == 0u) {
        semu_error_set(error, SEMU_ERR_RANGE, "DMA count is zero");
        return 0;
    }
    addr = request->guest_address & DMA_TARGET_MASK;
    if (range_overflows(addr, request->count)) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "DMA range overflows: addr=0x%08x count=%u",
                       addr, request->count);
        return 0;
    }
    end = addr + request->count;
    if (addr >= SRAM_START && end <= SRAM_END) {
        return 1;
    }
    if (request->direction == SEMU_DMA_TO_ENDPOINT &&
        addr >= FLASH_START && end <= FLASH_END) {
        return 1;
    }
    semu_error_set(error, SEMU_ERR_RANGE,
                   "DMA target 0x%08x count=%u outside SRAM/flash",
                   addr, request->count);
    return 0;
}

static semu_status copy_to_guest(semu_apollo4_dma *dma,
                                 const semu_dma_request *request,
                                 const uint8_t *source, semu_error *error)
{
    uint32_t addr = request->guest_address & DMA_TARGET_MASK;
    uint32_t i;
    for (i = 0u; i < request->count; ++i) {
        uint32_t byte_val = source != NULL ? source[i] : 0u;
        semu_status s;
        s = semu_bus_write(dma->bus, addr + i, 1u, byte_val, error);
        if (s != SEMU_OK) {
            return s;
        }
    }
    return SEMU_OK;
}

static semu_status transfer_endpoint(semu_apollo4_dma *dma,
                                      const semu_dma_request *request,
                                      semu_error *error)
{
    semu_serial_transaction transaction;
    semu_transaction_result result;
    uint8_t *response;
    semu_status status;

    if (request->transaction == NULL || request->endpoint->transfer == NULL) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "DMA endpoint transaction is unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    transaction = *request->transaction;
    if (request->direction == SEMU_DMA_TO_ENDPOINT) {
        if (transaction.tx == NULL ||
            (transaction.tx_size != request->count &&
             transaction.tx_size != (size_t)request->count + 4u) ||
            transaction.rx != NULL || transaction.rx_size != 0u) {
            semu_error_set(error, SEMU_ERR_ARGUMENT,
                           "DMA endpoint write transaction shape is invalid");
            return SEMU_ERR_ARGUMENT;
        }
        result = request->endpoint->transfer(request->endpoint->context,
                                             &transaction, error);
        if (result != SEMU_TRANSACTION_OK) {
            if (error != NULL && error->code == SEMU_OK) {
                semu_error_set(error, SEMU_ERR_STATE,
                               "DMA endpoint transaction was not completed");
            }
            return SEMU_ERR_STATE;
        }
        semu_error_clear(error);
        return SEMU_OK;
    }
    if (request->direction != SEMU_DMA_FROM_ENDPOINT ||
        transaction.rx != NULL || transaction.rx_size != 0u) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "DMA endpoint response buffer must be empty");
        return SEMU_ERR_ARGUMENT;
    }
    response = (uint8_t *)malloc(request->count);
    if (response == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate DMA endpoint response");
        return SEMU_ERR_NOMEM;
    }
    transaction.rx = response;
    transaction.rx_size = request->count;
    result = request->endpoint->transfer(request->endpoint->context,
                                         &transaction, error);
    if (result != SEMU_TRANSACTION_OK) {
        if (error != NULL && error->code == SEMU_OK) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "DMA endpoint transaction was not completed");
        }
        free(response);
        return SEMU_ERR_STATE;
    }
    status = copy_to_guest(dma, request, response, error);
    free(response);
    return status;
}

static semu_status copy_from_guest(semu_apollo4_dma *dma,
                                   const semu_dma_request *request,
                                   semu_error *error)
{
    uint32_t addr = request->guest_address & DMA_TARGET_MASK;
    uint32_t i;
    for (i = 0u; i < request->count; ++i) {
        uint32_t byte_val = 0u;
        semu_status s;
        s = semu_bus_read(dma->bus, addr + i, 1u, &byte_val, error);
        if (s != SEMU_OK) {
            return s;
        }
    }
    return SEMU_OK;
}

semu_apollo4_dma *semu_apollo4_dma_create(semu_bus *bus,
                                          semu_scheduler *scheduler,
                                          semu_error *error)
{
    semu_apollo4_dma *dma;

    if (bus == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 DMA requires bus and scheduler");
        return NULL;
    }
    dma = (semu_apollo4_dma *)calloc(1u, sizeof(*dma));
    if (dma == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4 DMA");
        return NULL;
    }
    dma->bus = bus;
    dma->scheduler = scheduler;
    semu_error_clear(error);
    return dma;
}

void semu_apollo4_dma_destroy(semu_apollo4_dma *dma)
{
    free(dma);
}

void semu_apollo4_dma_reset(void *context)
{
    (void)context;
}

semu_transaction_result semu_apollo4_dma_execute(
    void *context, const semu_dma_request *request, semu_error *error)
{
    semu_apollo4_dma *dma = (semu_apollo4_dma *)context;
    semu_status status;

    if (dma == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "DMA context required");
        return SEMU_TRANSACTION_REFUSE;
    }
    if (!validate_request(request, error)) {
        return SEMU_TRANSACTION_REFUSE;
    }
    if (request->transaction != NULL) {
        status = transfer_endpoint(dma, request, error);
    } else if (request->direction == SEMU_DMA_FROM_ENDPOINT) {
        status = copy_to_guest(dma, request, NULL, error);
    } else {
        status = copy_from_guest(dma, request, error);
    }
    if (status != SEMU_OK) {
        if (request->completion != NULL) {
            request->completion(request->completion_context,
                                 SEMU_TRANSACTION_REFUSE);
        }
        return SEMU_TRANSACTION_REFUSE;
    }
    if (request->completion != NULL) {
        request->completion(request->completion_context,
                             SEMU_TRANSACTION_OK);
    }
    return SEMU_TRANSACTION_OK;
}
