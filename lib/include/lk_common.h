#ifndef LKCHECKER_COMMON
#define LKCHECKER_COMMON

#ifdef __cplusplus
extern "C" {
#endif

/**
 * default position of the stress in a word. It starts from 0 and equals to
 *   the ordinal number of a vowel to be marked as stressed
 */
#define LK_STRESS_DEFAULT 1
/**
 * Maximum length of a word in bytes. 64 bytes must be enough but just in case
 *  the library uses 64
 */
#define LK_MAX_WORD_LEN 64

/** @enum lk_result
 * Most of the library functions returns lk_result
 */
typedef enum {
    LK_OK, /*!< Complited successfully */
    LK_FILE_READ_ERR, /*!< File was opened successfully but read failed */
    LK_EOF, /*!< End of file reached while reading it */
    LK_INVALID_FILE, /*!< Failed to open file or file reader struct is invalid */
    LK_BUFFER_SMALL, /*!< Output buffer size was less than the operation required */
    LK_INVALID_STRING, /*!< String is not UTF8 encoded one */
    LK_INVALID_ARG, /*!< Function detects invalid argument (e.g, NULL, invalid struct etc) */
    LK_OUT_OF_MEMORY, /*!< Failed to allocated memory */
    LK_WORD_NOT_FOUND, /*!< Suffix tree does not contain the word */
    LK_EXACT_MATCH, /*!< The word was found in the dictionary */
    LK_COMMENT, /*!< The parsed line is a comment and was skipped while processing data */
} lk_result;

/**
 * @enum lk_ablaut
 * Type of ablaut the word follows
 */
typedef enum {
    LK_ABLAUT_0,
    LK_ABLAUT_A,
    LK_ABLAUT_E,
    LK_ABLAUT_N,
} lk_ablaut;

/**
 * Predefined constants of UNICODE characters used in the language.
 * It is standard Latin letters with diacritic marks
 */
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
/**
 * Unicode character for glottal stop
 */
#define LK_QUOTE 700
#define LK_QUOTE2 8217

#ifdef __cplusplus
}
#endif


#endif
