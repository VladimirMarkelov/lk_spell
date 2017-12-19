#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <utf8proc.h>

#include "lk_common.h"
#include "lk_dict.h"
#include "lk_file.h"
#include "lk_utils.h"
#include "lk_tree.h"

/**
 * @struct lk_word
 * Keeps an information about one word form, used by lk_dictionary
 */
struct lk_word {
    struct lk_word *base; /*!< pointer to base word - used for extra word forms */
    struct lk_word *next; /*!< pointer to next word in the dictionary */

    char *word; /*!< the word form */
};

/**
 * @struct lk_dictionary
 * The storage for all word forms
 */
struct lk_dictionary {
    struct lk_word *head; /*!< the first dictionary word */
    struct lk_word *tail; /*!< the last dictionary word, used by add-word
                            feature for best performance */
    struct lk_tree *tree; /*!< suffix tree for quick lookup */
};

/**
 * @struct lk_word_form
 * A suggestion for a incorrect word. Used internally by dictionary lookup
 *  to avoid extra malloc calls
 */
struct lk_word_form {
    char form[LK_MAX_WORD_LEN];
    struct lk_word_form *next;
};

/**
 * Lookup the word in a dictionary and returns th elist of all possible words
 *  that can replace the original one if it is incorrect
 *
 *  @return NULL if no suggestion was found
 */
const struct lk_word_ptr* lk_dict_find_word(const struct lk_dictionary *dict, const char *word) {
    if (!lk_is_dict_valid(dict) || word == NULL)
        return NULL;

    static char low_word[LK_MAX_WORD_LEN];
    lk_result r = lk_to_low_case(word, low_word, LK_MAX_WORD_LEN);
    if (r != LK_OK)
        return NULL;

    return lk_tree_search(dict->tree, low_word);
}

static int lk_suggestions_no(const struct lk_word_ptr *words, const char *word) {
    int total = 0;

    while (words) {
        total++;
        if (strcmp(words->word->word, word) == 0) {
            total = 0;
            break;
        }
        words = words->next;
    }

    return total;
}

static lk_result lk_add_to_suggestions(char **suggestions, size_t idx, const char *word) {
    /* skip duplicates */
    int already_used = 0;
    for (size_t chk = 0; chk < idx; ++chk) {
        if (strcmp(suggestions[chk], word) == 0) {
            already_used = 1;
            break;
        }
    }
    if (already_used)
        return LK_EXACT_MATCH;

    suggestions[idx] = (char*)calloc(strlen(word) + 1, sizeof(char));
    if (suggestions[idx] == NULL)
        return LK_OUT_OF_MEMORY;

    strcpy(suggestions[idx], word);
    return LK_OK;
}

static void lk_free_suggestions(char **suggestions) {
    if (suggestions == NULL)
        return;

    char *f = *suggestions;
    while (f) {
        free(f);
        f++;
    }

    free(suggestions);
}

/**
 * Returns a list of words that may be a valid form of the original one. Do not
 *  free the list manually, use lk_exact_lookup_free.
 *
 * @param[in] dict is initialized dictionary to lookup
 * @param[in] word is the word to check whether it has correct spelling
 * @param[out] count is the total number of suggestions (it includes separators,
 *  so be careful when showing the list of suggestions to a user. If count is
 *  NULL then the function does nothing and returns immediately with NULL result
 *
 * @returns a list of suggestions or NULL
 *   NULL is returned in two cases:
 *    if count is zero it means that the spelling is correct and the word
 *     was found in dictionry
 *    if count is negative then an error occured while looking up for the
 *     word and count contains the negated lk_result (mostly it returns
 *     -LK_OUT_OF_MEMORY or -LK_INVALID_ARG)
 *  Some pointer:
 *   count contains the number of suggestions
 *
 * @sa lk_exact_lookup_free
 */
