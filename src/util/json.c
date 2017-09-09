#include "json.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

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
    jnum->int_value = 0;
    jnum->int_overflow = 0;
    jnum->int_negative = 0;
    jnum->type = JSON_INTEGER;
    char_buffer_init(&jnum->buffer);
    jnum->state = 0;
}

void json_number_uninit(json_number_t *jnum)
{
    char_buffer_uninit(&jnum->buffer);
}

#define NUM_STATE_INIT 0
#define NUM_STATE_GOT_MINUS 1
#define NUM_STATE_GOT_ZERO 2
#define NUM_STATE_GOT_SEPARATOR 3
#define NUM_STATE_GOT_FRACTION_DIGIT 4
#define NUM_STATE_GOT_NEGATIVE 5
#define NUM_STATE_GOT_NONZERO 6
#define NUM_STATE_GOT_EXPONENT 7
#define NUM_STATE_GOT_EXP_DIGIT 8
#define NUM_STATE_GOT_EXP_SIGN 9

#define IS_NONZERO_DIGIT(c) ((c) >= '1' && (c) <= '9')
#define IS_DIGIT(c) ((c) >= '0' && (c) <= '9')
#define IS_EXPONENT(c) ((c) == 'e' || (c) == 'E')

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
        assert(jnum->type == JSON_INTEGER);
        switch (*next_chunk->start) {
        case '0':
            char_buffer_append(&jnum->buffer, next_chunk->start, 1);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_ZERO;
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            goto at_NUM_STATE_GOT_ZERO;
        case '-':
            assert(jnum->int_negative == 0);
            jnum->int_negative = 1;
            char_buffer_append(&jnum->buffer, next_chunk->start, 1);
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
                jnum->int_value = (int64_t)(*next_chunk->start - '0');
                char_buffer_append(&jnum->buffer, next_chunk->start, 1);
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
            char_buffer_append(&jnum->buffer, next_chunk->start, 1);
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
                jnum->int_value += (int64_t)(*next_chunk->start - '0');
                char_buffer_append(&jnum->buffer, next_chunk->start, 1);
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
                if (!jnum->int_overflow) {
                    const uint64_t oldval = jnum->int_value;
                    jnum->int_value *= 10;
                    jnum->int_value += (int64_t)(*cur - '0');
                    /* detect overflow */
                    if (((uint64_t)jnum->int_value & 0x8000000000000000) != 0) {
                        /* may also represent underflow if the input is
                         * negative integer (when jnum->int_negative==1) */
                        jnum->int_overflow = 1;
                    }
                }
                ++cur;
            }
            if (cur > next_chunk->start) {
                const size_t len = cur - next_chunk->start;
                char_buffer_append(&jnum->buffer, next_chunk->start, len);
                strref_trim_front(next_chunk, len);
            }
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            switch (*next_chunk->start) {
            case '.':
                /* convert integer to string, so that later it can be converted
                 * into a floating point number */
                assert(jnum->type == JSON_INTEGER);
                jnum->type = JSON_FLOATING;
                char_buffer_append(&jnum->buffer, ".", 1);
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_SEPARATOR;
                if (next_chunk->size == 0) {
                    ret = JSON_NEED_MORE;
                    goto done;
                }
                goto at_NUM_STATE_GOT_SEPARATOR;
            case 'E':
            case 'e':
                assert(jnum->type == JSON_INTEGER);
                jnum->type = JSON_FLOATING;
                char_buffer_append(&jnum->buffer, "e", 1);
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_EXPONENT;
                if (next_chunk->size == 0) {
                    ret = JSON_NEED_MORE;
                    goto done;
                }
                goto at_NUM_STATE_GOT_EXPONENT;
            }
            ret = JSON_READY;
            goto done;
        }

