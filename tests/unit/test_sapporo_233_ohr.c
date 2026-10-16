/* Ticket 710 instance-36: the 2.33 OHR2 startup fixture. The E-SAP-0074
 * next-refusal boundary is the guest's first IOM2 address-0x10 exchange;
 * the lane answered the captured request frame with the shared transport
 * law (header echo, synthetic identity/zero bodies, CRC32). This suite
 * pins the body provider's ordered responses, atomic refusals, budget,
 * reset renewal and wrong-profile isolation. */
#include "../src/compat/sapporo_233_ohr.h"
#include "test.h"
#include <string.h>

typedef struct {
    semu_sapporo_233_ohr_context ctx;
    semu_layer_state state;
    semu_logger logger;
    FILE *log;
} fixture;

static int init(fixture *f, semu_error *e)
{
    const char *hashes[3];
    unsigned i;
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    if (!f->log) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = semu_sapporo_233_ohr_layer.component_hashes[i];
    }
    if (semu_layer_enable_checked(&f->state, &semu_sapporo_233_ohr_layer,
            "sapporo-2.33.16", hashes, 3u, e) != SEMU_OK) {
        return 0;
    }
    f->ctx.state = &f->state;
    f->ctx.logger = &f->logger;
    return 1;
}

static void test_ordered_responses(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint8_t request[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    static const uint16_t commands[7] = {
        0x0010u, 0x0000u, 0x0010u, 0x0000u, 0x000du, 0x000eu, 0x0006u
    };
    static const uint8_t states[7] = {
        SEMU_SAPPORO_OHR2_BSL, SEMU_SAPPORO_OHR2_BSL,
        SEMU_SAPPORO_OHR2_MAIN, SEMU_SAPPORO_OHR2_MAIN,
        SEMU_SAPPORO_OHR2_MAIN, SEMU_SAPPORO_OHR2_MAIN,
        SEMU_SAPPORO_OHR2_MAIN
    };
    unsigned i;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    for (i = 0u; i < 7u; ++i) {
        memset(request, 0xff, sizeof(request));
        if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE) {
            request[4] = 0x01u; /* payload[4] is frame byte 5 */
        }
        if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
            /* The lane echo body: ten caller bytes at 4..13. */
            memcpy(request + 4u,
                   (const uint8_t[]){0x48u, 0x60u, 0xe8u, 0x83u, 0xd5u,
                                     0x01u, 0x00u, 0x00u, 0x00u, 0x00u},
                   10u);
        }
        memset(response, 0xaa, sizeof(response));
        SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
            semu_sapporo_233_ohr_body_provider(&f.ctx, commands[i],
                (uint16_t)i, states[i], request, response, &e));
        SEMU_TEST_EQ_U64(context, i + 1u, f.state.hits);
        /* The transport overwrites payload 0..3 with the validated header;
         * the provider owns the body bytes only. */
        if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY &&
            states[i] == SEMU_SAPPORO_OHR2_BSL) {
            SEMU_TEST_ASSERT(context, memcmp(response + 9u, "BSL", 3u) == 0);
        } else if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_IDENTITY) {
            SEMU_TEST_ASSERT(context, memcmp(response + 9u, "MAIN", 4u) == 0);
        } else if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
            SEMU_TEST_ASSERT(context, memcmp(response + 4u, request + 4u,
                sizeof(response) - 4u) == 0);
        } else {
            SEMU_TEST_ASSERT(context, response[4u] == 0u &&
                response[53u] == 0u);
        }
    }
    (void)fclose(f.log);
}

static void test_refusals(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint8_t request[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    SEMU_TEST_ASSERT(context, init(&f, &e));
    memset(request, 0xff, sizeof(request));
    /* Wrong sequence before the pinned one refuses. */
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_233_ohr_body_provider(&f.ctx,
            SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 1u, SEMU_SAPPORO_OHR2_BSL,
            request, response, &e));
    /* After a refusal the fixture is sticky: even the pinned first request
     * refuses (no silent budget reset). */
    request[4] = 0x01u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_233_ohr_body_provider(&f.ctx,
            SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 0u, SEMU_SAPPORO_OHR2_BSL,
            request, response, &e));
    (void)fclose(f.log);
}

