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

#define LK_A_UP 193
#define LK_A_LOW 225
#define LK_O_UP 211
#define LK_O_LOW 243
#define LK_E_UP 201
#define LK_E_LOW 233
#define LK_I_UP 205
#define LK_I_LOW 237
#define LK_U_UP 218
#define LK_U_LOW 250

#define LK_N_UP 330
#define LK_N_LOW 331
#define LK_C_UP 268
#define LK_C_LOW 269
#define LK_Z_UP 381
#define LK_Z_LOW 382
#define LK_H_UP 542
#define LK_H_LOW 543
#define LK_G_UP 486
#define LK_G_LOW 487
#define LK_S_UP 352
#define LK_S_LOW 353
#define LK_QUOTE 700

#ifdef __cplusplus
}
#endif


#endif
