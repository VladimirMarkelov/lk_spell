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

struct lk_word {
    struct lk_word *base;
    struct lk_word *next;

    char *word;
    char *contracted;
    int wtype;
};
struct lk_ablauting {
    struct lk_ablauting *next;

    struct lk_word *word;
    lk_ablaut ablaut;
};

struct lk_dictionary {
    struct lk_word *head;
    struct lk_word *tail;
    struct lk_ablauting *ab_head;
    struct lk_ablauting *ab_tail;
    struct lk_tree *tree;
};

struct lk_word_form {
    char form[LK_MAX_WORD_LEN];
    struct lk_word_form *next;
};

static void lk_copy_word_base(const char *word, char *out) {
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;
        if (cp == 'A' || cp == LK_A_UP)
            break;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
    }
    *udst = '\0';
}

static void lk_append_ablaut(char *out, lk_ablaut ablaut, int is_ab_stressed) {
    utf8proc_int32_t cp;
    size_t len;

    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out + strlen(out);

    switch (ablaut) {
        case LK_ABLAUT_A:
            cp = is_ab_stressed ? LK_A_LOW : 'a';
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            break;
        case LK_ABLAUT_E:
            cp = is_ab_stressed ? LK_E_LOW : 'e';
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            break;
        case LK_ABLAUT_N:
            cp = is_ab_stressed ? LK_I_LOW : 'i';
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            cp = LK_N_LOW;
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            break;
    }

    *udst = '\0';
}

static void lk_generate_ablauted(const char *word, lk_ablaut ablaut, char *out) {
    int is_ab_stressed = lk_is_ablaut_stressed(word);
    lk_copy_word_base(word, out);
    lk_append_ablaut(out, ablaut, is_ab_stressed);
}

const struct lk_word_ptr* lk_dict_find_word(const struct lk_dictionary *dict, const char *word) {
    if (!lk_is_dict_valid(dict) || word == NULL)
        return NULL;

    static char low_word[LK_MAX_WORD_LEN];
    lk_result r = lk_to_low_case(word, low_word, LK_MAX_WORD_LEN);
    if (r != LK_OK)
        return NULL;

    return lk_tree_search(dict->tree, low_word);
}

