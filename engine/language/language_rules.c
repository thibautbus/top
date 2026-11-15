/* The rules that adapt a European text to the US code, applied at extraction on the user's machine: the European
 * text's words, the US text's control codes.  A table of exceptions names, per text and language, the rule that
 * makes its translation (language_table.c); every adapted text is checked (language_rule_check) before the pack
 * keeps it, and one that fails stays in English.
 *
 * What the US engine does with a text's commands (code/textbox.s in the disassembly): \pos places the box only as a
 * text's first command (checkInitialTextCommands) and does nothing elsewhere, where the European code ends the text;
 * a \jump goes on with another text of the group, a cmd8 ends the text, and no byte after either is read; \num1 prints
 * the number the code set; \opt marks a choice where it stands in the line; \call inserts another text of the group;
 * \sym prints one glyph. */
#include "language_internal.h"

#include <string.h>

#define CODE_END 0x00u
#define CODE_NEWLINE 0x01u
#define CODE_WITH_PARAMETER 0x06u
#define CODE_SYMBOL 0x06u
#define CODE_JUMP 0x07u
#define CODE_END_HERE 0x08u
#define CODE_COLOUR 0x09u
#define CODE_NAME 0x0au
#define CODE_COMMAND 0x0cu
#define CODE_SOUND 0x0eu
#define CODE_CALL 0x0fu
#define CODE_FIRST_CHARACTER 0x10u
#define COMMAND_NUMBER 0x08u     /* \num1 */
#define COMMAND_OPTION 0x10u     /* \opt */
#define COMMAND_STOP 0x18u       /* \stop */
#define COMMAND_NUMBER2 0x30u    /* \num2 */
#define COMMAND_SPEED 0u         /* the parameter's bits 3-7: the text's speed, */
#define COMMAND_SLOW 7u          /* and \slow */

static const char *const names[RULE_COUNT] = {
    "lead", "end-at-pos", "us-call", "shop-price", "join-next", "number-as-more", "option-numbers", "unshown-number", "symbols", "one-digit",
};

const char *language_rule_name(language_rule rule) { return rule < RULE_COUNT ? names[rule] : "?"; }

int language_rule_parse(const char *name, size_t length, language_rule *rule)
{
    for (unsigned r = 0; r < RULE_COUNT; r++)
        if (strlen(names[r]) == length && !memcmp(names[r], name, length)) { *rule = (language_rule)r; return 0; }
    return -1;
}

/* ---- a text's units: a character, a line break, or a control code with its parameter ---------------------------- */

static size_t unit(const uint8_t *t, size_t length, size_t at)
{
    return t[at] >= CODE_WITH_PARAMETER && t[at] < CODE_FIRST_CHARACTER && at + 1u < length ? 2u : 1u;
}

static int is_pos(const uint8_t *t, size_t at) { return t[at] == CODE_COMMAND && (t[at + 1u] >> 3) == 4u; }
static int is_command(const uint8_t *t, size_t at, uint8_t command) { return t[at] == CODE_COMMAND && t[at + 1u] == command; }
static int is_digit(uint8_t c) { return c >= '0' && c <= '9'; }
static int is_lower(uint8_t c) { return (c >= 'a' && c <= 'z') || (c >= 0xa0u && c <= 0xb0u) || (c >= 0xd0u && c <= 0xd8u); }

/* The width a unit takes in its line, as the game draws it (oracles_text_longest_line). */
static unsigned width(const uint8_t *t, size_t at, size_t n)
{
    if (n == 1u) return t[at] >= CODE_FIRST_CHARACTER;
    if (t[at] == CODE_SYMBOL) return 1;
    if (t[at] == CODE_NAME && t[at + 1u] < 2u) return 5;
    if (is_command(t, at, COMMAND_NUMBER) || is_command(t, at, COMMAND_NUMBER2)) return 2;
    if (is_command(t, at, COMMAND_OPTION)) return 1;
    return 0;
}

/* A text being built, its terminator not yet written. */
typedef struct builder { uint8_t *out; size_t capacity, length; int bad; } builder;

static void put(builder *b, const uint8_t *bytes, size_t n)
{
    if (b->bad || b->length + n > b->capacity) { b->bad = 1; return; }
    memcpy(b->out + b->length, bytes, n);
    b->length += n;
}

static void put_byte(builder *b, uint8_t byte) { put(b, &byte, 1); }