char** lk_dict_exact_lookup(const struct lk_dictionary *dict, const char *word, int *count) {
    char** suggestions = NULL;
    static char unstressed[LK_MAX_WORD_LEN];

    if (count == NULL)
        return NULL;

    if (word == NULL || !lk_is_dict_valid(dict)) {
        *count = -LK_INVALID_ARG;
        return NULL;
    }

    /* lookup the main word */
    /* if not found - return NULL & count = -LK_WORD_NOT_FOUND */
    const struct lk_word_ptr *match = lk_dict_find_word(dict, word);
    if (match == NULL && lk_stressed_vowels_no(word) > 0) {
        /* process invalid word stressing */
        lk_result r = lk_destress(word, unstressed, LK_MAX_WORD_LEN);
        if (r != LK_OK) {
            *count = -LK_INVALID_STRING;
            return NULL;
        }

        /* invalid stress detected - used unstressed word instead of user's one */
        match = lk_dict_find_word(dict, unstressed);
        if (match != NULL)
            word = unstressed;
    }

    if (match == NULL) {
        *count = -LK_WORD_NOT_FOUND;
        return NULL;
    }

    int total = lk_suggestions_no(match, word);
    if (total == 0) {
        *count = 0;
        return NULL;
    }
    int skip_match = total == 0;

    struct lk_word_form *w_list = NULL;

    /* add extra for NULL tail */
    total++;
    /* generate and return all the list */
    suggestions = (char**)calloc(total, sizeof(char*));
    if (suggestions == NULL) {
        *count = -LK_OUT_OF_MEMORY;
        return NULL;
    }

    size_t idx = 0;
    lk_result final = LK_OK;
    if (!skip_match) {
        const struct lk_word_ptr *cw = match;
        while (final == LK_OK && cw) {
            lk_result res = lk_add_to_suggestions(suggestions, idx, cw->word->word);
            if (res == LK_OUT_OF_MEMORY) {
                final = res;
            } else if (res == LK_OK) {
                ++idx;
            }

            cw = cw->next;
        }
    }

    if (final != LK_OK) {
        lk_free_suggestions(suggestions);
        *count = -LK_OUT_OF_MEMORY;
        return NULL;
    }

    *count = idx;

    return suggestions;
}

/**
 * Frees the suggestion list returned by lk_dict_exact_lookup.
 * Function does nothing if lookup is NULL
 *
 * @sa lk_dict_exact_lookup
 */
void lk_exact_lookup_free(char** lookup) {
    if (lookup == NULL)
        return;

    char** item = lookup;
    while (*item != NULL) {
        free(*item);
        item++;
    }
    free(lookup);
}

static lk_result dict_add_word(struct lk_dictionary* dict, const struct lk_word *word) {
    if (!lk_is_dict_valid(dict) || word == NULL)
        return LK_INVALID_ARG;

    if (dict->head == NULL) {
        dict->head = (struct lk_word*)word;
        dict->tail = (struct lk_word*)word;
    } else {
        dict->tail->next = (struct lk_word*)word;
        dict->tail = (struct lk_word*)word;
    }
    return LK_OK;
}

static const char* skip_spaces(const char *s) {
    if (s == NULL)
        return s;
    while (*s == ' ')
        s++;
    return s;
}

static void free_word(struct lk_word* word) {
    if (word == NULL)
        return;
    if (word->word)
        free(word->word);
    free(word);
}

static lk_result add_without_stop(struct lk_tree *tree, const char *word, const struct lk_word *base) {
    static char gs[LK_MAX_WORD_LEN];

    lk_result res = lk_remove_glottal_stop(word, gs, LK_MAX_WORD_LEN);
    if (res == LK_OK)
        res = lk_tree_add_word(tree, gs, base);

    return res;
}

static lk_result generate_ascii_forms(struct lk_tree *tree, const char *word, const struct lk_word *base) {
    static char buf[LK_MAX_WORD_LEN], tmp[LK_MAX_WORD_LEN];

    int has_stop = lk_has_glottal_stop(word);
    int vcnt = lk_stressed_vowels_no(word);
    /* add low case form */
    lk_result res = lk_to_low_case(word, tmp, LK_MAX_WORD_LEN);
    if (res != LK_OK)
        return res;
    if (strcmp(tmp, word) != 0) {
        res = lk_tree_add_word(tree, tmp, base);
        if (res != LK_OK)
            return res;
    }
    if (has_stop) {
        res = add_without_stop(tree, tmp, base);
        if (res != LK_OK)
            return res;
    }
    if (lk_is_ascii(word) && !has_stop)
        return LK_OK;

    if (vcnt > 0) {
        /* add a form without any stressed vowel */
        res = lk_destress(tmp, buf, LK_MAX_WORD_LEN);
        if (res == LK_OK && strcmp(buf, tmp) != 0) {
            res = lk_tree_add_word(tree, buf, base);
            if (res == LK_OK && has_stop)
                res = add_without_stop(tree, buf, base);
        }

        if (res != LK_OK)
            return res;
    } else {
        strcpy(buf, tmp);
    }

    lk_to_ascii(buf, tmp, LK_MAX_WORD_LEN);
    if (strcmp(buf, tmp) != 0) {
        res = lk_tree_add_word(tree, tmp, base);
        if (res == LK_OK && has_stop)
            res = add_without_stop(tree, tmp, base);

        if (res != LK_OK)
            return res;
    }

    char *aph = strchr(tmp, '\'');
    if (aph == NULL)
        return LK_OK;

    /* replace aphostrophes with tildes */
    while (aph != NULL) {
        *aph = '`';
        aph = strchr(aph, '\'');
    }

    return lk_tree_add_word(tree, tmp, base);
}

