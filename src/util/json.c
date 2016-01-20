#include "json.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>

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
    jnum->state = 0;
    jnum->negative = 0;
}

void json_number_uninit(json_number_t *jnum)
{
    char_buffer_uninit(&jnum->buffer);
}

static void append_integer_to_char_buffer(const json_int_t in, 
                                          struct char_buffer_t *cb)
{
    char buf[100];
    int ret = sprintf_s(buf, sizeof(buf), "%lld", (long long)in);
    assert(ret != -1);
    /* TODO: avoid extra copy here */
    char_buffer_append(cb, buf, strlen(buf));
}

#define NUM_STATE_INIT 0
#define NUM_STATE_GOT_MINUS 1
#define NUM_STATE_GOT_ZERO 2
#define NUM_STATE_GOT_SEPARATOR 3
#define NUM_STATE_GOT_FRACTION_DIGIT 4
#define NUM_STATE_GOT_NEGATIVE 5
#define NUM_STATE_GOT_NONZERO 6

#define IS_NONZERO_DIGIT(c) ((c) >= '1' && (c) <= '9')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')

enum json_code_t json_number_parse(json_number_t *jnum, strref_t *next_chunk)
{
    enum json_code_t ret;
    assert(next_chunk->size > 0);  /* always expect some input */

    /* to prevent warning about unused jump label */
    if (jnum->state == NUM_STATE_INIT)
        goto at_NUM_STATE_INIT;

    switch (jnum->state) {
at_NUM_STATE_INIT:
    case NUM_STATE_INIT:
        switch (*next_chunk->start) {
        case '0':
            assert(jnum->type == JSON_INTEGER);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_ZERO;
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            goto at_NUM_STATE_GOT_ZERO;
        case '-':
            assert(jnum->negative == 0);
            jnum->negative = 1;
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_NEGATIVE;
            if (next_chunk->size == 0) {
                ret = JSON_NEED_MORE;
                goto done;
            }
            goto at_NUM_STATE_GOT_NEGATIVE;
        default:
            if (IS_NONZERO_DIGIT(*next_chunk->start)) {
                assert(jnum->type == JSON_INTEGER);
                jnum->number.integer = (json_int_t)(*next_chunk->start - '0');
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_NONZERO;
                if (next_chunk->size == 0) {
                    ret = JSON_READY;
                    goto done;
                }
                goto at_NUM_STATE_GOT_NONZERO;
            }
            goto input_error;
        }

at_NUM_STATE_GOT_NEGATIVE:
    case NUM_STATE_GOT_NEGATIVE:
        assert(next_chunk->size > 0);
        switch (*next_chunk->start) {
        case '0':
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_ZERO;
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            goto at_NUM_STATE_GOT_ZERO;
        default:
            if (IS_NONZERO_DIGIT(*next_chunk->start)) {
                assert(jnum->type == JSON_INTEGER);
                jnum->number.integer *= 10;  /* TODO: handle overflow */
                jnum->number.integer = (json_int_t)(*next_chunk->start - '0');
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_NONZERO;
                if (next_chunk->size == 0) {
                    ret = JSON_READY;
                    goto done;
                }
                goto at_NUM_STATE_GOT_NONZERO;
            }
            goto input_error;
        }

at_NUM_STATE_GOT_NONZERO:
    case NUM_STATE_GOT_NONZERO:
        {
            char *cur, *end;
            assert(next_chunk->size > 0);
            assert(jnum->type == JSON_INTEGER);
            strref_get_start_and_end(next_chunk, &cur, &end);
            while (cur < end && IS_DIGIT(*cur)) {
                /* TODO: handle overflow */
                jnum->number.integer *= 10;
                jnum->number.integer += (json_int_t)(*cur - '0');
                ++cur;
            }
            if (cur > next_chunk->start) {
                const size_t len = cur - next_chunk->start;
                strref_trim_front(next_chunk, len);
            }
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            else if (*next_chunk->start == '.') {
                /* convert integer to string, so that later it can be converted
                 * into a floating point number */
                assert(jnum->type == JSON_INTEGER);
                jnum->type = JSON_FLOATING;
                assert(char_buffer_size(&jnum->buffer) == 0);
                if (jnum->negative)
                    char_buffer_append(&jnum->buffer, "-", 1);
                append_integer_to_char_buffer(
                    jnum->negative ? -jnum->number.integer :
                                     jnum->number.integer,
                    &jnum->buffer);
                char_buffer_append(&jnum->buffer, ".", 1);
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_SEPARATOR;
                if (next_chunk->size == 0) {
                    ret = JSON_NEED_MORE;
                    goto done;
                }
                goto at_NUM_STATE_GOT_SEPARATOR;
            }
            ret = JSON_READY;
            goto done;
        }

at_NUM_STATE_GOT_ZERO:
    case NUM_STATE_GOT_ZERO:
        assert(jnum->number.integer == 0);
        switch (*next_chunk->start) {
        case '.':
            /* convert integer to string, so that later it can be converted
             * into a floating point number */
            assert(jnum->type == JSON_INTEGER);
            jnum->type = JSON_FLOATING;
            assert(char_buffer_size(&jnum->buffer) == 0);
            if (jnum->negative)
                char_buffer_append(&jnum->buffer, "-", 1);
            append_integer_to_char_buffer(
                jnum->negative ? -jnum->number.integer :
                                 jnum->number.integer,
                &jnum->buffer);
            char_buffer_append(&jnum->buffer, ".", 1);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_SEPARATOR;
            if (next_chunk->size == 0) {
                ret = JSON_NEED_MORE;
                goto done;
            }
            goto at_NUM_STATE_GOT_SEPARATOR;
        default:
            ret = JSON_READY;
            goto done;
        }

at_NUM_STATE_GOT_SEPARATOR:
    case NUM_STATE_GOT_SEPARATOR:
        /* after the decimal fraction separator, at least one digit is needed */
        assert(next_chunk->size > 0);
        if (!IS_DIGIT(*next_chunk->start))
            goto input_error;
        char_buffer_append(&jnum->buffer, next_chunk->start, 1);
        strref_trim_front(next_chunk, 1);
        jnum->state = NUM_STATE_GOT_FRACTION_DIGIT;
        if (next_chunk->size == 0) {
            ret = JSON_READY;
            goto done;
        }
        goto at_NUM_STATE_GOT_FRACTION_DIGIT;

at_NUM_STATE_GOT_FRACTION_DIGIT:
    case NUM_STATE_GOT_FRACTION_DIGIT:
        {
            char *cur, *end;
            assert(next_chunk->size > 0);
            assert(char_buffer_size(&jnum->buffer) > 0);
            strref_get_start_and_end(next_chunk, &cur, &end);
            while (cur < end && IS_DIGIT(*cur))
                ++cur;
            if (cur > next_chunk->start) {
                const size_t len = cur - next_chunk->start;
                char_buffer_append(&jnum->buffer, next_chunk->start, len);
                strref_trim_front(next_chunk, len);
            }
            ret = JSON_READY;
            goto done;
        }
    }

done:
    return ret;
input_error:
    return JSON_INPUT_ERROR;
}

enum json_type_t json_number_result(json_number_t *jnum,
                                    union json_number_union_t *result)
{
    if (jnum->type == JSON_INTEGER) {
        result->integer = jnum->negative ? -jnum->number.integer :
                                           jnum->number.integer;
    }
    else {
        strref_t ref = {0};
        assert(jnum->type == JSON_FLOATING);
        char_buffer_get(&jnum->buffer, &ref);
        result->floating = atof(ref.start);
        strref_uninit(&ref);
    }

    return jnum->type;
}
