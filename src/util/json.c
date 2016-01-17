#include "json.h"
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

enum json_code_t json_string_parse(json_string_t *jstr, strref_t *next_chunk)
{
    struct char_buffer_t *output = &jstr->output;
    strref_t *input_lookback = &jstr->input_lookback;
    char *cur, *end;
    char c;

    assert(next_chunk->size > 0);  /* always expect some input */

    strref_get_start_and_end(next_chunk, &cur, &end);

    assert(cur < end);
    if (input_lookback->size > 0)
        goto escape_seq;

parse:
    do {
        if (*cur == '"')
            goto done;
        else if (*cur == '\\') {
            if (cur > next_chunk->start) {
                const size_t len = cur - next_chunk->start;
                char_buffer_append(output, next_chunk->start, len);
                strref_trim_front(next_chunk, len);
            }
            goto escape_seq;
        }
    } while (++cur < end);
    goto done;

escape_seq:
    assert(cur < end);
    assert(input_lookback->size > 0 || *cur == '\\');
    if (input_lookback->size == 0) {
        *input_lookback->start = '\\';
        input_lookback->size = 1;
        ++cur;
        strref_trim_front(next_chunk, 1);
    }
    assert(*input_lookback->start == '\\');

    if (cur == end)
        return JSON_NEED_MORE;

    switch (*cur) {
    case '"':
        c = '"';
        break;
    case '\\':
        c = '\\';
        break;
    case '/':
        c = '/';
        break;
    case 'b':
        c = '\b';
        break;
    case 'f':
        c = '\f';
        break;
    case 'n':
        c = '\n';
        break;
    case 'r':
        c = '\r';
        break;
    case 't':
        c = '\t';
        break;
    default:
        /* TODO: handle error */
        break;
    }

    strref_trim_front(next_chunk, 1);
    /* TODO: make a fast "append char" function */
    char_buffer_append(output, &c, 1);

    ++cur;

    if (cur < end)
        goto parse;

done:
    /* TODO: check if there is unprocessed escape sequence */

    {
        const size_t len = cur - next_chunk->start;
        char_buffer_append(output, next_chunk->start, len);
        strref_trim_front(next_chunk, len);
    }

    return JSON_READY;
}

void json_string_result(json_string_t *jstr, strref_t *result)
{
    char_buffer_get(&jstr->output, result);
}
