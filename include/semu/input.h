#ifndef SEMU_INPUT_H
#define SEMU_INPUT_H

#include <stdint.h>

typedef enum semu_input_kind {
    SEMU_INPUT_BUTTON = 0,
    SEMU_INPUT_CROWN_ROTATE,
    SEMU_INPUT_TOUCH
} semu_input_kind;

typedef enum semu_button_id {
    SEMU_BUTTON_UPPER = 0,
    SEMU_BUTTON_MIDDLE,
    SEMU_BUTTON_LOWER
} semu_button_id;

typedef struct semu_input_event {
    semu_input_kind kind;
    uint32_t code;
    int32_t value;
    int32_t x;
    int32_t y;
} semu_input_event;

#endif
