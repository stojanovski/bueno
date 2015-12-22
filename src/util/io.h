#ifndef IO_H
#define IO_H

struct strrdr_t
{
    int (* open)(void *);
    int (* read)(void *, char **);
    void (* get_error)(void *, const char **, int *);
    void *data;
};

int strrdr_open(struct strrdr_t *reader);
int strrdr_read(struct strrdr_t *reader, char **buf);
void strrdr_get_error(struct strrdr_t *reader, const char **error, int *errnum);

void po_file_reader_init(struct strrdr_t *reader, const char *path);
void po_file_reader_uninit(struct strrdr_t *reader);

void po_line_reader_init(struct strrdr_t *reader, struct strrdr_t *src_reader);
void po_line_reader_uninit(struct strrdr_t *reader);

#endif  /* IO_H */
