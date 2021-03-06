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

#include "../util/str.h"
#include "../util/io.h"
#include "../util/tree.h"
#include "../util/json.h"
#include "../util/common.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

static int line_reader_speed(int argc, char **argv)
{
    DWORD start_tm = GetTickCount();
    DWORD end_tm;
    struct strrdr_t fr;
    int ret;
    struct char_buffer_t cb;
    assert(argc > 0);

    char_buffer_init(&cb);
    po_file_reader_init(&fr, argv[0]);
    if ((ret = strrdr_open(&fr)) < 0) {
        const char *error;
        int errnum;
        strrdr_get_error(&fr, &error, &errnum);
        printf("Got errnum=%d error=\"%s\"\n", errnum, error);
        goto done;
    }

    {
        size_t iters = 0;
        size_t bytes = 0;
        ro_seg_t seg;
        while ((ret = strrdr_read(&fr, &seg)) > 0) {
            char_buffer_set_ro_seg(&cb, &seg);
            if (bytes % 10000000 == 0)
                printf("bytes=%u\n", (unsigned)bytes);
            ++iters;
            bytes += (size_t)ret;
        }

        end_tm = GetTickCount();
        printf("iters=%u bytes=%u tm=%u\n", (unsigned)iters, (unsigned)bytes, (unsigned)(end_tm - start_tm));
    }
done:
    po_file_reader_uninit(&fr);
    char_buffer_uninit(&cb);

    return 0;
}

#define FILE_READER_BUFLEN 1024

static int line_reader_speed_baseline(int argc, char **argv)
{
    DWORD start_tm = GetTickCount();
    DWORD end_tm;
    FILE *fp;
    assert(argc > 0);

    fp = fopen(argv[0], "rb");
    if (fp == NULL) {
        printf("Got errnum=%d\n", (int)GetLastError());
        goto done;
    }

    {
        char buf[FILE_READER_BUFLEN];
        size_t iters = 0;
        size_t bytes = 0;
        size_t ret;
        while ((ret = fread(buf, 1, FILE_READER_BUFLEN, fp)) > 0) {
            if (bytes % 10000000 == 0)
                printf("bytes=%u\n", (unsigned)bytes);
            ++iters;
            bytes += (size_t)ret;
        }

        end_tm = GetTickCount();
        printf("iters=%u bytes=%u tm=%u\n", (unsigned)iters, (unsigned)bytes, (unsigned)(end_tm - start_tm));
    }
done:
    fclose(fp);

    return 0;
}

static const char *line_reader_str =
"0123456789\n"
"01234567890123456789\n"
"012345678901234567890123456789\n"
"0123456789\n"
"\n"
"0123456789012345678901234567890123456789\n"
"012345678901234567890123456789\n"
"01234567890123456789\n"
"0123456789\n";

static int os_errno()
{
    return (int)GetLastError();
}

static int os_unlink(const char *file)
{
    return _unlink(file);
}

static int util_write_file(const char *file, const char *str)
{
    FILE *fp;
    int status = 0;
    size_t ret;
    size_t len = strlen(str);
    fp = fopen(file, "wb");
    if (fp == NULL) {
        printf("Unable to open file %s: error %d\n", file, os_errno());
        return -1;
    }

    ret = fwrite(str, 1, len, fp);
    if (ret != len) {
        printf("fwrite failed; file %s, error %d\n", file, os_errno());
        status = -1;
    }

    fclose(fp);
    return status;
}

static size_t util_count_chars(const char *str, char c)
{
    size_t n = 0;
    while (*str) {
        if (*str++ == c)
            ++n;
    }
    return n;
}

static unsigned long util_djb2_hash(const unsigned char *str,
                                    size_t len,
                                    unsigned long *resume_hash)
{
    unsigned long hash = resume_hash ? *resume_hash : 5381;
    const char *end = str + len;

    while (str != end)
        hash = ((hash << 5) + hash) + (int)(*str++); /* hash * 33 + c */

    return hash;
}

static void test_assert(int expr,
                        const char *assert_msg,
                        const char *file,
                        unsigned line)
{
    if (!expr) {
        printf("\nASSERT ERROR:\n");
        printf("%s:%u: %s\n", file, line, assert_msg);
        abort();
    }
}

static void abort_expression_return(const char *expr,
                                    int ret,
                                    const char *file,
                                    unsigned line)
{
    printf("\nASSERT ERROR:\n");
    printf("%s:%u: Exression  %s  returned %d.\n", file, line, expr, ret);
    abort();
}

