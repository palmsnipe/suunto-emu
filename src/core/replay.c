/*
 * Versioned input recording and replay (ticket 610).
 * Strict parser with format version, identity binding, ordering
 * validation, and bounded event count.  Fails closed on unknown
 * kinds/codes and time reversal.
 */

#include "semu/trace.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

semu_replay *semu_replay_create(semu_error *error)
{
    semu_replay *r = (semu_replay *)calloc(1u, sizeof(*r));
    if (r == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM, "replay: cannot allocate");
        return NULL;
    }
    r->version = SEMU_REPLAY_FORMAT_VERSION;
    return r;
}

void semu_replay_destroy(semu_replay *replay)
{
    free(replay);
}

void semu_replay_reset(semu_replay *replay)
{
    if (replay == NULL) {
        return;
    }
    replay->count = 0u;
    replay->profile_id[0] = '\0';
    replay->firmware_hash[0] = '\0';
    replay->version = SEMU_REPLAY_FORMAT_VERSION;
}

size_t semu_replay_event_count(const semu_replay *replay)
{
    return replay != NULL ? replay->count : 0u;
}

const semu_replay_event *semu_replay_event_get(const semu_replay *replay,
    size_t index)
{
    if (replay == NULL || index >= replay->count) {
        return NULL;
    }
    return &replay->events[index];
}

const char *semu_replay_profile_id(const semu_replay *replay)
{
    return replay != NULL ? replay->profile_id : NULL;
}

const char *semu_replay_firmware_hash(const semu_replay *replay)
{
    return replay != NULL ? replay->firmware_hash : NULL;
}

uint32_t semu_replay_version(const semu_replay *replay)
{
    return replay != NULL ? replay->version : 0u;
}

static int parse_uint64(const char *s, size_t len, uint64_t *out)
{
    uint64_t val = 0u;
    size_t i;
    if (len == 0u || len > 20u) {
        return 0;
    }
    for (i = 0u; i < len; ++i) {
        uint64_t digit;
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        digit = (uint64_t)(s[i] - '0');
        if (val > (UINT64_MAX - digit) / 10u) {
            return 0;
        }
        val = val * 10u + digit;
    }
    *out = val;
    return 1;
}

static int parse_int32(const char *s, size_t len, int32_t *out)
{
    int neg = 0;
    int64_t val = 0;
    size_t i = 0u;
    if (len == 0u || len > 11u) {
        return 0;
    }
    if (s[0] == '-') {
        neg = 1;
        i = 1u;
    }
    if (i == len) {
        return 0;
    }
    for (; i < len; ++i) {
        if (s[i] < '0' || s[i] > '9') {
            return 0;
        }
        val = val * 10 + (int64_t)(s[i] - '0');
    }
    if (neg) {
        val = -val;
    }
    if (val > 2147483647 || val < -2147483648) {
        return 0;
    }
    *out = (int32_t)val;
    return 1;
}

static int is_hex_char(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

static int parse_header_line(const char *key, size_t key_len,
    const char *val, size_t val_len, semu_replay *replay,
    uint32_t line_no, size_t *out_events, semu_error *error)
{
    if (key_len == 7u && memcmp(key, "version", 7u) == 0) {
        uint64_t v;
        if (!parse_uint64(val, val_len, &v) || v != SEMU_REPLAY_FORMAT_VERSION) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad or unsupported version", line_no);
            return 0;
        }
        replay->version = (uint32_t)v;
        return 1;
    }
    if (key_len == 7u && memcmp(key, "profile", 7u) == 0) {
        if (val_len == 0u || val_len >= SEMU_ID_MAX) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad profile id", line_no);
            return 0;
        }
        memcpy(replay->profile_id, val, val_len);
        replay->profile_id[val_len] = '\0';
        return 1;
    }
    if (key_len == 8u && memcmp(key, "firmware", 8u) == 0) {
        size_t i;
        if (val_len != SEMU_SHA256_SIZE * 2u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: firmware hash must be 64 hex chars",
                line_no);
            return 0;
        }
        for (i = 0u; i < val_len; ++i) {
            if (!is_hex_char(val[i])) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                    "replay: line %u: firmware hash not lowercase hex",
                    line_no);
                return 0;
            }
        }
        memcpy(replay->firmware_hash, val, val_len);
        replay->firmware_hash[val_len] = '\0';
        return 1;
    }
    if (key_len == 6u && memcmp(key, "events", 6u) == 0) {
        uint64_t n;
        if (!parse_uint64(val, val_len, &n) || n > SEMU_REPLAY_MAX_EVENTS) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad event count", line_no);
            return 0;
        }
        *out_events = (size_t)n;
        return 1;
    }
    semu_error_set(error, SEMU_ERR_FORMAT,
        "replay: line %u: unknown header key", line_no);
    return 0;
}

