#include "semu/trace.h"
#include "semu/cpu.h"
#include "semu/bus.h"
#include "semu/scheduler.h"
#include "cpu_fixture.h"
#include "cpu_fixture.c"
#include "test.h"

#include <string.h>
#include <stdlib.h>

static const char *PROFILE = "test-profile";
static const char *FWHASH =
    "c8f2d9e4c114fef0774056a316ad09c42d31b95e2e956f887ed691c3c15a9bfc";

static void test_round_trip(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s1, *s2;
    uint8_t buf[4096];
    size_t len;
    const uint8_t *rdata;
    size_t rsize;
    static const uint8_t payload[] = { 0x01u, 0x02u, 0x03u, 0x04u };

    semu_error_clear(&err);
    s1 = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s1 != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_set_identity(s1, PROFILE, FWHASH, &err));
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_write_section(s1, SEMU_SNAPSHOT_SECTION_CPU_STATE,
            payload, sizeof(payload), &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_snapshot_section_count(s1));

    len = semu_snapshot_serialize(s1, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    s2 = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s2 != NULL);
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_deserialize(s2, buf, len, &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_snapshot_section_count(s2));
    SEMU_TEST_ASSERT(context,
        strcmp(semu_snapshot_profile_id(s2), PROFILE) == 0);
    SEMU_TEST_ASSERT(context,
        strcmp(semu_snapshot_firmware_hash(s2), FWHASH) == 0);

    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_snapshot_read_section(s2, SEMU_SNAPSHOT_SECTION_CPU_STATE,
            &rdata, &rsize));
    SEMU_TEST_EQ_U64(context, sizeof(payload), rsize);
    SEMU_TEST_ASSERT(context, memcmp(rdata, payload, sizeof(payload)) == 0);

    semu_snapshot_destroy(s1);
    semu_snapshot_destroy(s2);
}

static void test_byte_identical(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s1, *s2;
    uint8_t buf1[4096];
    uint8_t buf2[4096];
    size_t len1, len2;
    static const uint8_t payload[] = { 0xDEu, 0xADu, 0xBEu, 0xEFu };

    semu_error_clear(&err);
    s1 = semu_snapshot_create(&err);
    s2 = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s1 != NULL && s2 != NULL);
    semu_snapshot_set_identity(s1, PROFILE, FWHASH, &err);
    semu_snapshot_set_identity(s2, PROFILE, FWHASH, &err);
    semu_snapshot_write_section(s1, SEMU_SNAPSHOT_SECTION_CPU_STATE,
        payload, sizeof(payload), &err);
    semu_snapshot_write_section(s2, SEMU_SNAPSHOT_SECTION_CPU_STATE,
        payload, sizeof(payload), &err);

    len1 = semu_snapshot_serialize(s1, buf1, sizeof(buf1));
    len2 = semu_snapshot_serialize(s2, buf2, sizeof(buf2));
    SEMU_TEST_EQ_U64(context, len1, len2);
    SEMU_TEST_ASSERT(context, memcmp(buf1, buf2, len1) == 0);

    semu_snapshot_destroy(s1);
    semu_snapshot_destroy(s2);
}

static void test_bad_magic(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[4096];
    size_t len;

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    buf[0] ^= 0xFFu;  /* corrupt magic */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, buf, len, &err));
    semu_snapshot_destroy(s);
}

static void test_bad_version(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[4096];
    size_t len;

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    buf[4] = 0xFFu;  /* corrupt version */
    buf[5] = 0u;
    buf[6] = 0u;
    buf[7] = 0u;
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, buf, len, &err));
    semu_snapshot_destroy(s);
}

static void test_truncated(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[4096];
    size_t len;

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    /* Truncate the buffer */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, buf, len / 2u, &err));
    semu_snapshot_destroy(s);
}

static void test_truncated_section_data(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[4096];
    size_t len;
    static const uint8_t payload[] = { 0x01u, 0x02u, 0x03u, 0x04u };

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    semu_snapshot_write_section(s, SEMU_SNAPSHOT_SECTION_CPU_STATE,
        payload, sizeof(payload), &err);
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 4u + 4u);

    /* Truncate by one byte to cut section data */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, buf, len - 1u, &err));
    semu_snapshot_destroy(s);
}

