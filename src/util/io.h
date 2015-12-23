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
