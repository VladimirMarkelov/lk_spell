#include <stdlib.h>
#include <stdio.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include <utf8proc.h>
#include "lk_common.h"
#include "lk_utils.h"

/* A table of UNICODE characters and their code
 * Just to keep the information somewhere at hand
 */
/* utf8 char -> uint32
 * á - 225 * Á - 193
 * ó - 243 * Ó - 211
 * é - 233 * É - 201
 * í - 237 * Í - 205
 * ú - 250 * Ú - 218
 * ŋ - 331 * Ŋ - 330
 * č - 269 * Č - 268
 * ž - 382 * Ž - 381
 * ȟ - 543 * Ȟ - 542 (H<)
 * ǧ - 487 * Ǧ - 486 (G<)
 * š - 353 * Š - 352
 * ' - 700 - ʼ
 */

/**
 * @enum lk_state
 * States for state machine to look for the next or beginning of the current one
 */
typedef enum {
    LK_STATE_SKIP_WHITE, /* skipping white spaces */
    LK_STATE_GOBBLE, /* the word is found so looking for the word end/start */
    LK_STATE_QUOTE, /* a possible glottal stop is detected */
    LK_STATE_DONE, /* parsing done */
} lk_state;


/*
 * All three array below must have the same number of elements in the same
 *  order. Vowels must go first.
 */
/* Array contains lowcase letters with diacritic marks as UNICODE characters */
const utf8proc_uint32_t lk_low_case[] = {
    LK_A_LOW, LK_O_LOW, LK_E_LOW, LK_I_LOW, LK_U_LOW,
    LK_N_LOW, LK_C_LOW, LK_Z_LOW, LK_H_LOW, LK_G_LOW, LK_S_LOW,
};
/* Array contains upcase letters with diacritic marks as UNICODE characters */
const utf8proc_uint32_t lk_up_case[] = {
    LK_A_UP, LK_O_UP, LK_E_UP, LK_I_UP, LK_U_UP,
    LK_N_UP, LK_C_UP, LK_Z_UP, LK_H_UP, LK_G_UP, LK_S_UP,
};
/* Array contains ASCII letters that represent the same characters as two
 * previous array but with diacritic marks removed
 */
const utf8proc_int32_t lk_low_ascii[] = {
    'a', 'o', 'e', 'i', 'u', 'n', 'c', 'z', 'h', 'g', 's',
};

/* a function type to use in string processing */
typedef utf8proc_int32_t (*ustr_func) (utf8proc_int32_t);

static int lk_is_stressed_vowel(utf8proc_uint32_t cp) {
    return (cp == LK_A_LOW || cp == LK_E_LOW || cp == LK_I_LOW
        || cp == LK_U_LOW || cp == LK_O_LOW);
}

static int lk_is_unstressed_vowel(utf8proc_uint32_t cp) {
    return (cp == 'a' || cp == 'e'
        || cp == 'i' || cp == 'o' || cp == 'u');
}

static int lk_is_vowel(utf8proc_uint32_t cp) {
    return lk_is_unstressed_vowel(cp) || lk_is_stressed_vowel(cp);
}

static int lk_is_glottal_stop(utf8proc_uint32_t cp) {
    return (cp == LK_QUOTE || cp == '\'' || cp == '`');
}

/* remove diacritic mark from a letter */
static utf8proc_uint32_t lk_char_to_ascii(utf8proc_uint32_t cp) {
    if (cp == '`' || cp == LK_QUOTE)
        return '\'';

    for (size_t idx = 0; idx < sizeof(lk_low_case)/sizeof(lk_low_case[0]); idx++) {
        if (cp == lk_low_case[idx])
            return lk_low_ascii[idx];
    }

    return cp;
}

/* add diacritic mark to a vowel */
static utf8proc_uint32_t lk_unstress_to_stress(utf8proc_uint32_t cp) {
    switch (cp) {
        case 'a':
            return LK_A_LOW;
        case 'e':
            return LK_E_LOW;
        case 'i':
            return LK_I_LOW;
        case 'o':
            return LK_O_LOW;
        case 'u':
            return LK_U_LOW;
    }
    return cp;
}

/* remove diacritic mark from a vowel */
static utf8proc_uint32_t lk_stress_to_unstress(utf8proc_uint32_t cp) {
    if (cp < 128)
        return cp;

    switch (cp) {
        case LK_A_LOW:
            return 'a';
        case LK_E_LOW:
            return 'e';
        case LK_I_LOW:
            return 'i';
        case LK_O_LOW:
            return 'o';
        case LK_U_LOW:
            return 'u';
    }
    return cp;
}

/* length of a UTF8 character in bytes */
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

