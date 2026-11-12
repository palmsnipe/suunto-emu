#include "gpio_internal.h"

#include <stdlib.h>
#include <string.h>

enum {
    PIN_CONFIGURATION_BASE = 0x00u,
    PIN_CONFIGURATION_END = 0x1fcu,
    PAD_KEY = 0x200u,
    INPUT_READ0 = 0x204u,
    INPUT_READ1 = 0x208u,
    INPUT_READ3 = 0x210u,
    OUTPUT_VALUE1 = 0x218u,
    OUTPUT_SET0 = 0x224u,
    OUTPUT_CLEAR0 = 0x234u,
    INTERRUPT_ENABLE0 = 0x2c0u,
    INTERRUPT_STATUS0 = 0x2c4u,
    INTERRUPT_CLEAR0 = 0x2c8u
};

static const uint32_t GPIO_PAD_KEY = UINT32_C(0x73);

static semu_apollo4_gpio_edge decode_pin_edge(uint32_t value)
{
    /* E-A4-GPIO-001: Apollo4 IRPTEN 01 is high-to-low; 10 is low-to-high. */
    switch ((value >> 6) & 3u) {
    case 1u: return SEMU_APOLLO4_GPIO_EDGE_FALLING;
    case 2u: return SEMU_APOLLO4_GPIO_EDGE_RISING;
    default: return (semu_apollo4_gpio_edge)((value >> 6) & 3u);
    }
}

static const semu_bus_device_ops gpio_ops = {
    semu_apollo4_gpio_read, semu_apollo4_gpio_write,
    semu_apollo4_gpio_reset
};

static semu_status validate_access(const semu_apollo4_gpio *gpio,
                                   uint32_t offset, unsigned width,
                                   semu_error *error)
{
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Apollo4 GPIO supports aligned 32-bit accesses only");
        return SEMU_ERR_UNSUPPORTED;
    }
    semu_error_clear(error);
    return SEMU_OK;
}

static int pin_register(uint32_t offset)
{
    return offset <= PIN_CONFIGURATION_END && (offset & 3u) == 0u
               ? (int)(offset >> 2) : -1;
}

static int output_bank_register(uint32_t offset, uint32_t first)
{
    return offset >= first && offset < first + 16u &&
                   ((offset - first) & 3u) == 0u
               ? (int)((offset - first) >> 2) : -1;
}

static int interrupt_bank_register(uint32_t offset, uint32_t first)
{
    return offset >= first && offset < first + 64u &&
                   ((offset - first) & 0x0fu) == 0u
               ? (int)((offset - first) >> 4) : -1;
}

static void report_irq(semu_apollo4_gpio *gpio, unsigned bank,
                       int old_level)
{
    int new_level = gpio->interrupt_status[bank] != 0u &&
                    gpio->interrupt_enable[bank] != 0u;
    if (old_level != new_level && gpio->irq != NULL) {
        gpio->irq(gpio->irq_context, SEMU_APOLLO4_GPIO_IRQ_BASE + bank,
                  new_level);
    }
}

static void report_output(semu_apollo4_gpio *gpio, unsigned pin,
                          int old_level, int new_level)
{
    if (old_level != new_level && gpio->output_observer != NULL) {
        gpio->output_observer(gpio->output_context, pin, new_level);
    }
}

static void reset_state(semu_apollo4_gpio *gpio, int report)
{
    unsigned bank;
    unsigned pin;
    if (report && gpio->irq != NULL) {
        for (bank = 0u; bank < SEMU_APOLLO4_GPIO_IRQ_BANKS; ++bank) {
            if (gpio->interrupt_status[bank] != 0u &&
                gpio->interrupt_enable[bank] != 0u) {
                gpio->irq(gpio->irq_context,
                          SEMU_APOLLO4_GPIO_IRQ_BASE + bank, 0);
            }
        }
    }
    memset(gpio->pin_configuration, 0, sizeof(gpio->pin_configuration));
    memset(gpio->direction, 0, sizeof(gpio->direction));
    memset(gpio->edge, 0, sizeof(gpio->edge));
    memset(gpio->output, 0, sizeof(gpio->output));
    memset(gpio->output_set, 0, sizeof(gpio->output_set));
    memset(gpio->output_clear, 0, sizeof(gpio->output_clear));
    memset(gpio->interrupt_enable, 0, sizeof(gpio->interrupt_enable));
    memset(gpio->interrupt_status, 0, sizeof(gpio->interrupt_status));
    for (pin = 0u; pin < SEMU_APOLLO4_GPIO_COUNT; ++pin) {
        gpio->input[pin] = 1u;
    }
    gpio->pad_key = 0u;
}

