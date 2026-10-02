#include "test.h"
#include "sapporo_235_ohr.h"
#include "../../src/boards/machine_internal.h"
#include <stdio.h>
#include <string.h>

typedef struct {
    semu_layer_state layer;
    semu_sapporo_235_ohr_context context;
    semu_logger logger;
    semu_error error;
    FILE *log;
    unsigned edges;
    int ready;
} fixture;

static int setup(fixture *f)
{
    memset(f, 0, sizeof(*f));
    f->log = tmpfile();
    if (!f->log) return 0;
    semu_log_init(&f->logger, f->log, SEMU_LOG_WARNING);
    f->context.state = &f->layer;
    f->context.logger = &f->logger;
    return semu_layer_enable_checked(&f->layer, &semu_sapporo_235_ohr_layer,
        "sapporo-2.35.34", semu_sapporo_235_ohr_layer.component_hashes,
        3u, &f->error) == SEMU_OK;
}

static void signal_ready(void *context, unsigned signal, int level)
{
    fixture *f = context;
    (void)signal;
    ++f->edges;
    f->ready = level;
}

static void make_body(uint8_t body[54], unsigned command, unsigned sequence)
{
    unsigned i;
    memset(body, 0xff, 54u);
    body[0] = (uint8_t)command; body[1] = 0u;
    body[2] = (uint8_t)sequence; body[3] = 0u;
    if (command == 16u) body[4] = 1u;
    if (command == 6u) for (i = 4u; i < 14u; ++i) body[i] = (uint8_t)i;
}

static void frame(uint8_t packet[59], unsigned command, unsigned sequence)
{
    uint32_t crc;
    unsigned i;
    packet[0] = 0u;
    make_body(packet + 1u, command, sequence);
    crc = semu_sapporo_ohr2_crc32(packet + 1u, 54u);
    for (i = 0u; i < 4u; ++i) packet[55u + i] = (uint8_t)(crc >> (8u * i));
}

static semu_transaction_result exchange(semu_serial_endpoint ep,
    const uint8_t *tx, size_t tx_size, uint8_t *rx, size_t rx_size,
    semu_error *error)
{
    semu_serial_transaction t;
    memset(&t, 0, sizeof(t));
    t.address = 0x10u; t.tx = tx; t.tx_size = tx_size;
    t.rx = rx; t.rx_size = rx_size;
    return ep.transfer(ep.context, &t, error);
}

static size_t parse_hex(const char *hex, uint8_t *out, size_t max)
{
    size_t i;
    for (i = 0u; i < max && hex[i * 2u] != '\0' && hex[i * 2u + 1u] != '\0'; ++i) {
        unsigned byte = 0u, j;
        for (j = 0u; j < 2u; ++j) {
            const char ch = hex[i * 2u + j];
            unsigned digit;
            if (ch >= '0' && ch <= '9') digit = (unsigned)(ch - '0');
            else if (ch >= 'a' && ch <= 'f') digit = (unsigned)(ch - 'a') + 10u;
            else return 0u;
            byte = (byte << 4u) | digit;
        }
        out[i] = (uint8_t)byte;
    }
    return i;
}

/* E-SAP-0041-EXT: the post-Done MAIN-state query round. The six request
 * frames below are the byte-exact packets an in-tree OHR2 byte census
 * captured from the five-layer 2.35.34 setup-walk at the point where the
 * eight-hit startup budget was exhausted, and each listed response frame was
 * reproduced twice byte-identically by the read-only lane endpoint (lane
 * replay /tmp/sap235-710ohr/lane-replay-extended.resc, normalised transcript
 * sha256 4d6309fccc4b71d897c236832d17dbaaab11bc3d0eb3fb1f0150cd0303d37d14
 * from the fresh slice-2 pair /tmp/sap235-710ohr/slice2-lane-ext-{1,2}.norm,
 * byte-identical to the slice-1 capture of the same sha256). Command 0x0004
 * sequence 13 was admitted by the ticket 710 integrator ruling of 2026-09-23:
 * the lane answered the provably guest-issued frame twice with a zero body.
 * Only the eight pinned startup frames plus these six may be answered; any
 * later request, a wrong sequence, or a payload outside the pinned shape keeps
 * refusing. */
