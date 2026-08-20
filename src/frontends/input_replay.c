/*
 * Deterministic input replay (ticket 518).
 * Parses a strict ASCII replay format into a bounded queue of
 * semantic button events.  Validates time ordering and press/release
 * state before scheduling.
 */

#include "input_replay.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    uint64_t time_ns;
    semu_input_event event;
} replay_entry;

struct semu_input_replay {
    replay_entry entries[INPUT_REPLAY_MAX_EVENTS];
    size_t count;
    size_t cursor;
};

semu_input_replay *semu_input_replay_create(semu_error *error)
{
    semu_input_replay *r;
    r = (semu_input_replay *)calloc(1u, sizeof(*r));
    if (r == NULL) {
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "input_replay: cannot allocate");
        return NULL;
    }
    return r;
}

void semu_input_replay_destroy(semu_input_replay *replay)
{
    free(replay);
}

void semu_input_replay_reset(semu_input_replay *replay)
{
    if (replay == NULL) {
        return;
    }
    replay->count = 0u;
    replay->cursor = 0u;
}

static int parse_button(const char *word, size_t len,
                          semu_button_id *out)
{
    if (len == 5u && memcmp(word, "upper", 5u) == 0) {
        *out = SEMU_BUTTON_UPPER;
        return 1;
    }
    if (len == 6u && memcmp(word, "middle", 6u) == 0) {
        *out = SEMU_BUTTON_MIDDLE;
        return 1;
    }
    if (len == 5u && memcmp(word, "lower", 5u) == 0) {
        *out = SEMU_BUTTON_LOWER;
        return 1;
    }
    return 0;
}

static int parse_value(const char *word, size_t len, int *out_down)
{
    if (len == 5u && memcmp(word, "press", 5u) == 0) {
        *out_down = 1;
        return 1;
    }
    if (len == 7u && memcmp(word, "release", 7u) == 0) {
        *out_down = 0;
        return 1;
    }
    return 0;
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

#define MAX_WORDS 4u
#define MAX_WORD_LEN 32u
#define INPUT_REPLAY_MAX_FILE_SIZE (1024u * 1024u)

static int split_line(const char *line, size_t line_len,
                       char words[MAX_WORDS][MAX_WORD_LEN],
                       size_t word_lens[MAX_WORDS],
                       size_t *out_count)
{
    size_t i = 0u;
    size_t wc = 0u;
    *out_count = 0u;
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

semu_status semu_input_replay_parse(semu_input_replay *replay,
    const char *text, size_t text_size, semu_error *error)
{
    semu_input_replay candidate;
    size_t i = 0u;
    uint32_t line_no = 0u;
    uint64_t last_time = 0u;
    int pressed[3] = { 0, 0, 0 };
    int have_last = 0;

    if (replay == NULL || text == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "input_replay: null argument");
        return SEMU_ERR_ARGUMENT;
    }
    memset(&candidate, 0, sizeof(candidate));

    while (i < text_size) {
        size_t start = i;
        ++line_no;
        while (i < text_size && text[i] != '\n') {
            ++i;
        }
        {
            size_t line_len = i - start;
            const char *line = text + start;
            char words[MAX_WORDS][MAX_WORD_LEN];
            size_t word_lens[MAX_WORDS];
            size_t wc;
            uint64_t time_ns;
            semu_button_id button;
            int down;

            if (i < text_size) {
                ++i;
            }

            while (line_len > 0u &&
                   isspace((unsigned char)line[line_len - 1u])) {
                --line_len;
            }
            if (line_len == 0u) {
                continue;
            }
            if (line[0] == '#') {
                continue;
            }
            if (!split_line(line, line_len, words, word_lens, &wc)) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: too many words",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (wc != 4u) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: expected 4 words",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (word_lens[1] != 6u ||
                memcmp(words[1], "button", 6u) != 0) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: expected 'button'",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (!parse_uint64(words[0], word_lens[0], &time_ns)) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: bad time", line_no);
                return SEMU_ERR_FORMAT;
            }
            if (!parse_button(words[2], word_lens[2], &button)) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: bad button code",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (!parse_value(words[3], word_lens[3], &down)) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: bad value", line_no);
                return SEMU_ERR_FORMAT;
            }
            if (have_last && time_ns < last_time) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: time out of order",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (have_last && time_ns == last_time) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: duplicate time",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (down && pressed[button]) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: duplicate press",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (!down && !pressed[button]) {
                semu_error_set(error, SEMU_ERR_FORMAT,
                               "input_replay: line %u: impossible release",
                               line_no);
                return SEMU_ERR_FORMAT;
            }
            if (candidate.count >= INPUT_REPLAY_MAX_EVENTS) {
                semu_error_set(error, SEMU_ERR_RANGE,
                               "input_replay: event overflow at line %u",
                               line_no);
                return SEMU_ERR_RANGE;
            }
            pressed[button] = down;
            candidate.entries[candidate.count].time_ns = time_ns;
            candidate.entries[candidate.count].event.kind = SEMU_INPUT_BUTTON;
            candidate.entries[candidate.count].event.code = (uint32_t)button;
            candidate.entries[candidate.count].event.value = down ? 0 : 1;
            candidate.entries[candidate.count].event.x = 0;
            candidate.entries[candidate.count].event.y = 0;
            ++candidate.count;
            last_time = time_ns;
            have_last = 1;
        }
    }
    *replay = candidate;
    return SEMU_OK;
}

