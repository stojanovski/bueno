#include "io.h"
#include "str.h"
#include <stdio.h>
#include <windows.h>
#include <assert.h>

int strrdr_open(struct strrdr_t *reader)
{
    return reader->open(reader->data);
}

int strrdr_read(struct strrdr_t *reader, char **buf)
{
    return reader->read(reader->data, buf);
}

void strrdr_get_error(struct strrdr_t *reader, const char **error, int *errnum)
{
    reader->get_error(reader->data, error, errnum);
}

/* file reader */

#define FILE_READER_BUFLEN 1024

struct file_reader_t
{
    char *path;
    FILE *fp;
    int errnum;
    char *error;
    char buf[FILE_READER_BUFLEN];
};

static int file_reader_open(void *data)
{
    struct file_reader_t *fr = (struct file_reader_t *)data;
    fr->fp = fopen(fr->path, "rb");
    if (fr->fp == NULL) {
        fr->errnum = GetLastError();
        fr->error = "";
        return -1;
    }
    return 0;
}

/* could be overriden for unit test purposes */
size_t file_reader_read_buflen = FILE_READER_BUFLEN;

static int file_reader_read(void *data, char **buf)
{
    struct file_reader_t *fr = (struct file_reader_t *)data;
    int ret = (int)fread(fr->buf, 1, file_reader_read_buflen, fr->fp);
    if (ret > 0)
        *buf = fr->buf;
    return ret;
}

static void file_reader_get_error(void *data, const char **buf, int *errnum)
{
    struct file_reader_t *fr = (struct file_reader_t *)data;
    *buf = "Got error";
    *errnum = fr->errnum;
}

static char *dup_str(const char *src)
{
    size_t len = strlen(src);
    char *dup = (char *)malloc(len + 1);
    memcpy(dup, src, len + 1);
    return dup;
}

void po_file_reader_init(struct strrdr_t *reader, const char *path)
{
    struct file_reader_t *fr = (struct file_reader_t *)malloc(sizeof(struct file_reader_t));
    assert(reader != NULL);
    fr->path = dup_str(path);
    fr->fp = NULL;
    reader->data = fr;
    reader->open = file_reader_open;
    reader->read = file_reader_read;
    reader->get_error = file_reader_get_error;
}

void po_file_reader_uninit(struct strrdr_t *reader)
{
    struct file_reader_t *fr = (struct file_reader_t *)reader->data;
    if (fr->fp != NULL)
        fclose(fr->fp);
    free(fr);
}

/* line reader */

struct line_reader_t
{
    struct strrdr_t *src_reader;
    struct char_buffer_t cb;
    size_t bytes_returned;
    int at_eof;
};

static int line_reader_open(void *data)
{
    struct line_reader_t *ln = (struct line_reader_t *)data;
    return strrdr_open(ln->src_reader);
}

static int line_reader_drain_buffer(struct line_reader_t *ln, char **buf)
{
    const char *ptr, *end;
    strref_t str = {0};
    int ret = 0;
    char_buffer_get(&ln->cb, &str);
    ptr = str.start;
    end = str.start + str.size;

    while (ptr < end) {
        if (*ptr == '\n') {
            ++ptr;
            *buf = str.start;
            ln->bytes_returned = ptr - str.start;
            ret = 1;
            goto done;
        }
        ++ptr;
    }

done:
    strref_uninit(&str);
    return ret;
}

static int line_reader_read(void *data, char **buf)
{
    struct line_reader_t *ln = (struct line_reader_t *)data;
    strref_t str = {0};
    int ret;

    if (ln->at_eof)
        return 0;

    char_buffer_pop_front(&ln->cb, ln->bytes_returned);
    ln->bytes_returned = 0;

    if (line_reader_drain_buffer(ln, buf)) {
        ret = (int)ln->bytes_returned;
        goto done;
    }

    while (1) {
        char *src_buf;
        ret = strrdr_read(ln->src_reader, &src_buf);
        if (ret == 0) {
            /* EOF: return whatever is inside the buffer */
            ln->at_eof = 1;
            char_buffer_get(&ln->cb, &str);
            *buf = str.start;
            ret = (int)str.size;
            goto done;
        }
        else if (ret < 0) {
            goto done;  /* return ret */
        }

        char_buffer_append(&ln->cb, src_buf, ret);

        if (line_reader_drain_buffer(ln, buf)) {
            ret = (int)ln->bytes_returned;
            goto done;
        }
    }

done:
    strref_uninit(&str);
    return ret;
}

static void line_reader_get_error(void *data, const char **buf, int *errnum)
{
    struct line_reader_t *ln = (struct line_reader_t *)data;
    strrdr_get_error(ln->src_reader, buf, errnum);
}

void po_line_reader_init(struct strrdr_t *reader, struct strrdr_t *src_reader)
{
    struct line_reader_t *ln = (struct line_reader_t *)malloc(sizeof(struct line_reader_t));
    char_buffer_init(&ln->cb);
    ln->bytes_returned = 0;
    ln->src_reader = src_reader;
    ln->at_eof = 0;
    reader->data = ln;
    reader->open = line_reader_open;
    reader->read = line_reader_read;
    reader->get_error = line_reader_get_error;
}

void po_line_reader_uninit(struct strrdr_t *reader)
{
    struct line_reader_t *ln = (struct line_reader_t *)reader->data;
    char_buffer_uninit(&ln->cb);
}