semu_apollo4_gpio *semu_apollo4_gpio_create(
    semu_bus *bus, semu_apollo4_gpio_irq_fn irq, void *irq_context,
    semu_error *error)
{
    semu_apollo4_gpio *gpio;
    if (bus == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO requires a bus");
        return NULL;
    }
    gpio = (semu_apollo4_gpio *)calloc(1u, sizeof(*gpio));
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "cannot allocate Apollo4 GPIO");
        return NULL;
    }
    gpio->bus = bus;
    gpio->irq = irq;
    gpio->irq_context = irq_context;
    reset_state(gpio, 0);
    if (semu_bus_map_device(bus, "apollo4.gpio", SEMU_APOLLO4_GPIO_BASE,
                            SEMU_APOLLO4_GPIO_SIZE, &gpio_ops, gpio,
                            error) != SEMU_OK) {
        free(gpio);
        return NULL;
    }
    semu_error_clear(error);
    return gpio;
}

void semu_apollo4_gpio_destroy(semu_apollo4_gpio *gpio)
{
    free(gpio);
}

void semu_apollo4_gpio_reset(void *context)
{
    semu_apollo4_gpio *gpio = (semu_apollo4_gpio *)context;
    if (gpio != NULL) {
        reset_state(gpio, 1);
    }
}