static const char *const ext_requests[] = {
    /* command 0x0010 sequence 8 */
    "001000080001ffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffffb7e4559f",
    /* command 0x0000 sequence 9 */
    "0000000900ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff4a8afad8",
    /* command 0x000e sequence 10 */
    "000e000a00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff59365f6a",
    /* command 0x0006 sequence 11 */
    "0006000b003ad7e883d50100000000ffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff88f916fe",
    /* command 0x0002 sequence 12 */
    "0002000c00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff5ed33fce",
    /* command 0x0004 sequence 13: payload byte 0x23 at offset 4, zeroes to
     * offset 18, pinned padding after (the first payload-carrying query). */
    "0004000d00230000000000000000000000000000ffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff276ae026",
};

static const char *const ext_responses[] = {
    /* command 0x0010 sequence 8 */
    "1000080000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000771a7cd7",
    /* command 0x0000 sequence 9 */
    "0000090000000000004d41494e00000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000440b375d",
    /* command 0x000e sequence 10 */
    "0e000a0000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000009c14f16",
    /* command 0x0006 sequence 11 */
    "06000b003ad7e883d50100000000ffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffff88f916fe",
    /* command 0x0002 sequence 12 */
    "02000c0000000000000000000000000000000000000000000000000000000000"
    "000000000000000000000000000000000000000000000e242fb2",
    /* command 0x0004 sequence 13: lane zero body, command/sequence echoed. */
    "04000d0000000000000000000000000000000000000000000000000000000000"
    "00000000000000000000000000000000000000000000b72d3ede",
};

static const char *const ext_unknown_request =
    /* command 0x0004 sequence 14: the observed frame advanced by one
     * sequence. It is outside the pinned round and must refuse. */
    "0004000e00230000000000000000000000000000ffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff306eb59c";

/* E-SAP-0041-TAIL (integrator ruling of 2026-09-23, option B'): once all
 * fourteen pinned entries are consumed, the guest keeps polling command
 * 0x0002 with the same default padding shape every ~955 ms of guest runtime.
 * The two request frames below are the byte-exact packets the in-tree OHR2
 * byte census captured from the five-layer 2.35.34 setup-walk at sequences 14
 * and 15 (census stream /tmp/sap235-710ohr/s3shim-probe-{1,2}.txt, sha256
 * ce9b54fa99671f81e51aba3712a2a4531b3813b31b09e83cf327de3ab5fafde6, twice
 * byte-identical), and both responses were reproduced twice byte-identically
 * by the read-only lane endpoint in the poll-family replay
 * (/tmp/sap235-710ohr/lane-replay-slice3-poll.resc, normalised transcript
 * sha256 b41b48839f41650dd559452ea7a8b48e5e31a0c7ee627399812c6168b0be7720,
 * from the pair /tmp/sap235-710ohr/slice3-poll-{1,2}.norm) - all 19 answers of
 * that replay are byte-identical to what the in-tree lane-law shim produced.
 * The lane's answer is a pure function of command and sequence: header echo
 * plus a zero body. The tail therefore answers by law rather than by
 * enumeration, so sequences past 15 are answered by the same function; any
 * other command, shape, state, or an unconsumed prefix still refuses. */
static const char *const tail_requests[] = {
    /* command 0x0002 sequence 14: default padding law, byte-identical to the
     * pinned sequence-12 frame apart from the sequence field. */
    "0002000e00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff442ba61d",
    /* command 0x0002 sequence 15: the next poll, one period later. */
    "0002000f00ffffffffffffffffffffffffffffffffffffffffffffffffffffff"
    "ffffffffffffffffffffffffffffffffffffffffffffff49d76a74",
};

static const char *const tail_responses[] = {
    /* command 0x0002 sequence 14: lane zero body, command/sequence echoed. */
    "02000e0000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000014dcb661",
    /* command 0x0002 sequence 15: same law, next sequence. */
    "02000f0000000000000000000000000000000000000000000000000000000000"
    "0000000000000000000000000000000000000000000019207a08",
};

