#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#include "lk_common.h"
#include "lk_file.h"

#define LK_BUFFER_SIZE 65536

/**
 * @struct lk_file
 * File reader struct for text files: a file of lines of text separated with
 *  line feed and carriage returns
 */
struct lk_file {
    FILE* fh;/*!< opened file handle */
    char *buffer;/*!< local buffer to read files */
    size_t len;/*!< current length of the buffer */
    size_t cap;/*!< current buffer capacity */
    size_t pos;/*!< position in the buffer */
};

/**
 * Opens a file and returns a pointer to file reader structure on success.
 * File is always opened for reading in binary mode.
 *
 * @param[in] path is a path to file to read. File name is always UTF8-string
 *
 * @return a pointer to allocated structure on success. It must be freed by a
 *  caller. If opening file fails then NULL is returned
 *
 * @sa lk_file_close
 */
struct lk_file* lk_file_open(const char* path) {
    struct lk_file *f = (struct lk_file*)malloc(sizeof(*f));
    if (!f)
        return NULL;

    f->buffer = (char *)malloc(sizeof(char) * LK_BUFFER_SIZE);
    if (!f->buffer) {
        free(f);
        return NULL;
    }

    f->fh = 0;
    f->pos = 0;
    f->cap = LK_BUFFER_SIZE;
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

/**
 * Frees all resources allocated for a file reader. If file is NULL the function
 *  does nothing.
 *
 * @param[in] file is a pointer to a file reader
 *
 * @see lk_file_open
 */
void lk_file_close(struct lk_file *file) {
    if (! file)
        return;

    if (file->fh)
        fclose(file->fh);

    if (file->cap)
        free(file->buffer);

    free(file);
}

/**
 * Checks if the pointer to a file reader is valid. It does not do complex
 *  checks: the file reader is valid if it is not NULL, the file is opened,
 *  and the buffer for reading is allocated
 *
 * @param[in] file is a pointer to a file reader
 *
 * @return 0 if file reader is invalid and non-zero is it looks good
 *
 * @see lk_file_open
 */
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

/**
 * Reads the next string from the file. It is OK to read beyond the end of file.
 *  in this case the function returns LK_EOF and the buffer has zero length.
 *
 * @param[in] file is a pointer to a file reader
 * @param[in] buffer is a destination filled with the next string content.
 * @param[in] buf_size is a buffer capacity
 *
 * @return the result of read operation:
 *  LK_OK - the next line was successfully read and the buffer is filled with
 *   the string content with trailing zero character. It is safe to use the
 *   buffer as C string. All CR and LF characters are not put to the buffer
 *  LK_EOF - the end of file is reached. Anyway, buffer can contain the last
 *   line of the file if the file does not end with CR/LF, so be sure that you
 *   checked buffer length after getting LK_EOF.
 *   Trying to read file after getting LK_EOF keeps returning LK_EOF with empty
 *   buffer (strlen(buffer) == 0).
 *  LK_INVALID_FILE - file was not opened or file is NULL
 *  LK_BUFFER_SMALL - the current line is longer than buf_size, so only the
 *   first buf_size characters were read. To get the whole line call lk_file_read
 *   in a loop until it returns LK_OK or LK_EOF, then concatenate all parts in
 *   one long string.
 *   NOTE: do not use standard string C functions on a buffer if you get this
 *    result because in this case the buffer does NOT end with zero character.
 *
 * @see lk_file_open
 */
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