/* run a function for the whole string - used by ustr_lowcase etc */
static lk_result process_ustr(char *src, ustr_func fn) {
    if (src == NULL)
        return LK_OK;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)src;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)src;
    utf8proc_int32_t cpsrc, cpdst;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cpsrc);
        if (cpsrc == -1)
            return LK_INVALID_STRING;

        cpdst = (*fn)(cpsrc);
        usrc += len;

        size_t lendst = cp_length(cpdst);
        if (lendst > len)
            return LK_BUFFER_SMALL;

        lendst = utf8proc_encode_char(cpdst, udst);
        udst += lendst;
    }

    return LK_OK;
}

/* convert a string to low case inplace */
static lk_result ustr_lowcase(char *src) {
    return process_ustr(src, utf8proc_tolower);
}

/**
 * Checks if the string ends with certain substring
 *
 * @param[in] orig is a string to check
 * @param[in] cmp is a substring to look at the string end
 *
 * @return 1 the string ends with the substring and 0 otherwise
 */
int lk_ends_with(const char *orig, const char *cmp) {
    if (orig == NULL && cmp == NULL)
        return 1;
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

static lk_result fix_glottal_stop(const char *word, char *out, size_t out_sz) {
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return LK_INVALID_STRING;

        if (cp == '\'' || cp == '`')
            cp = LK_QUOTE;

        usrc += len;
        len = cp_length(cp);
        if (len >= out_sz)
            return LK_BUFFER_SMALL;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

/**
 * Converts a string to lowcase. All single quotes and apostrophes are replaced
 *  with glottal stop as well
 *
 * @param[in] word is the original string
 * @param[out] out is the output buffer to put lowcased string
 * @param[in] out_sz the output buffer capacity
 *
 * @return
 *  LK_OK - successfull operation
 *  LK_INVALID_ARG - if word is NULL or output buffer is the same as
 *   the original string. The latest check is done because it is not possible
 *   to convert to lowcase inplace since it may convert quote mark to glottal
 *   stop that is 2 bytes long
 *  LK_BUFFER_SMALL - either out is NULL or its size is too small to keep
 *   the result
 *  LK_INVALID_STRING - the original string is not correct UTF8 sequence
 */
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

/**
 * Returns 1 if the word ends with ablaut, 0 - otherwise
 */
int lk_has_ablaut(const char *word) {
    if (word == NULL)
        return 0;

    if (lk_ends_with(word, "A")
        || lk_ends_with(word, "Aŋ")
        || lk_ends_with(word, "Á")
        || lk_ends_with(word, "Áŋ")
        )
        return 1;

    return 0;
}

/**
 * Returns 1 if the word ends with ablaut and it is stressed, 0 - otherwise
 */
int lk_is_ablaut_stressed(const char *word) {
    if (word == NULL)
        return 0;

    if (lk_ends_with(word, "Á")
        || lk_ends_with(word, "Áŋ")
        )
        return 1;

    return 0;
}

/**
 * Returns the number of stressed vowels in the word.
 * May return 0 if the word is not correct UTF8 sequence
 */
int lk_stressed_vowels_no(const char *word) {
    if (word == NULL)
        return 0;

    int cnt = 0;
    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;

    while (*uw) {
        size_t len = utf8proc_iterate(uw, -1, &cp);
        if (cp == -1)
            return 0;

        if (lk_is_stressed_vowel(cp))
            cnt++;

        uw += len;
    }

    return cnt;
}

/**
 * Returns the total number of vowels in the word.
 * May return 0 if the word is not correct UTF8 sequence
 */
int lk_vowels_no(const char *word) {
    if (word == NULL)
        return 0;

    int cnt = 0;
    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;

    while (*uw) {
        size_t len = utf8proc_iterate(uw, -1, &cp);
        if (cp == -1)
            return 0;

        if (lk_is_vowel(cp))
            cnt++;

        uw += len;
    }

    return cnt;
}

/**
 * Returns 1 if the word contains only ASCII charactes, and 0 - otherwise
 */
int lk_is_ascii(const char *word) {
    if (word == NULL)
        return 0;

    utf8proc_uint8_t *uw = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;

    while (*uw) {
        size_t len = utf8proc_iterate(uw, -1, &cp);
        if (cp == -1 || cp > 127)
            return 0;

        uw += len;
    }

    return 1;
}

/**
 * Converts a string to ASCII: diacritic marks are removed and glottal stop
 *  are replaced with single quote mark
 *
 * @param[in] word is the original string
 * @param[out] out is the output buffer to put lowcased string
 * @param[in] out_sz the output buffer capacity
 *
 * @return
 *  LK_OK - successfull operation
 *  LK_INVALID_ARG - if word is NULL or out is NULL
 *  LK_BUFFER_SMALL - either out is NULL or its size is too small to keep
 *   the result
 *  LK_INVALID_STRING - the original string is not correct UTF8 sequence
 */
lk_result lk_to_ascii(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return LK_INVALID_STRING;

        cp = lk_char_to_ascii(cp);
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

/**
 * Remove diacritic marks from all vowels in the string
 *
 * @param[in] word is the original string
 * @param[out] out is the output buffer to put lowcased string
 * @param[in] out_sz the output buffer capacity
 *
 * @return
 *  LK_OK - successfull operation
 *  LK_INVALID_ARG - if word is NULL or out is NULL
 *  LK_BUFFER_SMALL - either out is NULL or its size is too small to keep
 *   the result
 *  LK_INVALID_STRING - the original string is not correct UTF8 sequence
 */
lk_result lk_destress(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return LK_INVALID_STRING;

        cp = lk_stress_to_unstress(cp);
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

/**
 * Put stress on a vowel in the string
 *
 * @param[in] word the original string
 * @param[in] pos the ordinal number of the vowel to put stress (count starts
 *  from 0). If pos is negative then the default position is used (see,
 *  define LK_STRESS_DEFAULT). If pos is greater than the number of vowel then
 *  the latest gets stressed
 *
 * @return the result of operation:
 *  LK_OK - the stress put successfully
 *  LK_INVALID_ARG - either word or out is NULL, or pos greater than 10 or
 *   out equals word, or the word does not contain any vowel
 *  LK_BUFFER_SMALL - the output buffer is too small to keep a new string
 *  LK_INVALID_STRING - the string is not valid UTF8 sequence
 */
lk_result lk_put_stress(const char *word, int pos, char *out, size_t out_sz) {
    if (word == NULL || out == NULL || pos > 10 || word == out)
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

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);

        if (cp == -1) {
            return LK_INVALID_STRING;
        }

        if (pos >= 0 && lk_is_vowel(cp)) {
            if (pos == 0)
                cp = lk_unstress_to_stress(cp);
            pos--;
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
    return pos < 0 ? LK_OK : LK_INVALID_ARG;
}

/**
 * Removes all glotal stops from the string. Glottal stop is one of LK_QUOTE,
 *  single quote mark or apostroph
 *
 * @return the result of operation:
 *  LK_OK - the stress put successfully
 *  LK_INVALID_ARG - either word or out is NULL
 *  LK_BUFFER_SMALL - the output buffer is too small to keep a new string
 *  LK_INVALID_STRING - the string is not valid UTF8 sequence
 */
lk_result lk_remove_glottal_stop(const char *word, char *out, size_t out_sz) {
    if (word == NULL || out == NULL)
        return LK_INVALID_ARG;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_uint8_t *udst = (utf8proc_uint8_t*)out;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;

        if (lk_is_glottal_stop(cp))
            continue;

        if (cp_length(cp) >= out_sz)
            return LK_BUFFER_SMALL;

        len = utf8proc_encode_char(cp, udst);
        udst += len;
        out_sz -= len;
    }
    *udst = '\0';

    return LK_OK;
}

/**
 * Returns 1 if a string contains glotal stop. Glottal stop is one of LK_QUOTE,
 *  single quote mark or apostroph.
 */
int lk_has_glottal_stop(const char *word) {
    if (word == NULL)
        return 0;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)word;
    utf8proc_int32_t cp;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;

        if (lk_is_glottal_stop(cp))
            return 1;
    }

    return 0;
}

static int is_lk_char(utf8proc_uint32_t c) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == LK_QUOTE)
        return 1;

    size_t sz = sizeof(lk_low_case) / sizeof(lk_low_case[0]);
    for (size_t idx = 0; idx < sz; idx++) {
        if (lk_low_case[idx] == c)
            return 1;
    }

    sz = sizeof(lk_up_case) / sizeof(lk_up_case[0]);
    for (size_t idx = 0; idx < sz; idx++) {
        if (lk_up_case[idx] == c)
            return 1;
    }

    return 0;
}

