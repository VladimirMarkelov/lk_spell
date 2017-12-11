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
	LK_NONE,
	LK_1_SING,
	LK_1_DUAL,
	LK_1_PLUR,
	LK_2_SING,
	LK_2_PLUR,
	LK_3_SING,
	LK_3_PLUR,
	LK_COLECTIVE, /* stative + -wicha- */
	
	LK_1_S_2_S,
	LK_1_S_2_P,
	LK_1_S_3_S,
	LK_1_S_3_P,
	LK_1_D_2_S,
	LK_1_D_2_P,
	LK_1_D_3_S,
	LK_1_D_3_P,
	LK_1_P_2_S,
	LK_1_P_2_P,
	LK_1_P_3_S,
	LK_1_P_3_P,

	LK_2_S_1_S,
	LK_2_S_1_D,
	LK_2_S_1_P,
	LK_2_S_3_S,
	LK_2_S_3_P,

	LK_2_P_1_S,
	LK_2_P_1_D,
	LK_2_P_1_P,
	LK_2_P_3_S,
	LK_2_P_3_P,

	LK_3_S_1_S,
	LK_3_S_1_D,
	LK_3_S_1_P,
	LK_3_S_2_S,
	LK_3_S_2_P,

	LK_3_P_1_S,
	LK_3_P_1_D,
	LK_3_P_1_P,
	LK_3_P_2_S,
	LK_3_P_2_P,
} lk_conjugation;

typedef enum {
	LK_CONJ_TYPE_S,
	LK_CONJ_TYPE_1,
	LK_CONJ_TYPE_2,
	LK_CONJ_TYPE_3,
} lk_conjugation_type;

typedef enum {
    LK_VERB_BASE,
    LK_VERB_DAT1,
    LK_VERB_DAT2,
    LK_VERB_REFL,
    LK_VERB_RECIP,
    LK_VERB_REDUP,
    LK_VERB_POS,
    LK_VERB_CONT,
} lk_verb_type;

typedef enum {
    LK_ABLAUT_0,
    LK_ABLAUT_A,
    LK_ABLAUT_E,
    LK_ABLAUT_N,
} lk_ablaut;

#ifdef __cplusplus
}
#endif


#endif
