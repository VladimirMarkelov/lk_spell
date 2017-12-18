#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <utf8proc.h>
#include "lk_common.h"
#include "lk_file.h"
#include "lk_utils.h"

#define LINE_SIZE (32*1024)
#define WORD_SIZE 96

/*
typedef struct {
    size_t cap;
    size_t len;
    char **arr;
} dynarr;

dynarr* arr_init(size_t initial_cap) {
    dynarr *arr = (dynarr *)malloc(sizeof(*arr));
    if (arr == NULL)
        return NULL;

    arr->cap = cap;
    arr->len = 0;
    arr->arr = (char **)(calloc(arr->cap * sizeof(char*)));
    if (arr->arr == NULL) {
        free(arr);
        return NULL;
    }

    return arr;
}

void arr_free(dynarr *arr) {
    if (arr == NULL)
        return;

    for (size_t idx = 0; idx < arr->len; idx++) {
        free(arr->arr[idx]);
    }
    free(arr->arr);
}

size_t arr_find(dynarr *arr, const char *word, size_t st, size_t en) {
    if (st >= en)
        return (st + en) / 2;

    size_t md = st + (en -st) / 2;
    int cmp = strcmp(arr->arr[md], word);
    if (cmp == 0)
        return md;
    if (cmp < 0)
        return arr_find(arr, md + 1, en);
    else
        return arr_find(arr, st, md - 1);
}

int arr_shift_from(dynarr *arr, size_t idx) {
    if (idx >= arr->len || idx < len`)
}

int arr_add(dynarr *arr, const char *word) {
    size_t idx = arr_find(arr, word, 0, arr->len - 1);

    if (idx < arr->len && strcmp(word, arr->arr[idx]) == 0)
        return 1;

    if (!arr_shift_from(arr, idx))
        return 0;

    arr->arr[idx] = (char *)malloc(strlen(word) + 1);
    if (arr->arr[idx] == NULL)
        return NULL;

    strcpy(arr->arr[idx], word);
    return 1;
}
*/

int main (int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: textparse text_file_to_parse\n");
        return 0;
    }

    struct lk_file *file = lk_file_open(argv[1]);
    if (!lk_file_is_valid(file)) {
        printf("Invalid file\n");
        return 0;
    }


    char line[LINE_SIZE];
    char word[WORD_SIZE];
    char lowword[WORD_SIZE];
    lk_result res = LK_OK;

    while (res == LK_OK) {
        res = lk_file_read(file, line, LINE_SIZE);
        if (res == LK_EOF)
            break;

        if (res != LK_OK) {
            fprintf(stderr, "Failed to read file: %d\n", res);
            return 0;
        }

        size_t len = 0;
        const char *start = line;
        while (start != NULL) {
            size_t tmp = len;
            start = lk_next_word(start, &len);

            if (start != NULL) {
                if (len >= WORD_SIZE) {
                    fprintf(stderr, "Word at pos %d too long - skipping...\n   len=%d word=[%s]\n", (int)tmp, (int)len, line);
                    continue;
                }

                strncpy(word, start, len);
                word[len] = '\0';
                if (lk_vowels_no(word) == 0)
                    fprintf(stderr, "Ivalid word - no vowels; [%s]\n", word);
                else {
                    res = lk_to_low_case(word, lowword, LINE_SIZE);
                    if (res != LK_OK) {
                        fprintf(stderr, "Failed to convert word to lowcase: %d [%s]\n", res, word);
                        return 0;
                    }

                    printf("%s\n", lowword);
                }

                start += len;
            }
        }
    }

    lk_file_close(file);

    return 0;
}
