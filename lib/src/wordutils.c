#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "common.h"
#include "wordutils.h"
#include "utf8proc.h"

/* utf8 char -> uint32
 * á - 225 * Á - 193
 * ó - 243 * Ó - 211
 * é - 233 * É - 201
 * í - 237 * Í - 205
 * ú - 250 * Ú - 218
 * ŋ - 331 * Ŋ - 330
 * č - 269 * Č - 268
 * ž - 382 * Ž - 381
 * ȟ - 543 (h<) * Ȟ - 542 (H<)
 * ǧ - 487 (g<) * Ǧ - 486 (G<)
 * š - 353 * Š - 352
 * ' - 700 - ʼ
 */

#define VOWEL_COUNT 5
/* vowels must go first in lk_low_case & lk_up_case arrays */
const utf8proc_uint32_t lk_low_case[] = {
    225, 243, 233, 237, 250, 331, 269, 382, 543, 487, 353,
};
const utf8proc_uint32_t lk_up_case[] = {
    193, 211, 201, 205, 218, 330, 268, 381, 542, 486, 352,
};
/*
 * to convert lk_low_case to ascii symbol. Length must equal length of lk_low_case
 * and the order must the same as in lk_low_case
*/
const utf8proc_int32_t lk_low_ascii[] = {
    'a', 'o', 'e', 'i', 'u', 'n', 'c', 'z', 'h', 'g', 's',
};

/*
 * Internal functions
 */
typedef utf8proc_int32_t (*ustr_func) (utf8proc_int32_t);

static size_t cp_length(utf8proc_uint32_t cp) {
    if (cp < 0) {
        return 0;
    } else if (cp < 0x80) {
        return 1;
    } else if (cp < 0x800) {
        return 2;
    } else if (cp < 0x10000) {
        return 3;
    } else if (cp < 0x110000) {
        return 4;
    } else {
        return 0;
    }
}

static lk_result process_ustr(char *src, ustr_func fn) {
    if (src == NULL) {
        return LK_OK;
    }

    size_t len = 0, used = 0, lendst, processed = 0;
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)src;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)src;
    utf8proc_int32_t cpsrc, cpdst;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cpsrc);

        if (cpsrc == -1) {
            return LK_INVALID_STRING;
        }

        cpdst = (*fn)(cpsrc);
        usrc += len;

        lendst = cp_length(cpdst);
        if (lendst > len) {
            return LK_BUFFER_SMALL;
        }

        lendst = utf8proc_encode_char(cpdst, udst);
        udst += lendst;
        ++processed;
    }

    return LK_OK;
}

static lk_result ustr_lowcase(char *src) {
    return process_ustr(src, utf8proc_tolower);
}

/*
static size_t ustr_count(const char *str) {
    if (str == NULL || *str == '\0') {
        return 0;
    }

    size_t len = 0, sz;
    utf8proc_uint8_t *tmp = (utf8proc_uint8_t*)str;
    utf8proc_int32_t cp;
    while (*tmp) {
        sz = utf8proc_iterate(tmp, -1, &cp);

        if (cp == -1) {
            return (size_t)-1;
        }

        tmp += sz;
        len++;
    }

    return len;
}
*/

int lk_ends_with(const char *orig, const char *cmp) {
    if (orig == NULL && cmp == NULL) {
        return 1;
    }
    if (cmp == NULL)
        return 1;
    if (orig == NULL)
       return 0;

    size_t olen = strlen(orig);
    size_t clen = strlen(cmp);

    if (clen > olen)
        return 0;

    const char *from = orig + olen - clen;
    return strcmp(from, cmp) == 0 ? 1 : 0;
}