#define ASSERT_MSG(expr, msg) test_assert((expr), (msg), __FILE__, __LINE__)
#define ASSERT_EXP(expr) test_assert((expr), #expr, __FILE__, __LINE__)
#define ASSERT_ZERO(expr) do { \
    int ret; \
    if ((ret = (expr)) != 0) \
        abort_expression_return(#expr, ret, __FILE__, __LINE__); \
    } while(0)
#define ASSERT_NONZERO(expr) do { \
    int ret; \
    if ((ret = (expr)) == 0) \
        abort_expression_return(#expr, ret, __FILE__, __LINE__); \
    } while(0)
#define ASSERT_INT(expr, int_val) do { \
    int ret; \
    if ((ret = (expr)) != int_val) \
        abort_expression_return(#expr, ret, __FILE__, __LINE__); \
    } while(0)

#define TEST_TXT_FILE "test.txt"

static void line_reader_one_test(const char *file_contents, const char *file_path)
{
    struct strrdr_t fr, lr;
    ro_seg_t seg;
    int ret;
    struct char_buffer_t cb;
    const size_t expected_num_of_lines = util_count_chars(file_contents, '\n');
    const size_t file_contents_size = strlen(file_contents);
    const unsigned long expected_hash = util_djb2_hash(file_contents, file_contents_size, NULL);
    unsigned long computed_hash = 5381;
    size_t lines = 0;
    int got_error = 0, missing_newline = 0;

    ASSERT_EXP(util_write_file(file_path, file_contents) == 0);

    char_buffer_init(&cb);
    po_file_reader_init(&fr, file_path);
    po_line_reader_init(&lr, &fr);
    if ((ret = strrdr_open(&lr)) < 0) {
        const char *error;
        int errnum;
        strrdr_get_error(&lr, &error, &errnum);
        printf("Got errnum=%d error=\"%s\"\n", errnum, error);
        got_error = 1;
        goto done;
    }

    while ((ret = strrdr_read(&lr, &seg)) > 0) {
        ASSERT_EXP(!missing_newline);
        /* char_buffer_set_ro_seg(&cb, &seg); */
        computed_hash = util_djb2_hash(seg.start, seg.size, &computed_hash);
        /* printf("LINE (len=%4d): \"%s\"\n", ret, cb.buf); */
#if 0
        printf("LINE (len=%4d): \"", ret);
        fwrite(seg.start, 1, seg.size, stdout);
        printf("\"\n");
#endif
        if (seg.start[seg.size-1] != '\n') {
            /* handle case when file does not end with a newline */
            missing_newline = 1;
        }
        else
            ++lines;
    }

    ASSERT_EXP(lines == expected_num_of_lines);
    ASSERT_EXP(computed_hash == expected_hash);

    //printf("Finished line reader test.  Data: size=%4u lines=%u\n",
        //(unsigned)file_contents_size, (unsigned)lines);

done:
    po_line_reader_uninit(&lr);
    po_file_reader_uninit(&fr);
    char_buffer_uninit(&cb);

    if (os_unlink(file_path))
        printf("Unlink for file %s failed: %d\n", file_path, os_errno());
    ASSERT_MSG(!got_error, "Got error when opening file.");
}

extern size_t file_reader_read_buflen;

static int test_line_reader(int argc, char **argv)
{
    (void)argc; (void)argv;

    line_reader_one_test(line_reader_str, TEST_TXT_FILE);
    line_reader_one_test("abc",           TEST_TXT_FILE);
    line_reader_one_test("abc\ndef",      TEST_TXT_FILE);
    line_reader_one_test("abc\n\n\ndef",  TEST_TXT_FILE);
    line_reader_one_test("\n",            TEST_TXT_FILE);
    line_reader_one_test("",              TEST_TXT_FILE);

    /* run the same test as above, but decrease the read buffer */
    file_reader_read_buflen = 5;
    line_reader_one_test(line_reader_str, TEST_TXT_FILE);

    return 0;
}

/* TEST bintree */

typedef struct _ssize_t_bintree_node_t {
    bintree_node_t node;
    ssize_t value;
} ssize_t_bintree_node_t;

static size_t get_rand(size_t range)
{
    return (size_t)rand() % range;
}

static int ssize_t_less_then_comparator(const bintree_node_t *left,
                                        const bintree_node_t *right)
{
    const ssize_t left_val  = get_container(ssize_t_bintree_node_t, node, left)->value;
    const ssize_t right_val = get_container(ssize_t_bintree_node_t, node, right)->value;
    return left_val <= right_val;
}

static void ssize_t_free_func(bintree_node_t *node)
{
    ssize_t_bintree_node_t * cont =
        get_container(ssize_t_bintree_node_t, node, node);
    free(cont);
}

