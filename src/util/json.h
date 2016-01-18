/*
 * Copyright 2015 Igor Stojanovski
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _JSON_H_
#define _JSON_H_

#include "str.h"
#include "types.h"

enum json_code_t { JSON_READY, JSON_NEED_MORE, JSON_INPUT_ERROR };

typedef struct _json_string_t
{
    struct char_buffer_t output;
    /* bytes of escaped data read during escaping */
    size_t escape_seq_len;
    /* during unicode escaping (\uxxxx) stores the current incarnation of the
     * final value */
    uint16_t unicode_escaped_value;
} json_string_t;

void json_string_init(json_string_t *jstr);
void json_string_uninit(json_string_t *jstr);
enum json_code_t json_string_parse(json_string_t *jstr, strref_t *next_chunk);
void json_string_result(json_string_t *jstr, strref_t *result);

#endif  /* _JSON_H_ */
