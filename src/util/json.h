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
enum json_code_t json_string_parse(json_string_t *jstr, ro_seg_t *next_chunk);
/* TODO: make result const */
void json_string_result(json_string_t *jstr, seg_t *result);

union json_number_union_t {
    double floating;
    int64_t integer;
};

enum json_data_t { JSON_INTEGER, JSON_FLOATING };

typedef struct _json_number_t
{
    uint64_t int_value;
    int int_overflow;  /* flag that indicates both overflow and underflow */
    int int_negative;
    enum json_data_t type;
    struct char_buffer_t buffer;
    unsigned state;
} json_number_t;

void json_number_init(json_number_t *jnum);
void json_number_uninit(json_number_t *jnum);
enum json_code_t json_number_parse(json_number_t *jnum, ro_seg_t *next_chunk);
/* TODO: make result const */
enum json_code_t json_number_result(json_number_t *jnum,
                                    enum json_data_t *type,
                                    union json_number_union_t *result);
void json_number_as_str(json_number_t *jnum, seg_t *seg);

enum json_value_t
{
    JSON_NO_VALUE,
    JSON_STRING,
    JSON_NUMBER,
    JSON_TRUE,
    JSON_FALSE,
    JSON_NULL
};

union json_value_union_t
{
    json_string_t str;
    json_number_t num;
};

typedef struct _json_value_t
{
    enum json_value_t type;
    union json_value_union_t value;
    unsigned state;
    const char *literal;
    const char *literal_ptr;
} json_value_t;

void json_value_init(json_value_t *jval);
void json_value_uninit(json_value_t *jval);
enum json_code_t json_value_parse(json_value_t *jval, ro_seg_t *next_chunk);
/* TODO: make result const */
void json_value_result(json_value_t *jval,
                       enum json_value_t *type,
                       union json_value_union_t **result);

#endif  /* _JSON_H_ */
