/*
 * CLI profile identity table (ticket 705 dispatches).
 * Included by cli.c as one translation unit (precedent: cli_debug.c).
 */

#include <stdio.h>
#include <string.h>

static const char *profile_path(const char *argument)
{
    static const char *const pairs[][2] = {
        {"sapporo-2.22.60", "profiles/sapporo/2.22.60/profile.semu"},
        {"sapporo-2.33.16", "profiles/sapporo/2.33.16/profile.semu"},
        {"sapporo-2.35.34", "profiles/sapporo/2.35.34/profile.semu"},
        {"sapporo-2.39.20", "profiles/sapporo/2.39.20/profile.semu"}
    };
    size_t i;
    if (argument != NULL) {
        for (i = 0u; i < sizeof(pairs) / sizeof(pairs[0]); ++i) {
            if (strcmp(argument, pairs[i][0]) == 0) {
                return pairs[i][1];
            }
        }
    }
    return argument;
}

static int command_list(void)
{
    puts("sapporo-2.22.60  Sapporo  2.22.60.3383-P  interpreter-bring-up");
    puts("sapporo-2.33.16  Sapporo  2.33.16.17428-P  evidence-contract");
    puts("sapporo-2.35.34  Sapporo  2.35.34.18929-P  evidence-contract");
    puts("sapporo-2.39.20  Sapporo  2.39.20.22297-P  evidence-contract");
    return 0;
}
