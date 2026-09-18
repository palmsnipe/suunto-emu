#ifndef SEMU_APOLLO4_INTERNAL_H
#define SEMU_APOLLO4_INTERNAL_H

#include "semu/apollo4.h"
#include "../../core/scheduler_internal.h"

#include "auxiliary.h"
#include "clock.h"
#include "dma.h"
#include "gpio.h"
#include "mcu_control.h"
#include "mram.h"
#include "mspi.h"
#include "iom.h"
#include "power.h"
#include "reset.h"
#include "rstgen.h"
#include "stimer.h"
#include "timer.h"
#include "uart.h"
#include "watchdog.h"

#define SEMU_APOLLO4_GPIO_COUNT 128u

struct semu_apollo4 {
    semu_bus *bus;
    int initialized;
    int rtc_live; /* set by semu_apollo4_select_profile (E-SAP-0032) */
    struct semu_sapporo_iom4 *iom4_live; /* set by select_profile (E-SAP-0036) */
    semu_apollo4_irq_fn irq_sink;
    void *irq_context;
    uint8_t gpio_level[SEMU_APOLLO4_GPIO_COUNT];
    semu_apollo4_clock *clock;
    semu_apollo4_power *power;
    semu_apollo4_watchdog *watchdog;
    semu_apollo4_rstgen *rstgen;
    semu_apollo4_mcu_control *mcu_control;
    semu_apollo4_reset_controller *reset_ctrl;
    semu_apollo4_gpio *gpio;
    semu_apollo4_timer *timer;
    semu_apollo4_stimer *stimer;
    semu_apollo4_uart *uart;
    semu_apollo4_iom *iom0;
    semu_apollo4_iom *iom2;
    semu_apollo4_iom *iom3;
    semu_apollo4_iom *iom4;
    semu_apollo4_iom *iom6;
    semu_apollo4_dma *dma;
    semu_apollo4_mspi *mspi1;
    semu_apollo4_mspi *mspi2;
    semu_apollo4_mram *mram;
};

semu_status semu_apollo4_snapshot_write(
    const semu_apollo4 *soc, semu_snapshot_writer *writer, semu_error *error);
semu_status semu_apollo4_snapshot_read(
    semu_apollo4 *soc, semu_snapshot_reader *reader, semu_error *error);
semu_status semu_apollo4_snapshot_resolve_event(
    semu_apollo4 *soc, uint32_t kind, uint32_t subject,
    semu_event_callback *callback, void **context, semu_error *error);
semu_status semu_apollo4_snapshot_event_id_matches(
    const semu_apollo4 *soc, uint32_t kind, uint32_t subject,
    semu_event_id event_id, semu_error *error);
semu_status semu_apollo4_snapshot_event_links_match(
    const semu_apollo4 *soc, const semu_scheduled_event_state *events,
    size_t count, semu_error *error);

#endif