semu_status semu_apollo4_gpio_read(void *context, uint32_t offset,
                                   unsigned width, uint32_t *value,
                                   semu_error *error)
{
    semu_apollo4_gpio *gpio = (semu_apollo4_gpio *)context;
    int pin;
    int bank;
    unsigned bit;
    semu_status status = validate_access(gpio, offset, width, error);
    if (status != SEMU_OK) return status;
    if (value == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO read value required");
        return SEMU_ERR_ARGUMENT;
    }
    pin = pin_register(offset);
    if (pin >= 0) {
        *value = gpio->pin_configuration[pin];
        return SEMU_OK;
    }
    if (offset == PAD_KEY) {
        *value = gpio->pad_key;
        return SEMU_OK;
    }
    if (offset == INPUT_READ0 || offset == INPUT_READ1) {
        bank = (int)((offset - INPUT_READ0) >> 2);
        *value = 0u;
        for (bit = 0u; bit < 32u; ++bit) {
            if (gpio->input[(unsigned)bank * 32u + bit] != 0u)
                *value |= UINT32_C(1) << bit;
        }
        return SEMU_OK;
    }
    if (offset == INPUT_READ3 && gpio->race_s_inputs) {
        /* E-ULS-0096: bank 3 samples the enabled touch and crown input pads. */
        *value = 0u;
        if ((gpio->pin_configuration[100u] & 0x10u) != 0u && gpio->input[100u])
            *value |= UINT32_C(1) << 4u;
        if ((gpio->pin_configuration[104u] & 0x10u) != 0u && gpio->input[104u])
            *value |= UINT32_C(1) << 8u;
        return SEMU_OK;
    }
    if (offset == OUTPUT_VALUE1) {
        *value = gpio->output[1u];
        return SEMU_OK;
    }
    bank = output_bank_register(offset, OUTPUT_SET0);
    if (bank >= 0) {
        *value = gpio->output_set[bank];
        return SEMU_OK;
    }
    bank = output_bank_register(offset, OUTPUT_CLEAR0);
    if (bank >= 0) {
        *value = gpio->output_clear[bank];
        return SEMU_OK;
    }
    bank = interrupt_bank_register(offset, INTERRUPT_ENABLE0);
    if (bank >= 0) {
        *value = gpio->interrupt_enable[bank];
        return SEMU_OK;
    }
    bank = interrupt_bank_register(offset, INTERRUPT_STATUS0);
    if (bank >= 0) {
        *value = gpio->interrupt_status[bank];
        return SEMU_OK;
    }
    if (gpio->wismar_gap_writes) {
        /* E-WIS-0001 warn+0 law (E-ULS-0046 precedent): the lane's upstream
         * GPIO class answers unregistered in-window offsets with a warning
         * and 0; the Wismar boot reads them (first observed 0x250). */
        *value = 0u;
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 GPIO offset 0x%08x is unsupported", offset);
    return SEMU_ERR_UNSUPPORTED;
}

/* E-WIS-0001 lane-gap writes: the GPIO offsets inside the shared window
 * that the lane model logged as unhandled writes while the guest continued
 * on every twice-identical bounded trace. Plain stores, no state. */
static int wismar_gap_write_offset(uint32_t offset)
{
    return offset == 0x340u || offset == 0x400u;
}semu_status semu_apollo4_gpio_write(void *context, uint32_t offset,
                                    unsigned width, uint32_t value,
                                    semu_error *error)
{
    semu_apollo4_gpio *gpio = (semu_apollo4_gpio *)context;
    int pin;
    int bank;
    unsigned bit;
    semu_status status = validate_access(gpio, offset, width, error);
    if (status != SEMU_OK) return status;
    pin = pin_register(offset);
    if (pin >= 0) {
        if (gpio->pad_key != GPIO_PAD_KEY && !gpio->wismar_gap_writes) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Apollo4 GPIO pin configuration is locked");
            return SEMU_ERR_STATE;
        }
        gpio->pin_configuration[pin] = value;
        gpio->edge[pin] = (uint8_t)decode_pin_edge(value);
        return SEMU_OK;
    }
    if (offset == PAD_KEY) {
        if (value != 0u && value != GPIO_PAD_KEY) {
            semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                           "Apollo4 GPIO pad key value is unsupported");
            return SEMU_ERR_UNSUPPORTED;
        }
        gpio->pad_key = value;
        return SEMU_OK;
    }
    bank = output_bank_register(offset, OUTPUT_SET0);
    if (bank >= 0) {
        gpio->output_set[bank] = value;
        for (bit = 0u; bit < 32u; ++bit) {
            if ((value & (UINT32_C(1) << bit)) != 0u) {
                unsigned pin_number = (unsigned)bank * 32u + bit;
                int old_level = (gpio->output[bank] >> bit) & 1u;
                gpio->output[bank] |= UINT32_C(1) << bit;
                report_output(gpio, pin_number, old_level, 1);
            }
        }
        return SEMU_OK;
    }
    bank = output_bank_register(offset, OUTPUT_CLEAR0);
    if (bank >= 0) {
        gpio->output_clear[bank] = value;
        for (bit = 0u; bit < 32u; ++bit) {
            if ((value & (UINT32_C(1) << bit)) != 0u) {
                unsigned pin_number = (unsigned)bank * 32u + bit;
                int old_level = (gpio->output[bank] >> bit) & 1u;
                gpio->output[bank] &= ~(UINT32_C(1) << bit);
                report_output(gpio, pin_number, old_level, 0);
            }
        }
        return SEMU_OK;
    }
    bank = interrupt_bank_register(offset, INTERRUPT_ENABLE0);
    if (bank >= 0) {
        int old_level = gpio->interrupt_status[bank] != 0u &&
                         gpio->interrupt_enable[bank] != 0u;
        gpio->interrupt_enable[bank] = value;
        report_irq(gpio, (unsigned)bank, old_level);
        return SEMU_OK;
    }
    bank = interrupt_bank_register(offset, INTERRUPT_CLEAR0);
    if (bank >= 0) {
        int old_level = gpio->interrupt_status[bank] != 0u &&
                         gpio->interrupt_enable[bank] != 0u;
        gpio->interrupt_status[bank] &= ~value;
        report_irq(gpio, (unsigned)bank, old_level);
        return SEMU_OK;
    }
    if (gpio->wismar_gap_writes && wismar_gap_write_offset(offset)) {
        /* E-WIS-0001: the lane logged these writes and continued; a plain
         * drop preserves that behavior without inventing semantics. The
         * addresses past the window (0x45c..0x470) are served by the
         * separate wismar.gpio-gap device. */
        return SEMU_OK;
    }
    if (gpio->wismar_gap_writes) {
        /* E-WIS-0001 warn+0 law (the E-ULS-0046 precedent): the lane's
         * upstream GPIO class answers unregistered in-window offsets with
         * a warning and 0 on reads, and drops their writes; the Wismar boot
         * touches them (first observed 0x250), so the Wismar profile takes
         * the lane law while the shared profiles keep refusing. */
        return SEMU_OK;
    }
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 GPIO offset 0x%08x is unsupported", offset);
    return SEMU_ERR_UNSUPPORTED;
}

