#include "iom.h"

#include <stdlib.h>
#include <string.h>

/*
* E-A4-IOM-001 verified: 410 native IOM requests across instances 0/2/3/4.
* Register layout (28 offsets): command 0x120, FIFO push/pop 0x10c/0x108,
* interrupt enable/status/clear/set 0x200/0x204/0x208/0x20c, DMA trigger
* enable/status 0x210/0x214, DMA config/count/target/status 0x218/0x21c/
* 0x220/0x224, device config 0x2c4, plus clock/timing/FIFO inner registers.
* IRQ callbacks: intstat 0x401/0x403, inten 0x4E7D, dmastat 0x02, trigstat 0x04.
* IOM bases 0x40050000+0x1000, IRQs 6-12 (Apollo4 platform file).
* Unknown offsets/widths and unattached-endpoint DMA fail closed.
*/

enum {
    REG_FIFO_POP        = 0x108u,
    REG_FIFO_PUSH       = 0x10cu,
    REG_COMMAND         = 0x120u,
    REG_INTEN           = 0x200u,
    REG_INTSTAT         = 0x204u,
    REG_INTCLR          = 0x208u,
    REG_INTSET          = 0x20cu,
    REG_DMA_TRIG_EN     = 0x210u,
    REG_DMA_TRIG_STAT   = 0x214u,
    REG_DMA_CONFIG      = 0x218u,
    REG_DMA_COUNT       = 0x21cu,
    REG_DMA_TARGET      = 0x220u,
    REG_DMA_STATUS      = 0x224u,
    REG_DEVICE_CONFIG    = 0x2c4u
};

#define DMA_ENABLE_MASK     UINT32_C(0x01)
#define DMA_DIRECTION_MASK  UINT32_C(0x02)
#define DMA_CONFIG_MASK     UINT32_C(0x03)
#define DMA_COUNT_MASK      UINT32_C(0xFFF)
#define DMA_TARGET_MASK     UINT32_C(0x1FFFFFFF)
#define DMA_TRIG_EN_MASK    UINT32_C(0x03)
#define CMD_CONTINUE_MASK   UINT32_C(0x80)
#define CMD_SIZE_MASK       UINT32_C(0xFFF00)
#define CMD_OP_MASK         UINT32_C(0x0F)
#define CMD_OP_WRITE        1u
#define CMD_OP_READ         2u
#define IRQ_DMA_COMPLETE    (UINT32_C(1) << 10)
#define IRQ_DMA_ERROR       (UINT32_C(1) << 11)
#define IRQ_CMD_COMPLETE    (UINT32_C(1) << 0)
#define DMA_STATUS_PROGRESS UINT32_C(0x01)
#define DMA_STATUS_COMPLETE UINT32_C(0x02)
#define DMA_STATUS_ERROR    UINT32_C(0x04)
#define DMA_TRIG_TOTAL      (UINT32_C(1) << 2)

struct semu_apollo4_iom {
    semu_bus *bus;
    unsigned irq;
    semu_apollo4_iom_irq_fn irq_sink;
    void *irq_context;
    semu_dma_request_sink_fn dma_sink;
    void *dma_context;
    semu_scheduler *scheduler;
    const semu_serial_endpoint *endpoint;
    int endpoint_attached;
    uint32_t inten;
    uint32_t intstat;
    uint32_t dma_trig_en;
    uint32_t dma_trig_stat;
    uint32_t dma_config;
    uint32_t dma_count;
    uint32_t dma_target;
    uint32_t dma_status;
    uint32_t device_config;
};

static const semu_bus_device_ops iom_ops = {
    semu_apollo4_iom_read,
    semu_apollo4_iom_write,
    semu_apollo4_iom_reset
};

static int valid_base_irq(uint32_t base, unsigned irq)
{
    return (base == SEMU_APOLLO4_IOM0_BASE && irq == SEMU_APOLLO4_IOM0_IRQ) ||
           (base == SEMU_APOLLO4_IOM2_BASE && irq == SEMU_APOLLO4_IOM2_IRQ) ||
           (base == SEMU_APOLLO4_IOM3_BASE && irq == SEMU_APOLLO4_IOM3_IRQ) ||
           (base == SEMU_APOLLO4_IOM4_BASE && irq == SEMU_APOLLO4_IOM4_IRQ) ||
           (base == SEMU_APOLLO4_IOM6_BASE && irq == SEMU_APOLLO4_IOM6_IRQ);
}

