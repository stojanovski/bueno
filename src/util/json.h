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

enum json_code_t { JSON_DONE, JSON_NEED_MORE, JSON_INPUT_ERROR };

typedef struct _json_string_t
{
    strref_t output;
    /* stores escaping input data */
    char input_lookback_buf[6];
    strref_t input_lookback;
    int had_escapes;
} json_string_t;

void json_string_init(json_string_t *jstr);
void json_string_uninit(json_string_t *jstr);
enum json_code_t json_string_parse(json_string_t *jstr, strref_t *next_chunk);
strref_t *json_string_result(json_string_t *jstr);

#endif  /* _JSON_H_ */