semu_status semu_apollo4_gpio_set_input(semu_apollo4_gpio *gpio,
                                        unsigned pin, int level,
                                        semu_error *error)
{
    unsigned bank;
    uint32_t bit;
    int old_level;
    int new_level;
    uint8_t edge;
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (pin >= SEMU_APOLLO4_GPIO_COUNT) {
        semu_error_set(error, SEMU_ERR_RANGE, "invalid GPIO pin %u", pin);
        return SEMU_ERR_RANGE;
    }
    old_level = gpio->input[pin] != 0u;
    new_level = level != 0;
    if (old_level == new_level) return SEMU_OK;
    gpio->input[pin] = (uint8_t)new_level;
    edge = gpio->edge[pin];
    if ((old_level == 0 && new_level != 0 &&
         (edge == SEMU_APOLLO4_GPIO_EDGE_RISING ||
          edge == SEMU_APOLLO4_GPIO_EDGE_BOTH)) ||
        (old_level != 0 && new_level == 0 &&
         (edge == SEMU_APOLLO4_GPIO_EDGE_FALLING ||
          edge == SEMU_APOLLO4_GPIO_EDGE_BOTH))) {
        bank = pin / 32u;
        bit = UINT32_C(1) << (pin % 32u);
        {
            int old_irq = gpio->interrupt_status[bank] != 0u &&
                          gpio->interrupt_enable[bank] != 0u;
            gpio->interrupt_status[bank] |= bit;
            report_irq(gpio, bank, old_irq);
        }
    }
    semu_error_clear(error);
    return SEMU_OK;
}

int semu_apollo4_gpio_get_input(const semu_apollo4_gpio *gpio, unsigned pin)
{
    return gpio != NULL && pin < SEMU_APOLLO4_GPIO_COUNT
               ? gpio->input[pin] != 0u : -1;
}

void semu_apollo4_gpio_enable_race_s_inputs(semu_apollo4_gpio *gpio)
{
    gpio->race_s_inputs = 1;
}

void semu_apollo4_gpio_enable_wismar_gap_writes(semu_apollo4_gpio *gpio)
{
    gpio->wismar_gap_writes = 1;
}

/* The addresses past the shared 0x400 GPIO window (0x40010400 upward) are
 * outside the mapped device; the Wismar profile maps this gap block over
 * the span the guest's own literal pool names (0x40010400..0x400104b0:
 * pool entries 0x00, 0x04, 0x20, 0x3c, 0x58, 0x74, 0x90, 0xac plus the
 * E-WIS-0001 recorded 0x5c/0x64/0x68/0x70) so those accesses land as lane
 * warn+0 behavior instead of unmapped faults (E-WIS-0001, "non-existing
 * peripheral writes at 0x40010470/0x40010464/0x4001045c ... do not stop
 * execution"; the untagged SystemBus fallback reads 0 and drops writes per
 * the E-ULS-0038 engine-source law). */
static semu_status gpio_gap_read(void *context, uint32_t offset,
                                 unsigned width, uint32_t *value,
                                 semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Wismar GPIO gap read at 0x%08x width %u is unsupported",
                       0x40010400u + offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    return SEMU_OK;
}