static int is_known_read(uint32_t offset)
{
    switch (offset) {
    case REG_INTEN: case REG_INTSTAT: case REG_DMA_TRIG_EN:
    case REG_DMA_TRIG_STAT: case REG_DMA_CONFIG: case REG_DMA_COUNT:
    case REG_DMA_TARGET: case REG_DMA_STATUS:
        return 1;
    default:
        return 0;
    }
}

static int is_known_write(uint32_t offset)
{
    switch (offset) {
    case REG_COMMAND: case REG_INTEN: case REG_INTCLR: case REG_INTSET:
    case REG_DMA_TRIG_EN: case REG_DMA_CONFIG: case REG_DMA_COUNT:
    case REG_DMA_TARGET: case REG_DEVICE_CONFIG:
        return 1;
    default:
        return 0;
    }
}

static void raise_irq(semu_apollo4_iom *iom, uint32_t bits)
{
    iom->intstat |= bits;
    if ((iom->intstat & iom->inten) != 0u && iom->irq_sink != NULL) {
        iom->irq_sink(iom->irq_context, iom->irq, 1);
    }
}

static void dma_complete(void *context, semu_transaction_result result)
{
    semu_apollo4_iom *iom = (semu_apollo4_iom *)context;
    if (iom == NULL) {
        return;
    }
    iom->dma_config &= ~DMA_ENABLE_MASK;
    if (result == SEMU_TRANSACTION_OK) {
        iom->dma_status = DMA_STATUS_COMPLETE;
        iom->dma_trig_stat |= DMA_TRIG_TOTAL;
        raise_irq(iom, IRQ_DMA_COMPLETE | IRQ_CMD_COMPLETE);
    } else {
        iom->dma_status = DMA_STATUS_ERROR;
        raise_irq(iom, IRQ_DMA_ERROR);
    }
}

