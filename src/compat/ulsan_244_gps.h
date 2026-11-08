#ifndef SEMU_ULSAN_244_GPS_H
#define SEMU_ULSAN_244_GPS_H

#include "semu/compat.h"
#include "semu/cpu.h"
#include "../devices/sapporo_cxd5610.h"
#include "../soc/apollo4/gpio.h"
#include "../soc/apollo4/uart.h"

typedef struct {
    semu_apollo4_gpio *gpio;
    semu_apollo4_uart *uart;
    semu_sapporo_cxd5610 *transport;
    semu_layer_state *layer;
    semu_logger *logger;
    semu_scheduler *scheduler;
    semu_error refusal;
    unsigned stage;
} semu_ulsan_244_gps;

extern const semu_layer_descriptor semu_ulsan_244_gps_layer;
semu_status semu_ulsan_244_gps_bind(semu_ulsan_244_gps *gps, semu_bus *bus,
    semu_apollo4_gpio *gpio, semu_scheduler *scheduler, semu_apollo4_uart_irq_fn irq,
    void *irq_context, semu_layer_state *layer, semu_logger *logger, semu_error *error);
int semu_ulsan_244_gps_hook_pc(uint32_t pc);
semu_status semu_ulsan_244_gps_observe(semu_ulsan_244_gps *gps, semu_bus *bus,
    const semu_cpu_state *cpu, semu_error *error);
void semu_ulsan_244_gps_reset(semu_ulsan_244_gps *gps);
void semu_ulsan_244_gps_destroy(semu_ulsan_244_gps *gps);

#endif
