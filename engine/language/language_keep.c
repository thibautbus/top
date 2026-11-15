/* The texts the compact text's layout keeps as the game lays them out (config/vwf_keep_<game>.tsv, language_internal.h),
 * and the digest of what a layout depends on besides the texts. */
#include "language_internal.h"
#include "sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const unsigned char oracles_vwf_keep_ages[], oracles_vwf_keep_seasons[], oracles_vwf_keep_moonrise[], oracles_vwf_keep_kinomi[], oracles_vwf_keep_temple[];

#define KEEP_RANGES 16u

typedef struct keep_row {
    unsigned ranges, first[KEEP_RANGES], last[KEEP_RANGES];
    unsigned languages;   /* a bit per OraclesLanguage */
} keep_row;

struct language_keep {
    keep_row *rows;
    unsigned count;
};

static int keep_field(const char **at, const char *end, const char **start, size_t *length)
{
    *start = *at;
    const char *tab = memchr(*at, '\t', (size_t)(end - *at));
    *length = (size_t)((tab ? tab : end) - *at);
    *at = tab ? tab + 1 : end;
    return *length ? 0 : -1;
}

static int keep_texts(const char *texts, size_t length, keep_row *row)
{
    char copy[256];
    if (length >= sizeof copy) return -1;
    memcpy(copy, texts, length);
    copy[length] = 0;
    row->ranges = 0;
    for (const char *at = copy; *at;) {
        unsigned first = 0, last = 0;
        int used = 0;
        if (sscanf(at, "%4x-%4x%n", &first, &last, &used) == 2 && used == 9) {}
        else if (sscanf(at, "%4x%n", &first, &used) == 1 && used == 4) last = first;
        else return -1;
        if (row->ranges == KEEP_RANGES || last < first) return -1;
        row->first[row->ranges] = first;
        row->last[row->ranges++] = last;
        at += used;
        if (*at && *at != ' ') return -1;
        while (*at == ' ') at++;
    }
    return row->ranges ? 0 : -1;
}

static int keep_languages(const char *languages, size_t length, unsigned *mask)
{
    *mask = 0;
    if (length == 3 && !memcmp(languages, "all", 3)) {
        *mask = (1u << ORACLES_LANGUAGE_COUNT) - 1u;
        return 0;
    }
    for (const char *at = languages, *end = languages + length; at < end;) {
        const char *comma = memchr(at, ',', (size_t)(end - at));
        const size_t n = (size_t)((comma ? comma : end) - at);
        char code[8] = { 0 };
        OraclesLanguage language;
        if (!n || n >= sizeof code) return -1;
        memcpy(code, at, n);
        if (oracles_language_parse(code, &language) != 0) return -1;
        *mask |= 1u << language;
        at += n + 1u;
    }
    return *mask ? 0 : -1;
}

void language_keep_free(language_keep *keep)
{
    if (!keep) return;
    free(keep->rows);
    free(keep);
}

language_keep *language_keep_parse(const char *table, char *error, size_t capacity)
{
    language_keep *k = calloc(1, sizeof *k);
    unsigned lines = 1, number = 0;
    for (const char *t = table; *t; t++) lines += *t == '\n';
    if (!k || !(k->rows = calloc(lines, sizeof *k->rows))) {
        snprintf(error, capacity, "out of memory");
        language_keep_free(k);
        return NULL;
    }
    for (const char *line = table; *line;) {
        const char *end = strchr(line, '\n');
        if (!end) end = line + strlen(line);
        const char *next = *end ? end + 1 : end;
        number++;
        if (end > line && end[-1] == '\r') end--;
        if (*line != '#' && end > line) {
            const char *at = line, *t, *l, *why;
            size_t tl, ll, wl;
            keep_row *row = &k->rows[k->count];
            if (keep_field(&at, end, &t, &tl) != 0 || keep_field(&at, end, &l, &ll) != 0 || keep_field(&at, end, &why, &wl) != 0
                || keep_texts(t, tl, row) != 0 || keep_languages(l, ll, &row->languages) != 0) {
                snprintf(error, capacity, "the compact text's kept texts: row %u is not one", number);
                language_keep_free(k);
                return NULL;
            }
            k->count++;
        }
        line = next;
    }
    return k;
}

const char *language_keep_table(OraclesRomRevision revision)
{
    switch (revision) {
    case ORACLES_ROM_REVISION_AGES_US: return (const char *)oracles_vwf_keep_ages;
    case ORACLES_ROM_REVISION_SEASONS_US: return (const char *)oracles_vwf_keep_seasons;
    case ORACLES_ROM_REVISION_MOONRISE_1_0_6: return (const char *)oracles_vwf_keep_moonrise;
    case ORACLES_ROM_REVISION_KINOMI_1_1_2: return (const char *)oracles_vwf_keep_kinomi;
    case ORACLES_ROM_REVISION_TEMPLE_1_073: return (const char *)oracles_vwf_keep_temple;
    default: return "";
    }
}

language_keep *language_keep_load(OraclesRomRevision revision, char *error, size_t capacity)
{
    return language_keep_parse(language_keep_table(revision), error, capacity);
}

const char *language_keep_table_half(OraclesRomRevision revision, OraclesGame family)
{
    if (revision < ORACLES_ROM_REVISION_COMBINED_1_5_EN || revision > ORACLES_ROM_REVISION_COMBINED_1_5_IT) return language_keep_table(revision);
    return language_keep_table(family == ORACLES_GAME_SEASONS ? ORACLES_ROM_REVISION_SEASONS_US : ORACLES_ROM_REVISION_AGES_US);
}

void oracles_language_layout_digest(const OraclesLanguageLayout *layout, OraclesRomRevision revision, char out[41])
{
    const char *table = language_keep_table(revision), *ages = language_keep_table_half(revision, ORACLES_GAME_AGES);
    char *both = NULL;
    if (ages != table) {   /* an image of two games: both halves' */
        const char *seasons = language_keep_table_half(revision, ORACLES_GAME_SEASONS);
        if (!(both = malloc(strlen(ages) + strlen(seasons) + 1u))) { snprintf(out, 41, "%040u", 0u); return; }
        memcpy(both, ages, strlen(ages));
        memcpy(both + strlen(ages), seasons, strlen(seasons) + 1u);
        table = both;
    }
    const size_t n = strlen(table);
    char *bytes = malloc(n + 64u);
    if (!bytes) { free(both); snprintf(out, 41, "%040u", 0u); return; }
    const unsigned lines = layout && layout->lines ? layout->lines : 2u;
    char extra[32] = "";
    if (layout && layout->fixed) snprintf(extra, sizeof extra, " fixed");
    if (lines != 2u) snprintf(extra + strlen(extra), sizeof extra - strlen(extra), " lines %u", lines);
    const int head = snprintf(bytes, 64u, "layout %u width %u%s\n", ORACLES_LANGUAGE_LAYOUT_REVISION, layout ? layout->width : 0u, extra);
    memcpy(bytes + head, table, n);
    oracles_sha1_hex((const uint8_t *)bytes, (size_t)head + n, out);
    free(bytes);
    free(both);
}

int language_keep_has(const language_keep *keep, OraclesLanguage language, unsigned id)
{
    for (unsigned r = 0; keep && r < keep->count; r++) {
        if (!(keep->rows[r].languages & 1u << language)) continue;
        for (unsigned i = 0; i < keep->rows[r].ranges; i++)
            if (id >= keep->rows[r].first[i] && id <= keep->rows[r].last[i]) return 1;
    }
    return 0;
}
