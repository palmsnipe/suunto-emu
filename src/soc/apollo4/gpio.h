#ifndef SEMU_APOLLO4_GPIO_H
#define SEMU_APOLLO4_GPIO_H

#include <stdint.h>

#include "semu/bus.h"
#include "../../core/snapshot_io.h"

#define SEMU_APOLLO4_GPIO_BASE 0x40010000u
#define SEMU_APOLLO4_GPIO_SIZE 0x400u
#define SEMU_APOLLO4_GPIO_COUNT 128u
#define SEMU_APOLLO4_GPIO_IRQ_BASE 56u
#define SEMU_APOLLO4_GPIO_IRQ_BANKS 4u

typedef struct semu_apollo4_gpio semu_apollo4_gpio;

typedef enum semu_apollo4_gpio_direction {
    SEMU_APOLLO4_GPIO_INPUT = 0,
    SEMU_APOLLO4_GPIO_OUTPUT = 1
} semu_apollo4_gpio_direction;

typedef enum semu_apollo4_gpio_edge {
    SEMU_APOLLO4_GPIO_EDGE_DISABLED = 0,
    SEMU_APOLLO4_GPIO_EDGE_RISING = 1,
    SEMU_APOLLO4_GPIO_EDGE_FALLING = 2,
    SEMU_APOLLO4_GPIO_EDGE_BOTH = 3
} semu_apollo4_gpio_edge;

typedef void (*semu_apollo4_gpio_irq_fn)(void *context, unsigned irq,
                                         int level);
typedef void (*semu_apollo4_gpio_output_fn)(void *context, unsigned pin,
                                            int level);

semu_apollo4_gpio *semu_apollo4_gpio_create(
    semu_bus *bus, semu_apollo4_gpio_irq_fn irq, void *irq_context,
    semu_error *error);
void semu_apollo4_gpio_destroy(semu_apollo4_gpio *gpio);
void semu_apollo4_gpio_reset(void *context);

semu_status semu_apollo4_gpio_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error);
semu_status semu_apollo4_gpio_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error);
const semu_bus_device_ops *semu_apollo4_gpio_bus_ops(void);

semu_status semu_apollo4_gpio_set_input(semu_apollo4_gpio *gpio,
                                        unsigned pin, int level,
                                        semu_error *error);
int semu_apollo4_gpio_get_input(const semu_apollo4_gpio *gpio, unsigned pin);
void semu_apollo4_gpio_enable_race_s_inputs(semu_apollo4_gpio *gpio);
/* Wismar warn-not-stop gap writes (ticket 755, E-WIS-0001): inside the
 * shared GPIO window the profile accepts plain stores at offsets 0x340 and
 * 0x400; the addresses past the shared window are served by the separate
 * gap device mapped by semu_apollo4_gpio_gap_map. Reads of those offsets
 * still refuse, and the shared profiles are unchanged. */
void semu_apollo4_gpio_enable_wismar_gap_writes(semu_apollo4_gpio *gpio);
/* Maps the Wismar-only gap block at 0x40010400 (0x80 wide): plain 32-bit
 * stores at offsets 0x5c/0x64/0x70 (addresses 0x4001045c/0x40010464/
 * 0x40010470), everything else refuses (E-WIS-0001). */
semu_status semu_apollo4_gpio_gap_map(semu_bus *bus, semu_error *error);
/* Maps the Wismar-only DWT gap block at 0xE001E000 (0x1000 wide): plain
 * 32-bit warn+0 accesses (reads 0, writes dropped), everything narrower
 * refuses (E-WIS-0001 lane continuation; shared profiles never map it). */
semu_status semu_apollo4_dwt_gap_map(semu_bus *bus, semu_error *error);
int semu_apollo4_gpio_falling_input_armed(const semu_apollo4_gpio *gpio, unsigned pin);
semu_status semu_apollo4_gpio_configure_pin(
    semu_apollo4_gpio *gpio, unsigned pin,
    semu_apollo4_gpio_direction direction, semu_apollo4_gpio_edge edge,
    semu_error *error);
semu_status semu_apollo4_gpio_set_irq_sink(semu_apollo4_gpio *gpio,
                                           semu_apollo4_gpio_irq_fn irq,
                                           void *context, semu_error *error);
semu_status semu_apollo4_gpio_set_output_observer(
    semu_apollo4_gpio *gpio, semu_apollo4_gpio_output_fn observer,
    void *context, semu_error *error);
semu_status semu_apollo4_gpio_snapshot_write(
    const semu_apollo4_gpio *gpio, semu_snapshot_writer *writer,
    semu_error *error);
semu_status semu_apollo4_gpio_snapshot_read(
    semu_apollo4_gpio *gpio, semu_snapshot_reader *reader,
    semu_error *error);

#endif
