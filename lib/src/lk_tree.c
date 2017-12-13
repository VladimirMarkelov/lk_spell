#include <stdlib.h>
#include <stdio.h>

#include <utf8proc.h>
#include "lk_common.h"
#include "lk_tree.h"

struct lk_leaf {
    utf8proc_uint32_t c;
    struct lk_leaf *sibling;
    struct lk_leaf *next;
    struct lk_word_ptr *word;
};

struct lk_tree {
    struct lk_leaf *head;
};

struct lk_tree* lk_tree_init() {
    struct lk_tree *tree = (struct lk_tree*)malloc(sizeof(*tree));
    if (tree == NULL)
        return NULL;

    tree->head = NULL;
    return tree;
}

static void free_leaf(struct lk_leaf *leaf) {
    if (leaf == NULL)
        return;

    struct lk_word_ptr *w = leaf->word;
    while (w) {
        struct lk_word_ptr *next = w->next;
        free(w);
        w = next;
    }
}

static void free_tree(struct lk_leaf *leaf) {
    while (leaf) {
        if (leaf->sibling)
            free_tree(leaf->sibling);
        struct lk_leaf *next = leaf;
        free_leaf(leaf);
        leaf = next->next;
    }
}

void lk_tree_free(struct lk_tree *tree) {
    if (tree == NULL || tree->head == NULL)
        return;

    free_tree(tree->head);
}

static struct lk_leaf* add_char_to_level(struct lk_leaf *start, utf8proc_uint32_t c) {
    struct lk_leaf *n = start, *p = start;
    while (n) {
        if (n->c == c)
            return n;
        p = n;
        n = n->sibling;
    }

    n = (struct lk_leaf*)malloc(sizeof(*n));
    if (n == NULL)
        return NULL;

    p->sibling = n;
    n->c = c;
    n->sibling = NULL;
    n->next = NULL;
    n->word = NULL;
    return n;
}

static lk_result put_word_to_list(struct lk_leaf *leaf, const struct lk_word *word) {
    if (leaf->word == NULL) {
        struct lk_word_ptr *ptr = (struct lk_word_ptr*)malloc(sizeof(*ptr));
        if (ptr == NULL)
            return LK_OUT_OF_MEMORY;

        ptr->word = word;
        ptr->next = NULL;
        leaf->word = ptr;
    } else {
        struct lk_word_ptr *ptr = leaf->word, *prev = NULL;
        while (ptr != NULL) {
            if (ptr->word == word)
                return LK_OK;
            prev = ptr;
            ptr = ptr->next;
        }

        ptr = (struct lk_word_ptr*)malloc(sizeof(*ptr));
        if (ptr == NULL)
            return LK_OUT_OF_MEMORY;

        ptr->word = word;
        ptr->next = NULL;
        prev->next = ptr;
    }

    return LK_OK;
}

lk_result lk_tree_add_word(struct lk_tree *tree, const char *path, const struct lk_word *word) {
    if (tree == NULL || path == NULL || word == NULL)
        return LK_INVALID_ARG;
    if (*path == '\0')
        return LK_OK;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)path;
    utf8proc_int32_t cp;
    size_t len;

    struct lk_leaf *leaf = tree->head, *prev_leaf = NULL;
    while (*usrc) {
        len = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return LK_INVALID_STRING;
        usrc += len;

        if (leaf == NULL) {
            struct lk_leaf *n = (struct lk_leaf*)malloc(sizeof(*n));
            if (n == NULL)
                return LK_OUT_OF_MEMORY;

            n->c = cp;
            n->sibling = NULL;
            n->next = NULL;
            n->word = NULL;
            if (prev_leaf == NULL) {
                tree->head = n;
            } else {
                prev_leaf->next = n;
            }

            prev_leaf = n;
            leaf = NULL;
        } else {
            struct lk_leaf *search = add_char_to_level(leaf, cp);
            if (search == NULL)
                return LK_OUT_OF_MEMORY;

            prev_leaf = search;
            leaf = prev_leaf->next;
        }
    }

    return put_word_to_list(prev_leaf, word);
}

const struct lk_word_ptr* lk_tree_search(const struct lk_tree *tree, const char *path) {
    if (tree == NULL || path == NULL || *path == '\0')
        return NULL;

    utf8proc_uint8_t *usrc = (utf8proc_uint8_t*)path;
    utf8proc_int32_t cp;
    const struct lk_leaf *leaf = tree->head;

    while (*usrc) {
        size_t len = utf8proc_iterate(usrc, -1, &cp);
        if (cp == -1)
            return NULL;
        usrc += len;

        const struct lk_leaf *search = NULL, *item = leaf;
        while (item != NULL) {
            if (item->c == cp) {
                search = item;
                break;
            }
            item = item->sibling;
        }

        if (search == NULL)
            return NULL;

        if (*usrc)
            leaf = search->next;
        else
            leaf = search;
    }

    return leaf->word;
}