static void test_sapporo_235_ohr_post_done_queries(semu_test_context *c)
{
    const unsigned commands[] = {16u, 0u, 16u, 0u, 13u, 14u, 6u, 2u};
    const uint8_t selector = 0x3cu;
    uint8_t packet[59], response[58], expected[59];
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint ep;
    fixture f;
    unsigned i, j;
    char log[8192];
    SEMU_TEST_ASSERT(c, setup(&f));
    device = semu_sapporo_ohr2_create(signal_ready, &f,
        semu_sapporo_235_ohr_body_provider, &f.context, &f.error);
    SEMU_TEST_ASSERT(c, device != NULL);
    ep = semu_sapporo_ohr2_endpoint(device);
    for (i = 0u; i < 8u; ++i) {
        if (i == 2u) {
            frame(packet, 3u, 2u);
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
                exchange(ep, packet, 59u, NULL, 0u, &f.error));
        }
        frame(packet, commands[i], i);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
    }
    SEMU_TEST_EQ_U64(c, 8u, f.layer.hits);
    for (i = 0u; i < 6u; ++i) {
        SEMU_TEST_EQ_U64(c, 59u, parse_hex(ext_requests[i], packet, 59u));
        SEMU_TEST_EQ_U64(c, 58u, parse_hex(ext_responses[i], expected, 58u));
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 1u, f.ready);
        SEMU_TEST_EQ_U64(c, 9u + i, f.layer.hits);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        SEMU_TEST_ASSERT(c, memcmp(expected, response, 58u) == 0);
    }
    SEMU_TEST_EQ_U64(c, 14u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 28u, f.edges);

    /* E-SAP-0041-TAIL: with the pinned prefix consumed, the law-governed poll
     * tail answers command 0x0002 at two different sequences, which is the
     * claim that the answer is a function of command and sequence rather than
     * a fifteenth enumerated fixture entry. */
    for (i = 0u; i < 2u; ++i) {
        SEMU_TEST_EQ_U64(c, 59u, parse_hex(tail_requests[i], packet, 59u));
        SEMU_TEST_EQ_U64(c, 58u, parse_hex(tail_responses[i], expected, 58u));
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 1u, f.ready);
        SEMU_TEST_EQ_U64(c, 15u + i, f.layer.hits);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        SEMU_TEST_ASSERT(c, memcmp(expected, response, 58u) == 0);
    }
    SEMU_TEST_EQ_U64(c, 16u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 32u, f.edges);
    rewind(f.log);
    j = (unsigned)fread(log, 1u, sizeof(log) - 1u, f.log); log[j] = '\0';
    SEMU_TEST_ASSERT(c, strstr(log, "hit=16 maximum=30") != NULL);
    SEMU_TEST_ASSERT(c, strstr(log, "command=0004 sequence=13") != NULL);
    SEMU_TEST_ASSERT(c, strstr(log, "command=0002 sequence=15") != NULL);

    /* Refusal cases inside the tail, each without consuming budget: a pinned
     * one-shot query the tail does not admit, a known command out of its turn,
     * and the echo command whose body law belongs to the prefix only. */
    for (i = 0u; i < 3u; ++i) {
        frame(packet, i == 0u ? 4u : (i == 1u ? 14u : 6u), 16u);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        SEMU_TEST_EQ_U64(c, 16u, f.layer.hits);
        SEMU_TEST_EQ_U64(c, 32u, f.edges);
    }
    semu_sapporo_ohr2_destroy(device);
    fclose(f.log);
}

/* E-SAP-0041-EXT (integrator ruling of 2026-09-23): command 0x0004 sequence
 * 13 is admitted at round position 14, but only as the captured frame. Its
 * payload byte 0x23 at offset 4, the zero region through offset 18, the
 * pinned ff padding, sequence 13, and MAIN state are all required; every
 * deviation fails closed without consuming budget. */