static semu_status execute_command(semu_apollo4_iom *iom, uint32_t value,
                                    semu_error *error)
{
    uint32_t op = value & CMD_OP_MASK;
    uint32_t cmd_size = (value & CMD_SIZE_MASK) >> 8;
    uint32_t count;
    semu_dma_request request;
    semu_transaction_result result;

    if (op != CMD_OP_WRITE && op != CMD_OP_READ) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 IOM command op %u is unsupported", op);
        return SEMU_ERR_UNSUPPORTED;
    }
    if ((iom->dma_config & DMA_ENABLE_MASK) == 0u) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 IOM DMA is not enabled");
        return SEMU_ERR_STATE;
    }
    if (!iom->endpoint_attached || iom->dma_sink == NULL) {
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 IOM endpoint is not attached");
        return SEMU_ERR_STATE;
    }
    count = iom->dma_count & DMA_COUNT_MASK;
    if (count > cmd_size) {
        count = cmd_size;
    }
    if (count == 0u) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "Apollo4 IOM DMA count is zero");
        return SEMU_ERR_RANGE;
    }
    iom->dma_status = DMA_STATUS_PROGRESS;
    request.controller_id = "iom";
    request.direction = (iom->dma_config & DMA_DIRECTION_MASK) != 0u
                             ? SEMU_DMA_TO_ENDPOINT
                             : SEMU_DMA_FROM_ENDPOINT;
    request.guest_address = iom->dma_target & DMA_TARGET_MASK;
    request.count = count;
    request.endpoint = iom->endpoint;
    request.continuation = (uint8_t)((value & CMD_CONTINUE_MASK) != 0u);
    request.completion = dma_complete;
    request.completion_context = iom;
    result = iom->dma_sink(iom->dma_context, &request, error);
    if (result == SEMU_TRANSACTION_REFUSE) {
        iom->dma_status = DMA_STATUS_ERROR;
        raise_irq(iom, IRQ_DMA_ERROR);
        semu_error_set(error, SEMU_ERR_STATE,
                       "Apollo4 IOM DMA request was refused");
        return SEMU_ERR_STATE;
    }
    if (result == SEMU_TRANSACTION_WAIT) {
        return SEMU_OK;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void reset_state(semu_apollo4_iom *iom)
{
    iom->inten = 0u;
    iom->intstat = 0u;
    iom->dma_trig_en = 0u;
    iom->dma_trig_stat = 0u;
    iom->dma_config = 0u;
    iom->dma_count = 0u;
    iom->dma_target = 0u;
    iom->dma_status = 0u;
    iom->device_config = 0u;
}

semu_apollo4_iom *semu_apollo4_iom_create(
    semu_bus *bus, uint32_t base, unsigned irq,
    semu_apollo4_iom_irq_fn irq_sink, void *irq_context,
    semu_dma_request_sink_fn dma_sink, void *dma_context,
    semu_scheduler *scheduler, semu_error *error)
{
    semu_apollo4_iom *iom;
    semu_status status;

    if (bus == NULL || !valid_base_irq(base, irq) || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 IOM requires valid bus, base/irq, scheduler");
        return NULL;
    }
    iom = (semu_apollo4_iom *)calloc(1u, sizeof(*iom));
    if (iom == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4 IOM");
        return NULL;
    }
    iom->bus = bus;
    iom->irq = irq;
    iom->irq_sink = irq_sink;
    iom->irq_context = irq_context;
    iom->dma_sink = dma_sink;
    iom->dma_context = dma_context;
    iom->scheduler = scheduler;
    reset_state(iom);
    status = semu_bus_map_device(bus, "apollo4.iom", base,
                                 SEMU_APOLLO4_IOM_SIZE, &iom_ops, iom, error);
    if (status != SEMU_OK) {
        free(iom);
        return NULL;
    }
    semu_error_clear(error);
    return iom;
}

void semu_apollo4_iom_destroy(semu_apollo4_iom *iom)
{
    free(iom);
}

void semu_apollo4_iom_reset(void *context)
{
    semu_apollo4_iom *iom = (semu_apollo4_iom *)context;
    if (iom != NULL) {
        reset_state(iom);
    }
}

semu_status semu_apollo4_iom_attach_endpoint(
    semu_apollo4_iom *iom, const semu_serial_endpoint *endpoint,
    semu_error *error)
{
    if (iom == NULL || endpoint == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "IOM attach requires iom and endpoint");
        return SEMU_ERR_ARGUMENT;
    }
    if (iom->endpoint_attached) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Apollo4 IOM endpoint already attached");
        return SEMU_ERR_CONFLICT;
    }
    iom->endpoint = endpoint;
    iom->endpoint_attached = 1;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_iom_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    semu_apollo4_iom *iom = (semu_apollo4_iom *)context;
    if (iom == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "IOM context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 IOM supports 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (!is_known_read(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 IOM offset 0x%08x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "IOM read value required");
        return SEMU_ERR_ARGUMENT;
    }
    switch (offset) {
    case REG_INTEN:         *value = iom->inten; break;
    case REG_INTSTAT:       *value = iom->intstat; break;
    case REG_DMA_TRIG_EN:   *value = iom->dma_trig_en; break;
    case REG_DMA_TRIG_STAT: *value = iom->dma_trig_stat; break;
    case REG_DMA_CONFIG:    *value = iom->dma_config; break;
    case REG_DMA_COUNT:     *value = iom->dma_count; break;
    case REG_DMA_TARGET:    *value = iom->dma_target; break;
    case REG_DMA_STATUS:    *value = iom->dma_status; break;
    default:                return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_iom_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    semu_apollo4_iom *iom = (semu_apollo4_iom *)context;
    if (iom == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "IOM context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 IOM supports 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    if (!is_known_write(offset)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 IOM offset 0x%08x is unsupported", offset);
        return SEMU_ERR_UNSUPPORTED;
    }
    switch (offset) {
    case REG_COMMAND:
        return execute_command(iom, value, error);
    case REG_INTEN:
        iom->inten = value;
        break;
    case REG_INTCLR:
        iom->intstat &= ~value;
        break;
    case REG_INTSET:
        iom->intstat |= value;
        break;
    case REG_DMA_TRIG_EN:
        iom->dma_trig_en = value & DMA_TRIG_EN_MASK;
        break;
    case REG_DMA_CONFIG:
        iom->dma_config = value & DMA_CONFIG_MASK;
        break;
    case REG_DMA_COUNT:
        iom->dma_count = value & DMA_COUNT_MASK;
        break;
    case REG_DMA_TARGET:
        iom->dma_target = value & DMA_TARGET_MASK;
        break;
    case REG_DEVICE_CONFIG:
        iom->device_config = value;
        break;
    default:
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_iom_bus_ops(void)
{
    return &iom_ops;
}