static lk_result add_word_ascii_forms(struct lk_tree *tree, const struct lk_word *base) {
    /* add the word */
    const char *base_word = base->word;
    lk_result res = lk_tree_add_word(tree, base_word, base);
    if (res == LK_OK)
        res = generate_ascii_forms(tree, base_word, base);

    return res;
}

static lk_result add_word_to_tree(struct lk_dictionary *dict, const struct lk_word *base) {
    return add_word_ascii_forms(dict->tree, base);
}

static lk_result add_all_forms_to_dict(struct lk_dictionary *dict, struct lk_word *word) {
    lk_result res = lk_tree_add_word(dict->tree, word->word, word);
    if (res == LK_OK)
        res = add_word_to_tree(dict, word);

    return res;
}

static struct lk_word* lk_add_form_as_is(struct lk_dictionary *dict, const char *word,
       struct lk_word *base) {
    struct lk_word *out = (struct lk_word*)calloc(1, sizeof(struct lk_word));
    if (out == NULL)
        return NULL;
    out->word = (char *)calloc(1, strlen(word) + 1);
    if (out->word == NULL) {
        free(out);
        return NULL;
    }
    strcpy(out->word, word);
    out->base = base;
    dict_add_word(dict, out);

    lk_result res = add_all_forms_to_dict(dict, out);
    if (res != LK_OK)
        return NULL;

    return out;
}

static lk_result lk_iterate_forms(struct lk_dictionary *dict,
        char *start, struct lk_word *base) {
    const char *spc = skip_spaces(start);
    static char buf[LK_MAX_WORD_LEN];
    static char base_unstressed[LK_MAX_WORD_LEN];

    lk_result res = lk_destress(base->word, base_unstressed, LK_MAX_WORD_LEN);
    if (res != LK_OK)
        return res;

    while (start && *spc) {
        start = strchr(spc, ' ');
        size_t len = (start == NULL)? strlen(spc) : start - spc;
        if (len == 0) {
            spc = skip_spaces(start);
            continue;
        }

        if (len >= LK_MAX_WORD_LEN)
            return LK_BUFFER_SMALL;

        strncpy(buf, spc, len);
        buf[len] = '\0';

        struct lk_word *out = lk_add_form_as_is(dict, buf, base);
        if (out == NULL)
            return LK_OUT_OF_MEMORY;

        spc = skip_spaces(start);
    }

    return LK_OK;
}

static lk_result lk_read_base_form(struct lk_dictionary *dict, const char *s,
        struct lk_word *base) {
    char *start = strchr(s, ' ');
    size_t len = (start == NULL)? strlen(s) : start - s;
    if (len == 0) {
        free(base);
        return LK_INVALID_STRING;
    }
    base->word = (char*)calloc(len + 1, sizeof(char));
    if (base->word == NULL) {
        free(base);
        return LK_OUT_OF_MEMORY;
    }
    strncpy(base->word, s, len);

    lk_result res = dict_add_word(dict, base);
    if (res == LK_OK)
        res = add_all_forms_to_dict(dict, base);

    return res;
}

/**
 * Parses the string and if it correct word arcticle adds the word information
 *  to the dictionary.
 *
 * @param[in] dict must be initialized before calling the function. Otherwize
 *  the function returns LK_INVALID_ARG
 * @param[in] info a word article. Please see article format below
 *
 * @return the result of adding word to dictionary:
 *  LK_OK - the word was successfully added to the dictionary
 *  LK_INVALID_ARG - dictionary is not initialized or info is NULL
 *  LK_OUT_OF_MEMORY - failed to allocate memory
 *  LK_INVALID_STRING - info is not UTF8-encoded string or string does not
 *   have correct format
 *  LK_COMMENT - the string is a comment line, so it was skipped and nothing
 *   was added to the dictionary
 *
 * The arcticle format:
 *  if the string starts with '#' it means the line is a comment and should be skipped
 *  The line starts with the base word form
 *  Then a list of extra word forms may follow separated with spaces
 *
 * @sa lk_word_type
 * @sa lk_read_dictionary
 * @sa lk_dict_close
 * @sa lk_dict_init
 */
