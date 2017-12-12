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

#define LK_MAX_WORD_LEN 256

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

static void lk_generate_ablauted(const char *word, lk_ablaut ablaut, char *out) {
    int is_ab_stressed = lk_is_ablaut_stressed(word);
    utf8proc_int32_t cp;
    size_t len;

    /* create ablaut base */
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;
        if (cp == 'A' || cp == 'I'
            || cp == 193/* A' */ || cp == 205/* I' */) {
            break;
        }
        len = utf8proc_encode_char(cp, udst);
        udst += len;
    }
    *udst = '\0';

    udst = (utf8proc_uint8_t*)out + strlen(out);
    switch (ablaut) {
        case LK_ABLAUT_A:
            if (is_ab_stressed) {
                cp = 225;
            } else {
                cp = 'a';
            }
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            *udst = '\0';
            break;
        case LK_ABLAUT_E:
            if (is_ab_stressed) {
                cp = 233;
            } else {
                cp = 'e';
            }
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            *udst = '\0';
            break;
        case LK_ABLAUT_N:
            if (is_ab_stressed) {
                cp = 237;
            } else {
                cp = 'i';
            }
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            cp = 331; /* ng */
            len = utf8proc_encode_char(cp, udst);
            udst += len;
            *udst = '\0';
            break;
    }
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

char** lk_dict_exact_lookup(const struct lk_dictionary *dict, const char *word, const char *next_word, int *count) {
    char** suggestions = NULL;
    static char unstressed[LK_MAX_WORD_LEN];

    if (word == NULL || !lk_is_dict_valid(dict)) {
        if (count != NULL)
            *count = -LK_INVALID_ARG;
        return NULL;
    }

    /* detect next_word ablaut type. If next_word == NULL then skip this step */
    lk_ablaut ab = lk_find_word_ablaut(dict, next_word);

    /* lookup the main word */
    /* if not found - return NULL & count = -LK_WORD_NOT_FOUND */
    const struct lk_word_ptr *match = lk_dict_find_word(dict, word);
    if (match == NULL) {
        /* try look for incorrect stress */
        int scnt = lk_stressed_vowels_no(word);
        if (scnt > 0) {
            lk_result r = lk_destress(word, unstressed, LK_MAX_WORD_LEN);
            if (r != LK_OK) {
                if (count)
                    *count = -LK_INVALID_STRING;
                return NULL;
            }
            match = lk_dict_find_word(dict, unstressed);
            if (match == NULL) {
                if (count)
                    *count = -LK_WORD_NOT_FOUND;
                return NULL;
            }
            word = unstressed;
        } else {
            if (count)
                *count = -LK_WORD_NOT_FOUND;
            return NULL;
        }
    }

    int total = 0;
    const struct lk_word_ptr *cw = match;
    while (cw) {
        total++;
        if (strcmp(cw->word->word, word) == 0) {
            total = 0;
            break;
        }
        cw = cw->next;
    }

    if (total == 0 && ab == LK_ABLAUT_0) {
        if (count)
            *count = 0;
        return NULL;
    }

    struct lk_word_form *w_list = NULL;
    /* if ablaut then check if the form is correct */
    if (ab != LK_ABLAUT_0) {
        const struct lk_word_ptr *cw = match;
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
                if (frm == NULL) {
                    goto err_oom;
                }

                strcpy(frm->form, buf);
                if (w_list == NULL) {
                    w_list = frm;
                } else {
                    struct lk_word_form *tmp = w_list;
                    while (tmp->next != NULL)
                        tmp = tmp->next;
                    tmp->next = frm;
                }
                total++;
            }

            cw = cw->next;
        }
    }

    /* if there was incorrect ablaut then add extra
     * form as a separator between base forms and ablauted ones
     */
    if (w_list != NULL)
        total++;
    /* add extra for NULL tail */
    total++;

    /* generate and return all the list */
    suggestions = (char**)calloc(total, sizeof(char*));
    if (suggestions == NULL)
        goto err_oom;

    cw = match;
    size_t idx = 0;
    while (cw) {
        /* skip infinitive forms of ablautable words */
        if (lk_has_ablaut(cw->word->word)) {
            cw = cw->next;
            continue;
        }

        /* skip duplicates */
        int already_used = 0;
        for (size_t chk = 0; chk < idx; ++chk) {
            if (strcmp(suggestions[chk], cw->word->word) == 0) {
                already_used = 1;
                break;
            }
        }
        if (already_used) {
            cw = cw->next;
            continue;
        }

        suggestions[idx] = (char*)calloc(strlen(cw->word->word) + 1, sizeof(char));
        if (suggestions[idx] == NULL)
            goto err_oom;

        strcpy(suggestions[idx], cw->word->word);
        ++idx;
        cw = cw->next;
    }
    if (w_list != NULL) {
        suggestions[idx] = (char*)calloc(strlen("-") + 1, sizeof(char));
        if (suggestions[idx] == NULL)
            goto err_oom;
        strcpy(suggestions[idx++], "-");

        struct lk_word_form *tmp = w_list;
        while (tmp != NULL) {
            /* skip duplicates */
            int already_used = 0;
            for (size_t chk = 0; chk < idx; ++chk) {
                if (strcmp(suggestions[chk], tmp->form) == 0) {
                    already_used = 1;
                    break;
                }
            }
            if (already_used) {
                tmp = tmp->next;
                continue;
            }

            suggestions[idx] = (char*)calloc(strlen(tmp->form) + 1, sizeof(char));
            if (suggestions[idx] == NULL)
                goto err_oom;
            strcpy(suggestions[idx++], tmp->form);
            tmp = tmp->next;
        }
    }

    if (count != NULL)
        *count = idx;

    goto free_ablauts;