static semu_status gpio_gap_write(void *context, uint32_t offset,
                                  unsigned width, uint32_t value,
                                  semu_error *error)
{
    (void)context;
    (void)value;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Wismar GPIO gap write at 0x%08x width %u is "
                       "unsupported", 0x40010400u + offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

static const semu_bus_device_ops gpio_gap_ops = {
    gpio_gap_read, gpio_gap_write, NULL
};

semu_status semu_apollo4_gpio_gap_map(semu_bus *bus, semu_error *error)
{
    return semu_bus_map_device(bus, "wismar.gpio-gap", 0x40010400u, 0xb0u,
                               &gpio_gap_ops, NULL, error);
}

int semu_apollo4_gpio_falling_input_armed(const semu_apollo4_gpio *gpio, unsigned pin)
{
    return gpio != NULL && pin < SEMU_APOLLO4_GPIO_COUNT &&
           (gpio->edge[pin] == SEMU_APOLLO4_GPIO_EDGE_FALLING ||
            gpio->edge[pin] == SEMU_APOLLO4_GPIO_EDGE_BOTH) &&
           (gpio->interrupt_enable[pin / 32u] & (UINT32_C(1) << (pin % 32u))) != 0u;
}

semu_status semu_apollo4_gpio_configure_pin(
    semu_apollo4_gpio *gpio, unsigned pin,
    semu_apollo4_gpio_direction direction, semu_apollo4_gpio_edge edge,
    semu_error *error)
{
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO context required");
        return SEMU_ERR_ARGUMENT;
    }
    if (pin >= SEMU_APOLLO4_GPIO_COUNT || direction > SEMU_APOLLO4_GPIO_OUTPUT ||
        edge > SEMU_APOLLO4_GPIO_EDGE_BOTH) {
        semu_error_set(error, SEMU_ERR_RANGE,
                       "invalid Apollo4 GPIO pin configuration");
        return SEMU_ERR_RANGE;
    }
    gpio->direction[pin] = (uint8_t)direction;
    gpio->edge[pin] = (uint8_t)edge;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_gpio_set_irq_sink(semu_apollo4_gpio *gpio,
                                           semu_apollo4_gpio_irq_fn irq,
                                           void *context, semu_error *error)
{
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO context required");
        return SEMU_ERR_ARGUMENT;
    }
    gpio->irq = irq;
    gpio->irq_context = context;
    semu_error_clear(error);
    return SEMU_OK;
}

semu_status semu_apollo4_gpio_set_output_observer(
    semu_apollo4_gpio *gpio, semu_apollo4_gpio_output_fn observer,
    void *context, semu_error *error)
{
    if (gpio == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "Apollo4 GPIO context required");
        return SEMU_ERR_ARGUMENT;
    }
    gpio->output_observer = observer;
    gpio->output_context = context;
    semu_error_clear(error);
    return SEMU_OK;
}

const semu_bus_device_ops *semu_apollo4_gpio_bus_ops(void)
{
    return &gpio_ops;
}

/* The Wismar startup enables DEMCR.TRCENA and then touches the CoreSight
 * DWT window (0xE001E000..0xE001EFFF, first observed access 0xE001E300).
 * No in-tree device serves that debug window; the lane's CortexM accepts
 * the accesses (both twice-identical traces continued), so the Wismar
 * profile maps this plain warn+0 block: reads 0, writes dropped. Shared
 * profiles never map it and keep failing closed. */
static semu_status dwt_gap_read(void *context, uint32_t offset,
                                unsigned width, uint32_t *value,
                                semu_error *error)
{
    (void)context;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Wismar DWT read at 0x%08x width %u is unsupported",
                       0xe001e000u + offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    *value = 0u;
    return SEMU_OK;
}

static semu_status dwt_gap_write(void *context, uint32_t offset,
                                 unsigned width, uint32_t value,
                                 semu_error *error)
{
    (void)context;
    (void)value;
    if (width != 4u || (offset & 3u) != 0u) {
        semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                       "Wismar DWT write at 0x%08x width %u is unsupported",
                       0xe001e000u + offset, width);
        return SEMU_ERR_UNSUPPORTED;
    }
    return SEMU_OK;
}

static const semu_bus_device_ops dwt_gap_ops = {
    dwt_gap_read, dwt_gap_write, NULL
};

semu_status semu_apollo4_dwt_gap_map(semu_bus *bus, semu_error *error)
{
    return semu_bus_map_device(bus, "wismar.dwt-gap", 0xe001e000u, 0x1000u,
                               &dwt_gap_ops, NULL, error);
}
