#include "gpio.h"

#include <stdlib.h>
#include <string.h>

enum {
    PIN_CONFIGURATION_BASE = 0x00u,
    PIN_CONFIGURATION_END = 0x1fcu,
    PAD_KEY = 0x200u,
    INPUT_READ0 = 0x204u,
    INPUT_READ1 = 0x208u,
    OUTPUT_SET0 = 0x224u,
    OUTPUT_CLEAR0 = 0x234u,
    INTERRUPT_ENABLE0 = 0x2c0u,
    INTERRUPT_STATUS0 = 0x2c4u,
    INTERRUPT_CLEAR0 = 0x2c8u
};

static const uint32_t GPIO_PAD_KEY = UINT32_C(0x73);

struct semu_apollo4_gpio {
    semu_bus *bus;
    uint32_t pin_configuration[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t input[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t direction[SEMU_APOLLO4_GPIO_COUNT];
    uint8_t edge[SEMU_APOLLO4_GPIO_COUNT];
    uint32_t output[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t output_set[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t output_clear[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t interrupt_enable[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t interrupt_status[SEMU_APOLLO4_GPIO_IRQ_BANKS];
    uint32_t pad_key;
    semu_apollo4_gpio_irq_fn irq;
    void *irq_context;
    semu_apollo4_gpio_output_fn output_observer;
    void *output_context;
};

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
    semu_error_set(error, SEMU_ERR_UNSUPPORTED,
                   "Apollo4 GPIO offset 0x%08x is unsupported", offset);
    return SEMU_ERR_UNSUPPORTED;
}

semu_status semu_apollo4_gpio_write(void *context, uint32_t offset,
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
        if (gpio->pad_key != GPIO_PAD_KEY) {
            semu_error_set(error, SEMU_ERR_STATE,
                           "Apollo4 GPIO pin configuration is locked");
            return SEMU_ERR_STATE;
        }
        gpio->pin_configuration[pin] = value;
        gpio->edge[pin] = (uint8_t)((value >> 6) & 3u);
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
