/* Display › Text (ui_page_text.h): its rows' states and texts, the link's summary, the preview. */
#include "ui_page_text.h"

#include "text_box_enlarge.h"

#include <stdio.h>
#include <string.h>

/* The boxes, in the game's pixels (text_box.h, which tests/test_launcher_settings.c checks these against): the game's,
 * 18 tiles by 5, its 128 pixels of text between margins of a tile; the wide one the port draws, 192 pixels of text
 * between the same margins; two lines of 16 pixels and the arrow's row.  The preview stands the box 8 pixels above the
 * band's edge, where the game's own stands on its screen. */
#define BOX_MARGIN 8
#define BOX_GAME_TEXT 128
#define BOX_WIDE_TEXT 192
#define BOX_H 40
#define BOX_BOTTOM 8
#define LINE_H 16
#define CELL_W 8
/* Without the ROM's font, the sample's words as bars: a letter's width (Classic its cell, Compact about the font's
 * average), the bar's rows in a line's 16. */
#define BAR_COMPACT_W 6
#define BAR_SPACE_W 5
#define BAR_TOP 6
#define BAR_H 6

const char *const oracles_display_text_choices[2] = { "Classic", "Compact" };
const char *const oracles_display_text_box_choices[2] = { "Standard", "Wide" };
const char *const oracles_display_text_size_choices[ORACLES_DISPLAY_TEXT_SIZES] = { "Normal", "Large", "Very large" };
const char *const oracles_display_text_speed_choices[2] = { "Classic", "Instant" };
static const char enhanced_reason[] = "Enhanced only.";
static const char original_reason[] = "Needs an original ROM.";
/* The port's own words, never the game's. */
static const char sample[] = "Link, the Maku Tree has been waiting for you. Nayru was taken, and time itself is coming undone.";

/* The host's box (Wide) and Text size need the game's profile to qualify them, which a fan game's may not. */
static int box_refused(const OraclesHomeNav *nav) { return oracles_home_refuses(nav, ORACLES_HOME_REFUSES_TEXT_BOX); }
static int size_refused(const OraclesHomeNav *nav) { return oracles_home_refuses(nav, ORACLES_HOME_REFUSES_TEXT_SIZE); }

/* The instant text needs the game's profile, which an unrecognised ROM (a hack, a randomizer seed) plays without: the
 * running game's in the pause, the page's image on the home screen, as Display plays it in Faithful. */
static int text_speed_refused(const OraclesHomeNav *nav)
{
    if (nav->in_game) return nav->text_speed_refused;
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    return (game->fan ? game->image : game->rom) == ORACLES_ROM_UNRECOGNISED;
}

static int clamp(int value, int count) { return value < 0 ? 0 : value >= count ? count - 1 : value; }

const char *const *oracles_display_text_row_choices(unsigned row, unsigned *count)
{
    *count = row == ORACLES_DISPLAY_TEXT_SIZE ? ORACLES_DISPLAY_TEXT_SIZES : 2u;
    return row == ORACLES_DISPLAY_LETTERS ? oracles_display_text_choices : row == ORACLES_DISPLAY_TEXT_BOX ? oracles_display_text_box_choices
           : row == ORACLES_DISPLAY_TEXT_SIZE ? oracles_display_text_size_choices : oracles_display_text_speed_choices;
}

int oracles_display_text_applies(const OraclesHomeNav *nav) { return nav->display.profile == ORACLES_PROFILE_ENHANCED; }

int oracles_display_text_link_applies(const OraclesHomeNav *nav)
{
    for (unsigned r = ORACLES_DISPLAY_LETTERS; r <= ORACLES_DISPLAY_TEXT_SPEED; r++)
        if (oracles_display_text_row_live(nav, r)) return 1;
    return oracles_display_text_applies(nav) || !text_speed_refused(nav);
}

