#ifndef SEMU_LOG_H
#define SEMU_LOG_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

typedef enum semu_log_level {
    SEMU_LOG_TRACE = 0,
    SEMU_LOG_DEBUG,
    SEMU_LOG_INFO,
    SEMU_LOG_WARNING,
    SEMU_LOG_ERROR
} semu_log_level;

typedef struct semu_logger {
    FILE *stream;
    semu_log_level minimum_level;
    uint64_t virtual_time_ns;
} semu_logger;

void semu_log_init(semu_logger *logger, FILE *stream, semu_log_level minimum_level);
void semu_log_set_time(semu_logger *logger, uint64_t virtual_time_ns);
void semu_log_write(semu_logger *logger, semu_log_level level,
                    const char *subsystem, const char *event,
                    const char *format, ...);
void semu_log_vwrite(semu_logger *logger, semu_log_level level,
                     const char *subsystem, const char *event,
                     const char *format, va_list arguments);

#endif
