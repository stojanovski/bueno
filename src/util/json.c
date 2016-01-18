#include "json.h"
#include <string.h>
#include <assert.h>

void json_string_init(json_string_t *jstr)
{
    jstr->escape_seq_len = 0;
    jstr->unicode_escaped_value = 0;
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
        char_buffer_append(output, (char *)&c, 1);
    }
    else if (in <= 0x07ff) {
        /* codes that require two bytes to store */
        c = ((uint8_t)(in >> 6)) | 0xc0;
        char_buffer_append(output, (char *)&c, 1);
        c = ((uint8_t)(in & 0x003f)) | 0x80;
        char_buffer_append(output, (char *)&c, 1);
    }
    else {
        assert(in >= 0x0800);
        /* codes that require three bytes to store */
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
    char c;

    assert(next_chunk->size > 0);  /* always expect some input */

    if (jstr->escape_seq_len > 0) {
        if (jstr->escape_seq_len == 1)
            goto escape_seq;
        else
            goto unicode_escape_seq;
    }

parse:
    /* this section handles the unescaped input */
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
    /* process escaped input */
    assert(next_chunk->size > 0);
    assert(jstr->escape_seq_len > 0 || next_chunk->start[0] == '\\');
    if (jstr->escape_seq_len == 0) {
        jstr->escape_seq_len = 1;
        strref_trim_front(next_chunk, 1);
    }

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
        goto unicode_escape_seq;
    default:
        /* unsupported escape sequence */
        return JSON_INPUT_ERROR;
    }

    strref_trim_front(next_chunk, 1);
    /* TODO: make a fast "append char" function */
    char_buffer_append(output, &c, 1);
    jstr->escape_seq_len = 0;
    jstr->unicode_escaped_value = 0;

    if (next_chunk->size > 0)
        goto parse;
    else
        return JSON_READY;

unicode_escape_seq:
    /* process ecaped unicode value (\uxxxx) */
    assert(next_chunk->size > 0);
    assert(jstr->escape_seq_len > 0);
    assert(jstr->escape_seq_len > 1 || next_chunk->start[0] == 'u');

    if (jstr->escape_seq_len == 1) {
        ++jstr->escape_seq_len;
        strref_trim_front(next_chunk, 1);
    }

    assert(jstr->escape_seq_len >= 2);
    /* 6==strlen("\\uxxxx") */
    while (next_chunk->size > 0 && jstr->escape_seq_len < 6) {
        uint16_t hex_digit_value;
        c = next_chunk->start[0];
        if (c >= 'a' && c <= 'f')
            hex_digit_value = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F')
            hex_digit_value = c - 'A' + 10;
        else if (c >= '0' && c <= '9')
            hex_digit_value = c - '0';
        else
            return JSON_INPUT_ERROR;

        jstr->unicode_escaped_value <<= 4;
        jstr->unicode_escaped_value |= hex_digit_value;
        ++jstr->escape_seq_len;
        strref_trim_front(next_chunk, 1);
    }

    if (jstr->escape_seq_len < 6) {
        assert(next_chunk->size == 0);
        return JSON_NEED_MORE;
    }
    else {
        append_uint16_as_utf8(output, jstr->unicode_escaped_value);
        jstr->unicode_escaped_value = 0;
        jstr->escape_seq_len = 0;
    }

    if (next_chunk->size > 0)
        goto parse;

    return JSON_READY;
}

void json_string_result(json_string_t *jstr, strref_t *result)
{
    char_buffer_get(&jstr->output, result);
}


void json_number_init(json_number_t *jnum)
{
    jnum->number.integer = 0;
    jnum->type = JSON_INTEGER;
    char_buffer_init(&jnum->buffer);
}

void json_number_uninit(json_number_t *jnum)
{
    char_buffer_uninit(&jnum->buffer);
}

enum json_code_t json_number_parse(json_number_t *jnum, strref_t *next_chunk)
{
    return JSON_READY;
}

enum json_type_t json_number_result(json_number_t *jnum, json_number_t *result)
{
    memcpy(result, &jnum->number, sizeof(json_number_t));
    return jnum->type;
}