static int finish(builder *b, size_t *out_length)
{
    put_byte(b, CODE_END);
    if (b->bad) return -1;
    *out_length = b->length;
    return 0;
}

/* The length of a text without its terminator. */
static size_t body(const uint8_t *t, size_t length) { return length && t[length - 1u] == CODE_END ? length - 1u : length; }

/* The length of the leading run of \pos and \sfx. */
static size_t leading(const uint8_t *t, size_t length)
{
    size_t at = 0;
    while (at + 1u < length && (is_pos(t, at) || t[at] == CODE_SOUND)) at += 2u;
    return at;
}

/* ---- the commands as the US engine acts on them ---------------------------------------------------------------- */

typedef struct commands { uint16_t list[256]; unsigned count, middle_pos; } commands;

/* Whether a command only draws a glyph or paces the text, which Japanese leaves out of the comparison
 * (language_same_commands). */
static int drawn_or_paced(const uint8_t *t, size_t at)
{
    return t[at] == CODE_SYMBOL || (t[at] == CODE_COMMAND && (t[at + 1u] >> 3 == COMMAND_SPEED || t[at + 1u] >> 3 == COMMAND_SLOW));
}

/* The commands of a text that change what the US engine does: not the colours, the hero's names, \stop or the line
 * breaks; a \pos only as the first command; up to a \jump or a cmd8.  `symbols` keeps the \sym, `numbers` the \num;
 * `japanese` leaves out what only draws or paces the text. */
static void effective(const uint8_t *t, size_t length, int symbols, int numbers, int japanese, commands *c)
{
    c->count = c->middle_pos = 0;
    for (size_t at = 0, n; at < length; at += n) {
        n = unit(t, length, at);
        if (n == 1u) { if (t[at] == CODE_END) break; continue; }
        const uint8_t code = t[at], parameter = t[at + 1u];
        if (code == CODE_COLOUR || (code == CODE_NAME && parameter < 2u) || is_command(t, at, COMMAND_STOP)) continue;
        if (japanese && drawn_or_paced(t, at)) continue;
        if (is_pos(t, at) && at) { c->middle_pos++; continue; }
        if (code == CODE_SYMBOL && !symbols) continue;
        if ((is_command(t, at, COMMAND_NUMBER) || is_command(t, at, COMMAND_NUMBER2)) && !numbers) continue;
        if (c->count < sizeof c->list / sizeof c->list[0]) c->list[c->count] = (uint16_t)(code << 8 | parameter);
        c->count++;
        if (code == CODE_JUMP || code == CODE_END_HERE) break;
    }
}

int language_text_fits(const uint8_t *text, size_t length)
{
    size_t last = (size_t)-1;
    for (size_t at = 0, n; at < body(text, length); at += n) {
        n = unit(text, length, at);
        last = at;
    }
    return oracles_text_longest_line(text, length) <= LINE_CHARACTERS && !(last != (size_t)-1 && text[last] == CODE_NEWLINE);
}

/* language_text_fits, a \num1 one cell (one-digit): a number the US code never sets past 9. */
static int fits_one_digit(const uint8_t *t, size_t length)
{
    unsigned line = 0, longest = 0;
    size_t last = (size_t)-1;
    for (size_t at = 0, n; at < body(t, length); at += n) {
        n = unit(t, length, at);
        last = at;
        if (n == 1u && t[at] == CODE_NEWLINE) { line = 0; continue; }
        if (n == 2u && (t[at] == CODE_JUMP || t[at] == CODE_END_HERE)) break;
        line += n == 2u && is_command(t, at, COMMAND_NUMBER) ? 1u : width(t, at, n);
        if (line > longest) longest = line;
    }
    return longest <= LINE_CHARACTERS && !(last != (size_t)-1 && t[last] == CODE_NEWLINE);
}

int language_rule_holds(language_rule rule, OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length)
{
    commands a, b;
    const int symbols = rule != RULE_SYMBOLS, japanese = language == ORACLES_LANGUAGE_JAPANESE;
    effective(us, us_length, symbols, rule != RULE_UNSHOWN_NUMBER, japanese, &a);
    effective(text, length, symbols, 1, japanese, &b);
    return a.count == b.count && a.count <= sizeof a.list / sizeof a.list[0] && !memcmp(a.list, b.list, a.count * sizeof a.list[0])
           && b.middle_pos <= a.middle_pos && (rule == RULE_ONE_DIGIT ? fits_one_digit(text, length) : language_text_fits(text, length));
}