static void test_too_many_sections(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[8192];
    size_t len;
    uint32_t i;

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    /* Write max sections */
    for (i = 0u; i < SEMU_SNAPSHOT_MAX_SECTIONS; ++i) {
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(s, i, NULL, 0u, &err));
    }
    /* One more should fail */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_RANGE,
        semu_snapshot_write_section(s, SEMU_SNAPSHOT_MAX_SECTIONS,
            NULL, 0u, &err));
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    /* Corrupt section count to exceed max */
    {
        size_t header = 4u + 4u + SEMU_ID_MAX + SEMU_REPLAY_HASH_HEX_LEN;
        uint32_t bad = SEMU_SNAPSHOT_MAX_SECTIONS + 1u;
        buf[header] = (uint8_t)bad;
        buf[header + 1u] = (uint8_t)(bad >> 8);
        buf[header + 2u] = (uint8_t)(bad >> 16);
        buf[header + 3u] = (uint8_t)(bad >> 24);
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, buf, len, &err));
    semu_snapshot_destroy(s);
}

static void test_unchanged_after_refusal(semu_test_context *context)
{
    semu_error err;
    semu_snapshot *s;
    uint8_t buf[4096];
    uint8_t bad_buf[4096];
    size_t len;
    static const uint8_t payload[] = { 0x42u };

    semu_error_clear(&err);
    s = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, s != NULL);
    semu_snapshot_set_identity(s, PROFILE, FWHASH, &err);
    semu_snapshot_write_section(s, SEMU_SNAPSHOT_SECTION_CPU_STATE,
        payload, sizeof(payload), &err);
    len = semu_snapshot_serialize(s, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);
    SEMU_TEST_EQ_U64(context, 1u, semu_snapshot_section_count(s));

    /* Create a bad buffer (corrupt magic) */
    memcpy(bad_buf, buf, len);
    bad_buf[0] ^= 0xFFu;

    /* Failed deserialize must not change the snapshot */
    SEMU_TEST_EQ_U64(context, SEMU_ERR_FORMAT,
        semu_snapshot_deserialize(s, bad_buf, len, &err));
    SEMU_TEST_EQ_U64(context, 1u, semu_snapshot_section_count(s));
    {
        const uint8_t *rdata;
        size_t rsize;
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_read_section(s, SEMU_SNAPSHOT_SECTION_CPU_STATE,
                &rdata, &rsize));
        SEMU_TEST_EQ_U64(context, sizeof(payload), rsize);
        SEMU_TEST_ASSERT(context, rdata[0] == 0x42u);
    }
    semu_snapshot_destroy(s);
}

