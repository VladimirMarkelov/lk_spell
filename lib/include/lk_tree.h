#ifndef LKCHECKER_SUFTREE
#define LKCHECKER_SUFTREE

#ifdef __cplusplus
extern "C" {
#endif

struct lk_word_ptr {
    const struct lk_word *word;
    struct lk_word_ptr *next;
};

struct lk_word;
struct lk_tree;

struct lk_tree* lk_tree_init();
void lk_tree_free(struct lk_tree *tree);

lk_result lk_tree_add_word(struct lk_tree *tree, const char *path, const struct lk_word *word);
const struct lk_word_ptr* lk_tree_search(const struct lk_tree *tree, const char *path);

#ifdef __cplusplus
}
#endif

#endif
