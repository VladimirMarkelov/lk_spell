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
#include "dictionary.h"
#include "suftree.h"

int skip_failed_pkg = 1;
#include "unittest.h"

int tests_run = 0;
int tests_fail = 0;

struct lk_word {
    struct lk_word *base;
    struct lk_word *next;

    char *word;
    char *contracted;
    int wtype;
};


const char* test_parse() {
    lk_result r;

    struct lk_dictionary *dict = lk_dict_init();

    r = lk_parse_word("S lapa milapa nilapa", dict);
    ut_assert("Word #1 parsed", r == LK_OK && lk_word_count(dict) == 3);
    r = lk_parse_word("#S kin", dict);
    ut_assert("Comment parsed", r == LK_COMMENT && lk_word_count(dict) == 3);
    r = lk_parse_word("- aga", dict);
    ut_assert("simple service word parsed", r == LK_OK && lk_word_count(dict) == 4);
    r = lk_parse_word("-:n kin", dict);
    ut_assert("Ablauting parsed", r == LK_OK && lk_word_count(dict) == 5);
    r = lk_parse_word("S:sab sapA", dict);
    ut_assert("Word #2 - with unstressed ablaut parsed", r == LK_OK && lk_word_count(dict) == 9);
    r = lk_parse_word("S ditÍŋ", dict);
    ut_assert("Word #3 - witn stressed ablaut parsed", r == LK_OK && lk_word_count(dict) == 13);
    r = lk_parse_word("S kárAŋ mikárAŋ", dict);
    ut_assert("Word #4 - unstressed ablaut 2 forms parsed", r == LK_OK && lk_word_count(dict) == 21);
    r = lk_parse_word("t zeden wa~ ~pi", dict);
    ut_assert("Word #5 - tilde parsed", r == LK_OK && lk_word_count(dict) == 24);
    r = lk_parse_word("t zédún wa@pi @s", dict);
    ut_assert("Word #6 - AT parsed", r == LK_OK && lk_word_count(dict) == 27);
    r = lk_parse_word("t uya wa\% \%pi ~e", dict);
    ut_assert("Word #7 - percent parsed", r == LK_OK && lk_word_count(dict) == 31);

    /* _print_dict(dict); */

    lk_dict_close(dict);

    return 0;
}

const char* test_search() {
    struct lk_dictionary *dict = lk_dict_init();
    lk_parse_word("S lapa milapa nilapa", dict);
    lk_parse_word("-:n ktA", dict);
    lk_parse_word("t zédún wa@pi @s", dict);
    lk_parse_word("t uya wa\% \%pi ~e", dict);
    lk_parse_word("S sápA ma~ sapápi kuni@", dict);
    lk_parse_word("-:a he", dict);

    const char* words[] = {
        "lapa", "nilapa",
        "kta", "ktiŋ", "kte",
        "zédún", "wazédunpi", "zedúns",
        "uya", "wauya", "wauyapi", "uyae",
        "sápa", "masápa", "sapápi", "kunísapa",
        "he",
    };

    const struct lk_word_ptr* wptr;
    lk_ablaut ab;

    printf("Looking for main words\n");
    for (size_t idx = 0; idx < sizeof(words)/sizeof(words[0]); idx ++) {
        wptr = lk_dict_find_word(dict, words[idx]);
        ut_assert(words[idx],  wptr != NULL);
    }

    printf("Looking for no ablaut\n");
    const char* no_ab[] = {
        "milapa", "uya", "kto",
    };
    for (size_t idx = 0; idx < sizeof(no_ab)/sizeof(no_ab[0]); idx ++) {
        ab = lk_dict_check_ablaut(dict, no_ab[idx]);
        ut_assert(no_ab[idx], ab == LK_ABLAUT_0);
    }

    printf("Looking for yes ablaut\n");
    const char* yes_ab[] = {
        "kta", "kte", "ktiŋ", "he",
    };
    for (size_t idx = 0; idx < sizeof(yes_ab)/sizeof(yes_ab[0]); idx ++) {
        ab = lk_dict_check_ablaut(dict, yes_ab[idx]);
        ut_assert(yes_ab[idx], ab == (idx > 2 ? LK_ABLAUT_A : LK_ABLAUT_N));
    }

    lk_dict_close(dict);

    return 0;
}