/**
 * Looks for the beginning of a word that contains the byte number pos in it
 *  or a word that preceeds this byte if pos points to a non-letter character.
 *
 *  @param[in] str the string to look for a word
 *  @param[in] pos the position(the byte number) to start look for a word. The
 *   function looks for a word in reverse way (towards the string start from
 *   the initial position). It is OK if pos points to a byte in the middle of
 *   a UTF8 character
 *
 * @return the pointer to a character that is the first character of a word,
 *  or NULL in case of it is impossible(str is NULL, or str is not valid UTF8
 *  sequence, or between string start and given position there is no letters,
 *  eg. pos == 0 and the first character of str is double quote or digit)
 *
 * Examples:
 *    lk_word_begin("test str", 4) -> pointer to the first character, because
 *     4th byte is space, and moving backwards gets to the string beginning
 *    lk_word_begin("test str", 5) -> pointer to "str", because 5th byte is
 *    the character that starts the second word
 *
 * How it works:
 *  1. skips all non-letter characters backwards
 *  2. skips all letter-like characters backwards
 *  3. returns either the pointer to original string or to the first letter-like
 *     character that follows the current whitespace
 */
const char* lk_word_begin(const char *str, size_t pos) {
    if (str == NULL || pos >= strlen(str))
        return NULL;

    const char *wbegin = NULL;
    const char *idx = str + pos, *save = NULL;
    const char *begin = idx;
    utf8proc_uint8_t *usrc;
    utf8proc_int32_t cp;

    if (pos == 0) {
        usrc = (utf8proc_uint8_t *)idx;
        utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return NULL;

        return is_lk_char(cp) ? str : NULL;
    }

    lk_state state = LK_STATE_SKIP_WHITE;
    while (idx != str) {
        if ((((unsigned char)*idx) & 0xC0) == 0x80) {
            --idx;
            continue;
        }

        usrc = (utf8proc_uint8_t *)idx;
        utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return NULL;

        int is_lk = is_lk_char(cp);
        int is_quote = (cp == '\'' || cp == '`');

        if (is_lk) {
            if (state == LK_STATE_SKIP_WHITE || state == LK_STATE_QUOTE) {
                state = LK_STATE_GOBBLE;
            }
            save = idx--;
            if (idx == str) {
                usrc = (utf8proc_uint8_t *)idx;
                utf8proc_iterate(usrc, -1, &cp);
                if (! is_lk_char(cp))
                    state = LK_STATE_DONE;
            }
        } else if (is_quote)  {
            if (state == LK_STATE_GOBBLE) {
                state = LK_STATE_QUOTE;
            } else if (state == LK_STATE_QUOTE) {
                state = LK_STATE_DONE;
                break;
            }
            --idx;
            if (idx == str) {
                if (save != NULL && begin != save)
                    state = LK_STATE_DONE;
                else
                    state = LK_STATE_SKIP_WHITE;
            }
        } else {
            if (state == LK_STATE_GOBBLE) {
                state = LK_STATE_DONE;
                break;
            }
            --idx;
        }
    }

    if (state == LK_STATE_DONE)
        return save;
    if (state == LK_STATE_GOBBLE && idx == str)
        return idx;

    return NULL;
}

