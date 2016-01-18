#include "json.h"
#include "common.h"
#include <string.h>
#include <assert.h>

void json_string_init(json_string_t *jstr)
{
    jstr->input_lookback.start = jstr->input_lookback_buf;
    jstr->input_lookback.size = 0;
    char_buffer_init(&jstr->output);
}

void json_string_uninit(json_string_t *jstr)
{
    char_buffer_uninit(&jstr->output);
}

static void append_uint16_as_utf8(struct char_buffer_t *output, uint16_t in)
{
    uint8_t c;
    if (in <= 0x007f) {
        c = (char)in;
        /* TODO: add efficient add char function */
        char_buffer_append(output, &c, 1);
    }
    else if (in <= 0x07ff) {
        c = ((uint8_t)(in >> 6)) | 0xc0;
        char_buffer_append(output, (char *)&c, 1);
        c = ((uint8_t)(in & 0x003f)) | 0x80;
        char_buffer_append(output, (char *)&c, 1);
    }
    else {
        c = ((uint8_t)(in >> 12)) | 0xe0;
        char_buffer_append(output, (char *)&c, 1);
        c = ((uint8_t)(in >> 6) & 0x3f) | 0x80;
        char_buffer_append(output, (char *)&c, 1);
        c = ((uint8_t)(in & 0x3f)) | 0x80;
        char_buffer_append(output, (char *)&c, 1);
    }
}

enum json_code_t json_string_parse(json_string_t *jstr, strref_t *next_chunk)
{
    struct char_buffer_t *output = &jstr->output;
    strref_t *input_lookback = &jstr->input_lookback;
    char c;

    assert(next_chunk->size > 0);  /* always expect some input */

    assert(next_chunk->size > 0);
    if (input_lookback->size > 0) {
        if (input_lookback->size == 1)
            goto escape_seq;
        else
            goto unicode_seq;
    }

parse:
    {
        char *cur, *end;
        size_t len;
        strref_get_start_and_end(next_chunk, &cur, &end);
        assert(cur < end);

        do {
            if (*cur == '"')
                break;
            else if (*cur == '\\') {
                if (cur > next_chunk->start) {
                    const size_t len = cur - next_chunk->start;
                    char_buffer_append(output, next_chunk->start, len);
                    strref_trim_front(next_chunk, len);
                }
                goto escape_seq;
            }
        } while (++cur < end);

        len = cur - next_chunk->start;
        char_buffer_append(output, next_chunk->start, len);
        strref_trim_front(next_chunk, len);
        return JSON_READY;
    }

escape_seq:
    assert(next_chunk->size > 0);
    assert(input_lookback->size > 0 || next_chunk->start[0] == '\\');
    if (input_lookback->size == 0) {
        *input_lookback->start = '\\';
        input_lookback->size = 1;
        strref_trim_front(next_chunk, 1);
    }
    assert(*input_lookback->start == '\\');

    if (next_chunk->size == 0)
        return JSON_NEED_MORE;

    switch (next_chunk->start[0]) {
    case '"':  c = '"';  break;
    case '\\': c = '\\'; break;
    case '/':  c = '/';  break;
    case 'b':  c = '\b'; break;
    case 'f':  c = '\f'; break;
    case 'n':  c = '\n'; break;
    case 'r':  c = '\r'; break;
    case 't':  c = '\t'; break;
    case 'u':  /* unicode \uxxxx sequence */
        goto unicode_seq;
    default:
        /* unsupported escape sequence */
        return JSON_INPUT_ERROR;
    }

    strref_trim_front(next_chunk, 1);
    /* TODO: make a fast "append char" function */
    char_buffer_append(output, &c, 1);
    input_lookback->size = 0;

    if (next_chunk->size > 0)
        goto parse;
    else
        return JSON_READY;

unicode_seq:
    assert(next_chunk->size > 0);
    assert(input_lookback->size > 0 && input_lookback->start[0] == '\\');
    assert(input_lookback->size > 1 || next_chunk->start[0] == 'u');

    if (input_lookback->size == 1) {
        ++input_lookback->size;
        input_lookback->start[1] = 'u';
        strref_trim_front(next_chunk, 1);
    }

    assert(input_lookback->size >= 2);
    assert(input_lookback->start[1] == 'u');
    /* 6==strlen("\\uxxxx") */
    while (next_chunk->size > 0 && input_lookback->size < 6) {
        c = next_chunk->start[0];
        if (c >= 'a' && c <= 'f')
            input_lookback->start[input_lookback->size++] = c - ('a' - 'A');
        else if ((c >= 'A' && c <= 'F') || (c >= '0' && c <= '9'))
            input_lookback->start[input_lookback->size++] = c;
        else
            return JSON_INPUT_ERROR;
        strref_trim_front(next_chunk, 1);
    }

    if (input_lookback->size < 6) {
        assert(next_chunk->size == 0);
        return JSON_NEED_MORE;
    }
    else {
        uint16_t in2bytes = 0;
        char *hexptr = input_lookback->start + 2;
        char *hexend = input_lookback->start + 6;
        assert(input_lookback->size == 6);

        while (hexptr < hexend) {
            in2bytes <<= 4;
            if (*hexptr < 'A')
                in2bytes |= (uint16_t)(*hexptr - '0');
            else
                in2bytes |= (uint16_t)(*hexptr - 'A' + 10);
            ++hexptr;
        }
        input_lookback->size = 0;

        append_uint16_as_utf8(output, in2bytes);
    }

    if (next_chunk->size > 0)
        goto parse;

    return JSON_READY;
}

void json_string_result(json_string_t *jstr, strref_t *result)
{
    char_buffer_get(&jstr->output, result);
}