int oracles_display_text_row_live(const OraclesHomeNav *nav, unsigned row)
{
    switch (row) {
        case ORACLES_DISPLAY_TEXT_SPEED: return !text_speed_refused(nav);
        case ORACLES_DISPLAY_LETTERS: return oracles_display_text_applies(nav) && !oracles_display_row_fixed(nav, row);
        case ORACLES_DISPLAY_TEXT_BOX: return oracles_display_text_applies(nav) && !box_refused(nav) && !oracles_display_row_fixed(nav, row);
        case ORACLES_DISPLAY_TEXT_SIZE: return oracles_display_hud_applies(nav) && !size_refused(nav) && !nav->display.text_size_fixed;
        default: return 0;
    }
}

void oracles_display_text_summary(const OraclesHomeNav *nav, char *out, size_t capacity)
{
    /* What applies to the game shown: in Faithful the game's own text; the box and the size its profile qualifies (a fan
     * game's text in the game's box at its size); Instant where the game has it. */
    const char *const instant = nav->display.text_speed && !text_speed_refused(nav) ? " \xc2\xb7 Instant" : "";
    if (!oracles_display_text_applies(nav)) {
        snprintf(out, capacity, "As the game%s", instant);
        return;
    }
    const char *const box = box_refused(nav) ? NULL : oracles_display_text_box_choices[clamp(nav->display.text_box, 2)];
    const char *const size = size_refused(nav) ? NULL : oracles_display_text_size_choices[clamp(nav->display.text_size, ORACLES_DISPLAY_TEXT_SIZES)];
    snprintf(out, capacity, "%s%s%s%s%s%s", oracles_display_text_choices[clamp(nav->display.text, 2)], box ? " \xc2\xb7 " : "", box ? box : "",
             size ? " \xc2\xb7 " : "", size ? size : "", instant);
}

const char *oracles_display_text_explanation(unsigned row, int four_three)
{
    static const char *const texts[5][2] = {
        { "Letters, box, size and speed, with a preview.", "Letters, box, size and speed, with a preview." },
        { "Classic: the game's fixed-width letters. With the Standard box, the text is exactly the game's. Compact: letters take their own "
          "width, so more words fit on a line.",
          "Classic: the game's letters. Compact: more words per line." },
        { "Wide: a longer box with longer lines, about a third fewer boxes with Compact. Standard: the game's box.", "Wide: longer lines, fewer boxes." },
        { "Large and Very large draw the text box bigger where the view has room for it, for reading from a distance or on small screens.",
          "Large: a bigger box. Very large: twice as big." },
        { "Classic: letter by letter, at the speed chosen on the file screen. Instant: each line appears whole; A still turns the page.",
          "Instant: each line appears whole." },
    };
    const int i = row == ORACLES_DISPLAY_TEXT ? 0 : row >= ORACLES_DISPLAY_LETTERS && row <= ORACLES_DISPLAY_TEXT_SPEED ? (int)(row - ORACLES_DISPLAY_LETTERS) + 1 : -1;
    return i < 0 ? "" : texts[i][four_three != 0];
}

/* The box the game will show: the wide one chosen in Enhanced, on an Oracle's page; the game's otherwise. */
static int box_width(const OraclesHomeNav *nav, int enhanced, int *text_w)
{
    *text_w = enhanced && nav->display.text_box && !box_refused(nav) ? BOX_WIDE_TEXT : BOX_GAME_TEXT;
    return *text_w + 2 * BOX_MARGIN;
}

/* The size the box takes in a band of view_w x view_h shown at oracles_display_band_scale: Text size's choice where the
 * box so enlarged fits, else the next size down (view.h, text_box_enlarge.h); its scale against the band's pixels. */
static int size_shown(const OraclesHomeNav *nav, int view_w, int view_h, int box_w, unsigned *num, unsigned *den)
{
    *num = *den = 1u;
    int size = size_refused(nav) ? 0 : clamp(nav->display.text_size, ORACLES_DISPLAY_TEXT_SIZES);
    if (size == 2) {
        if (2 * box_w <= view_w) { *num = 2u; return 2; }
        size = 1;
    }
    if (size == 1) {
        const double k = oracles_display_band_scale(nav);
        int32_t rect[4];
        unsigned factor = 1u;
        const uint32_t x = (uint32_t)(view_w - box_w) / 2u, y = (uint32_t)(view_h - BOX_BOTTOM - BOX_H);
        if (oracles_text_box_enlarge(0.0, 0.0, (double)view_w * k, (double)view_h * k, k, x, y, (uint32_t)box_w, BOX_H, ORACLES_TEXT_BOX_ENLARGE_LARGE, 1, rect, &factor)) {
            *num = factor;
            *den = k >= 1.0 ? (unsigned)(k + 1e-6) : 1u;
            return 1;
        }
    }
    return 0;
}