static void test_continued_run_matches(semu_test_context *context)
{
    /* Run a synthetic program, snapshot state, continue both ways,
     * and verify the results match byte-for-byte. */
    static const uint8_t program[] = {
        /* MOV R0, #1 (Thumb: 2001) */
        0x01u, 0x20u,
        /* MOV R1, #2 (Thumb: 2102) */
        0x02u, 0x21u,
        /* ADD R0, R1 (Thumb: 1840) */
        0x40u, 0x18u,
        /* MOV R2, R0 (Thumb: 4602) */
        0x02u, 0x46u,
        /* MOV R3, #5 (Thumb: 2305) */
        0x05u, 0x23u,
        /* ADD R2, R3 (Thumb: 1852) */
        0x52u, 0x18u,
        /* MOV R4, R2 (Thumb: 460A... wait, Thumb16 MOV Rd, Rm is 460D for R4,R2... */
        /* Actually: MOV R4, R2 => 4614 (01000110 D Rm Rd) */
        /* For simplicity, just use NOPs after the first few */
        0x00u, 0xBFu,  /* NOP */
        0x00u, 0xBFu,  /* NOP */
        0x00u, 0xBFu,  /* NOP */
        0x00u, 0xBFu,  /* NOP */
    };
    semu_cpu_fixture fa, fb;
    semu_error err;
    semu_snapshot *snap;
    uint8_t buf[8192];
    size_t len;
    const semu_cpu_state *sa, *sb;

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_init(&fa, program, sizeof(program)));
    SEMU_TEST_ASSERT(context,
        semu_cpu_fixture_init(&fb, program, sizeof(program)));

    /* Run 3 instructions on both */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fa, 3u));
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fb, 3u));

    /* Verify they're in the same state */
    sa = semu_cpu_get_state(fa.cpu);
    sb = semu_cpu_get_state(fb.cpu);
    SEMU_TEST_EQ_U64(context, sa->r[0], sb->r[0]);
    SEMU_TEST_EQ_U64(context, sa->r[1], sb->r[1]);

    /* Snapshot fixture A's CPU state and RAM */
    snap = semu_snapshot_create(&err);
    SEMU_TEST_ASSERT(context, snap != NULL);
    semu_snapshot_set_identity(snap, PROFILE, FWHASH, &err);
    {
        semu_cpu_state state = *semu_cpu_get_state(fa.cpu);
        uint8_t ram[0x1000u];
        uint64_t vtime = semu_scheduler_now(fa.scheduler);

        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(snap, SEMU_SNAPSHOT_SECTION_CPU_STATE,
                (const uint8_t *)&state, sizeof(state), &err));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_bus_copy_out(fa.bus, 0u, ram, sizeof(ram), &err));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(snap, SEMU_SNAPSHOT_SECTION_RAM,
                ram, sizeof(ram), &err));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_write_section(snap,
                SEMU_SNAPSHOT_SECTION_VIRTUAL_TIME,
                (const uint8_t *)&vtime, sizeof(vtime), &err));
    }
    len = semu_snapshot_serialize(snap, buf, sizeof(buf));
    SEMU_TEST_ASSERT(context, len > 0u);

    /* Continue running fixture A for 4 more instructions */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fa, 4u));
    sa = semu_cpu_get_state(fa.cpu);

    /* Deserialize snapshot into a fresh snapshot, restore to fixture B */
    {
        semu_snapshot *snap2 = semu_snapshot_create(&err);
        const uint8_t *cdata;
        size_t csize;
        SEMU_TEST_ASSERT(context, snap2 != NULL);
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_deserialize(snap2, buf, len, &err));
        SEMU_TEST_EQ_U64(context, SEMU_OK,
            semu_snapshot_read_section(snap2, SEMU_SNAPSHOT_SECTION_CPU_STATE,
                &cdata, &csize));
        SEMU_TEST_EQ_U64(context, sizeof(semu_cpu_state), csize);
        {
            semu_cpu_state state;
            memcpy(&state, cdata, sizeof(state));
            semu_cpu_fixture_apply_state(&fb, &state);
        }
        semu_snapshot_destroy(snap2);
    }

    /* Run fixture B for the same 4 instructions */
    SEMU_TEST_EQ_U64(context, SEMU_OK, semu_cpu_fixture_run(&fb, 4u));
    sb = semu_cpu_get_state(fb.cpu);

    /* Results must match */
    SEMU_TEST_EQ_U64(context, sa->r[0], sb->r[0]);
    SEMU_TEST_EQ_U64(context, sa->r[1], sb->r[1]);
    SEMU_TEST_EQ_U64(context, sa->r[2], sb->r[2]);
    SEMU_TEST_EQ_U64(context, sa->r[3], sb->r[3]);
    SEMU_TEST_EQ_U64(context, sa->instructions, sb->instructions);

    semu_snapshot_destroy(snap);
    semu_cpu_fixture_destroy(&fa);
    semu_cpu_fixture_destroy(&fb);
}

static void test_null_safety(semu_test_context *context)
{
    semu_error err;
    uint8_t buf[16];

    semu_error_clear(&err);
    SEMU_TEST_ASSERT(context, semu_snapshot_create(&err) != NULL);
    SEMU_TEST_EQ_U64(context, 0u, semu_snapshot_section_count(NULL));
    SEMU_TEST_ASSERT(context, semu_snapshot_profile_id(NULL) == NULL);
    SEMU_TEST_ASSERT(context, semu_snapshot_firmware_hash(NULL) == NULL);
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_set_identity(NULL, "x",
            "0000000000000000000000000000000000000000000000000000000000000000",
            &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_write_section(NULL, 0u, NULL, 0u, &err));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_read_section(NULL, 0u, NULL, NULL));
    SEMU_TEST_EQ_U64(context, 0u,
        semu_snapshot_serialize(NULL, buf, sizeof(buf)));
    SEMU_TEST_EQ_U64(context, SEMU_ERR_ARGUMENT,
        semu_snapshot_deserialize(NULL, buf, 1u, &err));
    /* semu_snapshot_destroy(NULL) must not crash */
    semu_snapshot_destroy(NULL);
    semu_snapshot_reset(NULL);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_round_trip),
        SEMU_TEST_CASE(test_byte_identical),
        SEMU_TEST_CASE(test_bad_magic),
        SEMU_TEST_CASE(test_bad_version),
        SEMU_TEST_CASE(test_truncated),
        SEMU_TEST_CASE(test_truncated_section_data),
        SEMU_TEST_CASE(test_too_many_sections),
        SEMU_TEST_CASE(test_unchanged_after_refusal),
        SEMU_TEST_CASE(test_continued_run_matches),
        SEMU_TEST_CASE(test_null_safety)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