static void test_sapporo_235_ohr_result4_law(semu_test_context *c)
{
    static const unsigned mutations[] = {0u, 1u, 2u, 3u, 4u, 5u};
    unsigned i, k;
    for (i = 0u; i < 6u; ++i) {
        fixture f;
        uint8_t body[54], response[54];
        uint16_t sequence = 13u;
        SEMU_TEST_ASSERT(c, setup(&f));
        f.layer.hits = 13u;
        memset(body, 0xff, sizeof(body));
        body[0] = 4u; body[1] = 0u; body[2] = 13u; body[3] = 0u;
        body[4] = 0x23u;
        for (k = 5u; k < 19u; ++k) body[k] = 0u;
        switch (mutations[i]) {
        case 0: break;
        case 1: sequence = 14u; break;
        case 2: body[0] = 0u; break;
        case 3: body[4] = 0x24u; break;
        case 4: body[18] = 0xffu; break;
        case 5: body[53] = 0u; break;
        }
        memset(response, 0xa5, sizeof(response));
        if (mutations[i] == 0u)
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
                semu_sapporo_235_ohr_body_provider(&f.context,
                    (semu_sapporo_ohr2_command)4u, sequence,
                    SEMU_SAPPORO_OHR2_MAIN, body, response, &f.error));
        else
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
                semu_sapporo_235_ohr_body_provider(&f.context,
                    (semu_sapporo_ohr2_command)4u, sequence,
                    SEMU_SAPPORO_OHR2_MAIN, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, mutations[i] == 0u ? 14u : 13u, f.layer.hits);
        for (k = 0u; k < 54u; ++k)
            SEMU_TEST_EQ_U64(c, mutations[i] == 0u ? 0u : 0xa5u, response[k]);
        fclose(f.log);
    }
}

/* E-SAP-0041-TAIL (integrator ruling of 2026-09-23): the poll tail admits only
 * command 0x0002, only in MAIN, only with the default padding shape, and only
 * once all fourteen pinned entries were consumed; its answer is the pinned zero
 * body at whatever sequence the transport expects. Everything else fails closed
 * without consuming budget. */