static void test_exhaustion_and_reset_renewal(semu_test_context *context)
{
    fixture f;
    semu_error e;
    uint8_t request[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    uint8_t response[SEMU_SAPPORO_OHR2_PAYLOAD_SIZE];
    unsigned i;
    SEMU_TEST_ASSERT(context, init(&f, &e));
    memset(request, 0xff, sizeof(request));
    request[4] = 0x01u;
    for (i = 0u; i < 7u; ++i) {
        static const uint16_t commands[7] = {
            0x0010u, 0x0000u, 0x0010u, 0x0000u, 0x000du, 0x000eu, 0x0006u
        };
        const uint8_t state =
            (i == 0u || i == 1u) ? SEMU_SAPPORO_OHR2_BSL
                                 : SEMU_SAPPORO_OHR2_MAIN;
        /* Boot-mode rounds pin payload 4 = 1; the others are ff padding. */
        request[4] = (i == 0u || i == 2u) ? 0x01u : 0xffu;
        if (commands[i] == SEMU_SAPPORO_OHR2_COMMAND_ECHO) {
            memcpy(request + 4u,
                   (const uint8_t[]){0x48u, 0x60u, 0xe8u, 0x83u, 0xd5u,
                                     0x01u, 0x00u, 0x00u, 0x00u, 0x00u},
                   10u);
        }
        SEMU_TEST_ASSERT(context,
            semu_sapporo_233_ohr_body_provider(&f.ctx, commands[i],
                (uint16_t)i, state, request, response,
                &e) == SEMU_TRANSACTION_OK);
    }
    /* The budget is the staged-window count; a further request refuses. */
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_REFUSE,
        semu_sapporo_233_ohr_body_provider(&f.ctx,
            SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 0u, SEMU_SAPPORO_OHR2_BSL,
            request, response, &e));
    /* Reset clears the refusal and renews the round: the machine reset
     * re-enables the layer (hits back to zero) and the provider restarts. */
    semu_sapporo_233_ohr_reset(&f.ctx);
    f.ctx.state = &f.state;
    SEMU_TEST_EQ_U64(context, SEMU_OK,
        semu_layer_enable(&f.state, &semu_sapporo_233_ohr_layer,
            "sapporo-2.33.16", &e));
    SEMU_TEST_EQ_U64(context, 0u, f.state.hits);
    memset(request, 0xff, sizeof(request));
    request[4] = 0x01u;
    SEMU_TEST_EQ_U64(context, SEMU_TRANSACTION_OK,
        semu_sapporo_233_ohr_body_provider(&f.ctx,
            SEMU_SAPPORO_OHR2_COMMAND_BOOT_MODE, 0u, SEMU_SAPPORO_OHR2_BSL,
            request, response, &e));
    (void)fclose(f.log);
}

static void test_wrong_profile_refuses_enable(semu_test_context *context)
{
    fixture f;
    semu_error e;
    const char *hashes[3];
    unsigned i;
    memset(&f, 0, sizeof(f));
    f.log = tmpfile();
    SEMU_TEST_ASSERT(context, f.log);
    semu_log_init(&f.logger, f.log, SEMU_LOG_WARNING);
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = semu_sapporo_233_ohr_layer.component_hashes[i];
    }
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&f.state, &semu_sapporo_233_ohr_layer,
            "sapporo-2.35.34", hashes, 3u, &e));
    (void)fclose(f.log);
}

static void test_wrong_component_hash_refuses_enable(semu_test_context *context)
{
    fixture f;
    semu_error e;
    const char *hashes[3];
    unsigned i;
    memset(&f, 0, sizeof(f));
    f.log = tmpfile();
    SEMU_TEST_ASSERT(context, f.log);
    semu_log_init(&f.logger, f.log, SEMU_LOG_WARNING);
    for (i = 0u; i < 3u; ++i) {
        hashes[i] = semu_sapporo_233_ohr_layer.component_hashes[i];
    }
    hashes[1] = "0000000000000000000000000000000000000000000000000000000000000000";
    SEMU_TEST_EQ_U64(context, SEMU_ERR_CONFLICT,
        semu_layer_enable_checked(&f.state, &semu_sapporo_233_ohr_layer,
            "sapporo-2.33.16", hashes, 3u, &e));
    (void)fclose(f.log);
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_ordered_responses),
        SEMU_TEST_CASE(test_refusals),
        SEMU_TEST_CASE(test_exhaustion_and_reset_renewal),
        SEMU_TEST_CASE(test_wrong_profile_refuses_enable),
        SEMU_TEST_CASE(test_wrong_component_hash_refuses_enable)
    };
    return semu_test_run(cases, sizeof(cases) / sizeof(cases[0]));
}
