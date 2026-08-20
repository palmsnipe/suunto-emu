#include "gpio_internal.h"

semu_status semu_apollo4_gpio_snapshot_write(
    const semu_apollo4_gpio *gpio, semu_snapshot_writer *writer,
    semu_error *error)
{
    size_t index;
    if (gpio == NULL || writer == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "GPIO snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    for (index = 0u; index < SEMU_APOLLO4_GPIO_COUNT; ++index) {
        if (semu_snapshot_writer_u32(writer, gpio->pin_configuration[index],
                                     error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_writer_bytes(writer, gpio->input, sizeof(gpio->input),
                                   error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, gpio->direction,
                                   sizeof(gpio->direction), error) != SEMU_OK ||
        semu_snapshot_writer_bytes(writer, gpio->edge, sizeof(gpio->edge),
                                   error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_APOLLO4_GPIO_IRQ_BANKS; ++index) {
        if (semu_snapshot_writer_u32(writer, gpio->output[index], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, gpio->output_set[index], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, gpio->output_clear[index], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, gpio->interrupt_enable[index], error) != SEMU_OK ||
            semu_snapshot_writer_u32(writer, gpio->interrupt_status[index], error) != SEMU_OK)
            return error->code;
    }
    return semu_snapshot_writer_u32(writer, gpio->pad_key, error);
}

semu_status semu_apollo4_gpio_snapshot_read(
    semu_apollo4_gpio *gpio, semu_snapshot_reader *reader,
    semu_error *error)
{
    semu_apollo4_gpio candidate;
    size_t index;
    if (gpio == NULL || reader == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "GPIO snapshot arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    candidate = *gpio;
    for (index = 0u; index < SEMU_APOLLO4_GPIO_COUNT; ++index) {
        if (semu_snapshot_reader_u32(reader,
                                     &candidate.pin_configuration[index],
                                     error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_reader_bytes(reader, candidate.input,
                                   sizeof(candidate.input), error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.direction,
                                   sizeof(candidate.direction), error) != SEMU_OK ||
        semu_snapshot_reader_bytes(reader, candidate.edge,
                                   sizeof(candidate.edge), error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_APOLLO4_GPIO_IRQ_BANKS; ++index) {
        if (semu_snapshot_reader_u32(reader, &candidate.output[index], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &candidate.output_set[index], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &candidate.output_clear[index], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &candidate.interrupt_enable[index], error) != SEMU_OK ||
            semu_snapshot_reader_u32(reader, &candidate.interrupt_status[index], error) != SEMU_OK)
            return error->code;
    }
    if (semu_snapshot_reader_u32(reader, &candidate.pad_key, error) != SEMU_OK)
        return error->code;
    for (index = 0u; index < SEMU_APOLLO4_GPIO_COUNT; ++index) {
        if (candidate.input[index] > 1u || candidate.direction[index] > 1u ||
            candidate.edge[index] > SEMU_APOLLO4_GPIO_EDGE_BOTH) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                           "GPIO snapshot pin state is unreachable");
            return SEMU_ERR_FORMAT;
        }
    }
    if (candidate.pad_key != 0u && candidate.pad_key != UINT32_C(0x73)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
                       "GPIO snapshot pad key is invalid");
        return SEMU_ERR_FORMAT;
    }
    *gpio = candidate;
    return SEMU_OK;
}