err_oom:
    if (suggestions != NULL) {
        char *f = *suggestions;
        while (f) {
            free(f);
            f++;
        }

        free(suggestions);
        suggestions = NULL;
    }

    if (count != NULL)
        *count = -LK_OUT_OF_MEMORY;

free_ablauts:
    if (w_list) {
        struct lk_word_form *tmp = w_list;
        while (tmp != NULL) {
            struct lk_word_form *n = tmp->next;
            free(tmp);
            tmp = n;
        }
    }

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
    if (res != LK_OK)
        return res;

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
        if (res != LK_OK)
            return res;
        if (strcmp(buf, tmp) != 0) {
            res = lk_tree_add_word(tree, buf, base);
            if (res != LK_OK)
                return res;
            if (has_stop) {
                res = add_without_stop(tree, buf, base);
                if (res != LK_OK)
                    return res;
            }
        }
    } else {
        strcpy(buf, tmp);
    }

    /*res = */lk_to_ascii(buf, tmp, LK_MAX_WORD_LEN);
    if (strcmp(buf, tmp) != 0) {
        res = lk_tree_add_word(tree, tmp, base);
        if (res != LK_OK)
            return res;
        if (has_stop) {
            res = add_without_stop(tree, tmp, base);
            if (res != LK_OK)
                return res;
        }
    }

    char *aph = strchr(tmp, '\'');
    if (aph == NULL)
        return LK_OK;

    /* replace aphostrophes with tildes */
    while (aph != NULL) {
        *aph = '`';
        aph = strchr(aph, '\'');
    }
    res = lk_tree_add_word(tree, tmp, base);
    if (res != LK_OK)
        return res;

    return LK_OK;
}