int language_rule_check(language_rule rule, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length)
{
    return language_rule_holds(rule, ORACLES_LANGUAGE_FRENCH, us, us_length, text, length);
}

/* The next command of a Japanese text from *at that the comparison keeps, as oracles_text_same_commands keeps them
 * (every \pos), the \sym and the speed left out: code << 8 | parameter; -1 at its end, or past a \jump or a cmd8. */
static long next_japanese(const uint8_t *t, size_t length, size_t *at)
{
    while (*at < length) {
        const size_t n = unit(t, length, *at), here = *at;
        *at += n;
        if (n == 1u) continue;
        const uint8_t code = t[here], parameter = t[here + 1u];
        if (code == CODE_COLOUR || (code == CODE_NAME && parameter < 2u) || is_command(t, here, COMMAND_STOP) || drawn_or_paced(t, here)) continue;
        if (code == CODE_JUMP || code == CODE_END_HERE) *at = length;
        return (long)code << 8 | parameter;
    }
    return -1;
}

int language_same_commands(OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length)
{
    if (language != ORACLES_LANGUAGE_JAPANESE) return oracles_text_same_commands(us, us_length, text, length);
    size_t i = 0, k = 0;
    for (;;) {
        const long x = next_japanese(us, us_length, &i), y = next_japanese(text, length, &k);
        if (x != y) return 0;
        if (x < 0) return 1;
    }
}

/* ---- the rules ------------------------------------------------------------------------------------------------- */

/* lead: the US text's leading \pos and \sfx, then the European text past its own. */
static int lead(const uint8_t *us, size_t us_length, const uint8_t *t, size_t length, builder *b)
{
    put(b, us, leading(us, us_length));
    const size_t from = leading(t, length);
    put(b, t + from, body(t, length) - from);
    return 0;
}

/* end-at-pos: the European text up to its first \pos past the start, without the line break before it. */
static int end_at_pos(const uint8_t *t, size_t length, builder *b)
{
    size_t cut = 0, last_break = (size_t)-1;
    for (size_t at = leading(t, length), n; at < body(t, length) && !cut; at += n) {
        n = unit(t, length, at);
        if (n == 2u && is_pos(t, at)) cut = at;
        else if (n == 1u) last_break = t[at] == CODE_NEWLINE ? at : (size_t)-1;
        else if (width(t, at, n)) last_break = (size_t)-1;
    }
    if (!cut) return -1;
    if (last_break != (size_t)-1 && last_break + 1u == cut) cut = last_break;
    put(b, t, cut);
    return 0;
}

/* us-call: each \call with the US text's parameter, in order. */
static int us_call(const uint8_t *us, size_t us_length, const uint8_t *t, size_t length, builder *b)
{
    uint8_t calls[64];
    unsigned count = 0, used = 0;
    for (size_t at = 0, n; at < body(us, us_length); at += n) {
        n = unit(us, us_length, at);
        if (n == 2u && us[at] == CODE_CALL) { if (count == sizeof calls) return -1; calls[count++] = us[at + 1u]; }
    }
    for (size_t at = 0, n; at < body(t, length); at += n) {
        n = unit(t, length, at);
        if (n == 2u && t[at] == CODE_CALL) {
            if (used == count) return -1;
            const uint8_t call[2] = { CODE_CALL, calls[used++] };
            put(b, call, 2);
        } else put(b, t + at, n);
    }
    return used == count ? 0 : -1;
}

/* The \num1 of a text as the US engine reads it, up to a \jump or a cmd8. */
static unsigned numbers(const uint8_t *t, size_t length)
{
    commands c;
    unsigned count = 0;
    effective(t, length, 1, 1, 0, &c);
    for (unsigned i = 0; i < c.count && i < sizeof c.list / sizeof c.list[0]; i++) count += c.list[i] == (CODE_COMMAND << 8 | COMMAND_NUMBER);
    return count;
}

/* shop-price: the European text's printed price becomes the US code's \num1 when the US text has one more; and when
 * the US text goes on with a \jump to its shop's choice where the European text prints the choice itself, the text
 * ends at the choice's line with the US text's \jump. */
