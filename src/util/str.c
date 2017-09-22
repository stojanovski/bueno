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

#include "str.h"
#include <stdlib.h>
#include <string.h>

void str_init(str_t *str, size_t size)
{
    if (size > 0) {
        *(char **)&(str->start) = (char *)malloc(size);
        str->size = size;
    }
    else
        memset(str, 0, sizeof(str_t));
}

void str_uninit(str_t *str)
{
    free(*(char **)&(str->start));
}

void str_take_ownership(str_t *dst, str_t *src)
{
    free(*(char **)&(dst->start));
    memcpy(dst, src, sizeof(str_t));
    memset(src, 0, sizeof(str_t));
}

/* char_buffer_t */

#define CHAR_BUFFER_INIT_CAPACITY 10

void char_buffer_init(struct char_buffer_t *cb)
{
    strarray_init(&cb->arr, 1, CHAR_BUFFER_INIT_CAPACITY + 1);
    strarray_append(&cb->arr, "", 1);
}

void char_buffer_clear(struct char_buffer_t *cb)
{
    strarray_clear(&cb->arr);
    strarray_append(&cb->arr, "", 1);
}

void char_buffer_uninit(struct char_buffer_t *cb)
{
    strarray_uninit(&cb->arr);
}

void char_buffer_append(struct char_buffer_t *cb, const char *buf, size_t sz)
{
    strarray_pop_back(&cb->arr, 1);  /* remove the trailing null char first */
    strarray_append(&cb->arr, buf, sz);
    strarray_append(&cb->arr, "", 1);
}

void char_buffer_append_ro_seg(struct char_buffer_t *cb, const ro_seg_t *seg)
{
    char_buffer_append(cb, seg->start, seg->size);
}

void char_buffer_set(struct char_buffer_t *cb, const char *buf, size_t sz)
{
    strarray_set(&cb->arr, buf, sz);
    strarray_append(&cb->arr, "", 1);
}

void char_buffer_set_ro_seg(struct char_buffer_t *cb, const ro_seg_t *seg)
{
    char_buffer_set(cb, seg->start, seg->size);
}

void char_buffer_pop_front(struct char_buffer_t *cb, size_t sz)
{
    /* trailing null has to remain */
    assert(sz < strarray_size(&cb->arr));
    strarray_pop_front(&cb->arr, sz);
}

void char_buffer_get(struct char_buffer_t *cb, seg_t *seg)
{
    strarray_get_strref(&cb->arr, seg);
    /* remove the trailing null from the size */
    assert(seg->size > 0);
    --seg->size;
}

size_t char_buffer_size(struct char_buffer_t *cb)
{
    size_t size = strarray_size(&cb->arr);
    assert(size > 0);
    return size - 1;
}


/* strarray_t */

void strarray_init(strarray_t *arr, size_t element_size, size_t init_capacity)
{
    assert(init_capacity > 0);
    str_init(&arr->str, element_size * init_capacity);
    arr->size = 0;
    arr->element_size = element_size;
}

void strarray_clear(strarray_t *arr)
{
    arr->size = 0;
}

void strarray_uninit(strarray_t *arr)
{
    str_uninit(&arr->str);
}

size_t strarray_size(strarray_t *arr)
{
    return arr->size;
}

void strarray_append(strarray_t *arr, const void *buf, size_t sz)
{
    const size_t cur_capacity = str_size(&arr->str) / arr->element_size;
    size_t need_capacity = cur_capacity;
    while ((arr->size + sz) > need_capacity)
        need_capacity *= 2;

    if (need_capacity > cur_capacity) {
        str_t newstr;
        str_init(&newstr, need_capacity * arr->element_size);
        memcpy(newstr.start, arr->str.start, arr->size * arr->element_size);
        str_take_ownership(&arr->str, &newstr);
        str_uninit(&newstr);
    }

    memcpy(arr->str.start + (arr->size * arr->element_size), buf, sz * arr->element_size);
    arr->size += sz;
}

void strarray_set(strarray_t *arr, const void *buf, size_t sz)
{
    strarray_clear(arr);
    strarray_append(arr, buf, sz);
}

/* TODO: Alternate implementation is to simply forward the start pointer
 * rather than copy, making sure that appending more data does not result
 * in a degenerate situation */
void strarray_pop_front(strarray_t *arr, size_t sz)
{
    const char *src, *src_end;
    char *dest;
    assert(sz <= arr->size);
    if (sz == 0)
        return;

    arr->size -= sz;
    dest = arr->str.start;
    src = dest + sz * arr->element_size;
    src_end = src + (arr->size * arr->element_size);
    while (src < src_end)
        *dest++ = *src++;
}

void strarray_pop_back(strarray_t *arr, size_t sz)
{
    assert(sz <= arr->size);
    arr->size -= sz;
}

void strarray_get_strref(strarray_t *arr, seg_t *seg)
{
    seg_from_str(seg, &arr->str);
    seg->size = arr->size;
}