static lk_result add_word_ascii_forms(struct lk_tree *tree, const struct lk_word *base,
        int contracted) {
    static char buf[LK_MAX_WORD_LEN], tmp[LK_MAX_WORD_LEN];

    /* add the word */
    const char *base_word = contracted ? base->contracted : base->word;
    lk_result res = lk_tree_add_word(tree, base_word, base);
    if (res != LK_OK)
        return res;

    return generate_ascii_forms(tree, base_word, base);
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
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)base->word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)ab_base;
    utf8proc_int32_t cp;
    size_t len;
    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;
        if (cp == 'A' || cp == 'I'
            || cp == 193/* A' */ || cp == 205/* I' */) {
            break;
        }
        len = utf8proc_encode_char(cp, udst);
        udst += len;
    }
    *udst = '\0';
    size_t ab_base_len = strlen(ab_base);

    if (ab_base_len + 4 >= LK_MAX_WORD_LEN)
        return LK_OUT_OF_MEMORY;

    for (int i = 0; i < 3; i++) {
        strcpy(ab, ab_base);
        udst = (utf8proc_uint8_t*)ab + ab_base_len;
        switch (i) {
            case 0:
                if (is_ab_stressed) {
                    cp = 225;
                } else {
                    cp = 'a';
                }
                len = utf8proc_encode_char(cp, udst);
                udst += len;
                *udst = '\0';
                break;
            case 1:
                if (is_ab_stressed) {
                    cp = 233;
                } else {
                    cp = 'e';
                }
                len = utf8proc_encode_char(cp, udst);
                udst += len;
                *udst = '\0';
                break;
            case 2:
                if (is_ab_stressed) {
                    cp = 237;
                } else {
                    cp = 'i';
                }
                len = utf8proc_encode_char(cp, udst);
                udst += len;
                cp = 331; /* ng */
                len = utf8proc_encode_char(cp, udst);
                udst += len;
                *udst = '\0';
                break;
        }

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
        if (res != LK_OK)
            return res;

        if (ablaut != LK_ABLAUT_0) {
            res = dict_add_ablaut(dict, w, ablaut);
            if (res != LK_OK)
                return res;
        }
    }

    return LK_OK;
}

static lk_result add_all_forms_to_dict(struct lk_dictionary *dict, struct lk_word *word,
        lk_ablaut ablaut) {
    lk_result suf_res = lk_tree_add_word(dict->tree, word->word, word);
    if (suf_res != LK_OK)
        return suf_res;

    suf_res = add_word_to_tree(dict, word, ablaut);
    if (suf_res != LK_OK)
        return suf_res;

    if (word->contracted != NULL) {
        suf_res = lk_tree_add_word(dict->tree, word->contracted, word);
        if (suf_res != LK_OK)
            return suf_res;
        suf_res = add_word_ascii_forms(dict->tree, word, 1);
    }

    return suf_res;
}

