#ifndef LKCHECKER_WORDUTILS
#define LKCHECKER_WORDUTILS

lk_result lk_to_low_case(const char *word, char *out, size_t out_sz);
int lk_is_valid_word(const char *word);
int lk_has_ablaut(const char *word);
int lk_is_ablaut_stressed(const char *word);

int lk_stressed_vowels_no(const char *word);
int lk_vowels_no(const char *word);
size_t lk_first_stressed_vowel(const char *word);
int lk_is_ascii(const char *word);
lk_result lk_to_ascii(const char *word, char *out, size_t out_sz);
lk_result lk_destress(const char *word, char *out, size_t out_sz);
lk_result lk_put_stress(const char *word, int pos, char *out, size_t out_sz);
lk_result lk_remove_glottal_stop(const char *word, char *out, size_t out_sz);
int lk_has_glottal_stop(const char *word);

int lk_ends_with(const char *orig, const char *cmp);
#endif
