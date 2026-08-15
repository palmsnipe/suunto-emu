#include "semu/trace.h"
#include "semu/hash.h"
#include "semu/cpu.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>

/* --- Replay malformed corpus --- */

typedef struct {
    const char *name;
    const char *text;
    size_t len;
} corpus_case;

static const corpus_case REPLAY_CASES[] = {
    {"empty", "", 0u},
    {"missing_version",
     "profile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\n", 0u},
    {"bad_version",
     "version=2\n"
     "profile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\n", 0u},
    {"short_hash",
     "version=1\nprofile=test\nfirmware=abc\nevents=0\n", 0u},
    {"nonhex_hash",
     "version=1\nprofile=test\n"
     "firmware=XYZ000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\n", 0u},
    {"missing_firmware",
     "version=1\nprofile=test\nevents=0\n", 0u},
    {"unknown_kind",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=1\n1000000 sensor upper press\n", 0u},
    {"unknown_button",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=1\n1000000 button sideways press\n", 0u},
    {"time_reversal",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=2\n2000000 button upper press\n1000000 button upper release\n", 0u},
    {"count_mismatch_high",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=3\n1000000 button upper press\n", 0u},
    {"count_mismatch_low",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\n1000000 button upper press\n", 0u},
    {"bad_button_value",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=1\n1000000 button upper click\n", 0u},
    {"bad_time",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=1\nabc button upper press\n", 0u},
    {"unknown_header_key",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\ngarbage=value\n", 0u},
    {"missing_value",
     "version=1\nprofile=test\n"
     "firmware=0000000000000000000000000000000000000000000000000000000000000000\n"
     "events=0\ngarbage\n", 0u},
};

#define REPLAY_CASE_COUNT \
    (sizeof(REPLAY_CASES) / sizeof(REPLAY_CASES[0]))

/* --- Snapshot malformed corpus --- */

typedef struct {
    const char *name;
    const uint8_t *data;
    size_t len;
} snap_case;

/* Valid snapshot for corruption base */
static semu_status make_valid_snapshot(uint8_t *buf, size_t buf_size,
    size_t *out_len)
{
    semu_error err;
    semu_snapshot *s;
    size_t len;
    static const uint8_t payload[] = { 0x01u, 0x02u };
    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    if (s == NULL) {
        return SEMU_ERR_NOMEM;
    }
    semu_snapshot_set_identity(s, "test",
        "0000000000000000000000000000000000000000000000000000000000000000",
        &err);
    semu_snapshot_write_section(s, 0u, payload, sizeof(payload), &err);
    len = semu_snapshot_serialize(s, buf, buf_size);
    semu_snapshot_destroy(s);
    if (len == 0u) {
        return SEMU_ERR_RANGE;
    }
    *out_len = len;
    return SEMU_OK;
}

static void test_replay_corpus(semu_test_context *context)
{
    semu_error err;
    semu_replay *r;
    size_t i;
    char results[256];
    size_t res_len = 0u;
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash1[65];
    char hash2[65];

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);

    for (i = 0u; i < REPLAY_CASE_COUNT; ++i) {
        const corpus_case *c = &REPLAY_CASES[i];
        size_t len = c->len;
        semu_status s;
        if (len == 0u) {
            len = strlen(c->text);
        }
        semu_error_clear(&err);
        semu_replay_reset(r);
        s = semu_replay_parse(r, c->text, len, &err);
        if (s != SEMU_OK) {
            results[res_len++] = 'P';
        } else {
            results[res_len++] = 'F';
        }
    }
    semu_replay_destroy(r);
    results[res_len] = '\0';

    /* All cases must fail (P = parser refused as expected). */
    for (i = 0u; i < res_len; ++i) {
        SEMU_TEST_EQ_U64(context, (uint64_t)'P', (uint64_t)results[i]);
    }

    /* Deterministic hash: same results -> same hash. */
    semu_sha256(results, res_len, digest);
    semu_sha256_format(digest, hash1);
    semu_sha256(results, res_len, digest);
    semu_sha256_format(digest, hash2);
    SEMU_TEST_ASSERT(context, strcmp(hash1, hash2) == 0);
}

static void test_snapshot_corpus(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t valid[4096];
    size_t valid_len;
    char results[256];
    size_t res_len = 0u;
    uint8_t digest[SEMU_SHA256_SIZE];
    char hash1[65];
    char hash2[65];
    size_t i;

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        make_valid_snapshot(valid, sizeof(valid), &valid_len));

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);

    /* Case 1: empty buffer */
    {
        uint8_t empty[1] = { 0u };
        semu_error_clear(&err);
        semu_snapshot_reset(s);
        results[res_len++] =
            semu_snapshot_deserialize(s, empty, 0u, &err) != SEMU_OK
                ? 'P' : 'F';
    }
    /* Case 2: bad magic */
    {
        uint8_t buf[4096];
        memcpy(buf, valid, valid_len);
        buf[0] ^= 0xFFu;
        semu_error_clear(&err);
        semu_snapshot_reset(s);
        results[res_len++] =
            semu_snapshot_deserialize(s, buf, valid_len, &err) != SEMU_OK
                ? 'P' : 'F';
    }
    /* Case 3: bad version */
    {
        uint8_t buf[4096];
        memcpy(buf, valid, valid_len);
        buf[4] = 0xFFu;
        semu_error_clear(&err);
        semu_snapshot_reset(s);
        results[res_len++] =
            semu_snapshot_deserialize(s, buf, valid_len, &err) != SEMU_OK
                ? 'P' : 'F';
    }
    /* Case 4: truncated header */
    semu_error_clear(&err);
    semu_snapshot_reset(s);
    results[res_len++] =
        semu_snapshot_deserialize(s, valid, 8u, &err) != SEMU_OK
            ? 'P' : 'F';
    /* Case 5: truncated section data */
    {
        uint8_t buf[4096];
        memcpy(buf, valid, valid_len);
        semu_error_clear(&err);
        semu_snapshot_reset(s);
        results[res_len++] =
            semu_snapshot_deserialize(s, buf, valid_len - 1u, &err)
                != SEMU_OK ? 'P' : 'F';
    }
    /* Case 6: corrupted section count (too many) */
    {
        uint8_t buf[4096];
        size_t hdr = 4u + 4u + SEMU_ID_MAX + SEMU_REPLAY_HASH_HEX_LEN;
        memcpy(buf, valid, valid_len);
        buf[hdr] = 0xFFu;
        buf[hdr + 1u] = 0u;
        buf[hdr + 2u] = 0u;
        buf[hdr + 3u] = 0u;
        semu_error_clear(&err);
        semu_snapshot_reset(s);
        results[res_len++] =
            semu_snapshot_deserialize(s, buf, valid_len, &err) != SEMU_OK
                ? 'P' : 'F';
    }
    /* Case 7: null buffer */
    semu_error_clear(&err);
    semu_snapshot_reset(s);
    results[res_len++] =
        semu_snapshot_deserialize(s, NULL, 100u, &err) != SEMU_OK
            ? 'P' : 'F';
    /* Case 8: valid round-trip (should succeed = 'S') */
    semu_error_clear(&err);
    semu_snapshot_reset(s);
    {
        semu_status st = semu_snapshot_deserialize(s, valid, valid_len, &err);
        results[res_len++] = st == SEMU_OK ? 'S' : 'F';
    }

    semu_snapshot_destroy(s);
    results[res_len] = '\0';

    /* All malformed cases must fail; valid case must succeed. */
    for (i = 0u; i < res_len; ++i) {
        SEMU_TEST_ASSERT(context, results[i] != 'F');
    }
    SEMU_TEST_ASSERT(context, results[res_len - 1u] == 'S');

    /* Deterministic hash. */
    semu_sha256(results, res_len, digest);
    semu_sha256_format(digest, hash1);
    semu_sha256(results, res_len, digest);
    semu_sha256_format(digest, hash2);
    SEMU_TEST_ASSERT(context, strcmp(hash1, hash2) == 0);
}

static void test_cpu_corpus(semu_test_context *context)
{
    /* Exercise the CPU with instructions that must fail closed. */
    static const uint8_t undef_program[] = {
        0x00u, 0xDEu,  /* UDF #0 — undefined instruction */
    };
    semu_cpu_fixture fixture;
    semu_error err;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_init(&fixture, undef_program,
            sizeof(undef_program)));
    {
        semu_status s = semu_cpu_fixture_step(&fixture);
        SEMU_TEST_ASSERT(context, s != SEMU_OK);
    }
    {
        semu_stop_reason r = semu_cpu_stop_reason(fixture.cpu);
        SEMU_TEST_ASSERT(context,
            r == SEMU_STOP_UNSUPPORTED_INSTRUCTION ||
            r == SEMU_STOP_FIRMWARE_ASSERT);
    }
    semu_cpu_fixture_destroy(&fixture);
}

static void test_corpus_determinism(semu_test_context *context)
{
    /* Run the replay corpus twice and verify identical result hashes. */
    semu_error err;
    semu_replay *r;
    size_t i;
    char run1[256];
    char run2[256];
    size_t r1 = 0u, r2 = 0u;
    uint8_t d1[SEMU_SHA256_SIZE], d2[SEMU_SHA256_SIZE];
    char h1[65], h2[65];

    semu_error_clear(&err);
    r = semu_replay_create(&err);
    SEMU_TEST_ASSERT(context, r != NULL);

    for (i = 0u; i < REPLAY_CASE_COUNT; ++i) {
        size_t len = strlen(REPLAY_CASES[i].text);
        semu_error_clear(&err);
        semu_replay_reset(r);
        if (semu_replay_parse(r, REPLAY_CASES[i].text, len, &err) != SEMU_OK) {
            run1[r1++] = 'P';
        } else {
            run1[r1++] = 'F';
        }
    }
    for (i = 0u; i < REPLAY_CASE_COUNT; ++i) {
        size_t len = strlen(REPLAY_CASES[i].text);
        semu_error_clear(&err);
        semu_replay_reset(r);
        if (semu_replay_parse(r, REPLAY_CASES[i].text, len, &err) != SEMU_OK) {
            run2[r2++] = 'P';
        } else {
            run2[r2++] = 'F';
        }
    }
    semu_replay_destroy(r);
    run1[r1] = '\0';
    run2[r2] = '\0';
    SEMU_TEST_EQ_U64(context, r1, r2);
    SEMU_TEST_ASSERT(context, memcmp(run1, run2, r1) == 0);

    semu_sha256(run1, r1, d1);
    semu_sha256(run2, r2, d2);
    semu_sha256_format(d1, h1);
    semu_sha256_format(d2, h2);
    SEMU_TEST_ASSERT(context, strcmp(h1, h2) == 0);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_replay_corpus),
        SEMU_TEST_CASE(test_snapshot_corpus),
        SEMU_TEST_CASE(test_cpu_corpus),
        SEMU_TEST_CASE(test_corpus_determinism)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