static void insert_node(bintree_root_t *t,
                        const ssize_t newvalue)
{
    bintree_node_t **cur = &(t->node), *parent = NULL;
    ssize_t_bintree_node_t *newnode = (ssize_t_bintree_node_t *)
        malloc(sizeof(ssize_t_bintree_node_t));

    newnode->value = newvalue;

    while (*cur != NULL) {
        const ssize_t value =
            get_container(ssize_t_bintree_node_t, node, *cur)->value;

        parent = *cur;
        if (newvalue > value)
            cur = &((*cur)->right);
        else if (newvalue < value)
            cur = &((*cur)->left);
        else {
            free(newnode);
            return;  /* don't allow duplicates */
        }
    }

    bintree_attach(cur, parent, &newnode->node);
    bintree_balance(t, &newnode->node);
}

static void insert_values(bintree_root_t *t, size_t n, size_t range)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        insert_node(t, get_rand(range));
        ASSERT_ZERO(__bintree_validate(t, ssize_t_less_then_comparator));
    }
}

static bintree_node_t *find_node(bintree_root_t *t, ssize_t findvalue)
{
    bintree_node_t *cur = t->node;

    while (cur != NULL) {
        const ssize_t value =
            get_container(ssize_t_bintree_node_t, node, cur)->value;

        if (findvalue > value)
            cur = cur->right;
        else if (findvalue < value)
            cur = cur->left;
        else
            return cur;
    }

    return NULL;
}

static void try_remove_node(bintree_root_t *t, size_t range)
{
    const ssize_t value = (ssize_t)get_rand(range);
    bintree_node_t *node = find_node(t, value);
    if (node != NULL) {
        bintree_remove(t, node);
        ASSERT_ZERO(__bintree_validate(t, ssize_t_less_then_comparator));
        ssize_t_free_func(node);
    }
}

static int test_bintree(int argc, char **argv)
{
    bintree_root_t t;
    int i;

    srand((unsigned)time(NULL));
#if 0

    for (i = 0; i < 1000; ++i) {
        bintree_init(&t);
        ASSERT_ZERO(__bintree_validate(&t, ssize_t_less_then_comparator));
        insert_values(&t, 1000, 1000000000);
        if (i % 100 == 0)
            printf("i=%d size=%u\n", i, (unsigned)bintree_size(&t));
        ASSERT_ZERO(__bintree_validate(&t, ssize_t_less_then_comparator));
        bintree_clear(&t, ssize_t_free_func);
    }
#else

    for (i = 0; i < 1000; ++i) {
        bintree_init(&t);
        insert_values(&t, 100, 1000);
        ASSERT_ZERO(__bintree_validate(&t, ssize_t_less_then_comparator));
        while (bintree_size(&t) > 0)
            try_remove_node(&t, 1000);
        ASSERT_ZERO(__bintree_validate(&t, ssize_t_less_then_comparator));
        bintree_clear(&t, ssize_t_free_func);
    }
#endif

    return 0;
}

/* TEST json */

static int seg_are_equal(seg_t *left, seg_t *right)
{
    if (left->size == right->size)
        return memcmp(left->start, right->start, left->size) == 0;
    return 0;
}

static void test_one_json_string_value(char * const instr,
                                       char * const outstr,
                                       const size_t max_bytes_per_parse,
                                       const size_t unconsumed_chars,
                                       const enum json_code_t last_expected_retval)
{
#define BUFSIZE 1024
    char buf[BUFSIZE];
    ro_seg_t inref, next_chunk;
    seg_t outref;
    json_value_t jval;
    const size_t instr_len = strlen(instr);
    enum json_code_t retval;
    enum json_value_t type;
    union json_value_union_t *result;

    assert(instr_len < BUFSIZE - 3);

    buf[0] = '"';
    memcpy(&buf[1], instr, instr_len + 1); /* "+ 1" is for '\0' */
    inref.start = buf;
    inref.size = instr_len + 1;
    if (last_expected_retval == JSON_READY) {
        buf[instr_len + 1] = '"';
        buf[instr_len + 2] = '\0';
        ++inref.size;
    }

    json_value_init(&jval);

    while (1) {
        size_t orig_chunk_size;
        ro_seg_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        retval = json_value_parse(&jval, &next_chunk);
        ro_seg_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (next_chunk.size > 0)
            break;  /* ran into double-quotes: done */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == last_expected_retval);

    json_value_result(&jval, &type, &result);
    ASSERT_NONZERO(type == JSON_STRING);

    json_string_result(&result->str, &outref);
    ASSERT_NONZERO(seg_are_equal(&outref,
                                 seg_set_static(&outref, outstr)));

    json_value_uninit(&jval);

    /* it turns out it is a bit complicated to check this... */
    /* ASSERT_INT((int)inref.size, (int)unconsumed_chars); */
    (void)unconsumed_chars;
#undef BUFSIZE
}

