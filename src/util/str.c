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

void strref_init(strref_t *str, size_t size)
{
    if (size > 0) {
        str->__buf = (char *)malloc(size);
        str->start = str->__buf;
        str->size = size;
    }
    else
        memset(str, 0, sizeof(strref_t));
}

void strref_uninit(strref_t *str)
{
    free(str->__buf);
}

void strref_clear(strref_t *str)
{
    free(str->__buf);
    memset(str, 0, sizeof(strref_t));
}

void strref_assign_char(strref_t *str, char *start, size_t size)
{
    free(str->__buf);
    str->__buf = NULL;
    str->start = start;
    str->size = size;
}

static void strref_copy_helper(strref_t *str, const char *start, size_t size)
{
    if (size > 0) {
        assert(start != NULL);
        str->__buf = (char *)malloc(size);
        memcpy(str->__buf, start, size);
        str->start = str->__buf;
        str->size = size;
    }
    else
        memset(str, 0, sizeof(strref_t));
}

void strref_copy_char(strref_t *str, const char *start, size_t size)
{
    free(str->__buf);
    strref_copy_helper(str, start, size);
}

void strref_assign(strref_t *dst, const strref_t *src)
{
    free(dst->__buf);
    dst->__buf = NULL;
    dst->start = src->start;
    dst->size = src->size;
}

void strref_take_ownership(strref_t *dst, strref_t *src)
{
    free(dst->__buf);
    memcpy(dst, src, sizeof(strref_t));
    memset(src, 0, sizeof(strref_t));
}

void strref_copy(strref_t *dst, const strref_t *src)
{
    free(dst->__buf);
    strref_copy_helper(dst, src->start, src->size);
}

strref_t *strref_set_static(strref_t *str, char *null_term_str)
{
    strref_clear(str);
    if (null_term_str != NULL) {
        str->start = null_term_str;
        str->size = strlen(null_term_str);
    }
    return str;
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

void char_buffer_set(struct char_buffer_t *cb, const char *buf, size_t sz)
{
    strarray_set(&cb->arr, buf, sz);
    strarray_append(&cb->arr, "", 1);
}

void char_buffer_pop_front(struct char_buffer_t *cb, size_t sz)
{
    /* trailing null has to remain */
    assert(sz < strarray_size(&cb->arr));
    strarray_pop_front(&cb->arr, sz);
}

void char_buffer_get(struct char_buffer_t *cb, strref_t *str)
{
    strarray_get_strref(&cb->arr, str);
    /* remove the trailing null from the size */
    assert(str->size > 0);
    --str->size;
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
    strref_init(&arr->str, element_size * init_capacity);
    arr->size = 0;
    arr->element_size = element_size;
}

void strarray_clear(strarray_t *arr)
{
    arr->size = 0;
}

void strarray_uninit(strarray_t *arr)
{
    strref_uninit(&arr->str);
}

size_t strarray_size(strarray_t *arr)
{
    return arr->size;
}

void strarray_append(strarray_t *arr, const void *buf, size_t sz)
{
    const size_t cur_capacity = strref_size(&arr->str) / arr->element_size;
    size_t need_capacity = cur_capacity;
    while ((arr->size + sz) > need_capacity)
        need_capacity *= 2;

    if (need_capacity > cur_capacity) {
        strref_t newstr;
        strref_init(&newstr, need_capacity * arr->element_size);
        memcpy(newstr.start, arr->str.start, arr->size * arr->element_size);
        strref_take_ownership(&arr->str, &newstr);
        strref_uninit(&newstr);
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

void strarray_get_strref(strarray_t *arr, strref_t *str)
{
    strref_assign(str, &arr->str);
    str->size = arr->size;
}