static void test_sapporo_235_ohr_poll_tail_law(semu_test_context *c)
{
    struct tail_case {
        uint64_t hits;
        unsigned command;
        uint16_t sequence;
        semu_sapporo_ohr2_state state;
        unsigned mutate;
        int accept;
    };
    static const struct tail_case cases[] = {
        {14u, 2u, 14u, SEMU_SAPPORO_OHR2_MAIN, 0u, 1},
        {15u, 2u, 15u, SEMU_SAPPORO_OHR2_MAIN, 0u, 1},
        {14u, 2u, 14u, SEMU_SAPPORO_OHR2_BSL, 0u, 0},
        {14u, 4u, 14u, SEMU_SAPPORO_OHR2_MAIN, 1u, 0},
        {14u, 6u, 14u, SEMU_SAPPORO_OHR2_MAIN, 2u, 0},
        {14u, 14u, 14u, SEMU_SAPPORO_OHR2_MAIN, 0u, 0},
        {14u, 2u, 14u, SEMU_SAPPORO_OHR2_MAIN, 3u, 0},
        {14u, 2u, 14u, SEMU_SAPPORO_OHR2_MAIN, 4u, 0},
        {14u, 2u, 14u, SEMU_SAPPORO_OHR2_MAIN, 5u, 0},
        {13u, 2u, 13u, SEMU_SAPPORO_OHR2_MAIN, 0u, 0},
        {30u, 2u, 30u, SEMU_SAPPORO_OHR2_MAIN, 0u, 0}
    };
    const size_t count = sizeof(cases) / sizeof(cases[0]);
    size_t i;
    unsigned k;
    for (i = 0u; i < count; ++i) {
        fixture f;
        uint8_t body[54], response[54], packet[59];
        char want[40];
        semu_transaction_result want_result = cases[i].accept ?
            SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE;
        SEMU_TEST_ASSERT(c, setup(&f));
        f.layer.hits = cases[i].hits;
        memset(body, 0xff, sizeof(body));
        body[0] = (uint8_t)cases[i].command; body[1] = 0u;
        body[2] = (uint8_t)cases[i].sequence; body[3] = 0u;
        switch (cases[i].mutate) {
        case 0u: break;
        case 1u: /* the pinned one-shot command 0x0004 sequence-14 query */
            SEMU_TEST_EQ_U64(c, 59u,
                parse_hex(ext_unknown_request, packet, 59u));
            memcpy(body, packet + 1u, 54u);
            break;
        case 2u: for (k = 4u; k < 14u; ++k) body[k] = (uint8_t)k; break;
        case 3u: body[4] = 0u; break;
        case 4u: body[53] = 0u; break;
        case 5u: body[2] = (uint8_t)(cases[i].sequence + 1u); break;
        }
        memset(response, 0xa5, sizeof(response));
        SEMU_TEST_EQ_U64(c, want_result,
            semu_sapporo_235_ohr_body_provider(&f.context,
                (semu_sapporo_ohr2_command)cases[i].command, cases[i].sequence,
                cases[i].state, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, cases[i].accept ? cases[i].hits + 1u : cases[i].hits,
            f.layer.hits);
        for (k = 0u; k < 54u; ++k)
            SEMU_TEST_EQ_U64(c, cases[i].accept ? 0u : 0xa5u, response[k]);
        if (cases[i].accept) {
            char log[2048];
            size_t j;
            (void)snprintf(want, sizeof(want), "hit=%llu maximum=30",
                (unsigned long long)cases[i].hits + 1ull);
            rewind(f.log);
            j = fread(log, 1u, sizeof(log) - 1u, f.log); log[j] = '\0';
            SEMU_TEST_ASSERT(c, strstr(log, want) != NULL);
            SEMU_TEST_ASSERT(c, strstr(log, "trigger=ohr-poll") != NULL);
        }
        fclose(f.log);
    }
}

static void test_sapporo_235_ohr_startup_transport(semu_test_context *c)
{
    const unsigned commands[] = {16u, 0u, 16u, 0u, 13u, 14u, 6u, 2u};
    const uint8_t selector = 0x3cu;
    uint8_t packet[59], response[58], expected[59];
    semu_sapporo_ohr2 *device;
    semu_serial_endpoint ep;
    fixture f;
    unsigned i, j;
    char log[4096];
    SEMU_TEST_ASSERT(c, setup(&f));
    device = semu_sapporo_ohr2_create(signal_ready, &f,
        semu_sapporo_235_ohr_body_provider, &f.context, &f.error);
    SEMU_TEST_ASSERT(c, device != NULL);
    ep = semu_sapporo_ohr2_endpoint(device);
    for (i = 0u; i < 8u; ++i) {
        uint32_t crc;
        if (i == 2u) {
            frame(packet, 3u, 2u);
            SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
                exchange(ep, packet, 59u, NULL, 0u, &f.error));
            SEMU_TEST_EQ_U64(c, 0u, f.ready);
        }
        frame(packet, commands[i], i);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, packet, 59u, NULL, 0u, &f.error));
        SEMU_TEST_EQ_U64(c, 1u, f.ready);
        SEMU_TEST_EQ_U64(c, i + 1u, f.layer.hits);
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
            exchange(ep, &selector, 1u, response, 58u, &f.error));
        SEMU_TEST_EQ_U64(c, 0u, f.ready);
        memset(expected, 0, sizeof(expected));
        expected[0] = (uint8_t)commands[i]; expected[2] = (uint8_t)i;
        if (i == 1u) memcpy(expected + 9u, "BSL", 3u);
        if (i == 3u) memcpy(expected + 9u, "MAIN", 4u);
        if (i == 6u) memcpy(expected + 4u, packet + 5u, 50u);
        crc = semu_sapporo_ohr2_crc32(expected, 54u);
        for (j = 0u; j < 4u; ++j) expected[54u+j] = (uint8_t)(crc >> (8u*j));
        SEMU_TEST_ASSERT(c, memcmp(expected, response, 58u) == 0);
    }
    SEMU_TEST_EQ_U64(c, 16u, f.edges);
    /* E-SAP-0041-TAIL keeps the tail unreachable until the whole pinned
     * prefix was consumed: this poll-shaped request at the next free sequence
     * still refuses, latches the refusal, and consumes nothing. */
    frame(packet, 2u, 8u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 8u, f.layer.hits);
    SEMU_TEST_EQ_U64(c, 16u, f.edges);
    SEMU_TEST_ASSERT(c, f.context.refusal.code != SEMU_OK);
    rewind(f.log);
    j = (unsigned)fread(log, 1u, sizeof(log)-1u, f.log); log[j] = '\0';
    SEMU_TEST_ASSERT(c, strstr(log, "hit=8 maximum=30") != NULL);
    SEMU_TEST_ASSERT(c, strstr(log, "command=0002 sequence=7") != NULL);
    semu_sapporo_ohr2_destroy(device);
    fclose(f.log);
}

