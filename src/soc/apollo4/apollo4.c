#include "apollo4_internal.h"

#include "../../devices/sapporo_iom4.h"
#include "../../devices/sapporo_rtc.h"
#include "../../devices/ulsan_otpinfo.h"

#include <stdlib.h>
#include <string.h>

/*
 * Ticket 320: Apollo4 Phase 3 Integration.
 * Legacy regbanks are replaced by verified module instances. Each module
 * self-registers on the bus during its create function. The reset controller
 * coordinates ordered reset callbacks. Unknown offsets and widths fail
 * closed inside each module. semu_apollo4_create preserves the legacy
 * (bus, error) signature for machine.c; semu_apollo4_init wires the
 * scheduler, CPU IRQ sink, and creates all verified blocks.
 */

static void irq_sink_dispatch(void *context, unsigned irq, int level)
{
    semu_apollo4 *soc = (semu_apollo4 *)context;
    if (soc != NULL && soc->irq_sink != NULL) {
        soc->irq_sink(soc->irq_context, irq, level);
    }
}

static void gpio_irq_adapter(void *context, unsigned irq, int level)
{
    irq_sink_dispatch(context, irq, level);
}

static void timer_irq_adapter(void *context, unsigned channel, int level)
{
    /* E-A4-TIMER-001: CTIMER channels 0..15 are wired to NVIC 67..82. */
    if (channel < 16u) {
        irq_sink_dispatch(context, 67u + channel, level);
    }
}

static void stimer_irq_adapter(void *context, unsigned irq, int level)
{
    /* E-A4-STIMER-001: STIMER exposes its compare IRQs as IRQA..IRQI. */
    if (irq >= SEMU_APOLLO4_STIMER_IRQ_A &&
        irq <= SEMU_APOLLO4_STIMER_IRQ_I) {
        irq_sink_dispatch(context, irq, level);
    }
}

static semu_status clock_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_clock_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status power_reset_cb(void *context, semu_error *error)
{
    semu_apollo4 *soc = (semu_apollo4 *)context;
    semu_apollo4_power_reset(soc->power);
    semu_apollo4_watchdog_reset(soc->watchdog);
    semu_apollo4_rstgen_reset(soc->rstgen);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status mcu_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_mcu_control_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status gpio_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_gpio_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status stimer_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_stimer_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status iom_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_iom_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status dma_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_dma_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status mspi_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_mspi_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status mram_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_mram_reset(context);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status timer_reset_cb(void *context, semu_error *error)
{
    semu_apollo4_timer *timer = (semu_apollo4_timer *)context;
    if (timer != NULL) {
        semu_apollo4_timer_reset(timer);
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status uart_reset_cb(void *context, semu_error *error)
{
    semu_apollo4 *soc = context;
    semu_apollo4_uart_reset(soc->uart);
    semu_apollo4_uart_reset(soc->uart3);
    semu_error_clear(error);
    return SEMU_OK;
}

static semu_status timer_read_wrap(void *context, uint32_t offset,
                                    unsigned width, uint32_t *value,
                                    semu_error *error)
{
    return semu_apollo4_timer_read((semu_apollo4_timer *)context, offset,
                                    width, value, error);
}

static semu_status timer_write_wrap(void *context, uint32_t offset,
                                     unsigned width, uint32_t value,
                                     semu_error *error)
{
    return semu_apollo4_timer_write((semu_apollo4_timer *)context, offset,
                                     width, value, error);
}

static void timer_reset_wrap(void *context)
{
    semu_apollo4_timer *timer = (semu_apollo4_timer *)context;
    if (timer != NULL) {
        semu_apollo4_timer_reset(timer);
    }
}

static const semu_bus_device_ops timer_bus_ops = {
    timer_read_wrap, timer_write_wrap, timer_reset_wrap
};

semu_apollo4 *semu_apollo4_create(semu_bus *bus, semu_error *error)
{
    semu_apollo4 *soc;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "Apollo4 requires a bus");
        return NULL;
    }
    soc = (semu_apollo4 *)calloc(1u, sizeof(*soc));
    if (soc == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "cannot allocate Apollo4");
        return NULL;
    }
    soc->bus = bus;
    memset(soc->gpio_level, 1, sizeof(soc->gpio_level));
    semu_error_clear(error);
    return soc;
}

