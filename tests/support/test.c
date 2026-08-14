#include "test.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "transcript.c"

void semu_test_fail(semu_test_context *context, const char *file, unsigned line,
                    const char *expression)
{
    ++context->failures;
    (void)fprintf(stderr, "FAIL %s: %s:%u: %s\n",
                  context->name, file, line, expression);
}

int semu_test_run(const semu_test_case *cases, size_t count)
{
    size_t index;
    unsigned failed = 0u;
    for (index = 0u; index < count; ++index) {
        semu_test_context context;
        context.name = cases[index].name;
        context.failures = 0u;
        cases[index].function(&context);
        if (context.failures == 0u) {
            (void)fprintf(stdout, "PASS %s\n", context.name);
        } else {
            ++failed;
        }
    }
    (void)fprintf(stdout, "%lu tests, %u failed\n",
                  (unsigned long)count, failed);
    return failed == 0u ? 0 : 1;
}

int semu_test_temp_path(char *output, size_t capacity, const char *tag)
{
    static unsigned sequence;
    int count;
    if (output == NULL || tag == NULL || strchr(tag, '/') != NULL) {
        return 0;
    }
    count = snprintf(output, capacity, "/tmp/semu-test-%ld-%u-%s",
                     (long)getpid(), sequence++, tag);
    return count > 0 && (size_t)count < capacity;
}
