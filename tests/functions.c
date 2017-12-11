#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "common.h"
/* #include "utf8proc.h" */
#include "filereader.h"
#include "wordutils.h"

int skip_failed_pkg = 0;
#include "unittest.h"

int tests_run = 0;
int tests_fail = 0;

const char* test_fileopen() {
    FILE *ftxt = fopen("abcd.txt", "wb");
    ut_assert("File created", ftxt != 0);
    fputs("example\n", ftxt);
    fputs("test2\n", ftxt);
    fclose(ftxt);

    struct lk_file *f = lk_file_open("abcd.txt");
    ut_assert("ASCII NAME open", lk_file_is_valid(f));
    lk_file_close(f);

#ifdef _WIN32
    _wputenv(L"LK_DICTIONARY=abcd.txt");
#else
    setenv("LK_DICTIONARY=abcd.txt");
#endif
    f = lk_file_open(NULL);
    ut_assert("Open from env var", lk_file_is_valid(f));
    lk_file_close(f);

    return 0;
}

const char* test_fileread() {
#ifdef _WIN32
    FILE *ftxt = _wfopen(L"абвг.txt", L"wb");
#else
    FILE *ftxt = fopen("абвг.txt", "wb");
#endif
    ut_assert("File created", ftxt != 0);
    fputs("test\n", ftxt);
    fputs("example\n", ftxt);
    fputs("1234", ftxt);
    fclose(ftxt);

    char buf[64] = {0};
    lk_result r;

    struct lk_file *f = lk_file_open("abcd.txt");
    ut_assert("ASCII NAME open", lk_file_is_valid(f));

    r = lk_file_read(f, buf, 16);
    ut_assert("Read #1 success", r == LK_OK);
    ut_assert("Read #1 correct", strcmp(buf, "example") == 0);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #2 success", r == LK_OK);
    ut_assert("Read #2 correct", strcmp(buf, "test2") == 0);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #3 eof", r == LK_EOF);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #4 eof", r == LK_EOF);
    lk_file_close(f);

    f = lk_file_open("абвг.txt");
    ut_assert("UTF8 NAME open", lk_file_is_valid(f));

    r = lk_file_read(f, buf, 16);
    ut_assert("Read #11 success", r == LK_OK);
    ut_assert("Read #11 correct", strcmp(buf, "test") == 0);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #12 success", r == LK_OK);
    ut_assert("Read #12 correct", strcmp(buf, "example") == 0);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #13 success", r == LK_OK);
    ut_assert("Read #13 correct", strcmp(buf, "1234") == 0);
    r = lk_file_read(f, buf, 16);
    ut_assert("Read #14 eof", r == LK_EOF);
    lk_file_close(f);

    f = lk_file_open("abcde.txt");
    ut_assert("ASCII NAME open fail", !lk_file_is_valid(f));
    r = lk_file_read(f, buf, 16);
    ut_assert("Read invalid file", r == LK_INVALID_FILE);
    lk_file_close(f);

    f = lk_file_open("abcd.txt");
    buf[1] = '*';
    r = lk_file_read(f, buf, 1);
    ut_assert("Read small buffer", r == LK_BUFFER_SMALL && buf[0] == 'e' && buf[1] == '*');
    r = lk_file_read(f, buf, 10);
    ut_assert("Read the rest", r == LK_OK && strcmp(buf, "xample") == 0);
    lk_file_close(f);

    return 0;
}

const char* test_lowcase() {
    char *a[] = {
        NULL,
        "sačmeá",
        "tESt`a'b",
        "vow - 'ФлÁiÍbcúÚÉeoóÓíÍ`",
        "con - nŋŊčaČŠhgŽžčš",
        "hH - ȟȞ",
        "gG - ǧǦ",
    };
    char dest[64];

    lk_result r;
    r = lk_to_low_case(a[1], NULL, 64);
    ut_assert("NULL dest lowcase", r == LK_BUFFER_SMALL);
    r = lk_to_low_case(a[0], dest, 64);
    ut_assert("NULL src lowcase", r == LK_INVALID_ARG);
    r = lk_to_low_case(a[1], a[1], 64);
    ut_assert("Dest == Src", r == LK_INVALID_ARG);
    r = lk_to_low_case(a[1], dest, 4);
    ut_assert("no change", r == LK_BUFFER_SMALL);
    r = lk_to_low_case(a[1], dest, 64);
    ut_assert("no change", r == LK_OK && strcmp(dest, "sačmeá") == 0);
    r = lk_to_low_case(a[2], dest, 64);
    ut_assert("glottal stop", r == LK_OK && strcmp(dest, "testʼaʼb") == 0);
    r = lk_to_low_case(a[3], dest, 64);
    ut_assert("vowels", r == LK_OK && strcmp(dest, "vow - ʼфлáiíbcúúéeoóóííʼ") == 0);
    r = lk_to_low_case(a[4], dest, 64);
    ut_assert("consonants", r == LK_OK && strcmp(dest, "con - nŋŋčačšhgžžčš") == 0);
    r = lk_to_low_case(a[5], dest, 64);
    ut_assert("H<", r == LK_OK && strcmp(dest, "hh - ȟȟ") == 0);
    r = lk_to_low_case(a[6], dest, 64);
    ut_assert("G<", r == LK_OK && strcmp(dest, "gg - ǧǧ") == 0);

    return 0;
}