semu_status semu_apollo4_init(semu_apollo4 *soc, semu_scheduler *scheduler,
                               semu_apollo4_irq_fn irq_sink,
                               void *irq_context, semu_error *error)
{
    static const uint32_t ctimer0_base = 0x40008000u;
    static const uint32_t ctimer0_size = 0x800u;
    semu_dma_request_sink_fn dma_sink;
    semu_status status;

    if (soc == NULL || scheduler == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 init requires soc and scheduler");
        return SEMU_ERR_ARGUMENT;
    }
    if (soc->initialized) {
        semu_error_set(error, SEMU_ERR_CONFLICT,
                       "Apollo4 is already initialized");
        return SEMU_ERR_CONFLICT;
    }
    soc->irq_sink = irq_sink;
    soc->scheduler = scheduler;
    soc->irq_context = irq_context;

    soc->reset_ctrl = semu_apollo4_reset_controller_create(
        soc->bus, scheduler, error);
    if (soc->reset_ctrl == NULL) {
        return error->code;
    }

    soc->clock = semu_apollo4_clock_create(soc->bus, scheduler, error);
    if (soc->clock == NULL) {
        goto fail;
    }
    soc->power = semu_apollo4_power_create(soc->bus, scheduler, NULL, NULL,
                                             error);
    if (soc->power == NULL) {
        goto fail;
    }
    soc->watchdog = semu_apollo4_watchdog_create(soc->bus, error);
    if (soc->watchdog == NULL) {
        goto fail;
    }
    soc->rstgen = semu_apollo4_rstgen_create(soc->bus, error);
    if (soc->rstgen == NULL) {
        goto fail;
    }
    soc->mcu_control = semu_apollo4_mcu_control_create(soc->bus, scheduler,
                                                        error);
    if (soc->mcu_control == NULL) {
        goto fail;
    }
    soc->gpio = semu_apollo4_gpio_create(soc->bus, gpio_irq_adapter, soc,
                                          error);
    if (soc->gpio == NULL) {
        goto fail;
    }
    soc->timer = semu_apollo4_timer_create(scheduler, timer_irq_adapter, soc,
                                             error);
    if (soc->timer == NULL) {
        goto fail;
    }
    status = semu_bus_map_device(soc->bus, "apollo4.ctimer0", ctimer0_base,
                                 ctimer0_size, &timer_bus_ops, soc->timer,
                                 error);
    if (status != SEMU_OK) {
        goto fail;
    }
    soc->stimer = semu_apollo4_stimer_create(soc->bus, scheduler,
                                               stimer_irq_adapter, soc, error);
    if (soc->stimer == NULL) {
        goto fail;
    }
    soc->uart = semu_apollo4_uart_create(scheduler, error);
    if (soc->uart == NULL) {
        goto fail;
    }
    status = semu_apollo4_uart_set_irq_sink(soc->uart, irq_sink_dispatch,
                                             soc, error);
    if (status != SEMU_OK) {
        goto fail;
    }
    status = semu_bus_map_device(soc->bus, "apollo4.uart1",
                                 SEMU_APOLLO4_UART1_BASE, 0x1000u,
                                 semu_apollo4_uart_bus_ops(), soc->uart, error);
    if (status != SEMU_OK) {
        goto fail;
    }
    status = semu_apollo4_auxiliary_map(soc->bus, soc, error);
    if (status != SEMU_OK) {
        goto fail;
    }
    soc->dma = semu_apollo4_dma_create(soc->bus, scheduler, error);
    if (soc->dma == NULL) {
        goto fail;
    }
    dma_sink = semu_apollo4_dma_execute;

    soc->iom0 = semu_apollo4_iom_create(soc->bus, SEMU_APOLLO4_IOM0_BASE,
                                          6u, gpio_irq_adapter, soc,
                                          dma_sink, soc->dma, scheduler, error);
    if (soc->iom0 == NULL) {
        goto fail;
    }
    soc->iom2 = semu_apollo4_iom_create(soc->bus, SEMU_APOLLO4_IOM2_BASE,
                                          8u, gpio_irq_adapter, soc,
                                          dma_sink, soc->dma, scheduler, error);
    if (soc->iom2 == NULL) {
        goto fail;
    }
    soc->iom3 = semu_apollo4_iom_create(soc->bus, SEMU_APOLLO4_IOM3_BASE,
                                          9u, gpio_irq_adapter, soc,
                                          dma_sink, soc->dma, scheduler, error);
    if (soc->iom3 == NULL) {
        goto fail;
    }
    soc->iom4 = semu_apollo4_iom_create(soc->bus, SEMU_APOLLO4_IOM4_BASE,
                                          10u, gpio_irq_adapter, soc,
                                          dma_sink, soc->dma, scheduler, error);
    if (soc->iom4 == NULL) {
        goto fail;
    }
    soc->iom6 = semu_apollo4_iom_create(soc->bus, SEMU_APOLLO4_IOM6_BASE,
                                          12u, gpio_irq_adapter, soc,
                                          dma_sink, soc->dma, scheduler, error);
    if (soc->iom6 == NULL) {
        goto fail;
    }
    soc->mspi1 = semu_apollo4_mspi_create(soc->bus, SEMU_APOLLO4_MSPI1_BASE,
                                           21u, gpio_irq_adapter, soc,
                                           dma_sink, soc->dma, error);
    if (soc->mspi1 == NULL) {
        goto fail;
    }
    soc->mspi2 = semu_apollo4_mspi_create(soc->bus, SEMU_APOLLO4_MSPI2_BASE,
                                           22u, gpio_irq_adapter, soc,
                                           dma_sink, soc->dma, error);
    if (soc->mspi2 == NULL) {
        goto fail;
    }
    soc->mram = semu_apollo4_mram_create(soc->bus, error);
    if (soc->mram == NULL) {
        goto fail;
    }

    if (semu_apollo4_reset_controller_register(soc->reset_ctrl,
            clock_reset_cb, soc->clock, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            power_reset_cb, soc, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            mcu_reset_cb, soc->mcu_control, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            gpio_reset_cb, soc->gpio, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            timer_reset_cb, soc->timer, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            stimer_reset_cb, soc->stimer, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            uart_reset_cb, soc, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            iom_reset_cb, soc->iom0, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            iom_reset_cb, soc->iom2, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            iom_reset_cb, soc->iom3, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            iom_reset_cb, soc->iom4, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            iom_reset_cb, soc->iom6, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            dma_reset_cb, soc->dma, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            mspi_reset_cb, soc->mspi1, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            mspi_reset_cb, soc->mspi2, error) != SEMU_OK ||
        semu_apollo4_reset_controller_register(soc->reset_ctrl,
            mram_reset_cb, soc->mram, error) != SEMU_OK) {
        goto fail;
    }

    soc->initialized = 1;
    semu_error_clear(error);
    return SEMU_OK;

fail:
    semu_apollo4_destroy(soc);
    return error != NULL ? error->code : SEMU_ERR_STATE;
}