#define MAX_WORDS 6u
#define MAX_WORD_LEN 64u

static int split_words(const char *line, size_t line_len,
    char words[MAX_WORDS][MAX_WORD_LEN], size_t word_lens[MAX_WORDS],
    size_t *out_count)
{
    size_t i = 0u;
    size_t wc = 0u;
    while (i < line_len) {
        while (i < line_len && isspace((unsigned char)line[i])) {
            ++i;
        }
        if (i >= line_len) {
            break;
        }
        if (wc >= MAX_WORDS) {
            return 0;
        }
        {
            size_t wl = 0u;
            while (i < line_len && !isspace((unsigned char)line[i])) {
                if (wl >= MAX_WORD_LEN - 1u) {
                    return 0;
                }
                words[wc][wl] = line[i];
                ++wl;
                ++i;
            }
            words[wc][wl] = '\0';
            word_lens[wc] = wl;
            ++wc;
        }
    }
    *out_count = wc;
    return 1;
}

static int eq(const char *a, size_t alen, const char *b)
{
    size_t blen = strlen(b);
    return alen == blen && memcmp(a, b, alen) == 0;
}

static int parse_event_line(const char *line, size_t line_len,
    semu_replay *replay, uint64_t *last_time, int *have_last,
    uint32_t line_no, semu_error *error)
{
    char words[MAX_WORDS][MAX_WORD_LEN];
    size_t word_lens[MAX_WORDS];
    size_t wc;
    uint64_t time_ns;
    semu_replay_event ev;

    if (!split_words(line, line_len, words, word_lens, &wc)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: line %u: too many words", line_no);
        return 0;
    }
    if (wc < 3u) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: line %u: too few words", line_no);
        return 0;
    }
    if (!parse_uint64(words[0], word_lens[0], &time_ns)) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: line %u: bad time", line_no);
        return 0;
    }
    if (*have_last && time_ns < *last_time) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: line %u: time out of order", line_no);
        return 0;
    }
    memset(&ev, 0, sizeof(ev));
    ev.virtual_time_ns = time_ns;

    if (eq(words[1], word_lens[1], "button")) {
        if (wc != 4u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: button expects 4 words", line_no);
            return 0;
        }
        ev.kind = SEMU_INPUT_BUTTON;
        if (eq(words[2], word_lens[2], "upper")) {
            ev.code = SEMU_BUTTON_UPPER;
        } else if (eq(words[2], word_lens[2], "middle")) {
            ev.code = SEMU_BUTTON_MIDDLE;
        } else if (eq(words[2], word_lens[2], "lower")) {
            ev.code = SEMU_BUTTON_LOWER;
        } else {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: unknown button code", line_no);
            return 0;
        }
        if (eq(words[3], word_lens[3], "press")) {
            ev.value = 0;
        } else if (eq(words[3], word_lens[3], "release")) {
            ev.value = 1;
        } else {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad button value", line_no);
            return 0;
        }
    } else if (eq(words[1], word_lens[1], "crown")) {
        if (wc != 4u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: crown expects 4 words", line_no);
            return 0;
        }
        ev.kind = SEMU_INPUT_CROWN_ROTATE;
        if (!eq(words[2], word_lens[2], "rotate")) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: expected 'rotate'", line_no);
            return 0;
        }
        if (!parse_int32(words[3], word_lens[3], &ev.value)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad crown value", line_no);
            return 0;
        }
    } else if (eq(words[1], word_lens[1], "touch")) {
        if (wc != 5u) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: touch expects 5 words", line_no);
            return 0;
        }
        ev.kind = SEMU_INPUT_TOUCH;
        if (!parse_int32(words[2], word_lens[2], &ev.value)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad touch value", line_no);
            return 0;
        }
        if (!parse_int32(words[3], word_lens[3], &ev.x)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad touch x", line_no);
            return 0;
        }
        if (!parse_int32(words[4], word_lens[4], &ev.y)) {
            semu_error_set(error, SEMU_ERR_FORMAT,
                "replay: line %u: bad touch y", line_no);
            return 0;
        }
    } else {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: line %u: unknown kind", line_no);
        return 0;
    }
    if (replay->count >= SEMU_REPLAY_MAX_EVENTS) {
        semu_error_set(error, SEMU_ERR_RANGE,
            "replay: event overflow at line %u", line_no);
        return 0;
    }
    replay->events[replay->count] = ev;
    ++replay->count;
    *last_time = time_ns;
    *have_last = 1;
    return 1;
}