static int shop_price(const uint8_t *us, size_t us_length, const uint8_t *t, size_t length, builder *b)
{
    const size_t n_body = body(t, length);
    commands c;
    effective(us, us_length, 1, 1, 0, &c);
    if (c.count > sizeof c.list / sizeof c.list[0]) return -1;   /* more commands than a text of the game has: its last is not kept */
    const int us_jumps = c.count && c.list[c.count - 1u] >> 8 == CODE_JUMP;
    int jumps = 0;
    size_t choice_line = n_body, box = leading(t, length);
    for (size_t at = 0, line = 0, n; at < n_body; at += n) {
        n = unit(t, length, at);
        if (n == 1u && t[at] == CODE_NEWLINE) line = at + 1u;
        if (n == 2u && t[at] == CODE_JUMP) jumps = 1;
        if (n == 2u && is_command(t, at, COMMAND_OPTION) && choice_line == n_body) choice_line = line;
        if (n == 2u && is_command(t, at, COMMAND_STOP) && choice_line == n_body) box = at + 2u;
    }
    /* The price: the first number standing alone in the box that holds the choice, or the last box. */
    size_t price = 0, price_end = 0;
    if (numbers(us, us_length) > numbers(t, length)) {
        for (size_t at = box, prev = (size_t)-1, n; at < choice_line && !price_end; prev = at, at += n) {
            n = unit(t, length, at);
            if (n != 1u || !is_digit(t[at]) || (prev != (size_t)-1 && unit(t, length, prev) == 1u && t[prev] != ' ' && t[prev] != CODE_NEWLINE)) continue;
            size_t end = at;
            while (end < n_body && is_digit(t[end])) end++;
            if (end < n_body && ((t[end] >= 'A' && t[end] <= 'Z') || is_lower(t[end]))) { n = end - at; continue; }
            price = at;
            price_end = end;
        }
        if (!price_end) return -1;
    }
    size_t end = n_body;
    if (us_jumps && !jumps) {
        if (choice_line == n_body || !choice_line) return -1;
        end = choice_line;
    }
    if (price_end) {
        const uint8_t number[2] = { CODE_COMMAND, COMMAND_NUMBER };
        put(b, t, price);
        put(b, number, 2);
        put(b, t + price_end, end - price_end);
    } else put(b, t, end);
    if (us_jumps && !jumps) {
        const uint8_t jump[2] = { CODE_JUMP, (uint8_t)c.list[c.count - 1u] };
        put(b, jump, 2);
    }
    return 0;
}

/* join-next: the European text, a line break, and the next identifier's European text. */
static int join_next(const uint8_t *t, size_t length, const uint8_t *next, size_t next_length, builder *b)
{
    if (!next) return -1;
    put(b, t, body(t, length));
    put_byte(b, CODE_NEWLINE);
    put(b, next, body(next, next_length));
    return 0;
}

/* The line breaks fitted again after a rule lengthened a line: a line over LINE_CHARACTERS breaks at its last space
 * that fits, and the words put on their own line join the next line when they end with a hyphen that splits a word
 * across the two (a lower-case letter follows); never in Japanese, whose $2d is the katakana's long mark and whose kana
 * take the Latin letters' codes. */
static int fit(OraclesLanguage language, builder *b)
{
    for (unsigned rounds = 0; rounds < 64u; rounds++) {
        uint8_t *t = b->out;
        const size_t length = b->length;
        size_t line = 0, space = (size_t)-1, over = (size_t)-1;
        unsigned w = 0;
        for (size_t at = 0, n; at < length && over == (size_t)-1; at += n) {
            n = unit(t, length, at);
            if (n == 1u && t[at] == CODE_NEWLINE) { line = at + 1u; w = 0; space = (size_t)-1; continue; }
            if (n == 2u && (t[at] == CODE_JUMP || t[at] == CODE_END_HERE)) break;
            if (n == 1u && t[at] == ' ' && w <= LINE_CHARACTERS) space = at;
            w += width(t, at, n);
            if (w > LINE_CHARACTERS) over = at;
        }
        if (over == (size_t)-1) return 0;
        if (space == (size_t)-1 || space < line) return -1;
        t[space] = CODE_NEWLINE;
        /* The words moved: from space + 1 to the line's end; a hyphen there joins them to the next line's word. */
        size_t end = space + 1u;
        while (end < length && !(unit(t, length, end) == 1u && t[end] == CODE_NEWLINE)) end += unit(t, length, end);
        if (language != ORACLES_LANGUAGE_JAPANESE && end + 1u < length && end > space + 1u && t[end - 1u] == '-' && is_lower(t[end + 1u])) {
            memmove(t + end - 1u, t + end + 1u, length - end - 1u);
            b->length -= 2u;
        }
    }
    return -1;
}