static void test_one_json_string(char * const instr,
                                 char * const outstr,
                                 const size_t max_bytes_per_parse,
                                 const size_t unconsumed_chars,
                                 const enum json_code_t last_expected_retval)
{
    json_string_t jstr;
    ro_seg_t inref, next_chunk;
    seg_t outref, result;
    enum json_code_t retval;

    json_string_init(&jstr);

    ro_seg_set_static(&inref, instr);
    assert(inref.size > 0);

    while (1) {
        size_t orig_chunk_size;
        ro_seg_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        retval = json_string_parse(&jstr, &next_chunk);
        ro_seg_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (next_chunk.size > 0)
            break;  /* ran into double-quotes: done */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == last_expected_retval);

    json_string_result(&jstr, &result);

    ASSERT_INT((int)inref.size, (int)unconsumed_chars);
    ASSERT_NONZERO(seg_are_equal(&result,
                                  seg_set_static(&outref, outstr)));

    json_string_uninit(&jstr);

    /* test string inside json_value_t */
    test_one_json_string_value(instr, outstr, max_bytes_per_parse,
        unconsumed_chars, last_expected_retval);
}

static void test_json_higher_value_sequences()
{
    static const size_t bytes_per_parse[] = {100, 1, 2, 3, 6};
    unsigned i;

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        {
            /* testing 2-byte utf-8 codes */
            /* DZhe-o-Lj */
            uint8_t result_str[] = {0xc7, 0x84, (uint8_t)'o', 0xc7, 0x89, 0x00};
            test_one_json_string("\\u01c4o\\u01c9",
                                 (char *)result_str,
                                 bytes_per_parse[i],
                                 0,
                                 JSON_READY);
        }
        {
            /* testing 3-byte utf-8 codes */
            /* 2048,2128 */
            uint8_t result_str[] = {0xe0, 0xa0, 0x80, 0xe0, 0xa1, 0x90, 0x00};
            test_one_json_string("\\u0800\\u0850",
                                 (char *)result_str,
                                 bytes_per_parse[i],
                                 0,
                                 JSON_READY);
        }
        {
            /* testing boundary values */
            uint8_t result_str[] = {0xdf, 0xbf,       /* 2047(0x7ff) */
                                    0xe0, 0xa0, 0x80, /* 2048(0x800) */
                                    0xef, 0xbf, 0xbf, /* 65535(0xffff) */
                                    0x7f,             /* 127(0x7f) */
                                    0xc2, 0x80,       /* 128(0x80) */
                                    0x01,             /* 1(0x01) */
                                    0x00};
            test_one_json_string("\\u07ff\\u0800\\uffff\\u007f\\u0080\\u0001",
                                 (char *)result_str,
                                 bytes_per_parse[i],
                                 0,
                                 JSON_READY);
        }
        {
            /* testing boundary values: same as above, but with hex in caps */
            uint8_t result_str[] = {0xdf, 0xbf,       /* 2047(0x7ff) */
                                    0xe0, 0xa0, 0x80, /* 2048(0x800) */
                                    0xef, 0xbf, 0xbf, /* 65535(0xffff) */
                                    0x7f,             /* 127(0x7f) */
                                    0xc2, 0x80,       /* 128(0x80) */
                                    0x01,             /* 1(0x01) */
                                    0x00};
            test_one_json_string("\\u07Ff\\u0800\\ufFFF\\u007F\\u0080\\u0001",
                                 (char *)result_str,
                                 bytes_per_parse[i],
                                 0,
                                 JSON_READY);
        }
    }
}

