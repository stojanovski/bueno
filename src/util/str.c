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

#define STRBUF_REF_CALL(strref_t_ptr, func) ((*((refcnted_strbuf_funcs_t **)((strref_t_ptr)->__bk)))->func((strref_t_ptr)->__bk))
#define STRBUF_CREATE_NEW(ptr, str, start, size) \
    ((*((refcnted_strbuf_funcs_t **)((ptr)->__bk)))->create_new((str), (start), (size)))
#define STRBUF_GET_WRITABLE(ptr) \
    ((*((refcnted_strbuf_funcs_t **)((ptr)->__bk)))->get_writable((ptr)->__bk, (ptr)))

typedef struct _refcnted_strbuf_funcs_t
{
    void *(* create_new)(strref_t *, const char *, size_t);
    void (* grab_ref)(void *);
    void (* release_ref)(void *);
    void *(* get_writable)(void *, strref_t *);
} refcnted_strbuf_funcs_t;

typedef struct _strbuf_t
{
    refcnted_strbuf_funcs_t *funcs;
    size_t refcnt;
    size_t bufsize;
    char buf[0];
} strbuf_t;

size_t strref_size(strref_t *str)
{
    return str->size;
}

void strref_init(strref_t *str, void *bookkeeper)
{
    memset(str, 0, sizeof(strref_t));
    str->__bk = bookkeeper;
}

void strref_uninit(strref_t *str)
{
    if (str->__bk) {
        STRBUF_REF_CALL(str, release_ref);
    }
}

void strref_clear(strref_t *str)
{
    if (str->__bk) {
        STRBUF_REF_CALL(str, release_ref);
    }
    memset(str, 0, sizeof(strref_t));
}

const char *strref_readonly(strref_t *str)
{
    return str->start;
}

/* char *get_writable_from_string_buffer(str); */

char *strref_writable(strref_t *str)
{
    if (!str->__unsafe_to_write)
        return str->start;

    assert(str->__bk != NULL);
    /* str->__bk = ((refcnted_strbuf_funcs_t *)(str->__bk))->get_writable(str->__bk, str); */
    str->__bk = STRBUF_GET_WRITABLE(str);
    return str->start;
}

void strref_assign_char(strref_t *str, char *start, size_t size)
{
    strref_clear(str);
    str->start = start;
    str->size = size;
}

void strref_copy_char(strref_t *str, const char *start, size_t size)
{
    void *old_bk = str->__bk;
    assert(str->__bk);

    /* str->__bk = ((refcnted_strbuf_funcs_t *)(str->__bk))->create_new(str->__bk, str, start, size); */
    /* str->__bk = (*((refcnted_strbuf_funcs_t **)(str->__bk)))->create_new(str->__bk, str, start, size); */
    str->__bk = STRBUF_CREATE_NEW(str, str, start, size);
    /* str->__bk = ((strbuf_t *)(str->__bk))->funcs->create_new(str->__bk, str, start, size); */

    (*((refcnted_strbuf_funcs_t **)(old_bk)))->release_ref(old_bk);
    /* ((strbuf_t *)old_bk)->funcs->release_ref(str->__bk, str); */
    str->__unsafe_to_write = 0;
}

void strref_assign(strref_t *dest, strref_t *src)
{
    strref_clear(dest);

    memcpy(dest, src, sizeof(strref_t));
    if (dest->__bk) {
        dest->__unsafe_to_write = src->__unsafe_to_write = 1;
        STRBUF_REF_CALL(dest, grab_ref);
    }
    else {
        assert(dest->__unsafe_to_write == 0);
    }
}

void strref_take_ownership(strref_t *dest, strref_t *src)
{
    strref_clear(dest);
    memcpy(dest, src, sizeof(strref_t));
    memset(src, 0, sizeof(strref_t));
}

void strref_copy(strref_t *dest, strref_t *src)
{
    assert(src->__bk);
    strref_clear(dest);
    src->__bk = STRBUF_CREATE_NEW(src, dest, src->start, src->size);
}

#if OLD_IMPL_OF_CHAR_BUFFER
struct struct _default_strref_keeper
{
};

typedef struct _strbuf_t
{
    refcnted_strbuf_funcs_t *funcs;
    size_t refcnt;
    size_t bufsize;
    char buf[0];
} strbuf_t;
#endif

refcnted_strbuf_funcs_t *get_default_strref_keeper_funcs__instance();

static void *default_strref_keeper_create_new(strref_t *str,
                                              const char *start,
                                              size_t size)
{
    strbuf_t *sb = (strbuf_t *)malloc(sizeof(strbuf_t) + size);

    /* TODO: should get the funcs from bk? */
    sb->funcs = get_default_strref_keeper_funcs__instance();
    sb->bufsize = size;
    sb->refcnt = 1;
    if (size > 0 && start != NULL)
        memcpy(sb->buf, start, size);

    str->start = sb->buf;
    str->size = size;

    return sb;
}

static void default_strref_keeper_grab_ref(void *bk)
{
    strbuf_t *sb = (strbuf_t *)bk;
    ++sb->refcnt;
}

static void default_strref_keeper_release_ref(void *bk)
{
    strbuf_t *sb = (strbuf_t *)bk;
    /* refcnt == 0 is currently a sentinel value */
    if (sb->refcnt > 0 && --sb->refcnt == 0)
        free(sb);
}