/* That the size `chosen` falls back to `shown` in the view, empty when it does not. */
static const char *fallback(int shown, int chosen)
{
    if (shown >= chosen) return "";
    return shown == 1 ? "Large in this view: no room for Very large." : chosen == 2 ? "Normal in this view: no room for Very large."
                                                                       : "Normal in this view: no room for Large.";
}

/* The band and the box of the preview: whether the game plays in Enhanced, the band's size, the box's width and its
 * text's, the size it takes and its scale. */
typedef struct preview_box { int enhanced, view_w, view_h, box_w, text_w, size; unsigned num, den; } preview_box;

static preview_box preview_of(const OraclesHomeNav *nav)
{
    preview_box p;
    p.enhanced = oracles_display_played_profile(nav) == ORACLES_PROFILE_ENHANCED;
    oracles_display_surface(nav, &p.view_w, &p.view_h);
    p.box_w = box_width(nav, p.enhanced, &p.text_w);
    p.num = p.den = 1u;
    p.size = p.enhanced ? size_shown(nav, p.view_w, p.view_h, p.box_w, &p.num, &p.den) : 0;
    return p;
}

const char *oracles_display_text_note(const OraclesHomeNav *nav, unsigned row)
{
    if (row == ORACLES_DISPLAY_TEXT_SPEED) return oracles_display_text_row_live(nav, row) ? "" : original_reason;
    if (row == ORACLES_DISPLAY_TEXT_SIZE ? !oracles_display_hud_applies(nav) : !oracles_display_text_applies(nav)) return enhanced_reason;
    if ((row == ORACLES_DISPLAY_TEXT_BOX && box_refused(nav)) || (row == ORACLES_DISPLAY_TEXT_SIZE && size_refused(nav))) return oracles_display_fan_reason;
    if (oracles_display_pause_locked(nav, row)) return oracles_display_pause_reason;
    if (row == ORACLES_DISPLAY_LETTERS) return "Savestates taken with Compact need Compact.";   /* as Continuous transitions says of its own */
    if (row == ORACLES_DISPLAY_TEXT_BOX) return "Savestates taken with Wide need Wide.";
    if (row != ORACLES_DISPLAY_TEXT_SIZE) return "";
    const preview_box p = preview_of(nav);
    return p.enhanced ? fallback(p.size, clamp(nav->display.text_size, ORACLES_DISPLAY_TEXT_SIZES)) : "";
}

void oracles_display_text_preview(const OraclesHomeNav *nav, OraclesDisplayTextPreview *out)
{
    memset(out, 0, sizeof *out);
    const preview_box p = preview_of(nav);
    out->view_w = p.view_w;
    out->view_h = p.view_h;
    out->box_w = p.box_w;
    out->box_h = BOX_H;
    out->text_w = p.text_w;
    out->compact = p.enhanced && nav->display.text;
    out->size = p.size;
    out->scale_num = p.num;
    out->scale_den = p.den;
    out->note = "";
    if (!p.enhanced) {
        snprintf(out->label, sizeof out->label, "Faithful: the game's box, %d\xc3\x97%d", p.box_w, BOX_H);
    } else {
        /* The size it takes and its factor on the screen, two decimals at most. */
        char grown[32] = "";
        if (p.size) {
            char factor[16];
            snprintf(factor, sizeof factor, "%.2f", (double)p.num / (double)(p.den ? p.den : 1u));
            for (size_t n = strlen(factor); n && (factor[n - 1] == '0' || factor[n - 1] == '.'); n--) {
                const int point = factor[n - 1] == '.';
                factor[n - 1] = 0;
                if (point) break;
            }
            snprintf(grown, sizeof grown, ", %s (\xc3\x97%s)", p.size == 2 ? "very large" : "large", factor);
        }
        snprintf(out->label, sizeof out->label, "%s view, %d\xc3\x97%d \xc2\xb7 box %d\xc3\x97%d%s", oracles_display_view_names[clamp(nav->display.view, 3)],
                 p.view_w, p.view_h, p.box_w, BOX_H, grown);
        out->note = fallback(p.size, size_refused(nav) ? 0 : clamp(nav->display.text_size, ORACLES_DISPLAY_TEXT_SIZES));
    }
    const OraclesHomeGame *game = oracles_page_game_const(nav);
    oracles_display_text_sample(&game->font, out->compact, (unsigned)p.text_w, out->sample);
}