void semu_apollo4_destroy(semu_apollo4 *soc)
{
    if (soc == NULL) {
        return;
    }
    soc->irq_sink = NULL;
    if (soc->initialized) {
        semu_apollo4_mram_destroy(soc->mram);
        semu_apollo4_mspi_destroy(soc->mspi2);
        semu_apollo4_mspi_destroy(soc->mspi1);
        semu_apollo4_iom_destroy(soc->iom6);
        semu_apollo4_iom_set_live235(soc->iom4, NULL);
        semu_apollo4_iom_destroy(soc->iom4);
        semu_sapporo_iom4_destroy(soc->iom4_live);
        soc->iom4_live = NULL;
        semu_apollo4_iom_destroy(soc->iom3);
        semu_apollo4_iom_destroy(soc->iom2);
        semu_apollo4_iom_destroy(soc->iom0);
        semu_apollo4_dma_destroy(soc->dma);
        semu_apollo4_uart_destroy(soc->uart3);
        semu_apollo4_uart_destroy(soc->uart);
        semu_apollo4_stimer_destroy(soc->stimer);
        semu_apollo4_timer_destroy(soc->timer);
        semu_apollo4_gpio_destroy(soc->gpio);
        semu_apollo4_mcu_control_destroy(soc->mcu_control);
        semu_apollo4_rstgen_destroy(soc->rstgen);
        semu_apollo4_watchdog_destroy(soc->watchdog);
        semu_apollo4_power_destroy(soc->power);
        semu_apollo4_clock_destroy(soc->clock);
        semu_apollo4_reset_controller_destroy(soc->reset_ctrl);
    }
    semu_sapporo_rtc_destroy(soc->rtc);
    free(soc);
}

