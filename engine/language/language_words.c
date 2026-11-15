/* The words the compact text's layout learns of a language (language_internal.h, language_wrap_learn): those it writes
 * with a hyphen inside a line, and those it writes whole. */
#include "language_wrap_internal.h"

#include <stdlib.h>
#include <string.h>

int language_wrap_learned(const language_wrap *wrap, const uint8_t *word, size_t length)
{
    if (length >= LEARNED_BYTES) return 0;
    for (unsigned i = 0; i < wrap->learned; i++)
        if (!memcmp(wrap->words[i], word, length) && !wrap->words[i][length]) return 1;
    return 0;
}

/* Whether the language writes `word` after a hyphen, as the last part of a word it hyphenates ("-vous", "-ce"). */
int language_wrap_learned_tail(const language_wrap *wrap, const uint8_t *word, size_t length)
{
    for (unsigned i = 0; i < wrap->learned && length; i++) {
        size_t tail = 0;
        for (size_t k = 0; k < LEARNED_BYTES && wrap->words[i][k]; k++)
            if (wrap->words[i][k] == HYPHEN) tail = k + 1u;
        if (tail && tail + length < LEARNED_BYTES && !memcmp(wrap->words[i] + tail, word, length) && !wrap->words[i][tail + length]) return 1;
    }
    return 0;
}

/* The slot of the word `word` (letters, `length` < LEARNED_BYTES, folded) in the words written whole: its own, or the
 * empty one it would take. */
static uint8_t *written_slot(const language_wrap *wrap, const uint8_t *word, size_t length)
{
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < length; i++) hash = (hash ^ word[i]) * 16777619u;
    for (unsigned at = hash & (wrap->written_capacity - 1u);; at = (at + 1u) & (wrap->written_capacity - 1u)) {
        uint8_t *slot = wrap->whole[at];
        if (!slot[0] || (!memcmp(slot, word, length) && !slot[length])) return slot;
    }
}

static uint8_t folded(uint8_t c) { return c >= 'A' && c <= 'Z' ? (uint8_t)(c + ('a' - 'A')) : c; }

/* Whether the language writes `left` and `right` (letters) as one word, whatever the case of A to Z. */
int language_wrap_written_whole(const language_wrap *wrap, const uint8_t *left, size_t left_length, const uint8_t *right, size_t right_length)
{
    uint8_t word[LEARNED_BYTES];
    if (!wrap->written || left_length + right_length >= LEARNED_BYTES) return 0;
    for (size_t i = 0; i < left_length; i++) word[i] = folded(left[i]);
    for (size_t i = 0; i < right_length; i++) word[left_length + i] = folded(right[i]);
    return written_slot(wrap, word, left_length + right_length)[0] != 0;
}

/* The word `text`[at, end) (letters) among those written whole. */
void language_wrap_write_whole(language_wrap *wrap, const uint8_t *text, size_t length)
{
    uint8_t word[LEARNED_BYTES];
    if (!length || length >= LEARNED_BYTES) return;
    for (size_t i = 0; i < length; i++) word[i] = folded(text[i]);
    if (2u * (wrap->written + 1u) > wrap->written_capacity) {
        const unsigned capacity = wrap->written_capacity ? 2u * wrap->written_capacity : 1024u;
        uint8_t (*grown)[LEARNED_BYTES] = calloc(capacity, sizeof *grown), (*old)[LEARNED_BYTES] = wrap->whole;
        if (!grown) return;
        const unsigned old_capacity = wrap->written_capacity;
        wrap->whole = grown;
        wrap->written_capacity = capacity;
        for (unsigned i = 0; i < old_capacity; i++)
            if (old[i][0]) memcpy(written_slot(wrap, old[i], strlen((const char *)old[i])), old[i], LEARNED_BYTES);
        free(old);
    }
    uint8_t *slot = written_slot(wrap, word, length);
    if (slot[0]) return;
    memcpy(slot, word, length);
    wrap->written++;
}

void language_wrap_learn(language_wrap *wrap, const uint8_t *text, size_t length)
{
    if (wrap->japanese) return;
    /* The words written whole, in a language whose hyphens at a line's end are its compounds': but the parts of a word
     * broken at a line's end. */
    for (size_t at = 0; !wrap->syllables && at < length;) {
        if (!is_letter(text[at])) { at += code_bytes(text[at]); continue; }
        size_t end = at;
        while (end < length && is_letter(text[end])) end++;
        const int broken = (end + 1u < length && text[end] == HYPHEN && text[end + 1u] == CODE_NEWLINE)
            || (at >= 2u && text[at - 1u] == CODE_NEWLINE && text[at - 2u] == HYPHEN);
        if (!broken) language_wrap_write_whole(wrap, text + at, end - at);
        at = end;
    }
    for (size_t at = 0; at < length;) {
        if (!is_letter(text[at])) { at += code_bytes(text[at]); continue; }
        size_t end = at;
        int inside = 0;
        while (end < length && (is_letter(text[end]) || text[end] == HYPHEN)) {
            if (text[end] == HYPHEN && end + 1u < length && is_letter(text[end + 1u]) && end > at) inside = 1;
            end++;
        }
        if (inside && end - at < LEARNED_BYTES && !language_wrap_learned(wrap, text + at, end - at)) {
            if (wrap->learned == wrap->learned_capacity) {
                const unsigned capacity = wrap->learned_capacity ? 2u * wrap->learned_capacity : 64u;
                uint8_t (*grown)[LEARNED_BYTES] = realloc(wrap->words, capacity * sizeof *grown);
                if (!grown) return;
                wrap->words = grown;
                wrap->learned_capacity = capacity;
            }
            memset(wrap->words[wrap->learned], 0, LEARNED_BYTES);
            memcpy(wrap->words[wrap->learned++], text + at, end - at);
        }
        at = end;
    }
}
