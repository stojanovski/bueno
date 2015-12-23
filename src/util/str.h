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

typedef struct _strref_t
{
    char *start;
    size_t size;
    int __unsafe_to_write;
    void *__bk;
} strref_t;

void *get_empty_strref_keeper__instance();

void strref_init(strref_t *str, void *bookkeeper);
size_t strref_size(strref_t *str);
void strref_uninit(strref_t *str);
void strref_clear(strref_t *str);
const char *strref_readonly(strref_t *str);
char *strref_writable(strref_t *str);
void strref_assign_char(strref_t *str, char *start, size_t size);
void strref_copy_char(strref_t *str, const char *start, size_t size);
void strref_assign(strref_t *dest, strref_t *src);
void strref_take_ownership(strref_t *dest, strref_t *src);
void strref_copy(strref_t *dest, strref_t *src);

#define STRREF_INIT_CAPACITY(strref_t_ptr, size_in_bytes) \
    do { \
    strref_init((strref_t_ptr), get_empty_strref_keeper__instance()); \
    strref_copy_char((strref_t_ptr), NULL, (size_in_bytes)); \
    } while (0)
#define STRREF_UNINIT(strref_t_ptr) \
    do { \
    strref_uninit((strref_t_ptr)); \
    } while (0)
#define STRREF_WRITE(strref_t_ptr) (strref_writable((strref_t_ptr)))
#define STRREF_READ(strref_t_ptr) (strref_readonly((strref_t_ptr)))

/* strarray_t */

typedef struct _strarray_t
{
    strref_t str;
    size_t size;
    size_t element_size;
} strarray_t;

void strarray_init(strarray_t *arr, size_t element_size, size_t init_capacity);
void strarray_clear(strarray_t *arr);
void strarray_uninit(strarray_t *arr);
size_t strarray_size(strarray_t *arr);
void *strarray_writable(strarray_t *arr);
const void *strarray_readonly(strarray_t *arr);
void strarray_append(strarray_t *arr, const void *buf, size_t sz);
void strarray_set(strarray_t *arr, const void *buf, size_t sz);
void strarray_pop_front(strarray_t *arr, size_t sz);
void strarray_pop_back(strarray_t *arr, size_t sz);
void strarray_get_strref(strarray_t *arr, strref_t *str);

/* char_buffer_t */

#if OLD_IMPL_OF_CHAR_BUFFER
struct char_buffer_t
{
    char *buf;
    size_t size;
    size_t capacity;
};
#else
struct char_buffer_t
{
    strarray_t arr;
};
#endif

void char_buffer_init(struct char_buffer_t *cb);
void char_buffer_clear(struct char_buffer_t *cb);
void char_buffer_uninit(struct char_buffer_t *cb);
void char_buffer_append(struct char_buffer_t *cb, const char *buf, size_t sz);
void char_buffer_set(struct char_buffer_t *cb, const char *buf, size_t sz);
void char_buffer_pop_front(struct char_buffer_t *cb, size_t sz);
void char_buffer_get(struct char_buffer_t *cb, strref_t *str);

#endif  /* STR_H */