static void test_json_unicode_sequences()
{
    static const size_t bytes_per_parse[] = {100, 1, 2, 3, 6};
    unsigned i;

    test_one_json_string("\\u", "", 100, 0, JSON_NEED_MORE);
    test_one_json_string("\\u0", "", 100, 0, JSON_NEED_MORE);
    test_one_json_string("\\u00", "", 100, 0, JSON_NEED_MORE);
    test_one_json_string("\\u006", "", 100, 0, JSON_NEED_MORE);
    test_one_json_string("\\u0069", "i", 100, 0, JSON_READY);

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        test_one_json_string("\\u0069", "i", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("\\u0049\\u0067", "Ig", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("O\\u0049\\u0067", "OIg", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("O\\u0049\\u0067\"", "OIg", bytes_per_parse[i], 1, JSON_READY);
    }

    test_json_higher_value_sequences();
}

static int test_json_string(int argc, char **argv)
{
    static const size_t bytes_per_parse[] = {100, 1, 2, 3};
    unsigned i;

    test_one_json_string("\\", "", 1, 0, JSON_NEED_MORE);
    test_one_json_string("\"", "", 1, 1, JSON_READY);

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        test_one_json_string("igor", "igor", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("ig\\nor", "ig\nor", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("i\\rgor", "i\rgor", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("\\\\igor", "\\igor", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("igor\\\"", "igor\"", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("igo\\fr", "igo\fr", bytes_per_parse[i], 0, JSON_READY);
        test_one_json_string("ig\\tor\"", "ig\tor", bytes_per_parse[i], 1, JSON_READY);
        test_one_json_string("igor\\", "igor", bytes_per_parse[i], 0, JSON_NEED_MORE);
        test_one_json_string("\n\"", "\n", bytes_per_parse[i], 1, JSON_READY);
        test_one_json_string("\"X", "", bytes_per_parse[i], 2, JSON_READY);
        test_one_json_string("\\n\\n", "\n\n", bytes_per_parse[i], 0, JSON_READY);

        /* unsupported escape char */
        test_one_json_string("\\q", "", bytes_per_parse[i], 1, JSON_INPUT_ERROR);
        test_one_json_string("a\\q", "a", bytes_per_parse[i], 1, JSON_INPUT_ERROR);
    }

    test_json_unicode_sequences();

    return 0;
}

static int compare_char_and_seg(const char *ch, seg_t *seg)
{
    return strlen(ch) == seg->size &&
        (seg->size == 0 || strncmp(ch, seg->start, strlen(ch)) == 0);
}

static void test_one_json_number(char * const instr,
                                 const char * const outstr,
                                 const union json_number_union_t *out,
                                 const size_t max_bytes_per_parse,
                                 const size_t unconsumed_chars,
                                 const enum json_code_t last_expected_retval)
{
    json_number_t jnum;
    /* variables that end with _value pertain to testing "json value type" */
    ro_seg_t inref, next_chunk, next_chunk_value;
    enum json_code_t retval, retval_value;
    enum json_data_t type;
    union json_number_union_t result, result_value;
    json_value_t jval;

    json_number_init(&jnum);
    json_value_init(&jval);

    ro_seg_set_static(&inref, instr);
    assert(inref.size > 0);

    while (1) {
        size_t orig_chunk_size;

        ro_seg_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        next_chunk_value.start = next_chunk.start;
        next_chunk_value.size = next_chunk.size;

        retval = json_number_parse(&jnum, &next_chunk);
        retval_value = json_value_parse(&jval, &next_chunk_value);

        ro_seg_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (next_chunk.size > 0)
            break; /* ran into double-quotes: done */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == retval_value);
    ASSERT_NONZERO(next_chunk.size == next_chunk_value.size);

    if (retval == JSON_READY) {
        retval = json_number_result(&jnum, &type, &result);

        /* now, get the value from the "json value" type */
        {
            enum json_value_t value_type;
            union json_value_union_t *value_result;
            enum json_data_t value_data_type;

            json_value_result(&jval, &value_type, &value_result);
            ASSERT_NONZERO(value_type == JSON_NUMBER);

            retval_value = json_number_result(&value_result->num, &value_data_type, &result_value);
            ASSERT_NONZERO(retval == retval_value);
            ASSERT_NONZERO(type == value_data_type);
        }
    }

    ASSERT_NONZERO(retval == last_expected_retval);
    ASSERT_INT((int)inref.size, (int)unconsumed_chars);

    if (out != NULL) {
        if (type == JSON_INTEGER) {
            ASSERT_NONZERO(result.integer == out->integer);
            ASSERT_NONZERO(result_value.integer == out->integer);
        }
        else {
            ASSERT_NONZERO(result.floating == out->floating);
            ASSERT_NONZERO(result_value.floating == out->floating);
        }
    }

    if (outstr != NULL) {
        seg_t ref;
        json_number_as_str(&jnum, &ref);
        ASSERT_NONZERO(compare_char_and_seg(outstr, &ref));
    }

    json_number_uninit(&jnum);
    json_value_uninit(&jval);
}

static union json_number_union_t *int_value(union json_number_union_t *num,
                                            int64_t val)
{
    num->integer = val;
    return num;
}

static union json_number_union_t *flo_value(union json_number_union_t *num,
                                            double val)
{
    num->floating = val;
    return num;
}

static int test_json_number(int argc, char **argv)
{
#define BPP bytes_per_parse[i]
#define TEST_JSON_FLO(val, last_status) \
do { \
    test_one_json_number(#val, #val, flo_value(&result, val), BPP, 0, last_status); \
} while (0)
#define TEST_JSON_INT(val, last_status) \
do { \
    test_one_json_number(#val, #val, int_value(&result, val), BPP, 0, last_status); \
} while (0)
    union json_number_union_t result;
    static const size_t bytes_per_parse[] = {100, 1, 2, 3, 5};
    unsigned i;
    (void)argc; (void)argv;

    test_one_json_number("0", NULL, int_value(&result, 0), 1, 0, JSON_READY);

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        test_one_json_number("0.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("0x", NULL, int_value(&result, 0), BPP, 1, JSON_READY);
        TEST_JSON_FLO(0.3, JSON_READY);
        TEST_JSON_FLO(0.34, JSON_READY);
        TEST_JSON_FLO(0.345, JSON_READY);
        test_one_json_number("0.345  ", NULL, flo_value(&result, 0.345), BPP, 2, JSON_READY);
        test_one_json_number(".", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("x", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("0. ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);

        test_one_json_number("-", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("-0", NULL, int_value(&result, 0), BPP, 0, JSON_READY);
        test_one_json_number("-0-", NULL, int_value(&result, 0), BPP, 1, JSON_READY);

        test_one_json_number("-0.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        TEST_JSON_FLO(-0.3, JSON_READY);
        TEST_JSON_FLO(-0.34, JSON_READY);
        TEST_JSON_FLO(-0.345, JSON_READY);
        test_one_json_number("-0.345 ", NULL, flo_value(&result, -0.345), BPP, 1, JSON_READY);
        test_one_json_number("-X", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("-0. ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);

        TEST_JSON_INT(-1, JSON_READY);
        test_one_json_number("-1234 ", NULL, int_value(&result, -1234), BPP, 1, JSON_READY);

        TEST_JSON_INT(1, JSON_READY);
        TEST_JSON_INT(10000000, JSON_READY);
        TEST_JSON_INT(-3100000, JSON_READY);
        test_one_json_number("1 ", NULL, int_value(&result, 1), BPP, 1, JSON_READY);
        test_one_json_number("1234 ", NULL, int_value(&result, 1234), BPP, 1, JSON_READY);

        test_one_json_number("1.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("1234.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        TEST_JSON_FLO(1.1, JSON_READY);
        TEST_JSON_FLO(1234.1, JSON_READY);
        test_one_json_number("1234. ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);

        test_one_json_number("-1.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("-12345.", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        TEST_JSON_FLO(-1.1, JSON_READY);
        TEST_JSON_FLO(-12345.6789, JSON_READY);
        TEST_JSON_FLO(-12345.678900, JSON_READY);
        test_one_json_number("-12. ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("-1234567. ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);

        test_one_json_number("0e", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("0e-", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("0E+", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("0e ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("0e- ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("0E+ ", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("0.E+ ", NULL, NULL, BPP, 3, JSON_INPUT_ERROR);
        test_one_json_number("1.E+ ", NULL, NULL, BPP, 3, JSON_INPUT_ERROR);

        test_one_json_number("1e", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("12e-", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("123E+", NULL, NULL, BPP, 0, JSON_NEED_MORE);

        test_one_json_number("0e0", "0e0", flo_value(&result, 0.0), BPP, 0, JSON_READY);
        test_one_json_number("0e1", NULL, flo_value(&result, 0.0), BPP, 0, JSON_READY);
        test_one_json_number("0e2", NULL, flo_value(&result, 0.0), BPP, 0, JSON_READY);
        test_one_json_number("0e2 ", NULL, flo_value(&result, 0.0), BPP, 1, JSON_READY);
        test_one_json_number("0e12", NULL, flo_value(&result, 0.0), BPP, 0, JSON_READY);
        test_one_json_number("0e123  ", "0e123", flo_value(&result, 0.0), BPP, 2, JSON_READY);

        test_one_json_number("1e0", "1e0", flo_value(&result, 1.0), BPP, 0, JSON_READY);
        test_one_json_number("1e0 ", NULL, flo_value(&result, 1.0), BPP, 1, JSON_READY);
        test_one_json_number("1e+0", NULL, flo_value(&result, 1.0), BPP, 0, JSON_READY);
        test_one_json_number("1e+0  ", "1e+0", flo_value(&result, 1.0), BPP, 2, JSON_READY);

        test_one_json_number("1e1", NULL, flo_value(&result, 10.0), BPP, 0, JSON_READY);
        test_one_json_number("1e1 ", NULL, flo_value(&result, 10.0), BPP, 1, JSON_READY);
        test_one_json_number("1e+", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("1e+.", NULL, NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("1e+2", NULL, flo_value(&result, 100.0), BPP, 0, JSON_READY);
        test_one_json_number("10e+2 ", NULL, flo_value(&result, 1000.0), BPP, 1, JSON_READY);
        test_one_json_number("10e+2.", "10e+2", flo_value(&result, 1000.0), BPP, 1, JSON_READY);
        test_one_json_number("2e-", NULL, NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("2e-3", NULL, flo_value(&result, 0.002), BPP, 0, JSON_READY);
        test_one_json_number("2.3e+2", NULL, flo_value(&result, 230), BPP, 0, JSON_READY);
        test_one_json_number("2.31e+2", NULL, flo_value(&result, 231), BPP, 0, JSON_READY);
        test_one_json_number("2.30e-2 ", NULL, flo_value(&result, 0.023), BPP, 1, JSON_READY);
        test_one_json_number("1e+10", NULL, flo_value(&result, 10000000000.0), BPP, 0, JSON_READY);
        test_one_json_number("1e10 ", NULL, flo_value(&result, 10000000000.0), BPP, 1, JSON_READY);
        test_one_json_number("-2e-10", NULL, flo_value(&result, -0.0000000002), BPP, 0, JSON_READY);
        test_one_json_number("10.5e+2 ", "10.5e+2", flo_value(&result, 1050.0), BPP, 1, JSON_READY);
        test_one_json_number("1.0e2", NULL, flo_value(&result, 100.0), BPP, 0, JSON_READY);

        /* should error out on extreme values */
        test_one_json_number("1.0e1000000", "1.0e1000000", NULL, BPP, 0, JSON_INPUT_ERROR);
        test_one_json_number("-1.0e-1000000", "-1.0e-1000000", NULL, BPP, 0, JSON_INPUT_ERROR);
        test_one_json_number("100000000000000000000000000000000000000", "100000000000000000000000000000000000000", NULL, BPP, 0, JSON_INPUT_ERROR);
        test_one_json_number("-99999999999999999999999999999999999999", "-99999999999999999999999999999999999999", NULL, BPP, 0, JSON_INPUT_ERROR);

        /* check integer boundary values */
        test_one_json_number("9223372036854775807", "9223372036854775807", int_value(&result, LLONG_MAX), BPP, 0, JSON_READY);
        test_one_json_number("-9223372036854775808", "-9223372036854775808", int_value(&result, LLONG_MIN), BPP, 0, JSON_READY);
        test_one_json_number("9223372036854775808", "9223372036854775808", NULL, BPP, 0, JSON_INPUT_ERROR);
        test_one_json_number("-9223372036854775809", "-9223372036854775809", NULL, BPP, 0, JSON_INPUT_ERROR);
        test_one_json_number("92233720368547758070", "92233720368547758070", NULL, BPP, 0, JSON_INPUT_ERROR);
    }

    return 0;
#undef TEST_JSON_INT
#undef TEST_JSON_FLO
#undef BPP
}

static void test_one_json_value(char *instr,
                                const char *outstr,
                                union json_number_union_t *out,
                                size_t max_bytes_per_parse,
                                size_t unconsumed_chars,
                                enum json_code_t last_expected_retval)
{
}

/* tests "true", "false" and "null" value literals */
static void test_one_json_literal_value(char * const instr,
                                        const size_t max_bytes_per_parse,
                                        const size_t unconsumed_chars,
                                        const enum json_value_t expected_type,
                                        const enum json_code_t last_expected_retval)
{
    ro_seg_t inref, next_chunk;
    json_value_t jval;
    enum json_code_t retval;
    enum json_value_t type;

    (void)ro_seg_set_static(&inref, instr);

    json_value_init(&jval);

    while (1) {
        size_t orig_chunk_size;
        ro_seg_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        retval = json_value_parse(&jval, &next_chunk);
        ro_seg_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (retval == JSON_READY || retval == JSON_INPUT_ERROR)
            break; /* finished parsing the literal or error */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == last_expected_retval);

    if (retval == JSON_READY) {
        json_value_result(&jval, &type, NULL);
        ASSERT_NONZERO(type == expected_type);
    }

    json_value_uninit(&jval);

    ASSERT_INT((int)inref.size, (int)unconsumed_chars);
}

static int test_json_value(int argc, char **argv)
{
#define BPP bytes_per_parse[i]
    static const size_t bytes_per_parse[] = { 100, 1, 2, 3, 4, 5 };
    unsigned i;

    /* test general json value error */
    test_one_json_literal_value("X", 100, 1, JSON_NO_VALUE, JSON_INPUT_ERROR);

    test_one_json_string_value("", "", 1, 0, JSON_READY);
    test_one_json_string_value("", "", 1, 0, JSON_READY);

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        test_one_json_string_value("igor", "igor", BPP, 0, JSON_READY);
        test_one_json_string_value("\\", "", BPP, 0, JSON_NEED_MORE);

        /* test literals */
        test_one_json_literal_value("t", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("tr", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("tru", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("true", BPP, 0, JSON_TRUE, JSON_READY);
        test_one_json_literal_value("true ", BPP, 1, JSON_TRUE, JSON_READY);

        test_one_json_literal_value("f", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("fals", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("false", BPP, 0, JSON_FALSE, JSON_READY);

        test_one_json_literal_value("n", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("nul", BPP, 0, JSON_NO_VALUE, JSON_NEED_MORE);
        test_one_json_literal_value("nullXXX", BPP, 3, JSON_NULL, JSON_READY);

        /* test error */
        test_one_json_literal_value("tX", BPP, 1, JSON_NO_VALUE, JSON_INPUT_ERROR);
    }

    return 0;
#undef BPP
}

/****************************************************************************/

static struct {
    const char *cmd_name;
    int (* func)(int , char **);
    int min_args_needed;
    const char *args_description;
} argopts[] = {
    /* tests should take no cmdline arguments */
    {"test_line_reader", test_line_reader, 0, ""},
    {"test_bintree", test_bintree, 0, ""},
    {"test_json_string", test_json_string, 0, ""},
    {"test_json_number", test_json_number, 0, ""},
    {"test_json_value", test_json_value, 0, "" },

    /* test utilities */
    {"line_reader_speed_baseline", line_reader_speed_baseline, 1, "<input file>" },
    {"line_reader_speed", line_reader_speed, 1, "<input file>" },
};
static const unsigned argopts_size = ARRAY_SIZE(argopts);

static void usage()
{
    unsigned i;

    printf("USAGE:\n");
    printf("\nYou can use the following command line arguments:\n\n");
    for (i = 0; i < argopts_size; ++i)
        printf("  %s %s\n", argopts[i].cmd_name, argopts[i].args_description);
    printf("\nRunning unit tests:\n\n");
    printf("If you use \"test\" as command line argument, it will run all unit tests\n");
    printf("(that is, all of the above commands that begin with the string \"test\").\n");
    printf("If the first argument begins with the word \"test\", it will use that\n");
    printf("string as a prefix, causing it to run a subset of tests that have that\n");
    printf("common prefix.  For example, using \"test_json_\" will run all tests that\n");
    printf("begin with \"test_json_\".\n\n");

    exit(12);
}

int main(int argc, char **argv)
{
    int exit_code = 0;
    unsigned cmds_run = 0;
    unsigned tests_failed = 0;
    const int run_tests = argc > 1 ?
        strncmp(argv[1], "test", 4) == 0 : 0;

    if (argc > 1) {
        unsigned i;
        if (run_tests && argc > 2) {
            printf("WARN: Found %d trailing argument(s) which will be ignored.\n\n",
                argc - 2);
        }

        for (i = 0; i < argopts_size; ++i) {
            const char * const cmd = argopts[i].cmd_name;
            /* commands that begin with "test" are treated specially,
             * as unit tests */
            if (run_tests && strncmp(cmd, argv[1], strlen(argv[1])) == 0) {
                assert(argopts[i].min_args_needed == 0);
                printf("Running TEST %s...\n", cmd);
                int ret = argopts[i].func(argc - 2, argv + 2);
                if (ret) {
                    printf(" ERROR: TEST %s FAILED!  (code=%d)\n", cmd, ret);
                    exit_code = ret;  /* keep the latest error exit code */
                    ++tests_failed;
                }
                ++cmds_run;
            }
            else if (strcmp(argv[1], cmd) == 0) {
                assert(argc > 1);
                if (argc - 2 < argopts[i].min_args_needed) {
                    printf("ERROR: Wrong number of parameters for command %s.\n\n", cmd);
                    usage();
                }
                exit_code = argopts[i].func(argc - 2, argv + 2);
                ++cmds_run;
                break;
            }
        }

        if (cmds_run == 0) {
            /* couldn't match command with the specified cmdline: print usage */
            printf("ERROR: Command \"%s\" is invalid.\n\n", argv[1]);
            usage();
        }
    }
    else {
        usage();
    }

    if (exit_code == 0) {
        if (run_tests) {
            if (cmds_run > 1)
                printf("\nAll %u tests PASSED.\n", cmds_run);
            else
                printf("\nTest PASSED.\n");
        }
        else
            printf("\nDONE.\n");
    }
    else {
        if (run_tests) {
            if (cmds_run > 1) {
                printf("\n%u/%u test(s) FAILED!\n", tests_failed, cmds_run);
            }
            else {
                /* code already printed above */
                printf("\nTest FAILED!\n");
            }
        }
        else
            printf("\nERROR code=%d\n", exit_code);
    }

    return exit_code;
}
