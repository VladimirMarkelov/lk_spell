#ifndef LKCHECKER_COMMON
#define LKCHECKER_COMMON

#ifdef __cplusplus
extern "C" {
#endif

#define BUFFER_SIZE 65536
#define LK_STRESS_DEFAULT 1

typedef enum {
    LK_OK,
    LK_FILE_READ_ERR,
    LK_EOF,
    LK_INVALID_FILE,
    LK_BUFFER_SMALL,
    LK_INVALID_STRING,
    LK_INVALID_ARG,
    LK_OUT_OF_MEMORY,
    LK_WORD_NOT_FOUND,
    LK_EXACT_MATCH,
    LK_INVALID_CONJ,
    LK_COMMENT,
    LK_INCOMPLETE_VERB,
} lk_result;

typedef enum {
    LK_STATIC_VERB = 'S',
    LK_TRANS_VERB = 'T',
    LK_INTRANS_VERB = 'I',
    LK_NOUN = 'N',
    LK_SPEC = '-',
    LK_ADVERB = 'A',
} lk_word_type;

typedef enum {
    LK_ABLAUT_0,
    LK_ABLAUT_A,
    LK_ABLAUT_E,
    LK_ABLAUT_N,
} lk_ablaut;

typedef enum {
    LK_STATE_SKIP_WHITE,
    LK_STATE_GOBBLE,
    LK_STATE_QUOTE,
    LK_STATE_DONE,
} lk_state;

#ifdef __cplusplus
}
#endif


#endif