static void *default_strref_keeper_get_writable(void *bk, strref_t *str)
{
    strbuf_t *old_sb = (strbuf_t *)bk;
    strbuf_t *new_sb;
    (void)str;
    if (old_sb->refcnt == 1)
        return old_sb;

    new_sb = default_strref_keeper_create_new(str, str->start, str->size);
    default_strref_keeper_release_ref(old_sb);

    return new_sb;
}

static refcnted_strbuf_funcs_t default_strref_keeper;

refcnted_strbuf_funcs_t *get_default_strref_keeper()
{
    if (default_strref_keeper.create_new == NULL) {
        default_strref_keeper.create_new = default_strref_keeper_create_new;
        default_strref_keeper.get_writable = default_strref_keeper_get_writable;
        default_strref_keeper.grab_ref = default_strref_keeper_grab_ref;
        default_strref_keeper.release_ref = default_strref_keeper_release_ref;
    }

    return &default_strref_keeper;
}

refcnted_strbuf_funcs_t *get_default_strref_keeper_funcs__instance()
{
    return get_default_strref_keeper();
}

static strbuf_t empty_strref_keeper;

void *get_empty_strref_keeper__instance()
{
    if (empty_strref_keeper.funcs == NULL) {
        empty_strref_keeper.funcs = get_default_strref_keeper_funcs__instance();
        empty_strref_keeper.refcnt = 0;
    }
    return &empty_strref_keeper;
}

strbuf_t *duplicate_default_strref_keeper(strbuf_t *src)
{
    strbuf_t *sb = (strbuf_t *)malloc(sizeof(strbuf_t) + src->bufsize);
    return sb;  /* UNDONE!! */
}

/* char_buffer_t */

#define CHAR_BUFFER_INIT_CAPACITY 10

#if OLD_IMPL_OF_CHAR_BUFFER
void char_buffer_init(struct char_buffer_t *cb)
{
    cb->buf = (char *)malloc(CHAR_BUFFER_INIT_CAPACITY + 1);
    cb->buf[0] = '\0';
    cb->size = 0;
    cb->capacity = CHAR_BUFFER_INIT_CAPACITY;
}

void char_buffer_clear(struct char_buffer_t *cb)
{
    cb->buf[0] = '\0';
    cb->size = 0;
}

void char_buffer_uninit(struct char_buffer_t *cb)
{
    free(cb->buf);
}

void char_buffer_append(struct char_buffer_t *cb, const char *buf, size_t sz)
{
    size_t need_capacity = cb->capacity;
    while (cb->size + sz > need_capacity)
        need_capacity *= 2;

    if (need_capacity > cb->capacity) {
        char *new_buf = (char *)malloc(need_capacity + 1);
        memcpy(new_buf, cb->buf, cb->size + 1);  /* +1 for '\0' */
        free(cb->buf);
        cb->buf = new_buf;
        cb->capacity = need_capacity;
    }

    memcpy(cb->buf + cb->size, buf, sz);
    cb->buf[cb->size + sz] = '\0';
    cb->size += sz;
}

void char_buffer_set(struct char_buffer_t *cb, const char *buf, size_t sz)
{
    char_buffer_clear(cb);
    char_buffer_append(cb, buf, sz);
}

void char_buffer_pop_front(struct char_buffer_t *cb, size_t sz)
{
    size_t i;
    assert(sz <= cb->size);
    if (sz == 0)
        return;

    for (i = 0; i < cb->size - sz + 1; ++i)
        cb->buf[i] = cb->buf[i + sz];

    cb->size -= sz;
}
#else
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
#endif

/* strarray_t */

void strarray_init(strarray_t *arr, size_t element_size, size_t init_capacity)
{
    assert(init_capacity > 0);
    STRREF_INIT_CAPACITY(&arr->str, init_capacity * element_size);
    arr->size = 0;
    arr->element_size = element_size;
}

void strarray_clear(strarray_t *arr)
{
    arr->size = 0;
}

void strarray_uninit(strarray_t *arr)
{
    STRREF_UNINIT(&arr->str);
}

size_t strarray_size(strarray_t *arr)
{
    return arr->size;
}

void *strarray_writable(strarray_t *arr)
{
    return STRREF_WRITE(&arr->str);
}

const void *strarray_readonly(strarray_t *arr)
{
    return STRREF_READ(&arr->str);
}

void strarray_append(strarray_t *arr, const void *buf, size_t sz)
{
    const size_t cur_capacity = strref_size(&arr->str) / arr->element_size;
    size_t need_capacity = cur_capacity;
    while ((arr->size + sz) > need_capacity)
        need_capacity *= 2;

    if (need_capacity > cur_capacity) {
        strref_t newstr;
        STRREF_INIT_CAPACITY(&newstr, need_capacity * arr->element_size);
        memcpy(STRREF_WRITE(&newstr), STRREF_READ(&arr->str), arr->size * arr->element_size);
        strref_take_ownership(&arr->str, &newstr);
        STRREF_UNINIT(&newstr);
    }

    memcpy(STRREF_WRITE(&arr->str) + (arr->size * arr->element_size), buf, sz * arr->element_size);
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
    dest = (char *)STRREF_WRITE(&arr->str);
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

void strref_set_static(strref_t *str, char *null_term_str)
{
    strref_clear(str);
    if (null_term_str != NULL) {
        str->start = null_term_str;
        str->size = strlen(null_term_str);
    }
}