const char* test_lookup() {
    struct lk_dictionary *dict = lk_dict_init();
    lk_parse_word("-:n ktA", dict);
    lk_parse_word("S lapa milapa nilapa", dict);
    lk_parse_word("- kiŋ", dict);
    lk_parse_word("t zédún wa@pi @s", dict);
    lk_parse_word("t uya wa\% \%pi ~e", dict);
    lk_parse_word("S sápA ma~ sapápi kuni@", dict);
    lk_parse_word("-:a he", dict);
    lk_parse_word("s číkʼalA ma~", dict);
    lk_parse_word("s kóla makolÁ", dict);
    lk_parse_word("s kolá mákʼólA", dict);

    int cnt = 0;
    char **lookup;

    lookup = lk_dict_exact_lookup(dict, "kiŋg", NULL, &cnt);
    ut_assert("Non-existent word", cnt == -LK_WORD_NOT_FOUND && lookup == NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "kiŋ", NULL, &cnt);
    ut_assert("Exact match", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "kte", NULL, &cnt);
    ut_assert("Exact match ablauted form", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "ktin", NULL, &cnt);
    ut_assert("Ascii match #1", cnt == 1 && lookup != NULL && strcmp(lookup[0], "ktiŋ") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "kunisape", NULL, &cnt);
    ut_assert("Ascii match #2", cnt == 1 && lookup != NULL && strcmp(lookup[0], "kunísape") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "zedun", NULL, &cnt);
    ut_assert("Ascii match #3", cnt == 1 && lookup != NULL && strcmp(lookup[0], "zédún") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "sapá", NULL, &cnt);
    /* printf("Found %d = %p\n", cnt, lookup); */
    /* if (lookup) { for (size_t ii=0; ii<cnt; ++ii) printf("   + [%s]\n", lookup[ii]); }; */
    ut_assert("Incorrect stress #1", cnt == 1 && lookup != NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "zédun", NULL, &cnt);
    ut_assert("Incorrect stress #2", cnt == 1 && lookup != NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "sápa", ".", &cnt);
    /* printf("Found %d = %p\n", cnt, lookup); */
    /* if (lookup) { for (size_t ii=0; ii<cnt; ++ii) printf("   + [%s]\n", lookup[ii]); }; */
    ut_assert("Incorrect ablaut form #1", cnt == 3 && lookup != NULL
            && strcmp(lookup[1], "-") == 0
            && strcmp(lookup[2], "sápe") == 0
            && strcmp(lookup[0], "sápa") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "sapá", ".", &cnt);
    /* printf("Found %d = %p\n", cnt, lookup); */
    /* if (lookup) { for (size_t ii=0; ii<cnt; ++ii) printf("   + [%s]\n", lookup[ii]); }; */
    ut_assert("Incorrect ablaut and stress form #1", cnt == 3 && lookup != NULL
            && strcmp(lookup[1], "-") == 0
            && strcmp(lookup[2], "sápe") == 0
            && strcmp(lookup[0], "sápa") == 0);
    lk_exact_lookup_free(lookup);

    /* test apostrophes */
    lookup = lk_dict_exact_lookup(dict, "mačíkʼala", NULL, &cnt);
    ut_assert("Glottal #1", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačík'ala", NULL, &cnt);
    ut_assert("Glottal #2", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačík`ala", NULL, &cnt);
    ut_assert("Glottal #3", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačíkala", NULL, &cnt);
    ut_assert("Glottal #4", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "macikala", NULL, &cnt);
    ut_assert("Glottal #5", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);

    /* test multi-fit */
    lookup = lk_dict_exact_lookup(dict, "kola", NULL, &cnt);
    ut_assert("Multifit #1", cnt == 2 && lookup != NULL && (
                ( strcmp(lookup[0], "kóla") == 0 && strcmp(lookup[1], "kolá") == 0) ||
                ( strcmp(lookup[1], "kóla") == 0 && strcmp(lookup[0], "kolá") == 0)
                ));
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "makolin", NULL, &cnt);
    ut_assert("Multifit #2", cnt == 2 && lookup != NULL && (
                ( strcmp(lookup[0], "makolíŋ") == 0 && strcmp(lookup[1], "mákʼóliŋ") == 0) ||
                ( strcmp(lookup[1], "makolíŋ") == 0 && strcmp(lookup[0], "mákʼóliŋ") == 0)
                ));
    lk_exact_lookup_free(lookup);

    lk_dict_close(dict);

    return 0;
}

const char * run_all_test() {
    printf("=== Basic operations ===\n");

    ut_run_test("Dict basics", test_parse);
    ut_run_test("Dict search", test_search);
    ut_run_test("Dict suggestions", test_lookup);

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
