#ifndef SEMU_DEVICES_SAPPORO_IOM4_INTERNAL_H
#define SEMU_DEVICES_SAPPORO_IOM4_INTERNAL_H

#include "sapporo_iom4.h"
#include "sapporo_iom4_gauge.h"

enum {
    R_FIFO_IN = 0x020u, R_FIFO_SIZE = 0x100u, R_FIFO_THRESH = 0x104u,
    R_FIFO_POP = 0x108u, R_FIFO_PUSH = 0x10cu, R_FIFO_CTRL = 0x110u,
    R_FIFO_PTRS = 0x114u, R_IOCLK = 0x118u, R_SUBMOD = 0x11cu,
    R_COMMAND = 0x120u, R_DCX = 0x124u, R_OFFSET_HI = 0x128u,
    R_CMDSTAT = 0x12cu, R_INTEN = 0x200u, R_INTSTAT = 0x204u,
    R_INTCLR = 0x208u, R_INTSET = 0x20cu, R_TRIG_EN = 0x210u,
    R_TRIG_STAT = 0x214u, R_DMA_CFG = 0x218u, R_DMA_TOTAL = 0x21cu,
    R_DMA_TARGET = 0x220u, R_DMA_STATUS = 0x224u,
    R_CQ_CONF = 0x228u, R_CQ_TARGET = 0x22cu, R_CQ_FLAG = 0x234u,
    R_CQ_PAUSE = 0x23cu, R_CQ_CUR = 0x240u, R_CQ_END = 0x244u,
    R_MODULE_STATUS = 0x248u, R_SPI_CFG = 0x280u, R_I2C_CFG = 0x2c0u,
    R_DEVICE_CFG = 0x2c4u
};

#define INT_CMD 0x0001u
#define INT_FIFOTH 0x0002u
#define INT_RDUND 0x0004u
#define INT_WROVF 0x0008u
#define INT_ILLCMD 0x0040u
#define INT_DMA_CMP 0x0400u
#define INT_DMA_ERR 0x0800u
#define INT_MASK 0x7fffu
#define ST_ERROR 1u
#define ST_ACTIVE 2u
#define ST_IDLE 4u
#define ST_WAIT 6u
#define FIFO_WORDS 8u
#define ADDR_OBSERVED 0x28u
#define ADDR_GAUGE 0x36u
#define ADDR_HAPTIC 0x50u

struct semu_sapporo_iom4 {
    semu_bus *bus;
    semu_sapporo_iom4_irq_fn irq_sink;
    void *irq_context;
    int irq_level;
    uint32_t out_words[FIFO_WORDS], in_words[FIFO_WORDS];
    unsigned out_tail, in_tail, out_count, in_count;
    uint32_t thr_read, thr_write;
    int popwr, fifo_run;
    uint32_t ioclk;
    int spi_en, i2c_en;
    uint32_t offset_hi, cmd_reg;
    unsigned active_cmd;
    int active_cont;
    uint32_t size_left, cmdstat_status, engine_status;
    uint32_t inten, intstat;
    uint32_t trig_en, trig_stat, dma_cfg, dma_total, dma_target, dma_status;
    uint32_t i2c_cfg, devconf;
    int loaded_dma;
    uint32_t dma_active_count;
    semu_sapporo_iom4_gauge gauge;
    uint8_t haptic_registers[0x44];
    uint8_t haptic_selected;
};

void update_irq(semu_sapporo_iom4 *m);
void int_set(semu_sapporo_iom4 *m, uint32_t bit, int value);
void update_threshold(semu_sapporo_iom4 *m);
int out_push(semu_sapporo_iom4 *m, uint32_t word);
uint32_t in_pop(semu_sapporo_iom4 *m);
semu_status command_write(semu_sapporo_iom4 *m, uint32_t value,
                          semu_error *error);

semu_status haptic_validate(const semu_sapporo_iom4 *m, uint32_t command,
                            const uint8_t *source, semu_error *error);
void haptic_write(semu_sapporo_iom4 *m, const uint8_t *data, uint32_t n);
void haptic_read(semu_sapporo_iom4 *m, uint8_t *bytes, uint32_t n);

#endif
