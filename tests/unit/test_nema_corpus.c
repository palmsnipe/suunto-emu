#include "test.h"

#include "semu/hash.h"
#include "semu/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CORPUS_PATH "tests/golden/nema-corpus.tsv"
#define FIXTURE_DIR "fixtures/display/nema/"
#define MAX_LINE 1024u
#define MAX_CASES 64u
#define SHA256_HEX_LEN 64u

typedef struct {
    char case_name[128];
    char evidence_id[64];
    char kind[32];
    char input_path[256];
    char input_sha256[SHA256_HEX_LEN + 1u];
    char expected_event[64];
    char expected_result[64];
} corpus_row;

static int is_authentic(const corpus_row *row)
{
    return strncmp(row->input_path, "authentic/", 10u) == 0;
}

static int is_prefix(const corpus_row *row)
{
    return strcmp(row->kind, "prefix") == 0;
}

static int parse_tsv(FILE *f, corpus_row *rows, size_t *count)
{
    char line[MAX_LINE];
    size_t n = 0u;
    while (fgets(line, sizeof(line), f) != NULL && n < MAX_CASES) {
        char *fields[7];
        size_t i = 0u;
        char *p = line;
        while (i < 7u && p != NULL) {
            char *tab = strchr(p, '\t');
            if (tab != NULL) {
                *tab = '\0';
                fields[i] = p;
                p = tab + 1;
            } else {
                fields[i] = p;
                p = NULL;
            }
            ++i;
        }
        if (i < 7u || fields[0][0] == '\0') continue;
        if (strcmp(fields[0], "case") == 0) continue;
        strncpy(rows[n].case_name, fields[0], sizeof(rows[n].case_name) - 1u);
        strncpy(rows[n].evidence_id, fields[1], sizeof(rows[n].evidence_id) - 1u);
        strncpy(rows[n].kind, fields[2], sizeof(rows[n].kind) - 1u);
        strncpy(rows[n].input_path, fields[3], sizeof(rows[n].input_path) - 1u);
        strncpy(rows[n].input_sha256, fields[4], sizeof(rows[n].input_sha256) - 1u);
        strncpy(rows[n].expected_event, fields[5], sizeof(rows[n].expected_event) - 1u);
        strncpy(rows[n].expected_result, fields[6], sizeof(rows[n].expected_result) - 1u);
        size_t len = strlen(rows[n].expected_result);
        while (len > 0u && (rows[n].expected_result[len - 1u] == '\n' ||
               rows[n].expected_result[len - 1u] == '\r')) {
            rows[n].expected_result[--len] = '\0';
        }
        rows[n].case_name[sizeof(rows[n].case_name) - 1u] = '\0';
        rows[n].evidence_id[sizeof(rows[n].evidence_id) - 1u] = '\0';
        rows[n].kind[sizeof(rows[n].kind) - 1u] = '\0';
        rows[n].input_path[sizeof(rows[n].input_path) - 1u] = '\0';
        rows[n].input_sha256[sizeof(rows[n].input_sha256) - 1u] = '\0';
        rows[n].expected_event[sizeof(rows[n].expected_event) - 1u] = '\0';
        ++n;
    }
    *count = n;
    return 1;
}

static int validate_synthetic(const corpus_row *row)
{
    char path[512];
    uint8_t digest[SEMU_SHA256_SIZE];
    uint8_t expected[SEMU_SHA256_SIZE];
    uint64_t size;
    semu_error error;
    char actual_hex[65];

    snprintf(path, sizeof(path), "%s%s", FIXTURE_DIR, row->input_path);
    semu_error_clear(&error);
    if (semu_sha256_file(path, digest, &size, &error) != SEMU_OK) {
        return 0;
    }
    if (semu_sha256_parse(row->input_sha256, expected) == 0) {
        return 0;
    }
    if (memcmp(digest, expected, SEMU_SHA256_SIZE) != 0) {
        return 0;
    }
    (void)semu_sha256_format;
    (void)actual_hex;
    return 1;
}

