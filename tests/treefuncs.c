#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "common.h"
#include "utf8proc.h"
#include "suftree.h"

int skip_failed_pkg = 1;
#include "unittest.h"

int tests_run = 0;
int tests_fail = 0;

struct lk_word {
    struct lk_word* next;

    char *word;
    int wt;
    int vt;
    int ablaut;
    int ct;
    char **conj;
    char *cont;
    lk_conjugation conj_form;
    /* char *base; */
    char conj_count;
    char ablauted;
};

struct lk_leaf {
    utf8proc_uint32_t c;
    struct lk_leaf *sibling;
    struct lk_leaf *next;
    struct lk_word_ptr *word;
};

struct lk_tree {
    struct lk_leaf *head;
};


const char* test_basic() {
    int r;

    struct lk_tree *tree = lk_tree_init();
    ut_assert("Tree create", tree != NULL);

    char *p = "path";
    struct lk_word w = {};
    w.word = p;
    struct lk_word w2 = {};
    w2.word = p;

    r = lk_tree_add_word(tree, "abc", &w);
    ut_assert("Word 1", r == LK_OK);
    ut_assert("  #1.11", tree->head != NULL && tree->head->sibling == NULL);
    ut_assert("  #1.12", tree->head->next != NULL && tree->head->next->next != NULL);
    ut_assert("  #1.13", tree->head->next->word == NULL && tree->head->next->word == NULL);
    ut_assert("  #1.14", tree->head->next->next->next == NULL);
    ut_assert("  #1.15", tree->head->next->next->word->word != NULL);
    ut_assert("  #1.16", tree->head->next->next->word->word->word != NULL);
    ut_assert("  #1.17", strcmp(tree->head->next->next->word->word->word, "path") == 0);

    r = lk_tree_add_word(tree, "abc", &w);
    ut_assert("Word 1 double", r == LK_OK);
    ut_assert("  #1.21", tree->head != NULL && tree->head->sibling == NULL);
    ut_assert("  #1.22", tree->head->next != NULL && tree->head->next->next != NULL);
    ut_assert("  #1.23", tree->head->next->word == NULL && tree->head->next->word == NULL);
    ut_assert("  #1.24", tree->head->next->next->next == NULL);
    ut_assert("  #1.25", tree->head->next->next->word->word != NULL);
    ut_assert("  #1.26", tree->head->next->next->word->word->word != NULL);
    ut_assert("  #1.27", strcmp(tree->head->next->next->word->word->word, "path") == 0);
    ut_assert("  #1.28", tree->head->next->next->word->next == NULL);

    r = lk_tree_add_word(tree, "abc", &w2);
    ut_assert("Word synonym", r == LK_OK);
    ut_assert("  #1.31", tree->head != NULL && tree->head->sibling == NULL);
    ut_assert("  #1.32", tree->head->next != NULL && tree->head->next->next != NULL);
    ut_assert("  #1.33", tree->head->next->word == NULL && tree->head->next->word == NULL);
    ut_assert("  #1.34", tree->head->next->next->next == NULL);
    ut_assert("  #1.35", tree->head->next->next->word->word != NULL);
    ut_assert("  #1.36", tree->head->next->next->word->word->word != NULL);
    ut_assert("  #1.37", strcmp(tree->head->next->next->word->word->word, "path") == 0);
    ut_assert("  #1.38", tree->head->next->next->word->next != NULL);
    ut_assert("  #1.39", tree->head->next->next->word->next->next == NULL);
    ut_assert("  #1.310", tree->head->next->next->word->next->word == &w2);

    r = lk_tree_add_word(tree, "abcd", &w);
    ut_assert("Word 2", r == LK_OK);
    ut_assert("  #2.11", tree->head != NULL && tree->head->sibling == NULL);
    ut_assert("  #2.12", tree->head->next != NULL && tree->head->next->next != NULL);
    ut_assert("  #2.13", tree->head->next->word == NULL && tree->head->next->word == NULL);
    ut_assert("  #2.14", tree->head->next->next->next != NULL);
    ut_assert("  #2.14.1", tree->head->next->next->next->next == NULL);
    ut_assert("  #2.15.1", tree->head->next->next->word->word != NULL);
    ut_assert("  #2.15.2", tree->head->next->next->next->word->word != NULL);
    ut_assert("  #2.16", tree->head->next->next->word->word->word != NULL);
    ut_assert("  #2.17", strcmp(tree->head->next->next->word->word->word, "path") == 0);
    ut_assert("  #2.19.1", tree->head->next->c == 'b');
    ut_assert("  #2.19.2", tree->head->next->next->c == 'c');
    ut_assert("  #2.19.3", tree->head->next->next->next->c == 'd');
    ut_assert("  #2.18.1", tree->head->next->next->next->word->next == NULL);
    ut_assert("  #2.18.2", tree->head->next->next->word->next->next == NULL);

    r = lk_tree_add_word(tree, "ade", &w);
    ut_assert("Word 3", r == LK_OK);
    ut_assert("  #3.11", tree->head != NULL && tree->head->sibling == NULL);
    ut_assert("  #3.12", tree->head->next != NULL && tree->head->next->sibling != NULL);
    ut_assert("  #3.13", tree->head->next->sibling->next != NULL);
    ut_assert("  #3.14", tree->head->next->sibling->next->word != NULL);
    ut_assert("  #3.15", strcmp(tree->head->next->sibling->next->word->word->word, "path") == 0);


    lk_tree_free(tree);

    return 0;
}

const char* test_search() {
    int r;

    struct lk_tree *tree = lk_tree_init();
    ut_assert("Tree create", tree != NULL);

    char *p = "path", *p2 = "newpath";
    struct lk_word w = {};
    w.word = p;
    struct lk_word w2 = {};
    w2.word = p2;

    r = lk_tree_add_word(tree, "abc", &w);
    ut_assert("abc added", r == LK_OK);
    r = lk_tree_add_word(tree, "abcd", &w2);
    ut_assert("abcd added", r == LK_OK);
    r = lk_tree_add_word(tree, "ade", &w);
    ut_assert("ade added", r == LK_OK);
    r = lk_tree_add_word(tree, "éfgh", &w);
    ut_assert("éfgh added", r == LK_OK);

    const struct lk_word_ptr *sw = lk_tree_search(tree, "zed"), *sw2, *sw3, *sw4;
    ut_assert("Nonexistant word", sw == NULL);
    sw = lk_tree_search(tree, "abcd");
    ut_assert("abcd found", sw != NULL && strcmp(sw->word->word, "newpath") == 0);
    sw2 = lk_tree_search(tree, "abc");
    ut_assert("abc found", sw2 != NULL && strcmp(sw2->word->word, "path") == 0 && sw2 != sw);
    sw3 = lk_tree_search(tree, "ade");
    ut_assert("ade found", sw3 != NULL && strcmp(sw3->word->word, "path") == 0 && sw3 != sw2 && sw3 != sw);
    sw4 = lk_tree_search(tree, "éfgh");
    ut_assert("éfgh found", sw4 != NULL && strcmp(sw4->word->word, "path") == 0 && sw4 != sw3 && sw4 != sw2 && sw4 != sw);

    lk_tree_free(tree);

    return 0;
}

const char * run_all_test() {
    printf("=== Basic operations ===\n");

    ut_run_test("Tree basics", test_basic);
    ut_run_test("Tree search", test_search);

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