const char* test_validword() {
    char *a[] = {
        "sačmeá",
        "tщt`a'b",
        "anc-haŋ",
        "čikʼála",
        "testŋ",
        "Ablaút",
        "te12stŋ",
    };
    int res[] = {1, 0, 1, 1, 1, 0, 0};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_is_valid_word(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_has_ablaut() {
    char *a[] = {
        NULL,
        "á",
        "can",
        "caŋ",
        "čik'alÁ",
        "čik'alÁŋ",
        "čik'alÁŊ",
        "čik'alÍŋ",
        "čik'alA",
        "čik'alAŋ",
        "čik'alAŊ",
        "čik'alIŋ",
    };
    int res[] = {0, 0, 0, 0, 1, 1, 0, 1, 1, 1, 0, 1};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_has_ablaut(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_is_ascii() {
    char *a[] = {
        NULL,
        "á",
        "f",
        "can",
        "čan",
        "caŋ",
        "čik'alÁ",
        "ik'alA",
    };
    int res[] = {0, 0, 1, 1, 0, 0, 0, 1};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_is_ascii(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_stressed_no() {
    char *a[] = {
        NULL,
        "á",
        "a",
        "čaní",
        "cáŋúg",
        "áóíúéŋ",
        "nothing",
    };
    int res[] = {0, 1, 0, 1, 2, 5, 0};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_stressed_vowels_no(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_vowel_no() {
    char *a[] = {
        NULL,
        "á",
        "a",
        "c",
        "čaní",
        "cáŋúg",
        "áóíúéŋare",
        "nothing",
    };
    int res[] = {0, 1, 1, 0, 2, 2, 7, 2};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_vowels_no(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_to_ascii() {
    char *a[] = {
        NULL,
        "sačmeá",
        "tESt`a'b",
        "vow - 'Флáiíbcúúéeoóóíí`",
        "con - nŋŋčačšhgžžčš",
        "hH - ȟȞ",
        "gG - ǧǦ",
    };
    char *b[] = {
        NULL,
        "sacmea",
        "tESt'a'b",
        "vow - 'Флaiibcuueeoooii'",
        "con - nnncacshgzzcs",
        "hH - hȞ",
        "gG - gǦ",
    };

    char dest[64];
    lk_result r = lk_to_ascii(a[1], dest, 3);
    ut_assert("No room", r == LK_BUFFER_SMALL);
    for (size_t idx = 0; idx < sizeof(a)/sizeof(a[0]); idx++) {
        r = lk_to_ascii(a[idx], dest, 64);
        if (idx == 0) {
            ut_assert("NULL ascii", r = LK_INVALID_ARG);
        } else {
            ut_assert(a[idx], r == LK_OK && strcmp(dest, b[idx]) == 0);
        }
    }

    return 0;
}

const char* test_destress() {
    char *a[] = {
        NULL,
        "sačmeá",
        "tESt`a'b",
        "vow - 'Флáiíbcúúéŋeoóóíí`",
        "con - nŋŋčačšhgžžčšȟǧ",
    };
    char *b[] = {
        NULL,
        "sačmea",
        "tESt`a'b",
        "vow - 'Флaiibcuueŋeoooii`",
        "con - nŋŋčačšhgžžčšȟǧ",
    };

    char dest[64];
    lk_result r = lk_destress(a[1], dest, 3);
    ut_assert("No room", r == LK_BUFFER_SMALL);
    for (size_t idx = 0; idx < sizeof(a)/sizeof(a[0]); idx++) {
        r = lk_destress(a[idx], dest, 64);
        if (idx == 0) {
            ut_assert("NULL destress", r = LK_INVALID_ARG);
        } else {
            ut_assert(a[idx], r == LK_OK && strcmp(dest, b[idx]) == 0);
        }
    }

    return 0;
}

const char* test_first_stressed() {
    char *a[] = {
        NULL,
        "á",
        "a",
        "čaní",
        "cáŋug",
        "aoíuéŋ",
        "aoiuéŋ",
        "nothing",
    };
    int res[] = {0, 1, 0, 2, 1, 3, 5, 0};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_first_stressed_vowel(a[i]);
        ut_assert(a[i], res[i] == r);
    }

    return 0;
}

const char* test_put_stress() {
    char *a[] = {
        NULL,
        "a",
        "v",
        "čaŋi",
        "caŋug",
        "aoíuéŋ",
    };

    char dest[64];
    lk_result r = lk_put_stress(a[3], 1, dest, 1);
    ut_assert("No room", r == LK_BUFFER_SMALL);
    r = lk_put_stress(a[0], 1, dest, 64);
    ut_assert("NULL", r == LK_INVALID_ARG);

    r = lk_put_stress(a[5], 1, dest, 64);
    ut_assert("long word #1.1", r == LK_OK && strcmp(dest, "aóíuéŋ") == 0);
    r = lk_put_stress(a[5], 2, dest, 64);
    ut_assert("long word #1.2", r == LK_OK && strcmp(dest, "aoíuéŋ") == 0);
    r = lk_put_stress(a[5], 0, dest, 64);
    ut_assert("long word #1.3", r == LK_OK && strcmp(dest, "áoíuéŋ") == 0);
    r = lk_put_stress(a[5], 4, dest, 64);
    ut_assert("long word #1.4", r == LK_OK && strcmp(dest, "aoíuéŋ") == 0);

    r = lk_put_stress(a[4], 1, dest, 64);
    ut_assert("long word #2.1", r == LK_OK && strcmp(dest, "caŋúg") == 0);
    r = lk_put_stress(a[4], 2, dest, 64);
    ut_assert("long word #2.2", r == LK_OK && strcmp(dest, "caŋúg") == 0);
    r = lk_put_stress(a[4], 0, dest, 64);
    ut_assert("long word #2.3", r == LK_OK && strcmp(dest, "cáŋug") == 0);

    r = lk_put_stress(a[3], 1, dest, 64);
    ut_assert("short word #1.1", r == LK_OK && strcmp(dest, "čaŋí") == 0);
    r = lk_put_stress(a[3], 0, dest, 64);
    ut_assert("short word #1.2", r == LK_OK && strcmp(dest, "čáŋi") == 0);

    r = lk_put_stress(a[2], 0, dest, 64);
    ut_assert("no vowel", r == LK_INVALID_ARG);

    r = lk_put_stress(a[1], 1, dest, 64);
    ut_assert("one vowel", r == LK_OK && strcmp(dest, "á") == 0);
    r = lk_put_stress(a[1], 0, dest, 64);
    ut_assert("one vowel", r == LK_OK && strcmp(dest, "á") == 0);

    return 0;
}

const char* test_remove_stop() {
    char *a[] = {
        "číkala",
        "číkʼala",
        "číkʼa'la",
        "číkʼa'l`a'ʼ`",
    };
    char buf[64] = {0};

    for (size_t i = 0; i < sizeof(a)/sizeof(a[0]); i++) {
        int r = lk_remove_glottal_stop(a[i], buf, 64);
        ut_assert(a[i], r == LK_OK && strcmp(buf, "číkala") == 0);
    }

    return 0;
}

const char* test_word_begin() {
    const char *pure_ascii = "some example string";
    const char *ascii = "'some' ex'ample s`tri'ng";
    const char *utf = "číkʼala mákiŋ";

    const char *w = lk_word_begin(pure_ascii, 30);
    ut_assert("Too big", w == NULL);
    w = lk_word_begin(pure_ascii, 0);
    ut_assert("Pure Ascii String start", w != NULL && *w == 's');
    w = lk_word_begin(pure_ascii, 2);
    ut_assert("Pure Ascii word #1.1", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(pure_ascii, 3);
    ut_assert("Pure Ascii word #1.2", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(pure_ascii, 4);
    ut_assert("Pure Ascii word #1.3", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(pure_ascii, 5);
    ut_assert("Pure Ascii word #2.1", w != NULL && *w == 'e' && *(w+1) == 'x');

    w = lk_word_begin(ascii, 0);
    ut_assert("Ascii String start", w == NULL);
    w = lk_word_begin(ascii, 3);
    ut_assert("Ascii word #1.2", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(ascii, 4);
    ut_assert("Ascii word #1.3", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(ascii, 5);
    ut_assert("Ascii word #1.4", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(ascii, 6);
    ut_assert("Ascii word #1.5", w != NULL && *w == 's' && *(w+1) == 'o');
    w = lk_word_begin(ascii, 10);
    ut_assert("Ascii word #2.1", w != NULL && *w == 'e' && *(w+1) == 'x');
    w = lk_word_begin(ascii, 23);
    ut_assert("Ascii word #3.1", w != NULL && *w == 's' && *(w+1) == '`');

    w = lk_word_begin(utf, 0);
    ut_assert("UTF8 String start", w == utf);
    w = lk_word_begin(utf, 2);
    ut_assert("UTF8 word #1.1", w == utf);
    w = lk_word_begin(utf, 3);
    ut_assert("UTF8 word #1.2", w == utf);
    w = lk_word_begin(utf, 4);
    ut_assert("UTF8 word #1.3", w == utf);
    w = lk_word_begin(utf, 6);
    ut_assert("UTF8 word #1.4", w == utf);
    w = lk_word_begin(utf, 8);
    ut_assert("UTF8 word #1.5", w == utf);
    w = lk_word_begin(utf, 12);
    ut_assert("UTF8 word #2.1", w != NULL && *w == 'm');
    w = lk_word_begin(utf, 15);
    ut_assert("UTF8 word #2.2", w != NULL && *w == 'm');

    return 0;
}

const char* test_next_word() {
    const char *pure_ascii = "some example string";
    const char *ascii = "'some' ex'ample s`tri'ng";
    const char *utf = "číkʼala mákiŋ";

    const char *w;
    size_t wlen = 0;
    w = lk_next_word(pure_ascii, &wlen);
    ut_assert("ASCII pure - word #1.1", w == pure_ascii && wlen == 4);
    w = lk_next_word(w+wlen-1, &wlen);
    ut_assert("ASCII pure - word #1.2", w == w && wlen == 1);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII pure - word #2.1", w == pure_ascii+5 && wlen == 7);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII pure - word #3.1", w == pure_ascii+5+8 && wlen == 6);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII pure - word #4.1", w == NULL);

    w = lk_next_word(ascii, &wlen);
    ut_assert("ASCII - word #1.1", w == ascii+1 && wlen == 4);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII - word #2.1", w == ascii+7 && wlen == 8);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII - word #3.1", w == ascii+7+9 && wlen == 8);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("ASCII - word #4.1", w == NULL);

    w = lk_next_word(utf, &wlen);
    ut_assert("UTF - word #1.1", w == utf && wlen == 10);
    w = lk_next_word(utf+1, &wlen);
    ut_assert("UTF - word #1.2", w == utf+2 && wlen == 8);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("UTF - word #2.1", w == utf+11 && wlen == 7);
    w = lk_next_word(w+wlen, &wlen);
    ut_assert("UTF - word #3.1", w == NULL);

    return 0;
}

const char * run_all_test() {
    printf("=== Basic operations ===\n");

    ut_run_test("File open", test_fileopen);
    ut_run_test("File read", test_fileread);

    ut_run_test("lowcase", test_lowcase);
    ut_run_test("valid word", test_validword);
    ut_run_test("has ablaut", test_has_ablaut);
    ut_run_test("count stressed", test_stressed_no);
    ut_run_test("count vowels", test_vowel_no);
    ut_run_test("is ascii", test_is_ascii);
    ut_run_test("to ascii", test_to_ascii);
    ut_run_test("destress", test_destress);
    ut_run_test("first stressed", test_first_stressed);
    ut_run_test("put stress", test_put_stress);
    ut_run_test("remove glottal stop", test_remove_stop);
    ut_run_test("begin of word", test_word_begin);
    ut_run_test("next word", test_next_word);

    /*
    char *cc = "áóéíúÁÓÉÍÚŋŊčČžŽȟȞǧǦšŠ";
    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)cc;
    utf8proc_int32_t cp;
    size_t len;
    for (int i=0; i < 22; i++) {
        len = utf8proc_iterate(usrc, -1, &cp);
        usrc += len;
        printf("%d - [%d] - %u\n", i, (int)len, cp);
    }
    */

    return 0;
}

int main (int argc, char** argv) {

    const char* res = run_all_test();
    if (res && tests_fail == 0) {
        printf("%s\n", res);
    } else {
        printf("Tests run: %d\nSuccess: %d\nFail: %d\n", tests_run, tests_run - tests_fail, tests_fail);
    }
    return 0;
}
