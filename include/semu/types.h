#ifndef SEMU_TYPES_H
#define SEMU_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define SEMU_ARRAY_LEN(a) (sizeof(a) / sizeof((a)[0]))
#define SEMU_ERROR_TEXT_MAX 256u
#define SEMU_ID_MAX 64u
#define SEMU_PATH_MAX 512u
#define SEMU_SHA256_SIZE 32u

typedef enum semu_status {
    SEMU_OK = 0,
    SEMU_ERR_ARGUMENT,
    SEMU_ERR_IO,
    SEMU_ERR_FORMAT,
    SEMU_ERR_RANGE,
    SEMU_ERR_CONFLICT,
    SEMU_ERR_UNSUPPORTED,
    SEMU_ERR_STATE,
    SEMU_ERR_NOMEM
} semu_status;

typedef enum semu_stop_reason {
    SEMU_STOP_NONE = 0,
    SEMU_STOP_HALT,
    SEMU_STOP_BUDGET,
    SEMU_STOP_WFI_DEADLOCK,
    SEMU_STOP_UNMAPPED_ACCESS,
    SEMU_STOP_UNSUPPORTED_INSTRUCTION,
    SEMU_STOP_DEVICE_REFUSED,
    SEMU_STOP_FIRMWARE_ASSERT,
    SEMU_STOP_COMPAT_REFUSED,
    SEMU_STOP_USER
} semu_stop_reason;

typedef enum semu_transaction_result {
    SEMU_TRANSACTION_OK = 0,
    SEMU_TRANSACTION_WAIT,
    SEMU_TRANSACTION_REFUSE
} semu_transaction_result;

typedef struct semu_error {
    semu_status code;
    char text[SEMU_ERROR_TEXT_MAX];
} semu_error;

void semu_error_clear(semu_error *error);
void semu_error_set(semu_error *error, semu_status code, const char *format, ...);
const char *semu_status_name(semu_status status);
const char *semu_stop_reason_name(semu_stop_reason reason);

#endif
