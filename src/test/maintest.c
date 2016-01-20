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

static int test_line_reader_old(int argc, char **argv)
{
    struct strrdr_t fr, lr;
    char *buf;
    int ret;
    struct char_buffer_t cb;

    char_buffer_init(&cb);
    /* po_file_reader_init(&fr, "c:\\temp\\file.txt"); */
    po_file_reader_init(&fr, "C:\\Users\\igor\\Downloads\\SPL-88078_notes.txt");
    po_line_reader_init(&lr, &fr);
    if ((ret = strrdr_open(&lr)) < 0) {
        const char *error;
        int errnum;
        strrdr_get_error(&lr, &error, &errnum);
        printf("Got errnum=%d error=\"%s\"\n", errnum, error);
        goto done;
    }

    while ((ret = strrdr_read(&lr, &buf)) > 0) {
        char_buffer_set(&cb, buf, ret);
        /* printf("LINE (len=%4d): \"%s\"\n", ret, cb.buf); */
    }
done:
    po_line_reader_uninit(&lr);
    po_file_reader_uninit(&fr);
    char_buffer_uninit(&cb);

    return 0;
}

static void test_line_reader_speed()
{
    DWORD start_tm = GetTickCount();
    DWORD end_tm;
    struct strrdr_t fr;
    char *buf;
    int ret;
    struct char_buffer_t cb;

    char_buffer_init(&cb);
    /* po_file_reader_init(&fr, "c:\\temp\\file.txt"); */
    /* po_file_reader_init(&fr, "C:\\Users\\igor\\Downloads\\SPL-88078_notes.txt"); */
    /* po_file_reader_init(&fr, "C:\\Users\\igor\\Downloads\\movies\\Parada.2011.DVDRip.XviD.avi"); */
    po_file_reader_init(&fr, "C:\\Users\\igor\\Downloads\\Downloads.zip");
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
        while ((ret = strrdr_read(&fr, &buf)) > 0) {
            char_buffer_set(&cb, buf, ret);
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
}

#define FILE_READER_BUFLEN 1024

static void test_line_reader_speed_baseline()
{
    DWORD start_tm = GetTickCount();
    DWORD end_tm;
    FILE *fp;

    /* fp = fopen("C:\\Users\\igor\\Downloads\\movies\\Parada.2011.DVDRip.XviD.avi", "rb"); */
    fp = fopen("C:\\Users\\igor\\Downloads\\Downloads.zip", "rb");
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
    char *buf;
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

    while ((ret = strrdr_read(&lr, &buf)) > 0) {
        ASSERT_EXP(!missing_newline);
        /* char_buffer_set(&cb, buf, ret); */
        computed_hash = util_djb2_hash(buf, ret, &computed_hash);
        /* printf("LINE (len=%4d): \"%s\"\n", ret, cb.buf); */
#if 0
        printf("LINE (len=%4d): \"", ret);
        fwrite(buf, 1, ret, stdout);
        printf("\"\n");
#endif
        if (buf[ret-1] != '\n') {
            /* handle case when file does not end with a newline */
            missing_newline = 1;
        }
        else
            ++lines;
    }

    ASSERT_EXP(lines == expected_num_of_lines);
    ASSERT_EXP(computed_hash == expected_hash);

    printf("Finished line reader test.  Data: size=%4u lines=%u\n",
        (unsigned)file_contents_size, (unsigned)lines);

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

static print_strref(strref_t *str)
{
    fwrite(str->start, 1, str->size, stdout);
}

static int strref_are_equal(strref_t *left, strref_t *right)
{
    if (left->size == right->size)
        return memcmp(left->start, right->start, left->size) == 0;
    return 0;
}

static void test_one_json_string(char *instr,
                                 char *outstr,
                                 size_t max_bytes_per_parse,
                                 size_t unconsumed_chars,
                                 enum json_code_t last_expected_retval)
{
    json_string_t jstr;
    strref_t inref = {0}, result = {0}, outref = {0}, next_chunk = {0};
    enum json_code_t retval;

    json_string_init(&jstr);

    strref_set_static(&inref, instr);
    assert(inref.size > 0);

    while (1) {
        size_t orig_chunk_size;
        strref_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        retval = json_string_parse(&jstr, &next_chunk);
        strref_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (next_chunk.size > 0)
            break;  /* ran into double-quotes: done */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == last_expected_retval);

    json_string_result(&jstr, &result);
    json_string_uninit(&jstr);

    ASSERT_INT((int)inref.size, (int)unconsumed_chars);
    ASSERT_NONZERO(strref_are_equal(&result,
                                    strref_set_static(&outref, outstr)));

    strref_uninit(&inref);
    strref_uninit(&result);
    strref_uninit(&outref);
    strref_uninit(&next_chunk);
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

static void test_one_json_number(char *instr,
                                 union json_number_union_t *outstr,
                                 size_t max_bytes_per_parse,
                                 size_t unconsumed_chars,
                                 enum json_code_t last_expected_retval)
{
    json_number_t jnum;
    strref_t inref = {0}, outref = {0}, next_chunk = {0};
    enum json_code_t retval;
    enum json_type_t type;
    union json_number_union_t result;

    json_number_init(&jnum);

    strref_set_static(&inref, instr);
    assert(inref.size > 0);

    while (1) {
        size_t orig_chunk_size;
        strref_assign(&next_chunk, &inref);
        if (next_chunk.size > max_bytes_per_parse)
            next_chunk.size = max_bytes_per_parse;
        orig_chunk_size = next_chunk.size;

        retval = json_number_parse(&jnum, &next_chunk);
        strref_trim_front(&inref, orig_chunk_size - next_chunk.size);
        if (next_chunk.size > 0)
            break;  /* ran into double-quotes: done */
        else if (inref.size == 0)
            break; /* exhausted the input */
    }

    ASSERT_NONZERO(retval == last_expected_retval);

    type = json_number_result(&jnum, &result);
    json_number_uninit(&jnum);

    ASSERT_INT((int)inref.size, (int)unconsumed_chars);

    if (outstr != NULL) {
        if (type == JSON_INTEGER)
            ASSERT_NONZERO(result.integer == outstr->integer);
        else
            ASSERT_NONZERO(result.floating == outstr->floating);
    }

    strref_uninit(&inref);
    strref_uninit(&outref);
    strref_uninit(&next_chunk);
}

static union json_number_union_t *int_value(union json_number_union_t *num,
                                            size_t val)
{
    num->integer = val;
    return num;
}

static union json_number_union_t *flo_value(union json_number_union_t *num,
                                            json_double_t val)
{
    num->floating = val;
    return num;
}

static int test_json_number(int argc, char **argv)
{
#define TEST_JSON_FLO(val, last_status) \
do { \
    test_one_json_number(#val, flo_value(&result, val), 100, 0, last_status); \
} while (0)
#define TEST_JSON_INT(val, last_status) \
do { \
    test_one_json_number(#val, int_value(&result, val), bytes_per_parse[i], 0, last_status); \
} while (0)
#define BPP bytes_per_parse[i]
    union json_number_union_t result;
    static const size_t bytes_per_parse[] = {100, 1, 2, 3, 6};
    unsigned i;
    (void)argc; (void)argv;

    test_one_json_number("0", int_value(&result, 0), 1, 0, JSON_READY);

    for (i = 0; i < ARRAY_SIZE(bytes_per_parse); ++i)
    {
        test_one_json_number("0.", NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("0x", int_value(&result, 0), BPP, 1, JSON_READY);
        TEST_JSON_FLO(0.3, JSON_READY);
        TEST_JSON_FLO(0.34, JSON_READY);
        TEST_JSON_FLO(0.345, JSON_READY);
        test_one_json_number("0.345  ", flo_value(&result, 0.345), BPP, 2, JSON_READY);
        test_one_json_number(".", NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("x", NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("0. ", NULL, BPP, 1, JSON_INPUT_ERROR);

        test_one_json_number("-", NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("-0", int_value(&result, 0), BPP, 0, JSON_READY);
        test_one_json_number("-0-", int_value(&result, 0), BPP, 1, JSON_READY);

        test_one_json_number("-0.", NULL, BPP, 0, JSON_NEED_MORE);
        TEST_JSON_FLO(-0.3, JSON_READY);
        TEST_JSON_FLO(-0.34, JSON_READY);
        TEST_JSON_FLO(-0.345, JSON_READY);
        test_one_json_number("-0.345 ", flo_value(&result, -0.345), BPP, 1, JSON_READY);
        test_one_json_number("-X", NULL, BPP, 1, JSON_INPUT_ERROR);
        test_one_json_number("-0. ", NULL, BPP, 1, JSON_INPUT_ERROR);

        TEST_JSON_INT(-1, JSON_READY);
        test_one_json_number("-1234 ", int_value(&result, -1234), BPP, 1, JSON_READY);

        TEST_JSON_INT(1, JSON_READY);
        test_one_json_number("1 ", int_value(&result, 1), BPP, 1, JSON_READY);
        test_one_json_number("1234 ", int_value(&result, 1234), BPP, 1, JSON_READY);

        test_one_json_number("1.", NULL, BPP, 0, JSON_NEED_MORE);
        test_one_json_number("1234.", NULL, BPP, 0, JSON_NEED_MORE);
        TEST_JSON_FLO(1.1, JSON_READY);
        TEST_JSON_FLO(1234.1, JSON_READY);
        test_one_json_number("1234. ", NULL, BPP, 1, JSON_INPUT_ERROR);
    }

    return 0;
#undef TEST_JSON_INT
#undef TEST_JSON_FLO
#undef BPP
}

/****************************************************************************/

static struct {
    const char *opt_name;
    int (* func)(int , char **);
    int min_args_needed;
    const char *args_description;
} argopts[] = {
    {"test_line_reader", test_line_reader, 0, ""},
    {"test_bintree", test_bintree, 0, ""},
    {"test_json_string", test_json_string, 0, ""},
    {"test_json_number", test_json_number, 0, ""},
};
static const unsigned argopts_size = ARRAY_SIZE(argopts);

static void usage(char *argv0)
{
    unsigned i;

    printf("%s usage:\n\n", argv0);
    for (i = 0; i < argopts_size; ++i)
        printf("  %s %s\n\n", argopts[i].opt_name, argopts[i].args_description);

    exit(12);
}

int main(int argc, char **argv)
{
    int exit_code = 13;

    if (argc > 1) {
        unsigned i;
        for (i = 0; i < argopts_size; ++i) {
            if (strcmp(argv[1], argopts[i].opt_name) == 0) {
                if (argc - 2 < argopts[i].min_args_needed) {
                    printf("ERROR: Wrong number of parameters for option %s.\n\n",
                        argopts[i].opt_name);
                    usage(argv[0]);
                }
                exit_code = argopts[i].func(argc - 2, argv + 2);
                break;
            }
        }

        if (i == argopts_size) {
            /* couldn't file the specified option: print usage */
            printf("ERROR: Option \"%s\" is invalid.\n\n", argv[1]);
            usage(argv[0]);
        }
    }
    else {
        /* TODO: run all tests that don't require user params */
        usage(argv[0]);
    }

    if (exit_code == 0)
        printf("\nDONE.\n");
    else
        printf("\nERROR code=%d\n", exit_code);

    /* test_line_reader_speed(); */
    /* test_line_reader_speed_baseline(); */
    return exit_code;
}

/* 
func main() {
    var i = 23;
    ref j = 34;
    i = i + 22;
}
*/