lk_result lk_parse_word(const char *info, struct lk_dictionary* dict) {
    if (!lk_is_dict_valid(dict) || info == NULL)
        return LK_INVALID_ARG;

    if (*info == '#')
        return LK_COMMENT;

    struct lk_word *base = (struct lk_word*)calloc(1, sizeof(struct lk_word));
    if (base == NULL)
        return LK_OUT_OF_MEMORY;

    lk_result res;
    const char *spc = info;

    /* read the word base form */
    res = lk_read_base_form(dict, spc, base);
    if (res != LK_OK)
        return res;

    char *start = strchr(spc, ' ');
    if (start == NULL)
        return LK_OK;

    return lk_iterate_forms(dict, start, base);
}

/**
 * Simple check if the dictionary was initialized
 */
int lk_is_dict_valid(const struct lk_dictionary *dict) {
    if (dict == NULL)
        return 0;
    if ((dict->head != NULL && dict->tail == NULL) ||
        (dict->head == NULL && dict->tail != NULL)) {
        return 0;
    }
    return 1;
}

/**
 * @return the number of words in the dictionary including all word forms. So,
 *  the number can be greater than the number of articles in loaded file
 */
size_t lk_word_count(const struct lk_dictionary *dict) {
    if (!lk_is_dict_valid(dict))
        return 0;

    size_t cnt = 0;
    struct lk_word *word = dict->head;
    while (word != NULL) {
        cnt++;
        word = word->next;
    }

    return cnt;
}

/**
 * Allocates resources for the dictionary and initializes all its internal structures.
 * DO NOT free the pointer manually to avoid memory leaks - use lk_dict_close
 *
 * @return pointer to initialized dictionary or NULL is something went wrong
 *
 * @sa lk_dict_close
 */
struct lk_dictionary* lk_dict_init() {
    struct lk_dictionary *dict = (struct lk_dictionary *)calloc(1, sizeof(*dict));
    if (dict == NULL)
        return NULL;

    dict->tree = lk_tree_init();
    if (dict->tree == NULL) {
        free(dict);
        return NULL;
    }

    return dict;
}

/**
 * Frees all resources allocated for the dictionary. If dict is NULL the
 *  function does nothing
 */
void lk_dict_close(struct lk_dictionary* dict) {
    if (dict == NULL)
        return;

    if (dict->tree != NULL)
        lk_tree_free(dict->tree);

    struct lk_word *wr = dict->head, *wr_next;
    while (wr != NULL) {
        wr_next = wr->next;
        free_word(wr);
        wr = wr_next;
    }

    free(dict);
}

/**
 * Reads dictionary from a text UTF8-encoded without BOM file. If path is NULL
 *  then the function tries to read filename to open from environment
 *  variable LK_DICTIONARY.
 *  If you do not have a file and want to populate the dictionary manually use
 *   the function lk_parse_word to add a word one by one. Please see the
 *   required format in lk_parse_word description.
 *
 * @param[in] dict must be initialized before calling the function
 *
 * @return the result of operation:
 *  LK_OK - file content was processed successfully
 *  LK_INVALID_FILE - failed to open file
 *
 * @sa lk_dict_init
 * @sa lk_dict_close
 * @sa lk_parse_word
 */
lk_result lk_read_dictionary(struct lk_dictionary *dict, const char *path) {
    char buf[4096];
    struct lk_file *file = lk_file_open(path);
    if (file == NULL)
        return LK_INVALID_FILE;

    lk_result res = LK_OK, file_res = LK_OK;
    while (file_res == LK_OK) {
        file_res = lk_file_read(file, buf, 4096);
        if (file_res == LK_EOF)
            break;

        if (file_res != LK_OK) {
            res = file_res;
            break;
        }

        res = lk_parse_word(buf, dict);
        if (res != LK_OK)
            break;
    }

    lk_file_close(file);

    return res;
}

