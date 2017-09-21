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

#ifndef STR_H
#define STR_H

#include <assert.h>
#include <string.h>

typedef struct _seg_t
{
    char *start;
    size_t size;
} seg_t;

typedef struct _const_seg_t
{
    const char *start;
    size_t size;
} const_seg_t;

typedef struct _str_t
{
    char * const start;
    size_t size;
} str_t;


/* str_t */

void str_init(str_t *str, size_t size);
static size_t str_size(str_t *str)
{
    return str->size;
}
void str_uninit(str_t *str);
void str_take_ownership(str_t *dst, str_t *src);


/* seg_t, const_seg_t */

static void const_seg_trim_front(const_seg_t *seg, size_t len)
{
    assert(seg->start != (char *)0);
    assert(seg->size >= len);
    seg->start += len;
    seg->size -= len;
}

static void const_seg_get_start_and_end(const const_seg_t *seg,
                                        const char **start,
                                        const char **end)
{
    assert(seg->start != (char *)0);
    *start = seg->start;
    *end = *start + seg->size;
}

static void seg_from_str(seg_t *seg, const str_t *str)
{
    seg->start = str->start;
    seg->size = str->size;
}

static void const_seg_assign(const_seg_t *dst, const const_seg_t *src)
{
    dst->start = src->start;
    dst->size = src->size;
}

#define SEG_SET_STATIC(_seg_, _null_term_str_) \
    _seg_->start = _null_term_str_; \
    _seg_->size = strlen(_null_term_str_); \
    return _seg_;
static seg_t *seg_set_static(seg_t *seg, char *null_term_str)
{
    SEG_SET_STATIC(seg, null_term_str)
}
static const_seg_t *const_seg_set_static(const_seg_t *seg, char *null_term_str)
{
    SEG_SET_STATIC(seg, null_term_str)
}


/* strarray_t */

typedef struct _strarray_t
{
    str_t str;
    size_t size;
    size_t element_size;
} strarray_t;

void strarray_init(strarray_t *arr, size_t element_size, size_t init_capacity);
void strarray_clear(strarray_t *arr);
void strarray_uninit(strarray_t *arr);
size_t strarray_size(strarray_t *arr);
void strarray_append(strarray_t *arr, const void *buf, size_t sz);
void strarray_set(strarray_t *arr, const void *buf, size_t sz);
void strarray_pop_front(strarray_t *arr, size_t sz);
void strarray_pop_back(strarray_t *arr, size_t sz);
void strarray_get_strref(strarray_t *arr, seg_t *seg);


/* char_buffer_t */

struct char_buffer_t
{
    strarray_t arr;
};

void char_buffer_init(struct char_buffer_t *cb);
void char_buffer_clear(struct char_buffer_t *cb);
void char_buffer_uninit(struct char_buffer_t *cb);
void char_buffer_append(struct char_buffer_t *cb, const char *buf, size_t sz);
void char_buffer_set(struct char_buffer_t *cb, const char *buf, size_t sz);
void char_buffer_pop_front(struct char_buffer_t *cb, size_t sz);
void char_buffer_get(struct char_buffer_t *cb, seg_t *seg);
size_t char_buffer_size(struct char_buffer_t *cb);

#endif  /* STR_H */