semu_status semu_input_replay_parse_file(semu_input_replay *replay,
    const char *path, semu_error *error)
{
    FILE *stream;
    long length;
    char *text;
    size_t size;
    semu_status status;

    if (replay == NULL || path == NULL) {
        semu_error_set(error, SEMU_ERR_ARGUMENT,
                       "input_replay: file arguments are invalid");
        return SEMU_ERR_ARGUMENT;
    }
    stream = fopen(path, "rb");
    if (stream == NULL) {
        semu_error_set(error, SEMU_ERR_IO,
                       "input_replay: cannot open %s", path);
        return SEMU_ERR_IO;
    }
    if (fseek(stream, 0L, SEEK_END) != 0) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_IO,
                       "input_replay: cannot seek %s", path);
        return SEMU_ERR_IO;
    }
    length = ftell(stream);
    if (length < 0L || (uint64_t)length > INPUT_REPLAY_MAX_FILE_SIZE) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_RANGE,
                       "input_replay: %s exceeds %u bytes", path,
                       INPUT_REPLAY_MAX_FILE_SIZE);
        return SEMU_ERR_RANGE;
    }
    if (fseek(stream, 0L, SEEK_SET) != 0) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_IO,
                       "input_replay: cannot rewind %s", path);
        return SEMU_ERR_IO;
    }
    size = (size_t)length;
    text = (char *)malloc(size != 0u ? size : 1u);
    if (text == NULL) {
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_NOMEM,
                       "input_replay: cannot allocate file buffer");
        return SEMU_ERR_NOMEM;
    }
    if (size != 0u && fread(text, 1u, size, stream) != size) {
        free(text);
        (void)fclose(stream);
        semu_error_set(error, SEMU_ERR_IO,
                       "input_replay: cannot read %s", path);
        return SEMU_ERR_IO;
    }
    if (fclose(stream) != 0) {
        free(text);
        semu_error_set(error, SEMU_ERR_IO,
                       "input_replay: cannot close %s", path);
        return SEMU_ERR_IO;
    }
    status = semu_input_replay_parse(replay, text, size, error);
    free(text);
    return status;
}

size_t semu_input_replay_count(const semu_input_replay *replay)
{
    return replay != NULL ? replay->count : 0u;
}

size_t semu_input_replay_pump(semu_input_replay *replay,
    uint64_t current_time_ns,
    semu_input_replay_sink sink, void *sink_context,
    int *out_refused)
{
    size_t emitted = 0u;
    if (replay == NULL || sink == NULL) {
        if (out_refused != NULL) {
            *out_refused = 0;
        }
        return 0u;
    }
    if (out_refused != NULL) {
        *out_refused = 0;
    }
    while (replay->cursor < replay->count) {
        replay_entry *e = &replay->entries[replay->cursor];
        if (e->time_ns > current_time_ns) {
            break;
        }
        {
            int rc = sink(sink_context, &e->event, e->time_ns,
                            (uint32_t)replay->cursor);
            ++replay->cursor;
            if (rc != 0) {
                if (out_refused != NULL) {
                    *out_refused = 1;
                }
                break;
            }
            ++emitted;
        }
    }
    return emitted;
}