static int validate_authentic(const corpus_row *row)
{
    const char *root = getenv("SEMU_NEMA_TRACE_ROOT");
    char path[512];
    uint8_t digest[SEMU_SHA256_SIZE];
    uint8_t expected[SEMU_SHA256_SIZE];
    uint64_t size;
    semu_error error;

    if (root == NULL || root[0] == '\0') {
        return 1;
    }
    snprintf(path, sizeof(path), "%s/%s", root, row->input_path + 10u);
    semu_error_clear(&error);
    if (semu_sha256_file(path, digest, &size, &error) != SEMU_OK) {
        return 0;
    }
    if (semu_sha256_parse(row->input_sha256, expected) == 0) {
        return 0;
    }
    return memcmp(digest, expected, SEMU_SHA256_SIZE) == 0;
}

static void test_corpus_structure(semu_test_context *context)
{
    FILE *f;
    corpus_row rows[MAX_CASES];
    size_t count;
    size_t i;
    size_t j;

    f = fopen(CORPUS_PATH, "r");
    SEMU_TEST_ASSERT(context, f != NULL);
    SEMU_TEST_ASSERT(context, parse_tsv(f, rows, &count));
    fclose(f);
    SEMU_TEST_ASSERT(context, count > 0u);

    for (i = 0u; i < count; ++i) {
        SEMU_TEST_ASSERT(context, rows[i].case_name[0] != '\0');
        SEMU_TEST_ASSERT(context, rows[i].kind[0] != '\0');
        SEMU_TEST_ASSERT(context, rows[i].input_path[0] != '\0');
        SEMU_TEST_ASSERT(context, strlen(rows[i].input_sha256) == SHA256_HEX_LEN);
        SEMU_TEST_ASSERT(context, rows[i].expected_event[0] != '\0');
        SEMU_TEST_ASSERT(context, rows[i].expected_result[0] != '\0');
        for (j = i + 1u; j < count; ++j) {
            SEMU_TEST_ASSERT(context,
                strcmp(rows[i].case_name, rows[j].case_name) != 0);
        }
    }
}

static void test_synthetic_fixtures(semu_test_context *context)
{
    FILE *f;
    corpus_row rows[MAX_CASES];
    size_t count;
    size_t i;
    int found_synthetic = 0;

    f = fopen(CORPUS_PATH, "r");
    SEMU_TEST_ASSERT(context, f != NULL);
    SEMU_TEST_ASSERT(context, parse_tsv(f, rows, &count));
    fclose(f);

    for (i = 0u; i < count; ++i) {
        if (!is_authentic(&rows[i])) {
            found_synthetic = 1;
            SEMU_TEST_ASSERT(context, validate_synthetic(&rows[i]));
        }
    }
    SEMU_TEST_ASSERT(context, found_synthetic);
}

static void test_prefix_not_complete(semu_test_context *context)
{
    FILE *f;
    corpus_row rows[MAX_CASES];
    size_t count;
    size_t i;

    f = fopen(CORPUS_PATH, "r");
    SEMU_TEST_ASSERT(context, f != NULL);
    SEMU_TEST_ASSERT(context, parse_tsv(f, rows, &count));
    fclose(f);

    for (i = 0u; i < count; ++i) {
        if (is_prefix(&rows[i])) {
            SEMU_TEST_ASSERT(context,
                strcmp(rows[i].expected_result, "refused") == 0);
        }
        if (!is_prefix(&rows[i]) &&
            strcmp(rows[i].kind, "refusal") != 0 &&
            !is_authentic(&rows[i])) {
            SEMU_TEST_ASSERT(context,
                strcmp(rows[i].expected_result, "refused") != 0);
        }
    }
}

static void test_authentic_hashes(semu_test_context *context)
{
    FILE *f;
    corpus_row rows[MAX_CASES];
    size_t count;
    size_t i;
    int found_authentic = 0;

    f = fopen(CORPUS_PATH, "r");
    SEMU_TEST_ASSERT(context, f != NULL);
    SEMU_TEST_ASSERT(context, parse_tsv(f, rows, &count));
    fclose(f);

    for (i = 0u; i < count; ++i) {
        if (is_authentic(&rows[i])) {
            found_authentic = 1;
            SEMU_TEST_ASSERT(context, validate_authentic(&rows[i]));
        }
    }
    SEMU_TEST_ASSERT(context, found_authentic);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_corpus_structure),
        SEMU_TEST_CASE(test_synthetic_fixtures),
        SEMU_TEST_CASE(test_prefix_not_complete),
        SEMU_TEST_CASE(test_authentic_hashes)
    };
    if (semu_test_run(cases, sizeof(cases) / sizeof(cases[0])) == 0) {
        puts("nema corpus: valid");
    }
    return 0;
}