/**
 * Reads the next word in the string starting from the string beginning.
 *
 * @param[in] str is the string to look for a word
 * @param[out] len is filled with the length in bytes of the found word. The
 *  argument can be NULL - in this case the length of the current word is not
 *  returned to a caller.
 *
 * @return pointer to the first character of the found word or NULL in case of
 *  any error (str is NULL or no word was found in the string, e.g. is contains
 *  only digits. So, if the function returns non-NULL result you can read the
 *  found word with strncpy(<dest>, <result>, len)
 *
 * How it works:
 *  1. skips all non-letter characters
 *  2. marks the current character as the word beginning
 *  3. while str is not ended and the next character is letter-like moves forward
 *  4. returns the pointer to the word first character and the length of the word
 */
const char* lk_next_word(const char *str, size_t *len) {
    utf8proc_int32_t cp;
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t *)str, *save;
    const char *wstart = NULL;

    if (str == NULL)
        return NULL;

    if (len)
        *len = 0;

    lk_state state = LK_STATE_SKIP_WHITE;
    while (*usrc) {
        if (state == LK_STATE_SKIP_WHITE && (((unsigned char)*usrc) & 0xC0) == 0x80) {
            ++usrc;
            continue;
        }

        size_t cplen = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return NULL;

        int is_lk = is_lk_char(cp);
        int is_quote = (cp == '\'' || cp == '`');

        if (is_lk) {
            if (state == LK_STATE_SKIP_WHITE) {
                wstart = (const char *)usrc;
                state = LK_STATE_GOBBLE;
            } else if (state == LK_STATE_QUOTE) {
                state = LK_STATE_GOBBLE;
            }
        } else if (is_quote) {
            if (state == LK_STATE_GOBBLE) {
                state = LK_STATE_QUOTE;
                save = usrc;
            } else if (state == LK_STATE_QUOTE) {
                usrc = save;
                break;
            }
        } else {
            if (state == LK_STATE_GOBBLE) {
                break;
            } else if (state == LK_STATE_QUOTE) {
                usrc = save;
                break;
            }
        }

        usrc += cplen;
    }

    if (!wstart)
        return NULL;

    if (len)
        *len = (const char*)usrc - wstart;

    return wstart;
}