void semu_apollo4_reset(semu_apollo4 *soc)
{
    if (soc == NULL) {
        return;
    }
    memset(soc->gpio_level, 1, sizeof(soc->gpio_level));
    if (soc->initialized && soc->reset_ctrl != NULL) {
        semu_apollo4_reset_controller_reset(soc->reset_ctrl);
    }
}

semu_status semu_apollo4_set_gpio_input(semu_apollo4 *soc, unsigned pin,
                                        int level, semu_error *error)
{
    semu_status status;
    if (soc == NULL || pin >= SEMU_APOLLO4_GPIO_COUNT) {
        semu_error_set(error, SEMU_ERR_RANGE, "invalid GPIO pin %u", pin);
        return SEMU_ERR_RANGE;
    }
    status = soc->gpio != NULL
                 ? semu_apollo4_gpio_set_input(soc->gpio, pin, level, error)
                 : SEMU_OK;
    if (status != SEMU_OK) {
        return status;
    }
    soc->gpio_level[pin] = level != 0;
    semu_error_clear(error);
    return SEMU_OK;
}

int semu_apollo4_get_gpio_input(const semu_apollo4 *soc, unsigned pin)
{
    return soc != NULL && pin < SEMU_APOLLO4_GPIO_COUNT
               ? soc->gpio_level[pin] != 0
               : -1;
}