/* number-as-more: each \num1 becomes the language's word for "more", the lines fitted again.  Japanese counts after its
 * number ("\num1コまで", up to so many): the word takes the counter's place too. */
static int number_as_more(OraclesLanguage language, const uint8_t *t, size_t length, builder *b)
{
    static const char *const more[ORACLES_LANGUAGE_COUNT] = { "more", "more", "plus", "mehr", "pi\xae", "m\xd2s", "\x20\x82\x93\x73", "wi\202cej" };
    static const char *const counted[ORACLES_LANGUAGE_COUNT] = { "", "", "", "", "", "", "\xb9\x7e\xa4", "" };
    unsigned replaced = 0;
    for (size_t at = 0, n; at < body(t, length); at += n) {
        n = unit(t, length, at);
        if (n == 2u && is_command(t, at, COMMAND_NUMBER)) {
            const size_t counter = strlen(counted[language]);
            if (counter && (at + 2u + counter > body(t, length) || memcmp(t + at + 2u, counted[language], counter))) return -1;
            put(b, (const uint8_t *)more[language], strlen(more[language]));
            n += counter;
            replaced++;
        } else put(b, t + at, n);
    }
    if (!replaced || b->bad) return -1;
    return fit(language, b);
}

/* The digits following each \opt of a text: where they start and end, by choice. */
static unsigned option_labels(const uint8_t *t, size_t length, size_t starts[16], size_t ends[16])
{
    unsigned count = 0;
    for (size_t at = 0, n; at < body(t, length); at += n) {
        n = unit(t, length, at);
        if (n != 2u || !is_command(t, at, COMMAND_OPTION)) continue;
        if (count == 16u) return 17u;
        size_t end = at + 2u;
        while (end < body(t, length) && is_digit(t[end])) end++;
        starts[count] = at + 2u;
        ends[count++] = end;
    }
    return count;
}

/* option-numbers: a choice that is a number takes the US text's number. */
static int option_numbers(const uint8_t *us, size_t us_length, const uint8_t *t, size_t length, builder *b)
{
    size_t us_starts[16], us_ends[16], starts[16], ends[16];
    const unsigned count = option_labels(us, us_length, us_starts, us_ends);
    if (count > 16u || option_labels(t, length, starts, ends) != count) return -1;
    size_t from = 0;
    unsigned changed = 0;
    for (unsigned i = 0; i < count; i++) {
        if (us_starts[i] == us_ends[i] || starts[i] == ends[i]) continue;
        if (us_ends[i] - us_starts[i] == ends[i] - starts[i] && !memcmp(us + us_starts[i], t + starts[i], ends[i] - starts[i])) continue;
        put(b, t + from, starts[i] - from);
        put(b, us + us_starts[i], us_ends[i] - us_starts[i]);
        from = ends[i];
        changed++;
    }
    put(b, t + from, body(t, length) - from);
    return changed ? 0 : -1;
}

int language_rule_apply(language_rule rule, OraclesLanguage language, const uint8_t *us, size_t us_length, const uint8_t *text, size_t length,
                        const uint8_t *next, size_t next_length, uint8_t *out, size_t capacity, size_t *out_length)
{
    builder b = { out, capacity, 0, 0 };
    int status = -1;
    switch (rule) {
    case RULE_LEAD: status = lead(us, us_length, text, length, &b); break;
    case RULE_END_AT_POS: status = end_at_pos(text, length, &b); break;
    case RULE_US_CALL: status = us_call(us, us_length, text, length, &b); break;
    case RULE_SHOP_PRICE: status = shop_price(us, us_length, text, length, &b); break;
    case RULE_JOIN_NEXT: status = join_next(text, length, next, next_length, &b); break;
    case RULE_NUMBER_AS_MORE: status = language < ORACLES_LANGUAGE_COUNT ? number_as_more(language, text, length, &b) : -1; break;
    case RULE_OPTION_NUMBERS: status = option_numbers(us, us_length, text, length, &b); break;
    case RULE_UNSHOWN_NUMBER:
    case RULE_SYMBOLS:
    case RULE_ONE_DIGIT: put(&b, text, body(text, length)); status = 0; break;
    case RULE_COUNT: break;
    }
    return status == 0 ? finish(&b, out_length) : -1;
}
