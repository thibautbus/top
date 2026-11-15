/* The compact text's layout's own (language_wrap.c, language_words.c): the layout and the words it learns of a language. */
#ifndef ORACLES_LANGUAGE_WRAP_INTERNAL_H
#define ORACLES_LANGUAGE_WRAP_INTERNAL_H

#include "language_internal.h"

#define CODE_NEWLINE 0x01u
#define HYPHEN 0x2du
#define LEARNED_BYTES 24u

struct language_wrap {
    OraclesTextFont font;
    unsigned width;
    uint8_t name_letter, digit;   /* the widest letter a name may hold, the widest digit */
    int syllables;                /* the language breaks a word by its syllables at a line's end (German, Italian) */
    int japanese;                 /* Japanese: no hyphen ($2d is the katakana's long mark) nor Latin letter (kana take their codes) */
    int polish;                   /* Polish: $80 to $88 are its small letters, where the US font has its accented capitals */
    unsigned learned, learned_capacity;
    uint8_t (*words)[LEARNED_BYTES];   /* the words the language writes with a hyphen inside a line, 0-padded */
    unsigned written, written_capacity;   /* a power of two */
    uint8_t (*whole)[LEARNED_BYTES];      /* the words it writes, without a hyphen, its capitals A to Z small, 0-padded; an empty slot 0 */
};

static inline int is_small(uint8_t c) { return (c >= 'a' && c <= 'z') || c == 0x91u || (c >= 0xa0u && c <= 0xb0u) || (c >= 0xd0u && c <= 0xd8u); }
static inline int is_capital(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 0x80u && c <= 0x90u) || (c >= 0xc0u && c <= 0xc8u); }
static inline int is_letter(uint8_t c) { return is_small(c) || is_capital(c); }
/* A small letter of the layout's language: the Polish letters are, the European font's capitals in their place not. */
static inline int is_small_in(const language_wrap *wrap, uint8_t c) { return is_small(c) || (wrap->polish && c >= 0x80u && c <= 0x88u); }
/* A control code's bytes: one for the break and the end, two from the dictionary's on. */
static inline size_t code_bytes(uint8_t c) { return c >= ORACLES_TEXT_FIRST_CHARACTER || c <= CODE_NEWLINE ? 1u : 2u; }

/* language_words.c: whether the language writes `word` with its hyphen inside a line, or as the part after a hyphen
 * ("-vous"); whether it writes `left` and `right` as one word, whatever the case of A to Z; a word written whole. */
int language_wrap_learned(const language_wrap *wrap, const uint8_t *word, size_t length);
int language_wrap_learned_tail(const language_wrap *wrap, const uint8_t *word, size_t length);
int language_wrap_written_whole(const language_wrap *wrap, const uint8_t *left, size_t left_length, const uint8_t *right, size_t right_length);
void language_wrap_write_whole(language_wrap *wrap, const uint8_t *text, size_t length);

#endif