semu_status semu_replay_parse(semu_replay *replay,
    const char *text, size_t text_size, semu_error *error)
{
    semu_replay candidate;
    size_t i = 0u;
    uint32_t line_no = 0u;
    uint64_t last_time = 0u;
    int have_last = 0;
    int seen_version = 0;
    int seen_profile = 0;
    int seen_firmware = 0;
    int seen_events = 0;
    size_t declared_events = 0u;
    int in_events = 0;

    if (replay == NULL || text == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT, "replay: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    semu_replay_reset(&candidate);

    while (i < text_size) {
        size_t start = i;
        ++line_no;
        while (i < text_size && text[i] != '\n') {
            ++i;
        }
        {
            size_t line_len = i - start;
            const char *line = text + start;
            if (i < text_size) {
                ++i;
            }
            while (line_len > 0u && isspace((unsigned char)line[line_len - 1u])) {
                --line_len;
            }
            if (line_len == 0u) {
                continue;
            }
            if (line[0] == '#') {
                continue;
            }
            if (!in_events) {
                size_t eq_pos = 0u;
                int found_eq = 0;
                while (eq_pos < line_len && line[eq_pos] != '=') {
                    ++eq_pos;
                    if (eq_pos < line_len && line[eq_pos] == '=') {
                        found_eq = 1;
                        break;
                    }
                }
                if (!found_eq) {
                    semu_error_set(error, SEMU_ERR_FORMAT,
                        "replay: line %u: expected header 'key=value'",
                        line_no);
                    return SEMU_ERR_FORMAT;
                }
                {
                    const char *key = line;
                    size_t key_len = eq_pos;
                    const char *val = line + eq_pos + 1u;
                    size_t val_len = line_len - eq_pos - 1u;
                    while (key_len > 0u &&
                           isspace((unsigned char)key[key_len - 1u])) {
                        --key_len;
                    }
                    while (val_len > 0u &&
                           isspace((unsigned char)val[0])) {
                        ++val;
                        --val_len;
                    }
                    if ((key_len == 7u &&
                         memcmp(key, "version", 7u) == 0 && seen_version) ||
                        (key_len == 7u &&
                         memcmp(key, "profile", 7u) == 0 && seen_profile) ||
                        (key_len == 8u &&
                         memcmp(key, "firmware", 8u) == 0 && seen_firmware)) {
                        semu_error_set(error, SEMU_ERR_FORMAT,
                            "replay: line %u: duplicate header key", line_no);
                        return SEMU_ERR_FORMAT;
                    }
                    if (!parse_header_line(key, key_len, val, val_len,
                            &candidate, line_no, &declared_events, error)) {
                        return SEMU_ERR_FORMAT;
                    }
                    if (key_len == 7u && memcmp(key, "version", 7u) == 0) {
                        seen_version = 1;
                    }
                    if (key_len == 7u && memcmp(key, "profile", 7u) == 0) {
                        seen_profile = 1;
                    }
                    if (key_len == 8u && memcmp(key, "firmware", 8u) == 0) {
                        seen_firmware = 1;
                    }
                    if (key_len == 6u && memcmp(key, "events", 6u) == 0) {
                        seen_events = 1;
                        in_events = 1;
                    }
                }
            } else {
                if (!parse_event_line(line, line_len, &candidate, &last_time,
                        &have_last, line_no, error)) {
                    return SEMU_ERR_FORMAT;
                }
            }
        }
    }
    if (!seen_version || !seen_profile || !seen_firmware || !seen_events) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: missing header field(s)");
        return SEMU_ERR_FORMAT;
    }
    if (candidate.count != declared_events) {
        semu_error_set(error, SEMU_ERR_FORMAT,
            "replay: declared %u events but found %u",
            (unsigned)declared_events, (unsigned)candidate.count);
        return SEMU_ERR_FORMAT;
    }
    *replay = candidate;
    return SEMU_OK;
}
