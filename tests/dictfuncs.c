#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "lk_common.h"
/* #include "utf8proc.h" */
#include "lk_file.h"
#include "lk_dict.h"
#include "lk_tree.h"

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

    r = lk_parse_word("lapa milapa nilapa", dict);
    ut_assert("Word #1 parsed", r == LK_OK && lk_word_count(dict) == 3);
    r = lk_parse_word("# kin", dict);
    ut_assert("Comment parsed", r == LK_COMMENT && lk_word_count(dict) == 3);
    r = lk_parse_word("aga", dict);
    ut_assert("simple service word parsed", r == LK_OK && lk_word_count(dict) == 4);
    r = lk_parse_word("kin", dict);
    ut_assert("Ablauting parsed", r == LK_OK && lk_word_count(dict) == 5);
    r = lk_parse_word("sapA", dict);
    ut_assert("Word #2 - with unstressed ablaut parsed", r == LK_OK && lk_word_count(dict) == 6);
    r = lk_parse_word("ditÁŋ", dict);
    ut_assert("Word #3 - witn stressed ablaut parsed", r == LK_OK && lk_word_count(dict) == 7);
    r = lk_parse_word("kárAŋ mikárAŋ", dict);
    ut_assert("Word #4 - unstressed ablaut 2 forms parsed", r == LK_OK && lk_word_count(dict) == 9);

    lk_dict_close(dict);

    return 0;
}

const char* test_search() {
    struct lk_dictionary *dict = lk_dict_init();
    lk_parse_word("ktA", dict);
    lk_parse_word("lapa milapa nilapa", dict);
    lk_parse_word("kiŋ", dict);
    lk_parse_word("zédún wazédunpi zéduns", dict);
    lk_parse_word("uya wauya wauyapi uyae", dict);
    lk_parse_word("sápA masápA sapápi kunísapA", dict);
    lk_parse_word("he", dict);

    const char* words[] = {
        "lapa", "nilapa",
        "kta", "zédún", "wazédunpi", "zéduns",
        "uya", "wauya", "wauyapi", "uyae",
        "sápa", "masápa", "sapápi", "kunísapa",
        "he",
    };

    const struct lk_word_ptr* wptr;

    printf("Looking for main words\n");
    for (size_t idx = 0; idx < sizeof(words)/sizeof(words[0]); idx ++) {
        wptr = lk_dict_find_word(dict, words[idx]);
        ut_assert(words[idx],  wptr != NULL);
    }

    lk_dict_close(dict);

    return 0;
}

const char* test_lookup() {
    struct lk_dictionary *dict = lk_dict_init();
    lk_parse_word("kta", dict);
    lk_parse_word("lapa milapa nilapa", dict);
    lk_parse_word("kiŋ", dict);
    lk_parse_word("zédún wazédunpi wazédunpis", dict);
    lk_parse_word("uya wauya wauyapi uyae", dict);
    lk_parse_word("sápa masápa sapápi kunísapa", dict);
    lk_parse_word("he", dict);
    lk_parse_word("číkʼalA mačíkʼala", dict);
    lk_parse_word("kóla makolá", dict);
    lk_parse_word("kolá mákʼóla", dict);

    int cnt = 0;
    char **lookup;

    lookup = lk_dict_exact_lookup(dict, "kiŋg", &cnt);
    ut_assert("Non-existent word", cnt == -LK_WORD_NOT_FOUND && lookup == NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "kiŋ", &cnt);
    ut_assert("Exact match", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "kunisapa", &cnt);
    printf("-> %s\n",lookup[0]);
    ut_assert("Ascii match #2", cnt == 1 && lookup != NULL && strcmp(lookup[0], "kunísapa") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "zedun", &cnt);
    ut_assert("Ascii match #3", cnt == 1 && lookup != NULL && strcmp(lookup[0], "zédún") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "sapá", &cnt);
    ut_assert("Incorrect stress #1", cnt == 1 && lookup != NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "zédun", &cnt);
    ut_assert("Incorrect stress #2", cnt == 1 && lookup != NULL);
    lk_exact_lookup_free(lookup);

    /* test apostrophes */
    lookup = lk_dict_exact_lookup(dict, "mačíkʼala", &cnt);
    ut_assert("Glottal #1", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačík'ala", &cnt);
    ut_assert("Glottal #2", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačík`ala", &cnt);
    ut_assert("Glottal #3", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "mačíkala", &cnt);
    ut_assert("Glottal #4", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);
    lookup = lk_dict_exact_lookup(dict, "macikala", &cnt);
    ut_assert("Glottal #5", cnt == 1 && lookup != NULL && strcmp(lookup[0], "mačíkʼala") == 0);
    lk_exact_lookup_free(lookup);

    /* test multi-fit */
    lookup = lk_dict_exact_lookup(dict, "kola", &cnt);
    ut_assert("Multifit #1", cnt == 2 && lookup != NULL && (
                ( strcmp(lookup[0], "kóla") == 0 && strcmp(lookup[1], "kolá") == 0) ||
                ( strcmp(lookup[1], "kóla") == 0 && strcmp(lookup[0], "kolá") == 0)
                ));
    lk_exact_lookup_free(lookup);

    lk_dict_close(dict);

    return 0;
}

const char* test_dict_load() {
    FILE *f = fopen("lk.dict", "wb");
    ut_assert("File created", f != 0);

    fputs("kta\n", f);
    fputs("lapa milapa nilapa\n", f);
    fputs("kiŋ\n", f);
    fputs("zédún wazédunpi wazédunpis\n", f);
    fputs("uya wauya wauyapi uyae\n", f);
    fputs("sápa masápa sapápi kunísapa\n", f);
    fputs("he\n", f);
    fputs("číkʼalA mačíkʼala\n", f);
    fputs("kóla makolá\n", f);
    fputs("kolá mákʼóla\n", f);
    fclose(f);

    struct lk_dictionary *dict = lk_dict_init();
    lk_result r = lk_read_dictionary(dict, "lk.dict");
    ut_assert("Reading dictionary", r == LK_OK && lk_word_count(dict) > 0);

    int cnt = 0;
    char **lookup;

    lookup = lk_dict_exact_lookup(dict, "kunisapa", &cnt);
    ut_assert("Ascii match", cnt == 1 && lookup != NULL && strcmp(lookup[0], "kunísapa") == 0);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "zédun", &cnt);
    ut_assert("Incorrect stress", cnt == 1 && lookup != NULL);
    lk_exact_lookup_free(lookup);

    lookup = lk_dict_exact_lookup(dict, "mačíkʼala", &cnt);
    ut_assert("Glottal", cnt == 0 && lookup == NULL);
    lk_exact_lookup_free(lookup);

    /* test multi-fit */
    lookup = lk_dict_exact_lookup(dict, "kola", &cnt);
    ut_assert("Multifit", cnt == 2 && lookup != NULL && (
                ( strcmp(lookup[0], "kóla") == 0 && strcmp(lookup[1], "kolá") == 0) ||
                ( strcmp(lookup[1], "kóla") == 0 && strcmp(lookup[0], "kolá") == 0)
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
    ut_run_test("Dict load", test_dict_load);

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