semu_status semu_apollo4_select_profile(semu_apollo4 *soc,
                                        const char *profile_id,
                                        semu_error *error)
{
    if (soc == NULL || profile_id == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 profile selection requires soc and id");
        return SEMU_ERR_ARGUMENT;
    }
    /* E-WIS-0001: the lane's GPIO model logged warn-not-stop writes at
     * offsets 0x340/0x400 and beyond-the-block writes at 0x45c/0x464/0x470
     * while the guest continued on every twice-identical trace. The Wismar
     * profile admits those five writes as plain drops; the shared profiles
     * keep refusing them. Offsets 0x340 stays inside the shared GPIO
     * window; 0x400+ lands past it, so a Wismar-only gap device serves the
     * three recorded addresses and refuses everything else. */
    if (strcmp(profile_id, "wismar-2.46.14") == 0) {
        semu_status status = semu_apollo4_gpio_gap_map(soc->bus, error);
        if (status != SEMU_OK) {
            return status;
        }
        semu_apollo4_gpio_enable_wismar_gap_writes(soc->gpio);
        semu_apollo4_mcu_control_enable_wismar_warn_zero(soc->mcu_control);
        status = semu_apollo4_dwt_gap_map(soc->bus, error);
        if (status != SEMU_OK) {
            return status;
        }
        /* E-ULS-0012 law, same on the Wismar lane: the NVM_OTP/INFO1 tag
         * range has no peripheral, and the boot's two INFO1 reads
         * (0x42003240/0x42003310) answer 0. The device serves exactly
         * those words and refuses everything else, so reuse widens
         * nothing (first Wismar observation 0x42003310). */
        status = semu_ulsan_otpinfo_map(soc->bus, error);
        if (status != SEMU_OK) {
            return status;
        }
        semu_ulsan_otpinfo_enable_wismar_extra();
        /* The shared apollo4.power device (mapped during init) owns
         * 0x40021000; the Wismar warn+0 flag rides it instead of the
         * Ulsan pwrctrl device. */
        semu_apollo4_power_enable_wismar_warn_zero(soc->power);
        /* E-WIS-0001 records the Wismar lane's CLKGEN reads at
         * 0x4000400c/0x40004044 as warn-not-stop absences; the shared
         * apollo4.clock device (mapped during init) owns 0x40004000 and
         * takes the E-ULS-0010 class dictionary law via the flag. */
        semu_apollo4_clock_enable_wismar_dictionary(soc->clock);
        /* E-ULS-0046 class law: STIMER CONFIG is a plain store; the
         * Wismar guest writes value bits outside the shared evidenced
         * mask. */
        semu_apollo4_stimer_enable_wismar_plain_config(soc->stimer);
        /* E-WIS-0001: the Timer14 edge-mode startup write
         * (0x40008000+0x3c0, value 0x00000a10) takes the lane wrapper's
         * exact Edge->Upcount mapping. */
        semu_apollo4_timer_enable_wismar_timer14_edge(soc->timer);
    }
    if (strcmp(profile_id, "ulsan-2.44.52") == 0 && soc->uart3 == NULL) {
        semu_status status;
        if (!soc->initialized) {
            semu_error_set(error, SEMU_ERR_STATE, "Race S UART3 requires an initialized SoC");
            return SEMU_ERR_STATE;
        }
        soc->uart3 = semu_apollo4_uart3_create(soc->scheduler, error);
        if (soc->uart3 == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
        status = semu_apollo4_uart_set_irq_sink(soc->uart3, irq_sink_dispatch, soc, error);
        if (status == SEMU_OK)
            status = semu_bus_map_device(soc->bus, "apollo4.uart3",
                SEMU_APOLLO4_UART3_BASE, 0x1000u, semu_apollo4_uart_bus_ops(),
                soc->uart3, error);
        if (status != SEMU_OK) {
            semu_apollo4_uart_destroy(soc->uart3);
            soc->uart3 = NULL;
            return status;
        }
    }
    /* E-SAP-0032/E-SAP-0033: the 2.35.34 startup path arms the one-second
     * RTC alarm before parking.  E-SAP-0052: the 2.33.16 startup path issues
     * the identical 13-transaction arm block (lane census, twice
     * byte-identical), so both profiles take the live block; all other
     * verified Sapporo profiles keep the auxiliary register stub
     * byte-for-byte.  The live IOM4 law stays 2.35-only: the 2.33 lane
     * census names no IOM4 mirror. */
    soc->rtc_live = strcmp(profile_id, "sapporo-2.35.34") == 0 ||
                    strcmp(profile_id, "sapporo-2.33.16") == 0 ||
                    /* E-WIS-0002: the Wismar lane RTC model is the same
                     * AmbiqApollo4_RTC class (upstream wismar-platform.repl
                     * includes it); the guest touches RTC 0x204 (interrupt
                     * status) during startup. Same class law, same device. */
                    strcmp(profile_id, "wismar-2.46.14") == 0
                        ? 1 : 0;
    /* E-SAP-0073: 2.33 issues the same two absent-LPS22 identity probes
     * as E-SAP-0039's 2.35 lane. Keep the existing exact command/state law. */
    semu_apollo4_iom_set_pressure235(soc->iom2,
        strcmp(profile_id, "sapporo-2.35.34") == 0 ||
        strcmp(profile_id, "sapporo-2.33.16") == 0 ? 1 : 0);
    if (soc->rtc_live != 0 && soc->rtc == NULL) {
        soc->rtc = semu_sapporo_rtc_create(soc->scheduler,
                                          irq_sink_dispatch, soc, error);
        if (soc->rtc == NULL) return error != NULL ? error->code : SEMU_ERR_STATE;
    }
    /* E-SAP-0036: the 2.35 startup path replaces the shared IOM4 law
     * with the lane-mirror engine at 0x40054000 (IRQ 10); no other
     * profile reaches this seam. */
    /* E-WIS-0003 correction: the Wismar lane DOES include the same
     * SapporoApollo4Iom4 model (wismar-2.46.14.resc unregisters the
     * upstream iom4 and loads sapporo-iom4.repl), so the mirror serves
     * Wismar too. Unregistered-device commands take the documented
     * class error (0x204=0x40) instead of a bus fault. */
    if ((strcmp(profile_id, "sapporo-2.35.34") == 0 ||
         strcmp(profile_id, "wismar-2.46.14") == 0) &&
        soc->iom4_live == NULL) {
        soc->iom4_live = semu_sapporo_iom4_create(
            soc->bus, gpio_irq_adapter, soc, error);
        if (soc->iom4_live == NULL) {
            return error != NULL ? error->code : SEMU_ERR_STATE;
        }
        semu_apollo4_iom_set_live235(soc->iom4, soc->iom4_live);
    }
    semu_error_clear(error);
    return SEMU_OK;
}
