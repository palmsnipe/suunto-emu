#include "auxiliary.h"

#include "adc.h"
#include "apollo4_internal.h"
#include "../../devices/sapporo_rtc.h"

#include <stddef.h>

enum {
    CPU_COMPLEX_BASE = 0x48000000u,
    CPU_COMPLEX_SIZE = 0x1000u,
    SYNC_READ_BASE = 0x47ff0000u,
    RTC_BASE = 0x40004800u,
    BOOTROM_BASE = 0x08000000u,
    BOOTROM_SIZE = 0x1000u
};

typedef struct bootrom_halfword {
    uint16_t offset;
    uint16_t value;
} bootrom_halfword;

/* These are the instruction words documented by the Apollo4 Renode platform.
 * Unlisted offsets intentionally refuse reads instead of becoming RAM. */
static const bootrom_halfword bootrom_words[] = {
    { 0x30u, 0xf8dfu }, { 0x32u, 0x3014u },
    { 0x34u, 0xf8c3u }, { 0x36u, 0xe000u },
    { 0x38u, 0x4718u },
    { 0x48u, 0xfffcu }, { 0x4au, 0x07ffu },
    { 0x4cu, 0xf7ffu }, { 0x4eu, 0xfff0u },
    { 0x50u, 0xf7ffu }, { 0x52u, 0xffeeu },
    { 0x54u, 0xf7ffu }, { 0x56u, 0xffecu },
    { 0x58u, 0xf7ffu }, { 0x5au, 0xffeau },
    { 0x6cu, 0x2900u }, { 0x6eu, 0xd000u },
    { 0x70u, 0xe0d6u }, { 0x72u, 0xe0c5u },
    { 0x74u, 0x6800u }, { 0x76u, 0x4770u },
    { 0x78u, 0xf7ffu }, { 0x7au, 0xffdau },
    { 0x80u, 0xf7ffu }, { 0x82u, 0xffd6u },
    { 0x98u, 0xf7ffu }, { 0x9au, 0xffcau },
    { 0x9cu, 0x300fu }, { 0x9eu, 0x3801u },
    { 0xa0u, 0x2800u }, { 0xa2u, 0xd1fcu },
    { 0xa4u, 0x4770u },
    { 0x200u, 0x9800u }, { 0x202u, 0x0080u },
    { 0x204u, 0x009bu }, { 0x206u, 0x501au },
    { 0x208u, 0x3804u }, { 0x20au, 0x2800u },
    { 0x20cu, 0xdafbu }, { 0x20eu, 0x2000u },
    { 0x210u, 0x4770u },
    { 0x220u, 0x9800u }, { 0x222u, 0x0080u },
    { 0x224u, 0x009bu }, { 0x226u, 0x5811u },
    { 0x228u, 0x5019u }, { 0x22au, 0x3804u },
    { 0x22cu, 0x2800u }, { 0x22eu, 0xdafau },
    { 0x230u, 0x2000u }, { 0x232u, 0x4770u }
};

static semu_status cpu_complex_read(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error)
{
    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 CPU complex read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= CPU_COMPLEX_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 CPU complex read offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = offset == 0x54u ? 0x4u : 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status cpu_complex_write(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error)
{
    (void)value;
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 CPU complex write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset >= CPU_COMPLEX_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 CPU complex write offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void cpu_complex_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops cpu_complex_ops = {
    cpu_complex_read, cpu_complex_write, cpu_complex_reset
};

static semu_status sync_read_read(void *context, uint32_t offset,
                                  unsigned width, uint32_t *value,
                                  semu_error *error)
{
    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 sync read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 sync read offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status sync_read_write(void *context, uint32_t offset,
                                   unsigned width, uint32_t value,
                                   semu_error *error)
{
    (void)value;
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 sync write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || offset != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 sync write offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void sync_read_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops sync_read_ops = {
    sync_read_read, sync_read_write, sync_read_reset
};

static semu_status rtc_read(void *context, uint32_t offset, unsigned width,
                            uint32_t *value, semu_error *error)
{
    semu_apollo4 *soc = (semu_apollo4 *)context;

    if (soc != NULL && soc->rtc_live != 0) {
        return semu_sapporo_rtc_ops()->read(soc->rtc, offset, width, value,
                                            error);
    }
    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 RTC read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset != 0u && offset != 0x20u &&
                        offset != 0x24u && offset != 0x30u &&
                        offset != 0x200u && offset != 0x208u)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 RTC offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status rtc_write(void *context, uint32_t offset, unsigned width,
                             uint32_t value, semu_error *error)
{
    semu_apollo4 *soc = (semu_apollo4 *)context;

    if (soc != NULL && soc->rtc_live != 0) {
        return semu_sapporo_rtc_ops()->write(soc->rtc, offset, width, value,
                                             error);
    }
    (void)value;
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 RTC write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset != 0u && offset != 0x30u &&
                        offset != 0x200u && offset != 0x208u)) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 RTC offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static void rtc_reset(void *context)
{
    semu_apollo4 *soc = (semu_apollo4 *)context;

    if (soc != NULL && soc->rtc_live != 0) {
        semu_sapporo_rtc_ops()->reset(soc->rtc);
    }
}

static const semu_bus_device_ops rtc_ops = {
    rtc_read, rtc_write, rtc_reset
};

static semu_status bootrom_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    size_t index;

    if (context == NULL || value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 bootrom read requires context and value");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 2u || (offset & 1u) != 0u || offset >= BOOTROM_SIZE) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 bootrom offset/width unsupported");
        return SEMU_ERR_UNSUPPORTED;
    }
    for (index = 0u; index < sizeof(bootrom_words) / sizeof(bootrom_words[0]);
         ++index) {
        if (bootrom_words[index].offset == offset) {
            *value = bootrom_words[index].value;
            semu_error_clear(error);
            return SEMU_OK;
        }
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 bootrom offset 0x%08x is unsupported", offset);
    return SEMU_ERR_UNSUPPORTED;
}

static semu_status bootrom_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    (void)offset;
    (void)width;
    (void)value;
    if (context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 bootrom write requires context");
        return SEMU_ERR_ARGUMENT;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 bootrom is read-only");
    return SEMU_ERR_UNSUPPORTED;
}

static void bootrom_reset(void *context)
{
    (void)context;
}

static const semu_bus_device_ops bootrom_ops = {
    bootrom_read, bootrom_write, bootrom_reset
};

semu_status semu_apollo4_auxiliary_map(semu_bus *bus, void *context,
                                       semu_error *error)
{
    semu_status status;

    if (bus == NULL || context == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 auxiliary devices require bus and context");
        return SEMU_ERR_ARGUMENT;
    }
    status = semu_apollo4_adc_map(bus, context, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_bus_map_device(bus, "apollo4.cpu_complex",
                                 CPU_COMPLEX_BASE, CPU_COMPLEX_SIZE,
                                 &cpu_complex_ops, context, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_bus_map_device(bus, "apollo4.sync_read", SYNC_READ_BASE,
                                 4u, &sync_read_ops, context, error);
    if (status != SEMU_OK) {
        return status;
    }
    status = semu_bus_map_device(bus, "apollo4.bootrom", BOOTROM_BASE,
                                 BOOTROM_SIZE, &bootrom_ops, context, error);
    if (status != SEMU_OK) {
        return status;
    }
    /* 0x210 = the lane peripheral's IKnownSize (E-SAP-0035); the
     * previous 0x20c cut off InterruptSet at +0x20c. */
    return semu_bus_map_device(bus, "apollo4.rtc", RTC_BASE, 0x210u,
                               &rtc_ops, context, error);
}