/*
static lk_result ustr_translate(const char *src, char *dst, const char *what, const char *with) {
    if (dst == NULL)
        return LK_BUFFER_SMALL;

    if (src == NULL || *src == '\0') {
        *dst = '\0';
        return LK_OK;
    }

    size_t whatlen = ustr_count(what);
    if (whatlen == (size_t)-1) {
        return LK_INVALID_ARG;
    }
    size_t withlen = ustr_count(with);
    if (withlen == (size_t)-1) {
        return LK_INVALID_ARG;
    }

    utf8proc_int32_t *whatarr = (utf8proc_int32_t *)malloc(sizeof(utf8proc_int32_t) * whatlen);
    if (whatarr == NULL) {
        return LK_OUT_OF_MEMORY;
    }
    utf8proc_int32_t *witharr = (utf8proc_int32_t *)malloc(sizeof(utf8proc_int32_t) * whatlen);
    if (witharr == NULL) {
        free(whatarr);
        return LK_OUT_OF_MEMORY;
    }

    utf8proc_uint8_t *utmp = (utf8proc_uint8_t*)what;
    utf8proc_int32_t cp, dcp;
    size_t idx = 0, len, dlen;

    for (idx = 0; idx < whatlen; ++idx) {
        len = utf8proc_iterate(utmp, -1, &cp);

        if (cp == -1) {
            free(whatarr);
            free(witharr);
            return LK_INVALID_ARG;
        }

        whatarr[idx] = cp;
        utmp += len;
    }

    utmp = (utf8proc_uint8_t*)with;
    for (idx = 0; idx < whatlen; ++idx) {
        if (*utmp == '\0') {
            witharr[idx] = 0;
            continue;
        }

        len = utf8proc_iterate(utmp, -1, &cp);

        if (cp == -1) {
            free(whatarr);
            free(witharr);
            return LK_INVALID_ARG;
        }

        witharr[idx] = cp;
        utmp += len;
    }

    size_t used = 0;
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)src;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)dst;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);

        if (cp == -1) {
            free(whatarr);
            free(witharr);
            return LK_INVALID_ARG;
        }

        dcp = cp;
        for (size_t i = 0; i < whatlen; ++i) {
            if (whatarr[i] == cp) {
                dcp = witharr[i];
                break;
            }
        }

        if (udst != NULL && dcp != 0) {
            dlen = utf8proc_encode_char(dcp, udst);
            udst += dlen;
        }

        used += cp_length(dcp);
        usrc += len;
    }

    *udst = '\0';

    free(whatarr);
    free(witharr);
    return LK_OK;
}
*/

lk_result fix_glottal_stop(const char *word, char *out, size_t out_sz) {
    size_t len = 0;
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);

        if (cp == -1) {
            return LK_INVALID_STRING;
        }

        if (cp == '\'' || cp == '`')
            cp = 700;
        usrc += len;

        len = cp_length(cp);
        if (len >= out_sz) {
            return LK_BUFFER_SMALL;
        }

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

/* External API */
lk_result lk_to_low_case(const char *word, char *out, size_t out_sz) {
    if (word == NULL)
        return LK_INVALID_ARG;
    if (out == NULL)
        return LK_BUFFER_SMALL;
    if (out == word)
        return LK_INVALID_ARG;

    if (strlen(word) > out_sz)
        return LK_BUFFER_SMALL;

    lk_result r = fix_glottal_stop(word, out, out_sz);
    if (r != LK_OK)
        return r;

    return ustr_lowcase(out);
}

static int lk_is_valid_char(utf8proc_int32_t c) {
    if (c == '-' || c == 700)
        return 1;
    if (c >= 'a' && c <= 'z')
        return 1;

    size_t sz = sizeof(lk_low_case) / sizeof(lk_low_case[0]);
    for (size_t idx = 0; idx < sz; idx++) {
        if (lk_low_case[idx] == c)
            return 1;
    }

    return 0;
}

int lk_is_valid_word(const char *word) {
    if (word == NULL)
        return 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*uw) {
        len = utf8proc_iterate(uw, -1, &cp);

        if (cp == -1) {
            return 0;
        }

        if (! lk_is_valid_char(cp))
            return 0;

        uw += len;
    }

    return 1;
}

int lk_has_ablaut(const char *word) {
    if (word == NULL)
        return 0;

    if (lk_ends_with(word, "A")
        || lk_ends_with(word, "Aŋ")
        || lk_ends_with(word, "Iŋ")
        || lk_ends_with(word, "Á")
        || lk_ends_with(word, "Áŋ")
        || lk_ends_with(word, "Íŋ")
        )
        return 1;

    return 0;
}

int lk_is_ablaut_stressed(const char *word) {
    if (word == NULL)
        return 0;

    if (lk_ends_with(word, "Á")
        || lk_ends_with(word, "Áŋ")
        || lk_ends_with(word, "Íŋ")
        )
        return 1;

    return 0;
}

int lk_stressed_vowels_no(const char *word) {
    if (word == NULL)
        return 0;

    int cnt = 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*uw) {
        len = utf8proc_iterate(uw, -1, &cp);

        if (cp == -1) {
            return 0;
        }

        if (cp == 225 || cp == 243 || cp == 233
            || cp == 237 || cp == 250)
            cnt++;

        uw += len;
    }

    return cnt;
}

