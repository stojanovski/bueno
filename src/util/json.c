#include "json.h"
#include <string.h>
#include <assert.h>

void json_string_init(json_string_t *jstr)
{
    memset(jstr, 0, sizeof(json_string_t));
    jstr->input_lookback.start = jstr->input_lookback_buf;
    jstr->input_lookback.size = 0;
}

void json_string_uninit(json_string_t *jstr)
{
    strref_uninit(&jstr->output);
}

enum json_code_t json_string_parse(json_string_t *jstr, strref_t *next_chunk)
{
    strref_t *output = &jstr->output;
    const char *inp;

    if (jstr->had_escapes) {
        assert(output->size > 0); /* must at least contain the double-quotes */
        goto after_escapes;
    }
    else if (output->size == 0) {
        strref_assign(output, next_chunk);

        /* "next_chunk" must contain at least the opening double-quotes */
        assert(output->size > 0);
        assert(*output->start == '"');

        /* don't expect anything inside the lookback buffer */
        assert(jstr->input_lookback.size == 0);
    }

after_escapes:
    return JSON_DONE;
}

strref_t *json_string_result(json_string_t *jstr)
{
    return &jstr->output;
}