static lk_ablaut lk_find_word_ablaut(const struct lk_dictionary *dict, const char *word) {
    lk_ablaut ab = LK_ABLAUT_0;
    if (word == NULL)
        return ab;

    if (*word == '\0' || strchr(".!?;", *word) != NULL) {
        ab = LK_ABLAUT_E;
    } else {
        const struct lk_word_ptr *next_match = lk_dict_find_word(dict, word);
        /* All words must follow the same ablaut. Otherwise it is considered
         * that the current word does not need any correction
         */
        while (next_match) {
            const struct lk_word *w = next_match->word;
            lk_ablaut a = lk_dict_check_ablaut(dict, w->word);
            if (ab != LK_ABLAUT_0 && a != ab) {
                ab = LK_ABLAUT_0;
                break;
            }
            ab = a;
        }
    }

    return ab;
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

static lk_result lk_add_correct_ablaut(struct lk_word_form **forms,
        const char *word, const struct lk_word_ptr *wlist,
        lk_ablaut ab, int *total) {
    if (ab == LK_ABLAUT_0)
        return LK_OK;

    const struct lk_word_ptr *cw = wlist;
    while (cw) {
        const struct lk_word *wr = cw->word;
        while (wr->base)
            wr = wr->base;
        char buf[LK_MAX_WORD_LEN] = {0};

        if (lk_has_ablaut(wr->word)) {
            switch (ab) {
                case LK_ABLAUT_A:
                    if (!lk_ends_with(word, "a") && !lk_ends_with(word, "á")) {
                        lk_generate_ablauted(wr->word, ab, buf);
                    }
                    break;
                case LK_ABLAUT_E:
                    if (!lk_ends_with(word, "e") && !lk_ends_with(word, "é")) {
                        lk_generate_ablauted(wr->word, ab, buf);
                    }
                    break;
                case LK_ABLAUT_N:
                    if (!lk_ends_with(word, "iŋ") && !lk_ends_with(word, "íŋ")) {
                        lk_generate_ablauted(wr->word, ab, buf);
                    }
                    break;
            }
        }

        if (*buf != '\0') {
            struct lk_word_form *frm = (struct lk_word_form*)calloc(1, sizeof(*frm));
            if (frm == NULL)
                return LK_OUT_OF_MEMORY;

            strcpy(frm->form, buf);
            if (*forms == NULL) {
                *forms = frm;
            } else {
                struct lk_word_form *tmp = *forms;
                while (tmp->next != NULL)
                    tmp = tmp->next;
                tmp->next = frm;
            }
            (*total)++;
        }

        cw = cw->next;
    }

    *total++;
    return LK_OK;
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

static void lk_free_correct_ablauts(struct lk_word_form *forms) {
    if (forms == NULL)
        return;

    struct lk_word_form *tmp = forms;
    while (tmp != NULL) {
        struct lk_word_form *n = tmp->next;
        free(tmp);
        tmp = n;
    }
}

char** lk_dict_exact_lookup(const struct lk_dictionary *dict, const char *word, const char *next_word, int *count) {
    char** suggestions = NULL;
    static char unstressed[LK_MAX_WORD_LEN];

    if (count == NULL)
        return NULL;

    if (word == NULL || !lk_is_dict_valid(dict)) {
        *count = -LK_INVALID_ARG;
        return NULL;
    }

    /* detect next_word ablaut type. If next_word == NULL then skip this step */
    lk_ablaut ab = lk_find_word_ablaut(dict, next_word);

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
    if (total == 0 && ab == LK_ABLAUT_0) {
        *count = 0;
        return NULL;
    }
    int skip_match = total == 0;

    struct lk_word_form *w_list = NULL;
    /* if ablaut then check if the form is correct */
    lk_result ab_added = lk_add_correct_ablaut(&w_list, word, match, ab, &total);
    if (ab_added != LK_OK) {
        lk_free_correct_ablauts(w_list);
        *count = -LK_OUT_OF_MEMORY;
        return NULL;
    }

    /* add extra for NULL tail */
    total++;
    /* generate and return all the list */
    suggestions = (char**)calloc(total, sizeof(char*));
    if (suggestions == NULL) {
        lk_free_correct_ablauts(w_list);
        *count = -LK_OUT_OF_MEMORY;
        return NULL;
    }

    size_t idx = 0;
    lk_result final = LK_OK;
    if (!skip_match) {
        const struct lk_word_ptr *cw = match;
        while (final == LK_OK && cw) {
            /* skip infinitive forms of ablautable words */
            if (!lk_has_ablaut(cw->word->word)) {
                lk_result res = lk_add_to_suggestions(suggestions, idx, cw->word->word);
                if (res == LK_OUT_OF_MEMORY) {
                    final = res;
                } else if (res == LK_OK) {
                    ++idx;
                }
            }

            cw = cw->next;
        }
    }
    if (final == LK_OK && w_list != NULL) {
        suggestions[idx] = (char*)calloc(strlen("-") + 1, sizeof(char));
        if (suggestions[idx] == NULL) {
            final = LK_OUT_OF_MEMORY;
        } else {
            strcpy(suggestions[idx++], "-");
        }

        struct lk_word_form *tmp = w_list;
        while (final == LK_OK && tmp != NULL) {
            lk_result res = lk_add_to_suggestions(suggestions, idx, tmp->form);
            if (res == LK_OUT_OF_MEMORY) {
                final = res;
            } else if (res == LK_OK) {
                ++idx;
            }

            tmp = tmp->next;
        }
    }

    if (final != LK_OK) {
        lk_free_suggestions(suggestions);
        lk_free_correct_ablauts(w_list);
        *count = -LK_OUT_OF_MEMORY;
        return NULL;
    }

    *count = idx;
    lk_free_correct_ablauts(w_list);

    return suggestions;
}

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

lk_ablaut lk_dict_check_ablaut(const struct lk_dictionary *dict, const char *word) {
    lk_ablaut ab = LK_ABLAUT_0;
    if (!lk_is_dict_valid(dict) || word == NULL)
        return ab;

    const struct lk_ablauting *ab_item = dict->ab_head;
    while (ab_item != NULL) {
        if (ab_item->word && ab_item->word->word
            && strcmp(word, ab_item->word->word) == 0) {
            ab = ab_item->ablaut;
            break;
        }

        ab_item = ab_item->next;
    }

    return ab;
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

static lk_result dict_add_ablaut(struct lk_dictionary* dict, const struct lk_word *word,
        lk_ablaut ablaut) {
    if (!lk_is_dict_valid(dict) || word == NULL)
        return LK_INVALID_ARG;

    struct lk_ablauting *ab = (struct lk_ablauting*)calloc(1, sizeof(*ab));
    if (ab == NULL)
        return LK_OUT_OF_MEMORY;
    ab->word = (struct lk_word*)word;
    ab->ablaut = ablaut;

    if (dict->ab_head == NULL) {
        dict->ab_head = (struct lk_ablauting*)ab;
        dict->ab_tail = (struct lk_ablauting*)ab;
    } else {
        dict->ab_tail->next = (struct lk_ablauting*)ab;
        dict->ab_tail = (struct lk_ablauting*)ab;
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
    if (word->contracted)
        free(word->contracted);
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

static lk_result add_word_ascii_forms(struct lk_tree *tree, const struct lk_word *base,
        int contracted) {
    static char buf[LK_MAX_WORD_LEN], tmp[LK_MAX_WORD_LEN];

    /* add the word */
    const char *base_word = contracted ? base->contracted : base->word;
    lk_result res = lk_tree_add_word(tree, base_word, base);
    if (res == LK_OK)
        res = generate_ascii_forms(tree, base_word, base);

    return res;
}

static lk_result add_word_to_tree(struct lk_dictionary *dict, const struct lk_word *base,
        lk_ablaut ablaut) {
    /* add base word */
    lk_result res = add_word_ascii_forms(dict->tree, base, 0);
    if (res != LK_OK || !lk_has_ablaut(base->word))
        return res;

    if (ablaut != LK_ABLAUT_0) {
        res = dict_add_ablaut(dict, base, ablaut);
        if (res != LK_OK)
            return res;
    }

    /* add contracted form */
    if (base->contracted != NULL) {
        res = add_word_ascii_forms(dict->tree, base, 1);
        if (res != LK_OK || !lk_has_ablaut(base->word))
            return res;
    }

    /* generate all ablauted forms */
    static char ab_base[LK_MAX_WORD_LEN], ab[LK_MAX_WORD_LEN];
    int is_ab_stressed = lk_is_ablaut_stressed(base->word);

    /* create ablaut base */
    lk_copy_word_base(base->word, ab_base);

    size_t ab_base_len = strlen(ab_base);
    if (ab_base_len + 4 >= LK_MAX_WORD_LEN)
        return LK_OUT_OF_MEMORY;

    int ab_type[3] = {LK_ABLAUT_A, LK_ABLAUT_E, LK_ABLAUT_N};
    for (int i = 0; i < 3; i++) {
        strcpy(ab, ab_base);
        lk_append_ablaut(ab, ab_type[i], is_ab_stressed);
        struct lk_word *w = (struct lk_word*)calloc(1, sizeof(*w));
        if (w == NULL)
            return LK_OUT_OF_MEMORY;

        w->wtype = base->wtype;
        w->base = (struct lk_word*)base;
        w->word = (char*)calloc(strlen(ab) + 1, sizeof(char));
        if (w->word == NULL) {
            free(w);
            return LK_OUT_OF_MEMORY;
        }
        strcpy(w->word, ab);

        dict_add_word(dict, w);
        lk_result res = add_word_ascii_forms(dict->tree, w, 0);
        if (res == LK_OK && ablaut != LK_ABLAUT_0)
            res = dict_add_ablaut(dict, w, ablaut);

        if (res != LK_OK)
            return res;
    }

    return LK_OK;
}

static lk_result add_all_forms_to_dict(struct lk_dictionary *dict, struct lk_word *word,
        lk_ablaut ablaut) {
    lk_result res = lk_tree_add_word(dict->tree, word->word, word);
    if (res == LK_OK)
        res = add_word_to_tree(dict, word, ablaut);

    if (res == LK_OK && word->contracted != NULL) {
        res = lk_tree_add_word(dict->tree, word->contracted, word);
        if (res == LK_OK)
            res = add_word_ascii_forms(dict->tree, word, 1);
    }

    return res;
}

static lk_ablaut lk_ablaut_from_string(const char *s) {
    switch (*s) {
        case 'A': case 'a':
            return LK_ABLAUT_A;
        case 'E': case 'e':
            return LK_ABLAUT_E;
        case 'N': case 'n':
            return LK_ABLAUT_N;
        default:
            return LK_ABLAUT_0;
    }
}

static const char* lk_detect_special_char(const char* s) {
    const char* chr = strchr(s, '~');
    if (chr == NULL)
        chr = strchr(s, '@');
    if (chr == NULL)
        chr = strchr(s, '%');

    return chr;
}

static lk_result lk_replace_special_char(const char *templ,
        char c, char *buf,
        const struct lk_word *base, const char *unstressed,
        const struct lk_word *last) {
    const char *chr = strchr(templ, c);
    size_t len = chr - templ;
    if (len >= LK_MAX_WORD_LEN)
        return LK_BUFFER_SMALL;
    if (len > 0)
        strncpy(buf, templ, len);
    buf[len] = '\0';

    switch (c) {
        case '~':
            if (len + strlen(base->word) >= LK_MAX_WORD_LEN)
                return LK_BUFFER_SMALL;
            strcat(buf, base->word);
            break;
        case '@':
            if (len + strlen(unstressed) >= LK_MAX_WORD_LEN)
                return LK_BUFFER_SMALL;
            strcat(buf, unstressed);
            break;
        case '%':
            if (len + strlen(last->word) >= LK_MAX_WORD_LEN)
                return LK_BUFFER_SMALL;
            strcat(buf, last->word);
            break;
    }

    chr++;
    if (strlen(buf) + strlen(chr) >= LK_MAX_WORD_LEN)
        return LK_BUFFER_SMALL;
    strcat(buf, chr);

    return LK_OK;
}

static struct lk_word* lk_add_form_as_is(struct lk_dictionary *dict, const char *word,
       struct lk_word *base, lk_ablaut ab) {
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
    out->wtype = base->wtype;
    dict_add_word(dict, out);

    lk_result res = add_all_forms_to_dict(dict, out, ab);
    if (res != LK_OK)
        return NULL;

    return out;
}

static lk_result lk_iterate_forms(struct lk_dictionary *dict,
        char *start, struct lk_word *base, lk_ablaut ab) {
    const char *spc = skip_spaces(start);
    struct lk_word *last_full = base;
    static char buf[LK_MAX_WORD_LEN], tmp[LK_MAX_WORD_LEN];
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

        strncpy(tmp, spc, len);
        tmp[len] = '\0';

        const char *chr = lk_detect_special_char(tmp);
        if (chr == NULL) {
            strcpy(buf, tmp);
        } else {
            char c = *chr;
            lk_result repl = lk_replace_special_char(tmp, c, buf,
                                base, base_unstressed, last_full);

            if (repl == LK_OK && c == '@') {
                repl = lk_put_stress(buf, LK_STRESS_DEFAULT, tmp, LK_MAX_WORD_LEN);
                if (repl == LK_OK)
                    strcpy(buf, tmp);
            }

            if (repl != LK_OK)
                return repl;
        }

        struct lk_word *out = lk_add_form_as_is(dict, buf, base, ab);
        if (out == NULL)
            return LK_OUT_OF_MEMORY;

        last_full = out;
        spc = skip_spaces(start);
    }

    return LK_OK;
}

static lk_result lk_read_base_form(struct lk_dictionary *dict, const char *s,
        struct lk_word *base, lk_ablaut ab) {
    char *start = strchr(s, ' ');
    size_t len = (start == NULL)? strlen(s) : start - s;
    if (len == 0) {
        free(base->contracted);
        free(base);
        return LK_INVALID_STRING;
    }
    base->word = (char*)calloc(len + 1, sizeof(char));
    if (base->word == NULL) {
        free(base->contracted);
        free(base);
        return LK_OUT_OF_MEMORY;
    }
    strncpy(base->word, s, len);

    lk_result res = dict_add_word(dict, base);
    if (res == LK_OK && ab != LK_ABLAUT_0)
        res = dict_add_ablaut(dict, base, ab);
    if (res == LK_OK)
        res = add_all_forms_to_dict(dict, base, ab);

    return res;
}

static const char* lk_read_ablaut_and_contraction(const char *info,
        struct lk_word *base, lk_ablaut *ab, lk_result *res) {
    *res = LK_OK;
    if (*info != ':')
        return skip_spaces(info);

    /* ablaut or contraction */
    info++;
    if (strchr("SITR", base->wtype) == NULL) {
        *ab = lk_ablaut_from_string(info);
        if (*info)
            info++;

        return skip_spaces(info);
    } else {
        /* contracted form */
        const char *spc = strchr(info, ' ');
        if (spc == NULL) {
            free(base);
            *res = LK_INVALID_STRING;
            return NULL;
        }
        size_t len = spc - info;
        if (len != 0) {
            base->contracted = (char *)calloc(len + 1, sizeof(char));
            if (base->contracted == NULL) {
                free(base);
                *res = LK_OUT_OF_MEMORY;
                return NULL;
            }
            strncpy(base->contracted, info, len);
        }

        return skip_spaces(spc);
    }
}

lk_result lk_parse_word(const char *info, struct lk_dictionary* dict) {
    if (!lk_is_dict_valid(dict) || info == NULL)
        return LK_INVALID_ARG;

    if (*info == '#')
        return LK_COMMENT;

    struct lk_word *base = (struct lk_word*)calloc(1, sizeof(struct lk_word));
    if (base == NULL)
        return LK_OUT_OF_MEMORY;

    /* read word props */
    base->wtype = *info++;
    if (base->wtype >= 'a' && base->wtype <= 'z')
        base->wtype = base->wtype - 'a' + 'A';
    lk_result res;
    lk_ablaut ab = LK_ABLAUT_0;
    const char *spc = lk_read_ablaut_and_contraction(info, base, &ab, &res);
    if (spc == NULL)
        return res;

    /* read the word base form */
    res = lk_read_base_form(dict, spc, base, ab);
    if (res != LK_OK)
        return res;

    char *start = strchr(spc, ' ');
    if (start == NULL)
        return LK_OK;

    return lk_iterate_forms(dict, start, base, ab);
}

int lk_is_dict_valid(const struct lk_dictionary *dict) {
    if (dict == NULL)
        return 0;
    if ((dict->head != NULL && dict->tail == NULL) ||
        (dict->head == NULL && dict->tail != NULL)) {
        return 0;
    }
    return 1;
}

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

void lk_dict_close(struct lk_dictionary* dict) {
    if (dict == NULL)
        return;

    if (dict->tree != NULL)
        lk_tree_free(dict->tree);

    struct lk_ablauting *ab = dict->ab_head, *ab_next;
    while (ab != NULL) {
        ab_next = ab->next;
        free(ab);
        ab = ab_next;
    }

    struct lk_word *wr = dict->head, *wr_next;
    while (wr != NULL) {
        wr_next = wr->next;
        free_word(wr);
        wr = wr_next;
    }

    free(dict);
}

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

