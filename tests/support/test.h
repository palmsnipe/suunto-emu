#ifndef SEMU_TEST_H
#define SEMU_TEST_H

#include <stddef.h>
#include <stdint.h>

typedef struct semu_test_context {
    const char *name;
    unsigned failures;
} semu_test_context;

typedef void (*semu_test_function)(semu_test_context *context);

typedef struct semu_test_case {
    const char *name;
    semu_test_function function;
} semu_test_case;

int semu_test_run(const semu_test_case *cases, size_t count);
void semu_test_fail(semu_test_context *context, const char *file, unsigned line,
                    const char *expression);
int semu_test_temp_path(char *output, size_t capacity, const char *tag);

#define SEMU_TEST_ASSERT(context, expression) do { \
    if (!(expression)) { \
        semu_test_fail((context), __FILE__, __LINE__, #expression); \
        return; \
    } \
} while (0)

#define SEMU_TEST_EQ_U64(context, expected, actual) do { \
    uint64_t semu_expected_ = (uint64_t)(expected); \
    uint64_t semu_actual_ = (uint64_t)(actual); \
    if (semu_expected_ != semu_actual_) { \
        semu_test_fail((context), __FILE__, __LINE__, #expected " == " #actual); \
        return; \
    } \
} while (0)

#define SEMU_TEST_CASE(function) { #function, function }

#endif