int lk_vowels_no(const char *word) {
    if (word == NULL)
        return 0;

    int cnt = 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*uw) {
        len = utf8proc_iterate(uw, -1, &cp);

        if (cp == -1) {
            return 0;
        }

        if (cp == 225 || cp == 243 || cp == 233
            || cp == 237 || cp == 250 || cp == 'a'
            || cp == 'e' || cp == 'i' || cp == 'o'
            || cp == 'u')
            cnt++;

        uw += len;
    }

    return cnt;
}

int lk_is_ascii(const char *word) {
    if (word == NULL)
        return 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*uw) {
        len = utf8proc_iterate(uw, -1, &cp);

        if (cp == -1 || cp > 127) {
            return 0;
        }

        uw += len;
    }

    return 1;
}

lk_result lk_to_ascii(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;
    size_t len;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);

        if (cp == '`' || cp == 700)
            cp = '\'';
        else {
            for (size_t idx = 0; idx < sizeof(lk_low_case)/sizeof(lk_low_case[0]); idx++) {
                if (cp == lk_low_case[idx]) {
                    cp = lk_low_ascii[idx];
                    break;
                }
            }
        }

        usrc += len;
        if (cp_length(cp) >= out_sz)
            return LK_BUFFER_SMALL;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

lk_result lk_destress(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;
    size_t len;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);

        if (cp > 127) {
            for (size_t idx = 0; idx < VOWEL_COUNT; idx++) {
                if (cp == lk_low_case[idx]) {
                    cp = lk_low_ascii[idx];
                    break;
                }
            }
        }

        usrc += len;
        if (cp_length(cp) >= out_sz)
            return LK_BUFFER_SMALL;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

size_t lk_first_stressed_vowel(const char *word) {
    if (word == NULL)
        return 0;

    int no = 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*uw) {
        len = utf8proc_iterate(uw, -1, &cp);

        if (cp == -1) {
            return 0;
        }

        if (cp == 'a' || cp == 'e'
            || cp == 'i' || cp == 'o' || cp == 'u') {
            no++;
        } else if (cp == 225 || cp == 243 || cp == 233
                  || cp == 237 || cp == 250) {
            return no + 1;
        }

        uw += len;
    }

    return 0;
}

lk_result lk_put_stress(const char *word, int pos, char *out, size_t out_sz) {
    if (word == NULL || out == NULL || pos < 0 || pos > 10)
        return LK_INVALID_ARG;
    if (out_sz == 0)
        return LK_BUFFER_SMALL;

    int vcnt = lk_vowels_no(word);
    if (vcnt == 0)
        return LK_INVALID_ARG;

    if (pos >= vcnt)
        pos = vcnt - 1;
    if (pos < 0)
        pos = LK_STRESS_DEFAULT;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;
    size_t len;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);

        if (cp == -1) {
            return LK_INVALID_STRING;
        }

        if (pos != -1 && (cp == 'a' || cp == 'e'
            || cp == 'i' || cp == 'o' || cp == 'u'
            || cp == 225 || cp == 243 || cp == 233
            || cp == 237 || cp == 250)) {

            if (pos == 0) {
                switch (cp) {
                    case 'a':
                        cp = 225;
                        break;
                    case 'e':
                        cp = 233;
                        break;
                    case 'i':
                        cp = 237;
                        break;
                    case 'o':
                        cp = 243;
                        break;
                    case 'u':
                        cp = 250;
                        break;
                }
                pos = -1;
            } else {
                pos--;
            }
        }

        usrc += len;
        size_t l = cp_length(cp);
        if (out_sz <= l)
            return LK_BUFFER_SMALL;
        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }

    *udst = '\0';
    return pos == -1 ? LK_OK : LK_INVALID_ARG;
}

lk_result lk_remove_glottal_stop(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;
    size_t len;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;

        if (cp == 700 || cp == '\'' || cp == '`') {
            continue;
        }

        if (cp_length(cp) >= out_sz)
            return LK_BUFFER_SMALL;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

int lk_has_glottal_stop(const char *word) {
    if (word == NULL)
        return 0;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;
    size_t len;

    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;

        if (cp == 700 || cp == '\'' || cp == '`') {
            return 1;
        }
    }

    return 0;
}