lk_result lk_parse_word(const char *info, struct lk_dictionary* dict) {
    static char buf[LK_MAX_WORD_LEN], tmp[LK_MAX_WORD_LEN];
    static char base_unstressed[LK_MAX_WORD_LEN];

    if (!lk_is_dict_valid(dict))
        return LK_INVALID_ARG;

    if (info == NULL)
        return LK_INVALID_ARG;

    if (*info == '#')
        return LK_COMMENT;

    struct lk_word *base = (struct lk_word*)calloc(1, sizeof(struct lk_word));
    struct lk_word *out = NULL;
    if (base == NULL)
        return LK_OUT_OF_MEMORY;

    /* read word props */
    const char *spc = NULL;
    size_t len;
    lk_ablaut ab = LK_ABLAUT_0;
    base->wtype = *info++;
    if (base->wtype >= 'a' && base->wtype <= 'z')
        base->wtype = base->wtype - 'a' + 'A';
    if (*info == ':') {
        /* ablaut or contraction */
        info++;
        if (strchr("SITR", base->wtype) == NULL) {
            /* ablauting */
            switch (*info) {
                case 'A': case 'a':
                    ab = LK_ABLAUT_A;
                    break;
                case 'E': case 'e':
                    ab = LK_ABLAUT_E;
                    break;
                case 'N': case 'n':
                    ab = LK_ABLAUT_N;
                    break;
            }
            if (*info)
                info++;

            spc = skip_spaces(info);
        } else {
            /* contracted form */
            spc = strchr(info, ' ');
            if (spc == NULL) {
                free(base);
                return LK_INVALID_STRING;
            }
            len = spc - info;
            if (len != 0) {
                base->contracted = (char *)calloc(len + 1, sizeof(char));
                if (base->contracted == NULL) {
                    free(base);
                    return LK_OUT_OF_MEMORY;
                }
                strncpy(base->contracted, info, len);
            }

            spc = skip_spaces(spc);
        }
    } else {
        spc = skip_spaces(info);
    }

    /* read the word base form */
    char *start = strchr(spc, ' ');
    len = (start == NULL)? strlen(spc) : start - spc;
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
    strncpy(base->word, spc, len);
    dict_add_word(dict, base);
    if (ab != LK_ABLAUT_0) {
        dict_add_ablaut(dict, base, ab);
    }
    lk_destress(base->word, base_unstressed, LK_MAX_WORD_LEN);
    lk_result suf_res = add_all_forms_to_dict(dict, base, ab);
    if (suf_res != LK_OK)
        return suf_res;

    if (start == NULL)
        return LK_OK;

    spc = skip_spaces(start);
    struct lk_word *last_full = base;
    while (*spc) {
        start = strchr(spc, ' ');
        len = (start == NULL)? strlen(spc) : start - spc;
        if (len == 0) {
            spc = skip_spaces(start);
            continue;
        }

        if (len >= LK_MAX_WORD_LEN)
            return LK_BUFFER_SMALL;

        strncpy(tmp, spc, len);
        tmp[len] = '\0';

        const char *chr;
        char c = ' ';
        if ((chr = strchr(tmp, '~')) == NULL) {
            if ((chr = strchr(tmp, '@')) == NULL) {
                if ((chr = strchr(tmp, '%')) == NULL) {
                }
            }
        }
        if (chr == NULL) {
            strcpy(buf, tmp);
        } else {
            c = *chr;
            len = chr - tmp;
            if (len >= LK_MAX_WORD_LEN)
                return LK_BUFFER_SMALL;
            if (len > 0)
                strncpy(buf, tmp, len);

            buf[len] = '\0';
            switch (c) {
                case '~':
                    if (len + strlen(base->word) >= LK_MAX_WORD_LEN)
                        return LK_BUFFER_SMALL;
                    strcat(buf, base->word);
                    break;
                case '@':
                    if (len + strlen(base_unstressed) >= LK_MAX_WORD_LEN)
                        return LK_BUFFER_SMALL;
                    strcat(buf, base_unstressed);
                    break;
                case '%':
                    if (len + strlen(last_full->word) >= LK_MAX_WORD_LEN)
                        return LK_BUFFER_SMALL;
                    strcat(buf, last_full->word);
                    break;
            }
            chr++;
            if (strlen(buf) + strlen(chr) >= LK_MAX_WORD_LEN)
                return LK_BUFFER_SMALL;
            strcat(buf, chr);

            if (c == '@') {
                lk_result res = lk_put_stress(buf, LK_STRESS_DEFAULT, tmp, LK_MAX_WORD_LEN);
                if (res != LK_OK)
                    return res;
                strcpy(buf, tmp);
            }
        }

        out = (struct lk_word*)calloc(1, sizeof(struct lk_word));
        if (out == NULL)
            return LK_OUT_OF_MEMORY;
        out->word = (char *)calloc(1, strlen(buf) + 1);
        if (out->word == NULL) {
            free(out);
            return LK_OUT_OF_MEMORY;
        }
        strcpy(out->word, buf);
        out->base = base;
        out->wtype = base->wtype;
        dict_add_word(dict, out);
        last_full = out;
        lk_result suf_res = add_all_forms_to_dict(dict, out, ab);
        if (suf_res != LK_OK)
            return suf_res;

        if (start == NULL)
            break;
        spc = skip_spaces(start);
    }

    return LK_OK;
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

        if (file_res == LK_EOF) {
            break;
        }
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

