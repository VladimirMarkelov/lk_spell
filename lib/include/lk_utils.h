#ifndef LKCHECKER_WORDUTILS
#define LKCHECKER_WORDUTILS

#ifdef __cplusplus
extern "C" {
#endif

lk_result lk_to_low_case(const char *word, char *out, size_t out_sz);

int lk_stressed_vowels_no(const char *word);
int lk_vowels_no(const char *word);
int lk_is_ascii(const char *word);
lk_result lk_to_ascii(const char *word, char *out, size_t out_sz);
lk_result lk_destress(const char *word, char *out, size_t out_sz);
lk_result lk_remove_glottal_stop(const char *word, char *out, size_t out_sz);
int lk_has_glottal_stop(const char *word);

int lk_ends_with(const char *orig, const char *cmp);
const char* lk_word_begin(const char *str, size_t pos);
const char* lk_next_word(const char *str, size_t *len);

#ifdef __cplusplus
}
#endif

#endif