/* A character's index in the font, -1 for one it does not hold. */
static int font_index(char c)
{
    const int i = (unsigned char)c - ORACLES_HOME_FONT_FIRST;
    return i >= 0 && i < ORACLES_HOME_FONT_CHARACTERS ? i : -1;
}

/* How far a character moves the pen: its cell in Classic; its advance in Compact, or without a font a letter's average. */
static unsigned advance(const OraclesHomeFont *font, int compact, char c)
{
    if (!compact) return CELL_W;
    const int i = font_index(c);
    if (font && i >= 0) return font->advance[i];
    return c == ' ' ? BAR_SPACE_W : BAR_COMPACT_W;
}

static unsigned width_of(const OraclesHomeFont *font, int compact, const char *text, size_t length)
{
    unsigned w = 0;
    for (size_t i = 0; i < length; i++) w += advance(font, compact, text[i]);
    return w;
}

/* A word at (x, y): its letters' ink, or a bar as wide as it moves the pen, its last column left blank. */
static void put_word(const OraclesHomeFont *font, int compact, const char *word, size_t length, unsigned x, unsigned y, unsigned text_w,
                     uint8_t out[ORACLES_DISPLAY_SAMPLE_H][ORACLES_DISPLAY_SAMPLE_W])
{
    if (!font) {
        const unsigned w = width_of(NULL, compact, word, length);
        for (unsigned row = y + BAR_TOP; row < y + BAR_TOP + BAR_H; row++)
            for (unsigned col = x; col + 1u < x + w && col < text_w; col++) out[row][col] = 1;
        return;
    }
    unsigned pen = x;
    for (size_t k = 0; k < length; k++) {
        const int i = font_index(word[k]);
        const unsigned from = compact && i >= 0 ? font->left[i] : 0u, count = compact && i >= 0 ? font->width[i] : CELL_W;
        for (unsigned col = 0; i >= 0 && col < count; col++) {
            const unsigned at = pen + col;
            for (unsigned row = 0; at < text_w && row < LINE_H; row++)
                if (font->rows[i][row] & 0x80u >> (from + col)) out[y + row][at] = 1;
        }
        pen += advance(font, compact, word[k]);
    }
}

void oracles_display_text_sample(const OraclesHomeFont *font, int compact, unsigned text_w, uint8_t out[ORACLES_DISPLAY_SAMPLE_H][ORACLES_DISPLAY_SAMPLE_W])
{
    memset(out, 0, ORACLES_DISPLAY_SAMPLE_H * ORACLES_DISPLAY_SAMPLE_W);
    if (font && !font->read) font = NULL;
    if (text_w > ORACLES_DISPLAY_SAMPLE_W) text_w = ORACLES_DISPLAY_SAMPLE_W;
    /* The words in the box's lines, each line as many as its width holds, a space between them; two lines shown. */
    unsigned line = 0, pen = 0;
    const unsigned space = advance(font, compact, ' ');
    for (const char *p = sample; *p && line < ORACLES_DISPLAY_SAMPLE_H / LINE_H;) {
        const size_t length = strcspn(p, " ");
        const unsigned w = width_of(font, compact, p, length);
        if (pen && pen + space + w > text_w) { line++; pen = 0; }
        if (line >= ORACLES_DISPLAY_SAMPLE_H / LINE_H) break;
        if (pen) pen += space;
        put_word(font, compact, p, length, pen, line * LINE_H, text_w, out);
        pen += w;
        p += length;
        while (*p == ' ') p++;
    }
}
