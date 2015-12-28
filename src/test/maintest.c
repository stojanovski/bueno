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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#ifdef WIN32
#include <windows.h>

typedef SSIZE_T ssize_t;
#endif

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
        exit(19);
    }
}

#define ASSERT_MSG(expr, msg) test_assert((expr), (msg), __FILE__, __LINE__)
#define ASSERT_EXP(expr) test_assert((expr), #expr, __FILE__, __LINE__)

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

#define GETRAND() ((size_t)rand() % 1000)

static void insert_node(bintree_root_t *t,
                        ssize_t_bintree_node_t *node)
{
    bintree_node_t *succ = t->node;
    if (succ != NULL) {

        /* TODO */
    }
    bintree_insert(t, succ, &node->node);
}

static void insert_values(bintree_root_t *t, size_t n)
{
    size_t i;
    ASSERT_EXP(bintree_validate(t) == 0);
    for (i = 0; i < n; ++i) {
        ssize_t_bintree_node_t *node = (ssize_t_bintree_node_t *)
            malloc(sizeof(ssize_t_bintree_node_t));
        node->value = GETRAND();
        insert_node(t, node);
        ASSERT_EXP(bintree_validate(t) == 0);
    }
}

static int test_bintree(int argc, char **argv)
{
    bintree_root_t t;

    srand((unsigned)time(NULL));

    bintree_init(&t);
    ASSERT_EXP(bintree_validate(&t) == 0);

    insert_values(&t, 1);

    return 0;
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
};
static const unsigned argopts_size = sizeof(argopts) / sizeof(argopts[0]);

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
