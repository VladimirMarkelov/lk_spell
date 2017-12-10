#ifndef LKCHECKER_DICTIONARY
#define LKCHECKER_DICTIONARY

struct lk_dictionary;
struct lk_word;
struct lk_word_ptr;

struct lk_dictionary* lk_dict_init();
lk_result lk_read_dictionary(struct lk_dictionary *dict, const char *path);
void lk_dict_close(struct lk_dictionary* dict);
int lk_is_dict_valid(const struct lk_dictionary* dict);
size_t lk_word_count(const struct lk_dictionary *dict);

// lk_result lk_conjugate(const char *word, lk_conjugation conj, char *out, size_t out_sz);

lk_result lk_parse_word(const char *info, struct lk_dictionary* dict);
char** lk_dict_exact_lookup(const struct lk_dictionary *dict,
        const char *word, const char *next_word, int *count);
void lk_exact_lookup_free(char** lookup);


const struct lk_word_ptr* lk_dict_find_word(const struct lk_dictionary *dict, const char *word);
lk_ablaut lk_dict_check_ablaut(const struct lk_dictionary *dict, const char *word);
void _print_dict(const struct lk_dictionary *dict);
void _print_word(const struct lk_word *w);
#endif