at_NUM_STATE_GOT_ZERO:
    case NUM_STATE_GOT_ZERO:
        assert(jnum->int_value == 0);
        switch (*next_chunk->start) {
        case '.':
            /* convert integer to string, so that later it can be converted
             * into a floating point number */
            assert(jnum->type == JSON_INTEGER);
            jnum->type = JSON_FLOATING;
            char_buffer_append(&jnum->buffer, ".", 1);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_SEPARATOR;
            if (next_chunk->size == 0) {
                ret = JSON_NEED_MORE;
                goto done;
            }
            goto at_NUM_STATE_GOT_SEPARATOR;
        case 'E':
        case 'e':
            assert(jnum->type == JSON_INTEGER);
            jnum->type = JSON_FLOATING;
            char_buffer_append(&jnum->buffer, "e", 1);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_EXPONENT;
            if (next_chunk->size == 0) {
                ret = JSON_NEED_MORE;
                goto done;
            }
            goto at_NUM_STATE_GOT_EXPONENT;
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
            if (next_chunk->size > 0 && IS_EXPONENT(next_chunk->start[0])) {
                /* 'E' or 'e' found after the digit */
                assert(jnum->type == JSON_FLOATING);
                char_buffer_append(&jnum->buffer, "e", 1);
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_EXPONENT;
                if (next_chunk->size == 0) {
                    ret = JSON_NEED_MORE;
                    goto done;
                }
                goto at_NUM_STATE_GOT_EXPONENT;
            }
            ret = JSON_READY;
            goto done;
        }

at_NUM_STATE_GOT_EXPONENT:
    case NUM_STATE_GOT_EXPONENT:
        {
            char c;
            assert(next_chunk->size > 0);
            c = *next_chunk->start;
            switch (c) {
            case '+':
            case '-':
                char_buffer_append(&jnum->buffer, &c, 1);
                strref_trim_front(next_chunk, 1);
                jnum->state = NUM_STATE_GOT_EXP_SIGN;
                if (next_chunk->size == 0) {
                    ret = JSON_NEED_MORE;
                    goto done;
                }
                goto at_NUM_STATE_GOT_EXP_SIGN;
                break;
            default:
                if (!IS_DIGIT(c))
                    goto input_error;
            }

            char_buffer_append(&jnum->buffer, &c, 1);
            strref_trim_front(next_chunk, 1);
            jnum->state = NUM_STATE_GOT_EXP_DIGIT;
            if (next_chunk->size == 0) {
                ret = JSON_READY;
                goto done;
            }
            goto at_NUM_STATE_GOT_EXP_DIGIT;
        }

at_NUM_STATE_GOT_EXP_SIGN:
    case NUM_STATE_GOT_EXP_SIGN:
        assert(next_chunk->size > 0);
        if (!IS_DIGIT(*next_chunk->start)) {
            ret = JSON_READY;
            goto done;
        }
        goto at_NUM_STATE_GOT_EXP_DIGIT;

at_NUM_STATE_GOT_EXP_DIGIT:
    case NUM_STATE_GOT_EXP_DIGIT:
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

enum json_code_t json_number_result(json_number_t *jnum,
                                    enum json_type_t *type,
                                    union json_number_union_t *result)
{
    enum json_code_t retval = JSON_READY;

    if (jnum->type == JSON_INTEGER) {
        if (jnum->int_overflow) {
            retval = JSON_INPUT_ERROR;
        }
        else {
            result->integer = jnum->int_negative ?
                -(int64_t)jnum->int_value :
                (int64_t)jnum->int_value;
        }
    }
    else {
        strref_t ref = { 0 };
        char *endptr;
        assert(jnum->type == JSON_FLOATING);
        char_buffer_get(&jnum->buffer, &ref);
#ifdef _MSC_VER
        errno = 0;
        result->floating = strtod(ref.start, &endptr);
        int err = errno;
        if (*endptr != '\0' || errno == ERANGE)
            retval = JSON_INPUT_ERROR;
#else
#    error "Take care to implement this correctly on non-Windows platforms"
#endif
        strref_uninit(&ref);
    }

    *type = jnum->type;
    return retval;
}

void json_number_as_str(json_number_t *jnum, strref_t *str)
{
    char_buffer_get(&jnum->buffer, str);
}