static void test_sapporo_235_ohr_atomic_body_refusals(semu_test_context *c)
{
    unsigned i, j;
    for (i = 0u; i < 9u; ++i) {
        fixture f;
        uint8_t body[54], response[54];
        semu_sapporo_ohr2_state state = SEMU_SAPPORO_OHR2_BSL;
        unsigned command = 16u, sequence = 0u;
        SEMU_TEST_ASSERT(c, setup(&f));
        make_body(body, command, sequence);
        memset(response, 0xa5, sizeof(response));
        switch (i) {
        case 0: f.layer.enabled = 0; break;
        case 1: state = SEMU_SAPPORO_OHR2_MAIN; break;
        case 2: sequence = 1u; break;
        case 3: command = 0u; break;
        case 4: body[4] = 2u; break;
        case 5: body[53] = 0u; break;
        case 6: body[1] = 1u; break;
        case 7: f.layer.hits = 14u; break;
        case 8: f.layer.hits = 30u; break; /* pinned prefix + tail budget gone */
        }
        SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
            semu_sapporo_235_ohr_body_provider(&f.context,
                (semu_sapporo_ohr2_command)command, (uint16_t)sequence,
                state, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, i == 7u ? 14u : (i == 8u ? 30u : 0u), f.layer.hits);
        for (j = 0u; j < 54u; ++j) SEMU_TEST_EQ_U64(c, 0xa5u, response[j]);
        fclose(f.log);
    }
}

static void test_sapporo_235_ohr_pins_and_owners(semu_test_context *c)
{
    fixture a, b;
    const char *hashes[3];
    uint8_t body[54], response[54];
    unsigned i;
    SEMU_TEST_ASSERT(c, setup(&a));
    SEMU_TEST_ASSERT(c, setup(&b));
    make_body(body, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
        semu_sapporo_235_ohr_body_provider(&a.context, 16u, 0u,
            SEMU_SAPPORO_OHR2_BSL, body, response, &a.error));
    SEMU_TEST_EQ_U64(c, 0u, b.layer.hits);
    SEMU_TEST_ASSERT(c, semu_layer_enable_checked(&b.layer,
        &semu_sapporo_235_ohr_layer, "sapporo-2.39.20",
        semu_sapporo_235_ohr_layer.component_hashes, 3u, &b.error) != SEMU_OK);
    for (i = 0u; i < 3u; ++i) hashes[i] = semu_sapporo_235_ohr_layer.component_hashes[i];
    hashes[1] = "wrong";
    SEMU_TEST_ASSERT(c, semu_layer_enable_checked(&b.layer,
        &semu_sapporo_235_ohr_layer, "sapporo-2.35.34", hashes, 3u, &b.error) != SEMU_OK);
    SEMU_TEST_EQ_U64(c, 0u, b.layer.enabled);
    SEMU_TEST_EQ_U64(c, 1u, a.layer.hits);
    fclose(a.log); fclose(b.log);
}

