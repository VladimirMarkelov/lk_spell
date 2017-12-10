#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "common.h"
#include "filereader.h"

struct lk_file {
    FILE* fh;
    char *buffer;
    size_t len;
    size_t cap;
    size_t pos;
};

struct lk_file* lk_file_open(const char* path) {
    struct lk_file *f = (struct lk_file*)malloc(sizeof(*f));
    if (!f)
        return NULL;

    f->buffer = (char *)malloc(sizeof(char) * BUFFER_SIZE);
    if (!f->buffer) {
        free(f);
        return NULL;
    }

    f->fh = 0;
    f->pos = 0;
    f->cap = BUFFER_SIZE;
    f->len = 0;
#ifdef _WIN32
    wchar_t *wpath;
    int bufsz;

    if (path) {
        bufsz = MultiByteToWideChar(CP_UTF8, 0, path, -1, NULL, 0);
        wpath = (wchar_t*)malloc((bufsz + 1) * sizeof(wchar_t));
        if (wpath)
            MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, bufsz);
    } else {
        wpath = _wgetenv(L"LK_DICTIONARY");
    }

    if (wpath == NULL)
        return f;

    f->fh = _wfopen(wpath, L"rb");

    if (path)
        free(wpath);
#else
    const char *filepath = path;
    if (path == NULL)
        filepath = getenv("LK_DICTIONARY");
    if (filepath == NULL)
        return f;
    f->fh = fopen(filepath, "rb");
#endif

    return f;
}

void lk_file_close(struct lk_file *file) {
    if (! file)
        return;

    if (file->fh) {
        fclose(file->fh);
    }
    if (file->cap) {
        free(file->buffer);
    }
    free(file);
}

int lk_file_is_valid(const struct lk_file const *file) {
    return (file && file->fh && file->buffer && file->cap);
}

static lk_result lk_read_block(struct lk_file *file) {
    size_t readbytes = fread(file->buffer, sizeof(char), file->cap, file->fh);
    if (readbytes != file->cap && ferror(file->fh))
            return LK_FILE_READ_ERR;

    file->pos = 0;
    file->len = readbytes;
    return LK_OK;
}

lk_result lk_file_read(struct lk_file *file, char *buffer, size_t buf_size) {
    if (!lk_file_is_valid(file))
        return LK_INVALID_FILE;

    if (buffer == NULL || buf_size == 0)
        return LK_BUFFER_SMALL;

    if (file->len == 0 || file->pos >= file->len) {
        lk_result lk = lk_read_block(file);
        if (lk != LK_OK)
            return lk;
    }

    if (file->cap > file->len && file->pos >= file->len)
        return LK_EOF;

    char *b = buffer;
    while (buf_size > 0) {
        char c = file->buffer[file->pos];
        if (c == '\n' || c == '\r') {
            *b = '\0';
            break;
        }

        *b++ = c;
        buf_size--;
        file->pos++;
        if (buf_size == 0)
            return LK_BUFFER_SMALL;

        if (file->pos >= file->len) {
            lk_result lk = lk_read_block(file);
            if (lk != LK_OK)
                return lk;
            if (file->len == 0) {
                *b = '\0';
                break;
            }
        }
    }
    for (;;) {
        char c = file->buffer[file->pos];
        if (c != '\n' && c != '\r') {
            break;
        }

        file->pos++;
        if (file->pos >= file->len) {
            lk_result lk = lk_read_block(file);
            if (lk != LK_OK)
                return lk;
            if (file->len == 0)
                break;
        }
    }

    return LK_OK;
}