static void test_sapporo_235_ohr_machine_refusal_and_reset(semu_test_context *c)
{
    semu_machine machine;
    semu_run_limits limits = {1u, 1u};
    semu_serial_endpoint ep;
    const semu_serial_endpoint *mux;
    fixture f;
    uint8_t packet[59];
    SEMU_TEST_ASSERT(c, setup(&f));
    memset(&machine, 0, sizeof(machine));
    machine.logger = &f.logger;
    machine.layers[0] = f.layer; machine.layer_count = 1u;
    machine.bus = semu_bus_create(&f.error);
    machine.scheduler = semu_scheduler_create(&f.error);
    machine.cpu = semu_cpu_create(machine.bus, machine.scheduler, &f.error);
    machine.devices = semu_sapporo_devices_create(machine.scheduler, NULL, &f.error);
    SEMU_TEST_ASSERT(c, machine.bus && machine.scheduler && machine.cpu && machine.devices);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_select_profile(
        machine.devices, "sapporo-2.35.34", &f.error));
    mux = semu_sapporo_devices_iom_endpoint(machine.devices, 2u);
    SEMU_TEST_ASSERT(c, mux != NULL); ep = *mux;
    frame(packet, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error));
    SEMU_TEST_ASSERT(c, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error) != SEMU_OK);
    /* A valid-CRC but wrong startup sequence latches compatibility refusal. */
    frame(packet, 16u, 1u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_REFUSE,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, SEMU_STOP_COMPAT_REFUSED,
        semu_machine_run(&machine, &limits, &f.error));
    SEMU_TEST_EQ_U64(c, 0u, semu_cpu_get_state(machine.cpu)->instructions);
    SEMU_TEST_EQ_U64(c, 0u, machine.reset_request_count);
    SEMU_TEST_EQ_U64(c, 0u, machine.layers[0].hits);
    semu_sapporo_devices_reset(machine.devices);
    SEMU_TEST_EQ_U64(c, SEMU_OK, semu_sapporo_devices_bind_235_ohr(
        machine.devices, &machine.layers[0], &f.logger, &f.error));
    frame(packet, 16u, 0u);
    SEMU_TEST_EQ_U64(c, SEMU_TRANSACTION_OK,
        exchange(ep, packet, 59u, NULL, 0u, &f.error));
    SEMU_TEST_EQ_U64(c, 1u, machine.layers[0].hits);
    semu_sapporo_devices_destroy(machine.devices);
    semu_cpu_destroy(machine.cpu);
    semu_scheduler_destroy(machine.scheduler);
    semu_bus_destroy(machine.bus); fclose(f.log);
}

/* Exercise request shape from E-EMU-SAP235-EXERCISE-001; shares the
 * existing 30-hit budget and never supplies measurements. */
static void test_sapporo_235_ohr_exercise(semu_test_context *c)
{
    unsigned i, j;
    for (i = 0; i < 10; ++i) {
        fixture f;
        uint8_t body[54], response[54];
        semu_sapporo_ohr2_state state = SEMU_SAPPORO_OHR2_MAIN;
        uint64_t hits;
        SEMU_TEST_ASSERT(c, setup(&f));
        f.layer.hits = 21u;
        make_body(body, 4u, 21u);
        body[4] = 0xa3u; memset(body + 5u, 0, 14u);
        switch (i) {
        case 1: body[4] = 0x23u; break;
        case 2: body[18] = 1u; break;
        case 3: body[19] = 0u; break;
        case 4: body[2] = 22u; break;
        case 5: state = SEMU_SAPPORO_OHR2_BSL; break;
        case 6: f.layer.enabled = 0; break;
        case 7: f.layer.hits = 13u; break;
        case 8: f.layer.hits = 30u; break;
        case 9: body[53] = 0u; break;
        default: break;
        }
        hits = f.layer.hits;
        memset(response, 0xa5, sizeof(response));
        SEMU_TEST_EQ_U64(c, i == 0 ? SEMU_TRANSACTION_OK : SEMU_TRANSACTION_REFUSE,
            semu_sapporo_235_ohr_body_provider(&f.context, (semu_sapporo_ohr2_command)4u,
                21u, state, body, response, &f.error));
        SEMU_TEST_EQ_U64(c, hits + (i == 0), f.layer.hits);
        for (j = 0; j < 54u; ++j) SEMU_TEST_EQ_U64(c, i == 0 ? 0u : 0xa5u, response[j]);
        fclose(f.log);
    }
}

int main(void)
{
    static const semu_test_case cases[] = {
        SEMU_TEST_CASE(test_sapporo_235_ohr_exercise),
        SEMU_TEST_CASE(test_sapporo_235_ohr_startup_transport),
        SEMU_TEST_CASE(test_sapporo_235_ohr_atomic_body_refusals),
        SEMU_TEST_CASE(test_sapporo_235_ohr_pins_and_owners),
        SEMU_TEST_CASE(test_sapporo_235_ohr_post_done_queries),
        SEMU_TEST_CASE(test_sapporo_235_ohr_result4_law),
        SEMU_TEST_CASE(test_sapporo_235_ohr_poll_tail_law),
        SEMU_TEST_CASE(test_sapporo_235_ohr_machine_refusal_and_reset)
    };
    return semu_test_run(cases, sizeof(cases)/sizeof(cases[0]));
}
