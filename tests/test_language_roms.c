/* The languages on the user's ROMs, found by their SHA-1 in $ORACLES_ROM_DIR (skipped, exit 77, without them): the
 * pack of each European ROM, its glyphs, the European English against the US one, and per language the source of
 * each US text's translation (the European text of its identifier, a moved one, an adapted one, the port's own, or
 * the US English), every text served holding the US text's commands and lines; each language composed into the US
 * image, and the coverage printed; the graphics that carry text, each loaded from the composed image as the European
 * game loads it in the language, and nothing else of the US ROM changed but the font and the header's sums; every row of
 * the corrections changing every text it names; and each fan game made on the US ROM by a BPS patch of the directory,
 * its texts that are US texts translated, its own in its English, each language composed in the banks its profile
 * leaves to it; each language, English (US) included, laid out for the compact text, every text laid out again
 * checked and the boxes counted; and each fan game's compact text: its routines the US ones, its English laid out
 * and checked the same way; and each image of Oracles Combined made by a patch of the directory on the European ROMs,
 * which holds its own language, its texts laid out in it a half at a time; Japanese from each Japanese ROM, every
 * US text served, its font the Japanese ROM's, the graphics of the screens it draws in Japanese loaded from the composed
 * image as the Japanese ROM loads them, its title's colours and sprites; and Polish from each Polish ROM, or from Kaxus's patch of the directory
 * applied to the US ROM, every US text served, its corrections made, the graphics of the screens it translates loaded
 * from the composed image as the Polish ROM loads them.  Exit 77 when the directory holds no game's ROMs. */
#include "bps.h"
#include "guest.h"
#include "sha1.h"
#include "language_internal.h"
#include "profile.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

typedef struct rom_file { uint8_t *data; size_t size; } rom_file;

/* The ROM of `revision` in `dir`, or an empty one. */
static rom_file find(const char *dir, OraclesRomRevision revision)
{
    rom_file found = { NULL, 0 };
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && !found.data && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".gbc")) continue;
        char path[1024], error[128];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0;
        uint8_t *data = oracles_rom_read_file(path, &size, error, sizeof error);
        OraclesRomInfo info;
        if (data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 && info.known && info.revision == revision) found = (rom_file){ data, size };
        else free(data);
    }
    if (listing) closedir(listing);
    return found;
}

/* The ids of the US texts whose European English differs. */
static unsigned english_differences(const rom_file *us, const rom_file *eu, OraclesGame game, char *list, size_t capacity)
{
    const OraclesRom us_rom = { us->data, us->size }, eu_rom = { eu->data, eu->size };
    OraclesTextSource a, b;
    CHECK(language_source_us(&us_rom, game, &a) == 0 && language_source_european(&eu_rom, game, ORACLES_LANGUAGE_ENGLISH, &b) == 0);
    unsigned differences = 0;
    list[0] = 0;
    uint8_t x[ORACLES_TEXT_MAX], y[ORACLES_TEXT_MAX];
    const unsigned groups = oracles_text_groups(&us_rom, &a);
    for (unsigned g = ORACLES_TEXT_DICTIONARY_GROUPS; g < groups; g++) {
        const int count = oracles_text_count(&us_rom, &a, g);
        for (int i = 0; i < count; i++) {
            size_t nx = 0, ny = 0;
            CHECK(oracles_text_decode(&us_rom, &a, g, (unsigned)i, x, sizeof x, &nx) == ORACLES_OK);
            const int same = oracles_text_decode(&eu_rom, &b, g, (unsigned)i, y, sizeof y, &ny) == ORACLES_OK && nx == ny && !memcmp(x, y, nx);
            if (same) continue;
            differences++;
            const size_t used = strlen(list);
            snprintf(list + used, capacity - used, "%s%04x", used ? " " : "", (g - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)i);
        }
    }
    return differences;
}

/* What each game's ROMs hold, as measured: the US texts, the texts whose European English is not the US one, and per
 * language the US texts the European text of their identifier serves as it is (the others: the table's). */
typedef struct expected {
    OraclesGame game;
    OraclesRomRevision us, eu;
    unsigned texts;
    const char *english_differs;
    unsigned same[ORACLES_LANGUAGE_COUNT];
} expected;

/* Whether `bytes` are in the text's. */
static int holds(const uint8_t *text, size_t length, const uint8_t *bytes, size_t n)
{
    for (size_t at = 0; at + n <= length; at++) if (!memcmp(text + at, bytes, n)) return 1;
    return 0;
}

/* A text corrected or rewritten, first-last in a language: the words it must hold, and the wrong ones it must not, in
 * config/language_own.tsv's syntax. */
typedef struct correction {
    OraclesGame game;
    OraclesLanguage language;
    unsigned first, last;
    const char *present, *absent;
} correction;

#define AGES ORACLES_GAME_AGES
#define SEASONS ORACLES_GAME_SEASONS
#define EN ORACLES_LANGUAGE_ENGLISH_EU
#define FR ORACLES_LANGUAGE_FRENCH
#define DE ORACLES_LANGUAGE_GERMAN
#define IT ORACLES_LANGUAGE_ITALIAN
#define ES ORACLES_LANGUAGE_SPANISH

static const correction corrections[] = {
    { AGES, ES, 0x2f10, 0x2f12, "Veran", "Onox" },
    { AGES, IT, 0x3104, 0x3104, "almeno due o tre", "una o due" },
    { AGES, FR, 0x313b, 0x313b, "\\col(03)Holodrum", "Labrynna" },
    { AGES, IT, 0x550b, 0x550b, "\\col(03)Holodrum", "Labrynna" },
    { SEASONS, ES, 0x301a, 0x301a, "\\col(03)Holodrum", "Labrynna" },
    { AGES, FR, 0x003e, 0x003e, "Steak de Roc", "Filet" },
    { AGES, IT, 0x3606, 0x3606, "Bulbo\\nTokay", "Globo" },
    { AGES, IT, 0x360c, 0x360c, "Bulbo Tokay", "Globo" },
    { AGES, IT, 0x2c0d, 0x2c0d, "Radiomine", "Radiobombe" },
    { AGES, DE, 0x3606, 0x3606, "Tokay-Glasauge", "Glausauge" },
    { AGES, FR, 0x4309, 0x4309, "Noix Gasha", "Graines" },
    { AGES, FR, 0x430a, 0x430a, "Noix\\nGasha", "Graine" },
    { AGES, FR, 0x430c, 0x430c, "force des\\n\\col(01)Noix Gasha", "force des\\n\\col(01)Graines" },
    { AGES, FR, 0x430d, 0x430d, "Noix\\nGasha", "Graines" },
    { AGES, FR, 0x430b, 0x430b, "noix", "graine" },
    { AGES, FR, 0x4311, 0x4311, "noix", "graines" },
    { SEASONS, FR, 0x430d, 0x430d, "Noix\\nGasha", "Graines" },
    { SEASONS, FR, 0x3101, 0x3101, "Noix Gasha", "Graines" },
    { SEASONS, ES, 0x3101, 0x3101, "Nueces\\nGasha", "Semillas" },
    { AGES, FR, 0x1e04, 0x1e04, "Graines\\nMystiques", "Mystère" },
    { AGES, FR, 0x1e06, 0x1e06, "Graines\\nMystiques", "Mystère" },
    { AGES, ES, 0x301a, 0x301a, "búsqueda ni", "búsqueda o" },
    { AGES, ES, 0x3026, 0x3026, "búsqueda ni", "búsqueda o" },
    { SEASONS, ES, 0x301a, 0x301a, "búsqueda ni", "búsqueda o" },
    { SEASONS, ES, 0x3026, 0x3026, "búsqueda ni", "búsqueda o" },
    { AGES, FR, 0x0010, 0x0010, "obtenez\\nl'\\col(01)Echo du\\nHurlement", "Hurlement Echo" },
    { AGES, FR, 0x05b4, 0x05b4, "l'\\col(02)Echo du\\nHurlement", "Cri de" },
    { AGES, FR, 0x05c4, 0x05c4, "l'\\col(02)Echo du\\nHurlement", "Cri de" },
    { AGES, DE, 0x0071, 0x0071, "Harfe der\\nZeiten", "Harfe der Zeit" },
    { AGES, DE, 0x0078, 0x0078, "Harfe der\\nZeiten\\col(00) zurück", "Harfe der Zeit" },
    { AGES, FR, 0x0068, 0x006c, "Flûte\\nEtrange", "Etrange\\nFlûte" },
    { AGES, FR, 0x0047, 0x0047, "Ecaille de\\nZora", "Ecaille des" },
    { AGES, FR, 0x3432, 0x3432, "Ecaille de\\nZora", "Ecaille des" },
    { AGES, FR, 0x344b, 0x344b, "Ecaille de\\nZora", "Ecaille des" },
    { AGES, FR, 0x3132, 0x3132, "l'Epée Biggoron\\col(00)!", "Epée de" },
    { AGES, FR, 0x303a, 0x303a, "Boîte à Anneaux", "Anneau\\col(00)" },
    { SEASONS, FR, 0x300b, 0x300b, "à Anneaux\\col(00)", "Anneau\\col(00)" },
    { AGES, ES, 0x2d04, 0x2d04, "Nuez de Tuni", "Nuez Tuni" },
    { AGES, DE, 0x0d01, 0x0d01, "Magieflasche", "Magische" },
    { SEASONS, DE, 0x0d01, 0x0d01, "Magieflasche", "Magische" },
    { SEASONS, FR, 0x003b, 0x003b, "Bizarre", "Etrange" },
    { SEASONS, FR, 0x0069, 0x006a, "Bizarre", "Etrange" },
    { SEASONS, FR, 0x0e1b, 0x0e1b, "Bizarre", "Etrange" },
    { SEASONS, FR, 0x172b, 0x172b, "Apaisante", "Appaisante" },
    { SEASONS, FR, 0x1728, 0x1728, "Nutritive", "Nourrissante" },
    { SEASONS, FR, 0x172a, 0x172a, "Nutritive", "Nourrissante" },
    { SEASONS, FR, 0x0063, 0x0063, "Oiseau de", "Oiseau en" },
    { SEASONS, FR, 0x3b14, 0x3b14, "Minerai\\nSolide", "Minerai Dur" },
    { SEASONS, FR, 0x0032, 0x0032, "des\\n\\col(01)Missiles", "Bombes" },
    { AGES, FR, 0x541e, 0x541e, "Aucun dégât", "1/2" },
    { SEASONS, IT, 0x5330, 0x5332, "a \\col(03)Monte Goron\\col(00) da", "da \\col(03)Monte" },
    { SEASONS, FR, 0x0b3b, 0x0b3b, "chez\\n\\col(03)Malon\\col(00).", "maison" },
    { SEASONS, ES, 0x5306, 0x5306, "Hadas\\col(00) de\\n\\col(03)Labrynna", "Hadas\\col(00) me" },
    { AGES, FR, 0x280c, 0x280c, "Labrynna\\col(00)", "\\col(04)" },
    { AGES, FR, 0x094d, 0x094d, "Steak de Roc", "Filet" },
    { AGES, IT, 0x094d, 0x094d, "Petto di Roccia", "Filetto" },
    { AGES, FR, 0x4309, 0x4309, "plantée la", "planté la" },
    { SEASONS, FR, 0x3101, 0x3101, "La noix est", "La graine" },
    { AGES, FR, 0x0046, 0x0046, "Graines\\nMystiques", "Mystique\\col(00)" },
    { AGES, FR, 0x0f01, 0x0f01, "Graines\\nMystiques", "Mystique\\col(00)" },
    { AGES, FR, 0x303a, 0x303a, "Graines\\nMystiques", "Mystique\\col(00)" },
    { AGES, FR, 0x3202, 0x3202, "Graines\\nMystiques", "Mystique\\col(00)" },
    { AGES, FR, 0x320c, 0x320c, "Graines\\nMystiques", "Mystique\\col(00)" },
    { SEASONS, FR, 0x0046, 0x0046, "Graines\\nMystiques", "Mystique\\col(00)" },
    { SEASONS, FR, 0x303a, 0x303a, "Graines\\nMystiques", "Mystique\\col(00)" },
    { SEASONS, FR, 0x3202, 0x3202, "Graines\\nMystiques", "Mystique\\col(00)" },
    { SEASONS, FR, 0x320c, 0x320c, "Graines\\nMystiques", "Mystique\\col(00)" },
    { AGES, FR, 0x4807, 0x4807, "Graines\\nMystère", "Mystères" },
    { AGES, FR, 0x480c, 0x480c, "Graines\\nMystère", "Mystères" },
    { AGES, DE, 0x0046, 0x0046, "Mystische\\nKerne", "Arkanum" },
    { AGES, DE, 0x0f01, 0x0f01, "Mystische Kerne", "Arkanum" },
    { AGES, DE, 0x1e04, 0x1e04, "Mystische\\nKerne", "Arkanum" },
    { AGES, DE, 0x1e06, 0x1e06, "Mysti-\\nschen Kerne", "Arkanum" },
    { AGES, DE, 0x303a, 0x303a, "Mystischen\\nKernen", "Arkanum" },
    { SEASONS, DE, 0x0046, 0x0046, "Mystische\\nKerne", "Arkanum" },
    { SEASONS, DE, 0x303a, 0x303a, "Mystischen\\nKernen", "Arkanum" },
    { AGES, DE, 0x301a, 0x301a, "weder\\nabgeschlossen\\nnoch", "nicht\\nabgeschlossen" },
    { AGES, DE, 0x3026, 0x3026, "weder\\nabgeschlossen\\nnoch", "nicht\\nabgeschlossen" },
    { SEASONS, DE, 0x301a, 0x301a, "weder\\nabgeschlossen\\nnoch", "nicht\\nabgeschlossen" },
    { SEASONS, DE, 0x3026, 0x3026, "weder\\nabgeschlossen\\nnoch", "nicht\\nabgeschlossen" },
    { AGES, FR, 0x301a, 0x301a, "n'as ni\\nfini la quête,\\nni utilisé de", "tous" },
    { SEASONS, FR, 0x301a, 0x301a, "n'as ni\\nfini la quête,\\nni utilisé de", "tous" },
    { AGES, FR, 0x3026, 0x3026, "et que tu n'es", "ou que" },
    { SEASONS, FR, 0x3026, 0x3026, "et que tu n'es", "ou que" },
    { AGES, IT, 0x301a, 0x301a, "meta né\\nhai usato", "non hai usato" },
    { SEASONS, IT, 0x301a, 0x301a, "meta né\\nhai usato", "non hai usato" },
    { AGES, IT, 0x3026, 0x3026, "prima\\nné sei venuto", "non sei venuto" },
    { SEASONS, IT, 0x3026, 0x3026, "prima\\nné sei venuto", "non sei venuto" },
    { AGES, FR, 0x05b4, 0x05b4, "Cité\\nLynna\\col(04)", "Lynnna" },
    { AGES, FR, 0x05c4, 0x05c4, "Cité\\nLynna\\col(04)", "Lynnna" },
    { AGES, FR, 0x0068, 0x006c, "appeler", "appeller" },
    { AGES, FR, 0x3602, 0x3602, "Ecaille\\nde Zora", "Ecaille des" },
    { SEASONS, FR, 0x300c, 0x300c, "Boîte à Anneaux", "Boîte à anneaux" },
    { SEASONS, FR, 0x303b, 0x303b, "de \\col(01)Boîte à\\nAnneaux", "boîte" },
    { SEASONS, FR, 0x5306, 0x5306, "les \\col(03)fées\\col(00) du \\col(03)Bois", "de\\nLabrynna" },
    { AGES, DE, 0x3700, 0x3700, "Lynna-Stadt", "Lynna-Dorf" },
    { SEASONS, DE, 0x0b58, 0x0b58, "Ich verrate dir", "lehre" },
    { AGES, FR, 0x1615, 0x161f, "Cité\\nLynna", "Cité de" },
    { AGES, FR, 0x1d16, 0x1d16, "Cité Lynna", "Cité de" },
    { AGES, FR, 0x05b3, 0x05b3, "Ile\\nCroissant", "du Croissant" },
    { AGES, FR, 0x05c3, 0x05c3, "Ile\\nCroissant", "du Croissant" },
    { AGES, FR, 0x360c, 0x360c, "Ile\\nCroissant", "Ile du" },
    { AGES, FR, 0x3415, 0x3416, "Roi des Zora", "Roi Zora" },
    { AGES, FR, 0x3443, 0x3443, "Roi des Zora", "Roi Zora" },
    { AGES, FR, 0x3427, 0x3427, "Roi des Zora", "Rois" },
    { AGES, FR, 0x550f, 0x550f, "Roi des\\nZora\\col(00)", "Zoras" },
    { AGES, FR, 0x3448, 0x3448, "Roi des\\nZora\\col(00)", "des\\col(00)" },
    { SEASONS, FR, 0x0b58, 0x0b58, "à \\col(03)Labrynna", "dans" },
    { SEASONS, FR, 0x4c2c, 0x4c2c, "à \\col(03)Labrynna", "dans" },
    { SEASONS, FR, 0x0c10, 0x0c10, "à\\n\\col(03)Labrynna", "dans\\n\\col(03)Labrynna" },
    { SEASONS, FR, 0x3106, 0x3106, "à\\n\\col(03)Labrynna", "en\\n\\col(03)Labrynna" },
    { SEASONS, FR, 0x4408, 0x4408, "appeler", "appeller" },
    { AGES, EN, 0x541e, 0x541e, "No damage", "1/2" },
    { AGES, EN, 0x094d, 0x094d, "Rock Brisket", "Sirloin" },
    { AGES, EN, 0x05b4, 0x05b4, "Lynna City", "Lynnna" },
    { AGES, EN, 0x05c4, 0x05c4, "Lynna City", "Lynnna" },
    { AGES, EN, 0x301a, 0x301a, "Oracle of\\nSeasons game.", "Seasons here" },
    { SEASONS, EN, 0x4f00, 0x4f00, "sorrow, despair", "depair" },
    { SEASONS, EN, 0x4c12, 0x4c12, "I don't\\nunderstand...", "undertand" },
    /* The port's own texts. */
    { AGES, FR, 0x4d28, 0x4d28, "Ce n'est pas\\nn'importe quel", "vieux" },
    { AGES, FR, 0x5516, 0x5516, "Note de la\\n\\col(03)Grande Fée\\col(00)\\npour \\col(03)Tingle", "pour la" },
};

static const expected games[] = {
    { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_AGES_US, ORACLES_ROM_REVISION_AGES_EU, 2824,
      "000c 0034 005d 0116 0200 0201 0202 0203 0204 0205 0206 0207 0208 0209 020a 020b 020c 020d 0505 0506 0525 0526 05b2 05c2 "
      "0700 0709 070d 070e 0711 0712 0a53 0b10 0e12 0e22 0e23 0e24 0e25 1e05 2400 2420 2492 2496 249e 2827 2a20 2f02 2f0a 2f1e "
      "2f1f 2f20 2f21 2f22 2f23 2f24 2f25 3018 301e 3024 302e 313b 3503 4509 450a 450b 450c 450d 4b02 4d23 4d25 4d26",
      { 0, 2820, 2817, 2794, 2818, 2811 } },
    { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_SEASONS_US, ORACLES_ROM_REVISION_SEASONS_EU, 1885,
      "0034 0037 0201 0203 0204 0208 0700 0709 0b42 0c12 0c18 1100 1729 1730 230d 2600 2c00 2e0d 2f19 2f1a 2f1b 2f1c 2f1d 2f1e "
      "2f1f 2f20 2f21 2f22 2f23 2f24 2f25 3003 30ae 3210 3404 3503 3a38 4101 450b 501f 5313 5317",
      { 0, 1883, 1879, 1877, 1878, 1878 } },
};

/* ---- the graphics, loaded as the games load them (an implementation of their own, apart from language_gfx.c's) ---- */

#define SPACE (16u * 0x6000u)   /* VRAM and WRAM, $8000-$dfff, by the destination's bank (its low 4 bits) */

typedef struct gfx_load { uint16_t value[SPACE]; int16_t entry[SPACE]; unsigned count; size_t at[64]; } gfx_load;

/* Loads header h of the table at `table` as the US loadGfxHeader does (language < 0) or the European one in
 * `language`; 0, or -1 when it does not decompress. */
static int load_header(const uint8_t *rom, size_t size, size_t table, unsigned h, int language, gfx_load *m)
{
    for (unsigned i = 0; i < SPACE; i++) { m->value[i] = 0xffff; m->entry[i] = -1; }
    m->count = 0;
    size_t at = table / 0x4000u * 0x4000u + (size_t)((rom[table + 2u * h] | rom[table + 2u * h + 1u] << 8) - 0x4000u);
    for (;;) {
        const uint8_t *e = rom + at, *source = e, *rest = e + 3;
        m->at[m->count] = at;
        if (language >= 0 && !e[0]) { source = e + 1 + 3 * language; rest = e + 16; at += 19; }
        else at += 6;
        const unsigned dest = (unsigned)rest[0] << 8 | rest[1], bytes = 16u * ((rest[2] & 0x7fu) + 1u);
        uint8_t data[2048];
        if (language_gfx_decompress(rom, size, oracles_rom_offset(source[0] & 0x3fu, (unsigned)source[1] << 8 | source[2]), source[0] >> 6, data, bytes, NULL) != 0) return -1;
        for (unsigned i = 0; i < bytes; i++) {
            const unsigned address = (dest & 0xfff0u) + i;
            if (address < 0x8000u || address >= 0xe000u) return -1;
            const size_t k = (dest & 0xfu) * 0x6000u + address - 0x8000u;
            m->value[k] = data[i];
            m->entry[k] = (int16_t)m->count;
        }
        if (++m->count == 64u) return -1;
        if (!(rest[2] & 0x80u)) return 0;
    }
}

/* What the ROMs hold, as measured: the US graphics a language replaces (bank:address), and how many each replaces. */
typedef struct expected_graphics {
    const char *sources;
    unsigned count[ORACLES_LANGUAGE_COUNT];
} expected_graphics;

/* Each language's graphics: every header the European ROM chooses by language loads, from the composed image as the
 * US game does, what the European game loads in the language over every byte the replaced graphics write; the
 * replaced graphics have their US sizes; the bytes still different are those of graphics the European ROM redrew in
 * every language (the name entry's layout), printed. */
static void check_graphics(const char *name, OraclesGame game, const OraclesLanguagePack *pack, const rom_file *us, const rom_file *eu,
                           const expected_graphics *expected)
{
    static gfx_load composed, european, original;
    static char sources[4096];
    sources[0] = 0;
    static uint8_t replaced_any[0x40][0x4000];   /* the US graphics some language replaces, listed by source */
    memset(replaced_any, 0, sizeof replaced_any);
    const size_t us_table = language_gfx_headers_us(game), eu_table = language_gfx_headers_european(game);
    /* Each US graphic's largest size in the US headers, in blocks (0: none loads it). */
    static uint8_t largest[0x40][0x4000];
    memset(largest, 0, sizeof largest);
    for (unsigned h = 0; h < GFX_HEADERS; h++) {
        size_t at = us_table / 0x4000u * 0x4000u + (size_t)((us->data[us_table + 2u * h] | us->data[us_table + 2u * h + 1u] << 8) - 0x4000u);
        for (const uint8_t *x = us->data + at;; x += 6) {
            uint8_t *slot = &largest[x[0] & 0x3fu][((unsigned)x[1] << 8 | x[2]) & 0x3fffu];
            if ((x[5] & 0x7fu) + 1u > *slot) *slot = (uint8_t)((x[5] & 0x7fu) + 1u);
            if (!(x[5] & 0x80u)) break;
        }
    }
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
        const unsigned count = oracles_language_pack_graphic_count(pack, language);
        static char replaced[4096];
        replaced[0] = 0;
        for (unsigned n = 0; n < count; n++) {
            OraclesLanguageGraphic g;
            CHECK(oracles_language_pack_graphic(pack, language, n, &g) == 0);
            char source[16];
            snprintf(source, sizeof source, "%02x:%04x", g.bank, g.address);
            snprintf(replaced + strlen(replaced), sizeof replaced - strlen(replaced), " %s", source);
            replaced_any[g.bank][g.address & 0x3fffu] = 1;
            /* Its US size: the largest any US header loads it with. */
            uint8_t data[2048];
            size_t consumed = 0;
            CHECK(language_gfx_decompress(g.data, g.length, 0, g.mode, data, 16u * (g.blocks + 1u), &consumed) == 0 && consumed == g.length);
            CHECK(largest[g.bank][g.address & 0x3fffu] == g.blocks + 1u);
        }
        uint8_t *image = NULL;
        size_t size = 0;
        OraclesLanguageReport report;
        char error[256];
        CHECK(oracles_language_compose(pack, language, us->data, us->size, NULL, &image, &size, &report, error, sizeof error) == 0);
        if (!image) continue;
        unsigned headers = 0, compared = 0, mismatched = 0, repointed = 0, left = 0;
        for (unsigned h = 0; h < GFX_HEADERS; h++) {
            const size_t at = eu_table / 0x4000u * 0x4000u + (size_t)((eu->data[eu_table + 2u * h] | eu->data[eu_table + 2u * h + 1u] << 8) - 0x4000u);
            int chosen = 0;
            for (size_t p = at; p < at + 64u * 19u; ) {
                if (!eu->data[p]) { chosen = 1; break; }
                if (!(eu->data[p + 5] & 0x80u)) break;
                p += 6;
            }
            if (!chosen) continue;
            headers++;
            CHECK(load_header(image, size, us_table, h, -1, &composed) == 0);
            CHECK(load_header(us->data, us->size, us_table, h, -1, &original) == 0);
            CHECK(load_header(eu->data, eu->size, eu_table, h, (int)language_european_index(language), &european) == 0);
            for (size_t k = 0; k < SPACE; k++) {
                if (composed.entry[k] < 0 || european.value[k] == 0xffff) continue;
                const size_t entry = composed.at[composed.entry[k]];
                char named[16];
                snprintf(named, sizeof named, "%02x:%02x%02x", us->data[entry] & 0x3fu, us->data[entry + 1u], us->data[entry + 2u]);
                const int moved = strstr(replaced, named) != NULL;
                if (moved) { compared++; mismatched += composed.value[k] != european.value[k]; }
                else left += composed.value[k] != european.value[k];
            }
            for (unsigned j = 0; j < composed.count; j++) {
                if (memcmp(image + composed.at[j], us->data + composed.at[j], 6) == 0) continue;
                repointed++;
                CHECK(!memcmp(image + composed.at[j] + 3, us->data + composed.at[j] + 3, 3));   /* the destination and the size stay */
            }
        }
        printf("%s %s graphics: %u replaced, %u compressed bytes; %u headers chosen by language, %u entries repointed, %u bytes as the "
               "European game loads them, %u different, %u left as the US game loads them\n",
               name, oracles_language_code(language), report.graphics, report.graphics_bytes, headers, repointed, compared, mismatched, left);
        CHECK(report.graphics == oracles_language_pack_graphic_count(pack, language) && report.graphics == expected->count[language]);
        CHECK(mismatched == 0 && compared > 0);
        free(image);
    }
    for (unsigned b = 0; b < 0x40u; b++)
        for (unsigned a = 0; a < 0x4000u; a++)
            if (replaced_any[b][a]) snprintf(sources + strlen(sources), sizeof sources - strlen(sources), "%s%02x:%04x", sources[0] ? " " : "", b, 0x4000u + a);
    printf("%s graphics replaced: %s\n", name, sources);
    CHECK(!strcmp(sources, expected->sources));
}

static const expected_graphics graphics_expected[] = {
    { "29:56ac 29:5a51 29:5b5f 29:5ca9 29:5de7 29:5f33 29:6071 29:61ac 29:62ff 29:644d 29:6589 29:66cc 29:67dd 37:7544 37:7cba "
      "37:7e5d 37:7edf 37:7f62 38:4470 38:456a 38:470d 38:48b5 38:4930 38:4e0a 38:5274 38:5426 38:55bb 38:5641 38:57cd 38:5982 "
      "38:5b12 38:5b90 38:5d07 38:664f",
      { 0, 22, 34, 34, 34, 34 } },
    { "28:5199 28:5542 28:5646 28:57d2 28:593e 28:5a9b 28:5c23 28:5d56 28:5e99 28:5feb 28:6184 28:628f 37:7440 37:7bbc 37:7d5f "
      "37:7de1 37:7e64 38:4372 38:446c 38:460f 38:47b7 38:4832 38:4d0c 38:5176 38:5328 38:54bd 38:5543 38:56d4 38:5889 38:5a19 "
      "38:5a97 38:5c0e 38:6556",
      { 0, 21, 33, 33, 33, 33 } },
};

/* The image composed in a language against the US ROM: in its first megabyte, nothing changed but the header's size and
 * sums, the font's glyphs, and the graphics headers' entries that load another graphic than the US one (repointed, or
 * whose graphic was written in place) with the bytes they now point to; the mods' banks empty; the language in its
 * own.  English (US) composes nothing. */
static void check_composed(const char *name, OraclesGame game, const OraclesLanguagePack *pack, const rom_file *us)
{
    static uint8_t allowed[0x100000];
    const size_t table = language_gfx_headers_us(game), font = language_font_us(game);
    char error[256];
    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_ENGLISH, us->data, us->size, NULL, &image, &size, &report, error, sizeof error) == -1 && !image);
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
        CHECK(oracles_language_compose(pack, language, us->data, us->size, NULL, &image, &size, &report, error, sizeof error) == 0);
        if (!image) continue;
        CHECK(size == 2u * us->size && us->size == sizeof allowed);
        memset(allowed, 0, sizeof allowed);
        allowed[0x148] = allowed[0x14d] = allowed[0x14e] = allowed[0x14f] = 1;
        memset(allowed + font, 1, 0x100u * GLYPH_BYTES);
        unsigned entries = 0, graphic_bytes = 0;
        for (unsigned h = 0; h < GFX_HEADERS; h++) {
            const size_t at = table / 0x4000u * 0x4000u + (size_t)((us->data[table + 2u * h] | us->data[table + 2u * h + 1u] << 8) - 0x4000u);
            for (size_t e = at;; e += 6u) {
                uint8_t data[2048], original[2048];
                size_t consumed = 0;
                const size_t bytes = 16u * ((image[e + 5u] & 0x7fu) + 1u);
                const size_t source = oracles_rom_offset(image[e] & 0x3fu, (unsigned)image[e + 1u] << 8 | image[e + 2u]);
                const size_t us_source = oracles_rom_offset(us->data[e] & 0x3fu, (unsigned)us->data[e + 1u] << 8 | us->data[e + 2u]);
                CHECK(language_gfx_decompress(image, size, source, image[e] >> 6, data, bytes, &consumed) == 0);
                CHECK(language_gfx_decompress(us->data, us->size, us_source, us->data[e] >> 6, original, bytes, NULL) == 0);
                if (memcmp(image + e, us->data + e, 3) || memcmp(data, original, bytes)) {
                    CHECK(source + consumed <= sizeof allowed);
                    memset(allowed + e, 1, 3);
                    if (source + consumed <= sizeof allowed) memset(allowed + source, 1, consumed);
                    entries++;
                    graphic_bytes += (unsigned)consumed;
                }
                if (!(us->data[e + 5u] & 0x80u)) break;
            }
        }
        unsigned changed = 0, outside = 0, mods = 0;
        for (size_t i = 0; i < us->size; i++) {
            changed += image[i] != us->data[i];
            outside += image[i] != us->data[i] && !allowed[i];
        }
        for (size_t i = us->size; i < (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u; i++) mods += image[i] != 0;
        printf("%s %s composed: %u bytes of the US ROM changed, %u of them elsewhere than the header, the font, %u entries loading the language's graphics "
               "and the %u bytes they load; %u bytes written in the mods' banks\n", name, oracles_language_code(language), changed, outside, entries,
               graphic_bytes, mods);
        CHECK(outside == 0 && mods == 0 && entries > 0);
        CHECK(!memcmp(image + (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES));
        free(image);
        image = NULL;
    }
}

/* The compact text's layout of a game's six languages, as measured: the texts laid out again, those kept by the table
 * or a call within a line, those that did not hold, the boxes of all the texts before and after, and the hyphens the
 * texts laid out again dropped, those of the words joined at a line's end. */
typedef struct expected_layout { unsigned laid_out, kept, failed, boxes, boxes_laid_out, joined; } expected_layout;
static const expected_layout layouts_expected[2][ORACLES_LANGUAGE_COUNT] = {
    { { 1638, 18, 0, 7911, 6318, 20 }, { 1643, 18, 0, 7922, 6326, 20 }, { 1756, 16, 0, 8481, 6431, 10 },
      { 1764, 17, 0, 8875, 7244, 1125 }, { 1658, 17, 0, 7806, 6106, 129 }, { 1729, 18, 0, 8595, 6733, 0 } },
    { { 929, 24, 0, 4833, 4038, 3 }, { 940, 24, 0, 4853, 4053, 3 }, { 1025, 18, 0, 5391, 4212, 6 },
      { 1044, 23, 0, 5487, 4532, 700 }, { 968, 23, 0, 4946, 3985, 165 }, { 1001, 23, 0, 5241, 4269, 0 } },
};

/* The length of the text stored at `at`, its terminator included. */
static size_t stored_length(const uint8_t *image, size_t size, size_t at)
{
    size_t n = 0;
    while (at + n < size && image[at + n]) n += image[at + n] >= 0x10u || image[at + n] == 0x01u ? 1u : 2u;
    return n + 1u;
}

/* Whether a text is one the layout leaves to its rules: a choice, a call, a secret, a jump, a cmd8. */
static int with_structure(const uint8_t *text, size_t length)
{
    for (size_t at = 0; at + 1u < length; at += text[at] >= 0x10u || text[at] <= 0x01u ? 1u : 2u) {
        const uint8_t c = text[at], p = text[at + 1u];
        if (c == 0x07 || c == 0x08 || c == 0x0f || (c == 0x0a && p >= 2u) || (c == 0x0c && p >> 3 == 2u)) return 1;
    }
    return 0;
}

typedef struct layout_count { unsigned flagged, unheld, boxes, boxes_laid_out, joined; } layout_count;

static unsigned hyphens(const uint8_t *text, size_t length)
{
    unsigned n = 0;
    for (size_t at = 0; at < length; at += text[at] >= 0x10u || text[at] <= 0x01u ? 1u : 2u) n += text[at] == '-';
    return n;
}

/* The texts whose hyphen at a line's end the layout keeps, a compound's, or drops, joining a word broken by syllables or
 * that the language writes whole elsewhere: what each laid out holds. */
static const struct { OraclesGame game; OraclesLanguage language; unsigned id; const char *holds; } hyphens_expected[] = {
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x2464, "hot lava-like passion" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x0a67, "real-looking" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH_EU, 0x0a67, "real-looking" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x0564, "something" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x3306, "without" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x1d08, "grand-chose" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x2f13, "sous-estim" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_FRENCH, 0x2490, "Rouleroche" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_GERMAN, 0x000a, "Vergangenheit" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ITALIAN, 0x001c, "coraggio" },
    { ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_ENGLISH, 0x2701, "nice-sized" }, { ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_ENGLISH, 0x3703, "year-round" },
    { ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_FRENCH, 0x3a10, "pensent-ils" }, { ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_FRENCH, 0x002b, "donneront" },
    { ORACLES_GAME_SEASONS, ORACLES_LANGUAGE_GERMAN, 0x000a, "herbeizurufen" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x0b00, "headstone" }, { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH, 0x0b01, "headstone" },
    { ORACLES_GAME_AGES, ORACLES_LANGUAGE_ENGLISH_EU, 0x0b00, "headstone" },
};

/* The same in the fan games' English: the closed compounds the games break at a line's end, joined. */
static const struct { OraclesRomRevision revision; unsigned id; const char *holds; } fan_hyphens_expected[] = {
    { ORACLES_ROM_REVISION_KINOMI_1_1_2, 0x0605, "afterlife" }, { ORACLES_ROM_REVISION_KINOMI_1_1_2, 0x3431, "underwater" },
    { ORACLES_ROM_REVISION_MOONRISE_1_0_6, 0x0b00, "headstone" }, { ORACLES_ROM_REVISION_MOONRISE_1_0_6, 0x0b01, "headstone" },
};

/* The text TX_`id` the index composed at `first` serves laid out again, its terminator included; NULL when it is not. */
static const uint8_t *laid_text(const uint8_t *image, size_t size, size_t first, unsigned id, size_t *length)
{
    const uint8_t *index = image + first, *row = index + ORACLES_LANGUAGE_INDEX_GROUPS + 4u * (ORACLES_TEXT_DICTIONARY_GROUPS + (id >> 8));
    const unsigned entries = row[0] | row[1] << 8, count = row[2] | row[3] << 8;
    if ((id & 0xffu) >= count) return NULL;
    const uint8_t *entry = index + entries + 3u * (id & 0xffu);
    const unsigned addr = entry[1] | entry[2] << 8;
    if (!(addr & ORACLES_LANGUAGE_LAID_OUT)) return NULL;
    const size_t at = (size_t)entry[0] * 0x4000u + ((addr & ~ORACLES_LANGUAGE_LAID_OUT) - 0x4000u);
    *length = stored_length(image, size, at);
    return image + at;
}

/* Whether `text` holds the characters `holds`, its line breaks read as spaces. */
static int holds_words(const uint8_t *text, size_t length, const char *holds)
{
    const size_t n = strlen(holds);
    for (size_t at = 0; at + n <= length; at++) {
        size_t k = 0;
        while (k < n && (text[at + k] == (uint8_t)holds[k] || (text[at + k] == 0x01u && holds[k] == ' '))) k++;
        if (k == n) return 1;
    }
    return 0;
}

/* The texts of the index composed at `first` in `image` for `language`: those laid out again (flagged), checked against
 * the text they stand for (English's decoded from `rom` by `source`, a language's in `pack`): within the layout's width,
 * the same commands, no more boxes; and the study's boxes, before and after. */
static layout_count count_layout(const uint8_t *image, size_t size, size_t first, const OraclesRom *rom, const OraclesTextSource *source, unsigned groups,
                                 const OraclesLanguagePack *pack, OraclesLanguage language, const language_wrap *wrap)
{
    layout_count n = { 0, 0, 0, 0, 0 };
    const uint8_t *index = image + first;
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups && wrap; group++) {
        const uint8_t *row = index + ORACLES_LANGUAGE_INDEX_GROUPS + 4u * group;
        const unsigned entries = row[0] | row[1] << 8, count = row[2] | row[3] << 8;
        for (unsigned i = 0; i < count; i++) {
            const unsigned id = (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | i;
            const uint8_t *entry = index + entries + 3u * i;
            const unsigned addr = entry[1] | entry[2] << 8;
            uint8_t decoded[ORACLES_TEXT_MAX];
            size_t length = 0;
            const uint8_t *text = language == ORACLES_LANGUAGE_ENGLISH
                ? (oracles_text_decode(rom, source, group, i, decoded, sizeof decoded, &length) == ORACLES_OK ? decoded : NULL)
                : oracles_language_pack_text(pack, language, id, &length);
            if (!text) continue;
            const uint8_t *served = text;
            size_t served_length = length;
            if (addr & ORACLES_LANGUAGE_LAID_OUT) {
                n.flagged++;
                const size_t at = (size_t)entry[0] * 0x4000u + ((addr & ~ORACLES_LANGUAGE_LAID_OUT) - 0x4000u);
                served = image + at;
                served_length = stored_length(image, size, at);
                if (!language_wrap_fits(wrap, served, served_length) || !oracles_text_same_commands(text, length, served, served_length)
                    || language_text_boxes(served, served_length) > language_text_boxes(text, length)) n.unheld++;
                n.joined += hyphens(text, length) - hyphens(served, served_length);
            }
            /* The study's measure: the boxes of the texts the text box shows, but those the layout leaves to their rules. */
            if (group != ORACLES_TEXT_DICTIONARY_GROUPS + 9u && !with_structure(text, length)) {
                n.boxes += language_text_boxes(text, length);
                n.boxes_laid_out += language_text_boxes(served, served_length);
            }
        }
    }
    return n;
}

/* Each language laid out for the compact text, English (US) included: every text laid out again in the image fits a
 * line of 128 pixels in the image's font, gives the commands of the text it stands for and shows no more boxes; the
 * image differs from the one composed without the layout in the language's banks only; the boxes as measured. */
static void check_layout(const char *name, OraclesGame game, const OraclesLanguagePack *pack, const rom_file *us)
{
    const OraclesLanguageLayout layout = { 128, 0, 0, NULL };
    const OraclesRom us_rom = { us->data, us->size };
    OraclesTextSource source;
    CHECK(language_source_us(&us_rom, game, &source) == 0);
    const unsigned groups = oracles_text_groups(&us_rom, &source);
    OraclesRomInfo info;
    char error[256];
    CHECK(oracles_rom_identify(us->data, us->size, &info, error, sizeof error) == 0);
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
        uint8_t *image = NULL, *classic = NULL;
        size_t size = 0, classic_size = 0;
        OraclesLanguageReport report, plain;
        if (language == ORACLES_LANGUAGE_ENGLISH)
            CHECK(oracles_language_compose_english(info.sha1, us->data, us->size, &layout, &image, &size, &report, error, sizeof error) == 0);
        else {
            CHECK(oracles_language_compose(pack, language, us->data, us->size, &layout, &image, &size, &report, error, sizeof error) == 0);
            CHECK(oracles_language_compose(pack, language, us->data, us->size, NULL, &classic, &classic_size, &plain, error, sizeof error) == 0);
        }
        if (!image) { free(classic); continue; }
        const size_t first = (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
        if (classic) {
            unsigned outside = 0;
            for (size_t i = 0; i < first && i < classic_size; i++) outside += image[i] != classic[i] && i != 0x14eu && i != 0x14fu;
            CHECK(classic_size == size && outside == 0);
        }
        const OraclesRom rom = { image, size };
        const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
        OraclesTextFont font;
        CHECK(oracles_text_font(&rom, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
        language_wrap *wrap = language_wrap_new(&font, layout.width, language);
        const layout_count n = count_layout(image, size, first, &us_rom, &source, groups, pack, language, wrap);
        const expected_layout *e = &layouts_expected[game == ORACLES_GAME_SEASONS][language];
        printf("%s %s laid out for the compact text: %u texts laid out again (%u in the index), %u kept by the table or a call, %u that did not hold; "
               "%u boxes for %u, %.1f %%; the texts of the box without a choice, a call, a secret or a jump: %u boxes for %u, %.1f %%; %u hyphens joined\n",
               name, oracles_language_code(language), report.laid_out, n.flagged, report.layout_kept, report.layout_failed, report.boxes_laid_out, report.boxes,
               100.0 * ((double)report.boxes_laid_out - report.boxes) / (report.boxes ? report.boxes : 1u), n.boxes_laid_out, n.boxes,
               100.0 * ((double)n.boxes_laid_out - n.boxes) / (n.boxes ? n.boxes : 1u), n.joined);
        CHECK(wrap && n.flagged == report.laid_out && n.unheld == 0);
        CHECK(report.laid_out == e->laid_out && report.layout_kept == e->kept && report.layout_failed == e->failed);
        CHECK(report.boxes == e->boxes && report.boxes_laid_out == e->boxes_laid_out && n.joined == e->joined);
        for (unsigned h = 0; h < sizeof hyphens_expected / sizeof hyphens_expected[0]; h++) {
            if (hyphens_expected[h].game != game || hyphens_expected[h].language != language) continue;
            size_t laid_length = 0;
            const uint8_t *laid = laid_text(image, size, first, hyphens_expected[h].id, &laid_length);
            if (!laid || !holds_words(laid, laid_length, hyphens_expected[h].holds))
                fprintf(stderr, "%s %s: TX_%04x laid out does not hold \"%s\"\n", name, oracles_language_code(language), hyphens_expected[h].id, hyphens_expected[h].holds);
            CHECK(laid && holds_words(laid, laid_length, hyphens_expected[h].holds));
        }
        language_wrap_free(wrap);
        free(image);
        free(classic);
    }
}

/* Every row of config/language_corrections.tsv changed every text it names in every language it names, and every
 * corrected text held. */
static void check_corrections_used(const char *name, const language_corrections *corrections)
{
    unsigned dead = 0;
    for (unsigned n = 0; n < language_corrections_count(corrections); n++) {
        if (language_corrections_applied(corrections, n) == language_corrections_named(corrections, n)) continue;
        fprintf(stderr, "%s: the corrections' row %u of the game changes %u of the %u texts and languages it names\n", name, n + 1u,
                language_corrections_applied(corrections, n), language_corrections_named(corrections, n));
        dead++;
    }
    printf("%s: %u rows of corrections, %u changing less than they name, %u corrected texts refused\n", name, language_corrections_count(corrections), dead,
           language_corrections_refused(corrections));
    CHECK(dead == 0 && language_corrections_refused(corrections) == 0 && language_corrections_count(corrections) > 0);
}

/* ---- the fan games ------------------------------------------------------------------------------------------------ */

/* What a fan game's image holds, as measured: its texts, those that are the US text of their identifier, of another
 * identifier, and its own; per language, the texts translated and those left in its English; its English laid out for
 * the compact text. */
typedef struct expected_fan {
    OraclesRomRevision revision;
    OraclesGame game;
    const char *name;
    unsigned texts, same_id, elsewhere, own;
    unsigned translated[ORACLES_LANGUAGE_COUNT], english[ORACLES_LANGUAGE_COUNT];
    unsigned glyphs;
    expected_layout layout;
} expected_fan;

static const expected_fan fans[] = {
    { ORACLES_ROM_REVISION_MOONRISE_1_0_6, ORACLES_GAME_AGES, "moonrise", 2838, 2534, 33, 271, { 0, 2567, 2567, 2567, 2567, 2567 }, { 0, 271, 271, 271, 271, 271 }, 35,
      { 1637, 18, 0, 8075, 6474, 19 } },
    { ORACLES_ROM_REVISION_KINOMI_1_1_2, ORACLES_GAME_AGES, "kinomi", 2953, 2453, 4, 496, { 0, 2457, 2457, 2457, 2457, 2457 }, { 0, 496, 496, 496, 496, 496 }, 35,
      { 1726, 24, 0, 8918, 7074, 34 } },
    { ORACLES_ROM_REVISION_TEMPLE_1_073, ORACLES_GAME_SEASONS, "temple", 1906, 1521, 37, 348, { 0, 1558, 1558, 1558, 1558, 1558 }, { 0, 348, 348, 348, 348, 348 }, 35,
      { 919, 24, 0, 4927, 4120, 3 } },
};

/* The image of fan game `revision`: a BPS patch of `dir` applied to the US ROM. */
static rom_file find_fan(const char *dir, const rom_file *us, OraclesRomRevision revision)
{
    rom_file found = { NULL, 0 };
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && !found.data && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".bps")) continue;
        char path[1024], error[128];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t patch_size = 0, size = 0;
        uint8_t *patch = oracles_rom_read_file(path, &patch_size, error, sizeof error);
        uint8_t *data = patch ? oracles_bps_apply(us->data, us->size, patch, patch_size, &size, error, sizeof error) : NULL;
        free(patch);
        OraclesRomInfo info;
        if (data && oracles_rom_identify(data, size, &info, error, sizeof error) == 0 && info.known && info.revision == revision) found = (rom_file){ data, size };
        else free(data);
    }
    if (listing) closedir(listing);
    return found;
}

static unsigned read16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* getTextAddress, where the profile binds it: its code reads wTextIndexH and wTextIndexL, the two bases' tables and
 * the split, and writes w7ActiveBank, at the profile's addresses (textbox.s). */
static void check_hook(const char *name, const uint8_t *rom, const OraclesGuestTables *t)
{
    const uint8_t *g = rom + oracles_rom_offset(t->get_text_address.bank, t->get_text_address.addr);
    const int bound = g[0] == 0xd5 && g[10] == 0xfa && read16(g + 11) == t->text_index_h.addr && g[24] == 0xfa && read16(g + 25) == t->text_index_l.addr
                      && g[40] == 0x21 && read16(g + 41) == t->text_offset1_table.addr && g[52] == 0xfe && g[53] == t->text_offset_split_index
                      && g[60] == 0x21 && read16(g + 61) == t->text_offset2_table.addr && g[81] == 0xea && read16(g + 82) == t->active_text_bank.addr
                      && g[84] == 0xcb && g[88] == 0xd1 && g[89] == 0xc9;
    printf("%s: getTextAddress %02x:%04x, text tables %04x and %04x, split %02x: %s\n", name, t->get_text_address.bank, t->get_text_address.addr,
           t->text_offset1_table.addr, t->text_offset2_table.addr, t->text_offset_split_index, bound ? "the routine's code" : "NOT the routine");
    CHECK(bound);
}

/* The bytes of the SM83 instruction that starts with `op`. */
static unsigned instruction_bytes(uint8_t op)
{
    switch (op) {
    case 0x01: case 0x11: case 0x21: case 0x31: case 0x08: case 0xc2: case 0xc3: case 0xca: case 0xd2: case 0xda:
    case 0xc4: case 0xcc: case 0xcd: case 0xd4: case 0xdc: case 0xea: case 0xfa:
        return 3;
    case 0x06: case 0x0e: case 0x16: case 0x1e: case 0x26: case 0x2e: case 0x36: case 0x3e: case 0x18: case 0x20: case 0x28:
    case 0x30: case 0x38: case 0xc6: case 0xce: case 0xd6: case 0xde: case 0xe6: case 0xee: case 0xf6: case 0xfe: case 0xe0:
    case 0xf0: case 0xe8: case 0xf8: case 0xcb: case 0x10:
        return 2;
    default:
        return 1;
    }
}

/* Whether a fan game's routine at `fan` is the US routine at `us` of `length` bytes, instruction for instruction: the
 * same bytes but the addresses in the ROM its calls, jumps and loads name (the code moved), the WRAM it reads and writes
 * the US routine's. */
static int same_routine(const uint8_t *us, const uint8_t *fan, size_t length)
{
    for (size_t at = 0; at < length;) {
        const unsigned n = instruction_bytes(us[at]);
        if (at + n > length || fan[at] != us[at]) return 0;
        if (n == 3u && (us[at + 2u] < 0x80u) != (fan[at + 2u] < 0x80u)) return 0;
        if (n == 3u && us[at + 2u] >= 0x80u && (fan[at + 1u] != us[at + 1u] || fan[at + 2u] != us[at + 2u])) return 0;
        if (n == 2u && fan[at + 1u] != us[at + 1u]) return 0;
        at += n;
    }
    return 1;
}

/* The compact text's points in a fan game (`fan`, profile tables `t`), against the US ROM of its Oracle (`us`, tables
 * `u`): drawLineOfText, setLineTextBuffers, displayNextTextCharacter up to its sound and textControlCodeC_2 up to its
 * search of a free place, each the US routine (textbox.s); the WRAM the transaction reads and writes the US one; the
 * reveal's and the cursor place's points found in them as in the US. */
static void check_compact_points(const char *name, const uint8_t *fan, size_t fan_size, const uint8_t *us, const OraclesGuestTables *t,
                                 const OraclesGuestTables *u)
{
    const struct { const OraclesGuestSym *fan, *us; size_t length; } routines[] = {
        { &t->draw_line_of_text, &u->draw_line_of_text, 0x3c }, { &t->set_line_text_buffers, &u->set_line_text_buffers, 0x26 },
        { &t->display_next_text_character, &u->display_next_text_character, 0x59 }, { &t->text_control_code_option, &u->text_control_code_option, 0x1c },
    };
    unsigned same = 0;
    for (size_t r = 0; r < sizeof routines / sizeof routines[0]; r++)
        same += routines[r].fan->bank == routines[r].us->bank
                && same_routine(us + oracles_rom_offset(routines[r].us->bank, routines[r].us->addr),
                                fan + oracles_rom_offset(routines[r].fan->bank, routines[r].fan->addr), routines[r].length);
    const OraclesGuestSym *wram[][2] = {
        { &t->text_gfx_buffer, &u->text_gfx_buffer }, { &t->line_text_buffer, &u->line_text_buffer },
        { &t->line_attributes_buffer, &u->line_attributes_buffer }, { &t->line_delays_buffer, &u->line_delays_buffer },
        { &t->line_sounds_buffer, &u->line_sounds_buffer }, { &t->line_sound_effects_buffer, &u->line_sound_effects_buffer },
        { &t->next_text_column, &u->next_text_column }, { &t->text_line_tiles, &u->text_line_tiles }, { &t->textbox_column, &u->textbox_column },
        { &t->text_display_state, &u->text_display_state }, { &t->text_display_mode, &u->text_display_mode },
        { &t->textbox_attributes, &u->textbox_attributes }, { &t->textbox_option_positions, &u->textbox_option_positions },
        { &t->text_is_active, &u->text_is_active }, { &t->textbox_map, &u->textbox_map }, { &t->font_start, &u->font_start },
        { &t->secret_text, &u->secret_text }, { &t->secret_text_end, &u->secret_text_end },
    };
    const unsigned symbols = (unsigned)(sizeof wram / sizeof wram[0]);
    unsigned bound = 0;
    for (unsigned w = 0; w < symbols; w++) bound += wram[w][0]->bank == wram[w][1]->bank && wram[w][0]->addr == wram[w][1]->addr;
    const int points = oracles_guest_find_reveal_point(t, fan, fan_size) == t->display_next_text_character.addr + 0x3du
                       && oracles_guest_find_cursor_place_point(t, fan, fan_size) == t->text_control_code_option.addr + 0x11u;
    printf("%s: the compact text's routines %02x:%04x, %02x:%04x, %02x:%04x, %02x:%04x: %u of 4 the US code; %u of %u symbols of the WRAM and the font "
           "the US ones; its points %s\n", name, t->draw_line_of_text.bank, t->draw_line_of_text.addr, t->set_line_text_buffers.bank,
           t->set_line_text_buffers.addr, t->display_next_text_character.bank, t->display_next_text_character.addr, t->text_control_code_option.bank,
           t->text_control_code_option.addr, same, bound, symbols, points ? "found" : "NOT found");
    CHECK(same == 4u && bound == symbols && points);
}

/* A fan game's English laid out for the compact text in the banks its profile leaves to a language: every text laid out
 * again fits a line of 128 pixels in the image's font, gives the commands of its text and shows no more boxes; nothing
 * else of the image changed but the header's sums; the boxes as measured.  A translation of it is not laid out. */
static void check_fan_layout(const expected_fan *e, const rom_file *fan, const OraclesRomInfo *info, const OraclesCompatProfile *profile,
                             const OraclesLanguagePack *pack)
{
    const OraclesLanguageLayout layout = { 128, 0, 0, NULL };
    const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
    const OraclesRom fan_rom = { fan->data, fan->size };
    OraclesTextSource source;
    CHECK(language_source_tables(&fan_rom, t, &source) == 0);
    const unsigned groups = oracles_text_groups(&fan_rom, &source);
    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    char error[256];
    CHECK(oracles_language_compose_english(info->sha1, fan->data, fan->size, &layout, &image, &size, &report, error, sizeof error) == 0);
    if (!image) { fprintf(stderr, "%s: %s\n", e->name, error); return; }
    const size_t first = (size_t)oracles_compat_language_bank(profile) * 0x4000u, end = (LANGUAGE_LAST_BANK + 1u) * 0x4000u;
    unsigned changed = 0;
    for (size_t i = 0; i < size && i < fan->size; i++) changed += image[i] != fan->data[i] && !(i >= first && i < end) && !(i >= 0x14du && i <= 0x14fu);
    const OraclesRom rom = { image, size };
    OraclesTextFont font;
    CHECK(oracles_text_font(&rom, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
    language_wrap *wrap = language_wrap_new(&font, layout.width, ORACLES_LANGUAGE_ENGLISH);
    const layout_count n = count_layout(image, size, first, &fan_rom, &source, groups, NULL, ORACLES_LANGUAGE_ENGLISH, wrap);
    printf("%s en laid out for the compact text: %u texts laid out again (%u in the index), %u kept by the table or a call, %u that did not hold; "
           "%u boxes for %u, %.1f %%; the texts of the box without a choice, a call, a secret or a jump: %u boxes for %u, %.1f %%; %u hyphens joined; "
           "%u bytes changed outside its language's banks and the header's sums\n",
           e->name, report.laid_out, n.flagged, report.layout_kept, report.layout_failed, report.boxes_laid_out, report.boxes,
           100.0 * ((double)report.boxes_laid_out - report.boxes) / (report.boxes ? report.boxes : 1u), n.boxes_laid_out, n.boxes,
           100.0 * ((double)n.boxes_laid_out - n.boxes) / (n.boxes ? n.boxes : 1u), n.joined, changed);
    CHECK(size == fan->size && changed == 0 && wrap && n.flagged == report.laid_out && n.unheld == 0 && report.missing == 0);
    CHECK(report.laid_out == e->layout.laid_out && report.layout_kept == e->layout.kept && report.layout_failed == e->layout.failed);
    CHECK(report.boxes == e->layout.boxes && report.boxes_laid_out == e->layout.boxes_laid_out && n.joined == e->layout.joined);
    for (unsigned h = 0; h < sizeof fan_hyphens_expected / sizeof fan_hyphens_expected[0]; h++) {
        if (fan_hyphens_expected[h].revision != e->revision) continue;
        size_t laid_length = 0;
        const uint8_t *laid = laid_text(image, size, first, fan_hyphens_expected[h].id, &laid_length);
        if (!laid || !holds_words(laid, laid_length, fan_hyphens_expected[h].holds))
            fprintf(stderr, "%s en: TX_%04x laid out does not hold \"%s\"\n", e->name, fan_hyphens_expected[h].id, fan_hyphens_expected[h].holds);
        CHECK(laid && holds_words(laid, laid_length, fan_hyphens_expected[h].holds));
    }
    language_wrap_free(wrap);
    free(image);
    image = NULL;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, fan->data, fan->size, &layout, &image, &size, &report, error, sizeof error) == -1);
    free(image);
}

/* Each fan game made on `us`: the profile's text symbols, its pack derived from the game's, the counts per language,
 * every text translated holding the fan game's text's commands, each language composed into its image in the banks
 * its profile leaves to it, the rest of the image unchanged but the font's glyphs and the header's sums; and its pack's
 * file read back. */
static void check_fans(const char *dir, const char *work, const OraclesLanguagePack *base, const rom_file *us)
{
    for (size_t f = 0; f < sizeof fans / sizeof fans[0]; f++) {
        const expected_fan *e = &fans[f];
        if (e->game != base->game) continue;
        rom_file fan = find_fan(dir, us, e->revision);
        if (!fan.data) { printf("%s: no patch of it in %s: not checked\n", e->name, dir); continue; }
        char error[256];
        OraclesRomInfo info;
        CHECK(oracles_rom_identify(fan.data, fan.size, &info, error, sizeof error) == 0);
        const OraclesCompatProfile *profile = oracles_compat_find(&info);
        const OraclesGuestTables *t = oracles_compat_guest_tables(profile);
        check_hook(e->name, fan.data, t);
        check_compact_points(e->name, fan.data, fan.size, us->data, t, base->game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons);
        OraclesLanguagePack *pack = oracles_language_pack_derive(base, us->data, us->size, fan.data, fan.size, error, sizeof error);
        CHECK(pack != NULL);
        if (!pack) { fprintf(stderr, "%s: %s\n", e->name, error); free(fan.data); continue; }
        CHECK(!strcmp(oracles_language_pack_image(pack), info.sha1) && !strcmp(oracles_language_pack_source(pack), oracles_language_pack_source(base)));
        const OraclesRom fan_rom = { fan.data, fan.size }, us_rom = { us->data, us->size };
        OraclesTextSource fan_source, us_source;
        CHECK(language_source_tables(&fan_rom, t, &fan_source) == 0 && language_source_us(&us_rom, base->game, &us_source) == 0);
        /* The fan game's texts against the US ones, whatever the language. */
        unsigned texts = 0, same_id = 0, elsewhere = 0, own = 0;
        const unsigned groups = oracles_text_groups(&fan_rom, &fan_source), us_groups = oracles_text_groups(&us_rom, &us_source);
        uint8_t text[ORACLES_TEXT_MAX], original[ORACLES_TEXT_MAX];
        for (unsigned g = ORACLES_TEXT_DICTIONARY_GROUPS; g < groups; g++) {
            const int count = oracles_text_count(&fan_rom, &fan_source, g);
            for (int i = 0; i < count; i++) {
                size_t length = 0, us_length = 0;
                texts++;
                CHECK(oracles_text_decode(&fan_rom, &fan_source, g, (unsigned)i, text, sizeof text, &length) == ORACLES_OK);
                const int same = g < us_groups && i < oracles_text_count(&us_rom, &us_source, g)
                                 && oracles_text_decode(&us_rom, &us_source, g, (unsigned)i, original, sizeof original, &us_length) == ORACLES_OK
                                 && us_length == length && !memcmp(text, original, length);
                const OraclesLanguageSource source = oracles_language_pack_source_of(pack, ORACLES_LANGUAGE_FRENCH, (g - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)i);
                if (same) same_id++;
                else if (source == ORACLES_LANGUAGE_SOURCE_FAN) own++;
                else elsewhere++;
                /* Translated, the fan game's text's commands, as the US text's are its own. */
                for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
                    size_t translated_length = 0;
                    const unsigned id = (g - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)i;
                    const uint8_t *translated = oracles_language_pack_text(pack, language, id, &translated_length);
                    const OraclesLanguageSource from = oracles_language_pack_source_of(pack, language, id);
                    if (translated && from != ORACLES_LANGUAGE_SOURCE_ADAPTED) CHECK(oracles_text_same_commands(text, length, translated, translated_length));
                    if (translated) CHECK(oracles_text_longest_line(translated, translated_length) <= LINE_CHARACTERS);
                }
            }
        }
        const uint8_t *characters, *bitmaps;
        const unsigned glyphs = oracles_language_pack_glyphs(pack, &characters, &bitmaps);
        printf("%s: %u texts, %u the US text of their identifier, %u of another, %u its own; %u European glyphs\n", e->name, texts, same_id, elsewhere, own, glyphs);
        CHECK(texts == e->texts && same_id == e->same_id && elsewhere == e->elsewhere && own == e->own && glyphs == e->glyphs);
        /* The texts it shares with the US ROM are those the pack translates, in every language. */
        unsigned shared = 0;
        for (unsigned n = 0; n < oracles_language_pack_count(pack, ORACLES_LANGUAGE_FRENCH); n++) {
            unsigned id = 0, every = 1;
            oracles_language_pack_nth(pack, ORACLES_LANGUAGE_FRENCH, n, &id, NULL, NULL);
            for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++)
                every &= oracles_language_pack_text(pack, language, id, NULL) != NULL;
            shared += every;
        }
        CHECK(oracles_language_pack_count(pack, ORACLES_LANGUAGE_FRENCH) == texts && shared == same_id + elsewhere);
        const size_t first = (size_t)oracles_compat_language_bank(profile) * 0x4000u, font = oracles_rom_offset(t->font_start.bank, t->font_start.addr);
        for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
            unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT];
            oracles_language_pack_tally(pack, language, counts);
            const unsigned translated = counts[ORACLES_LANGUAGE_SOURCE_SAME] + counts[ORACLES_LANGUAGE_SOURCE_MOVED] + counts[ORACLES_LANGUAGE_SOURCE_ADAPTED]
                                        + counts[ORACLES_LANGUAGE_SOURCE_OWN];
            const unsigned english = counts[ORACLES_LANGUAGE_SOURCE_FAN] + counts[ORACLES_LANGUAGE_SOURCE_KEPT];
            uint8_t *image = NULL;
            size_t size = 0;
            OraclesLanguageReport report;
            CHECK(oracles_language_compose(pack, language, fan.data, fan.size, NULL, &image, &size, &report, error, sizeof error) == 0);
            unsigned changed = 0, banks = 0;
            if (image) {
                for (size_t i = 0; i < first; i++)
                    if (image[i] != fan.data[i] && !(i >= 0x14d && i <= 0x14f) && !(i >= font && i < font + 0x100u * GLYPH_BYTES)) changed++;
                for (size_t i = (LANGUAGE_LAST_BANK + 1u) * 0x4000u; i < size; i++) changed += image[i] != fan.data[i];
                CHECK(!memcmp(image + first, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES));
                for (size_t b = first / 0x4000u; b <= LANGUAGE_LAST_BANK; b++) {
                    int used = 0;
                    for (size_t i = 0; i < 0x4000u && !used; i++) used = image[b * 0x4000u + i] != 0;
                    banks += used;
                }
            }
            printf("%s %s: %u translated (%u same, %u moved, %u adapted, %u own), %u in its English; composed: %u served, %u in English, "
                   "%u bytes in %u banks from $%02x\n", e->name, oracles_language_code(language), translated, counts[ORACLES_LANGUAGE_SOURCE_SAME],
                   counts[ORACLES_LANGUAGE_SOURCE_MOVED], counts[ORACLES_LANGUAGE_SOURCE_ADAPTED], counts[ORACLES_LANGUAGE_SOURCE_OWN], english,
                   report.served, report.english, report.bytes, banks, (unsigned)(first / 0x4000u));
            CHECK(size == fan.size && changed == 0 && report.graphics == 0);
            CHECK(translated == e->translated[language] && english == e->english[language]);
            CHECK(report.served == translated && report.english == english && report.missing == 0);
            free(image);
        }
        check_fan_layout(e, &fan, &info, profile, pack);
        /* The fan game's pack plays on its image only. */
        uint8_t *image = NULL;
        size_t size = 0;
        OraclesLanguageReport report;
        CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, us->data, us->size, NULL, &image, &size, &report, error, sizeof error) == -1);
        CHECK(oracles_language_compose(base, ORACLES_LANGUAGE_FRENCH, fan.data, fan.size, NULL, &image, &size, &report, error, sizeof error) == -1);
        CHECK(oracles_language_pack_derive(base, fan.data, fan.size, fan.data, fan.size, error, sizeof error) == NULL);
        /* Its file, read back. */
        char path[1024];
        snprintf(path, sizeof path, "%s/%s.pack", work, e->name);
        CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
        OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
        CHECK(back != NULL);
        if (back) {
            uint8_t *a = NULL, *b = NULL;
            size_t a_size = 0, b_size = 0;
            CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_GERMAN, fan.data, fan.size, NULL, &a, &a_size, &report, error, sizeof error) == 0);
            CHECK(oracles_language_compose(back, ORACLES_LANGUAGE_GERMAN, fan.data, fan.size, NULL, &b, &b_size, &report, error, sizeof error) == 0);
            CHECK(a && b && a_size == b_size && !memcmp(a, b, a_size));
            free(a);
            free(b);
            oracles_language_pack_free(back);
        }
        remove(path);
        oracles_language_pack_free(pack);
        free(fan.data);
    }
}

/* ---- Oracles Combined ------------------------------------------------------------------------------------------ */

/* An image of Oracles Combined, as measured: per half (Ages', Seasons'), its texts and those the European ROM of its game
 * holds alike in its language; and laid out for the compact text (128 pixels), the texts laid out again, those kept by
 * the half's game's table or a call within a line, the boxes of all its texts before and after. */
typedef struct expected_combined {
    OraclesRomRevision revision;
    OraclesLanguage language;
    unsigned texts[2], same[2];
    expected_layout layout[2];
} expected_combined;

static const expected_combined combined_expected[] = {
    { ORACLES_ROM_REVISION_COMBINED_1_5_EN, ORACLES_LANGUAGE_ENGLISH, { 2851, 1919 }, { 2822, 1885 }, { { 1649, 0, 0, 6474, 4971, 20 }, { 951, 0, 0, 3815, 3069, 3 } } },
    { ORACLES_ROM_REVISION_COMBINED_1_5_FR, ORACLES_LANGUAGE_FRENCH, { 2851, 1919 }, { 2813, 1881 }, { { 1767, 0, 0, 6977, 5027, 10 }, { 1041, 0, 0, 4278, 3153, 6 } } },
    { ORACLES_ROM_REVISION_COMBINED_1_5_DE, ORACLES_LANGUAGE_GERMAN, { 2851, 1919 }, { 2808, 1883 }, { { 1771, 0, 0, 7307, 5751, 1130 }, { 1055, 0, 0, 4334, 3433, 706 } } },
    { ORACLES_ROM_REVISION_COMBINED_1_5_ES, ORACLES_LANGUAGE_SPANISH, { 2851, 1919 }, { 2818, 1882 }, { { 1738, 0, 0, 7087, 5319, 0 }, { 1018, 0, 0, 4144, 3224, 0 } } },
    { ORACLES_ROM_REVISION_COMBINED_1_5_IT, ORACLES_LANGUAGE_ITALIAN, { 2851, 1919 }, { 2819, 1883 }, { { 1664, 0, 0, 6404, 4797, 131 }, { 980, 0, 0, 3901, 3003, 165 } } },
};

/* One half of a Combined image laid out by `layout` (its own language, its own font, its index at bank $70 of the half):
 * every text laid out again fits the width and keeps its text's commands and boxes; the index names the language. */
static void check_combined_half(const char *name, const expected_combined *e, unsigned h, const OraclesCompatProfile *half, const uint8_t *image,
                                const uint8_t *composed, size_t size, const OraclesLanguageLayout *layout, int pinned)
{
    size_t half_size = 0, composed_size = 0;
    const uint8_t *before = oracles_rom_half(image, size, oracles_compat_rom_bank_base(half), &half_size);
    const uint8_t *after = oracles_rom_half(composed, size, oracles_compat_rom_bank_base(half), &composed_size);
    const OraclesRom rom = { before, half_size }, laid = { after, composed_size };
    OraclesTextSource source;
    CHECK(language_source_profile(&rom, half, &source) == 0);
    const unsigned groups = oracles_text_groups(&rom, &source);
    const OraclesGuestTables *t = oracles_compat_guest_tables(half);
    OraclesTextFont font;
    CHECK(oracles_text_font(&laid, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
    if (layout->fixed) oracles_text_font_fixed(&font, &font);
    language_wrap *wrap = language_wrap_new(&font, layout->width, e->language);
    const size_t first = (size_t)oracles_compat_language_bank(half) * 0x4000u;
    CHECK(first == (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u && !memcmp(after + first, ORACLES_LANGUAGE_INDEX_MAGIC, ORACLES_LANGUAGE_INDEX_MAGIC_BYTES));
    CHECK(!strcmp((const char *)after + first + ORACLES_LANGUAGE_INDEX_CODE, oracles_language_code(e->language)));
    const layout_count n = count_layout(after, composed_size, first, &rom, &source, groups, NULL, ORACLES_LANGUAGE_ENGLISH, wrap);
    unsigned changed = 0;
    const size_t end = (LANGUAGE_LAST_BANK + 1u) * 0x4000u;
    for (size_t i = 0; i < half_size; i++) changed += after[i] != before[i] && (i < first || i >= end) && !(i >= 0x14du && i <= 0x14fu && !h);
    printf("%s half %u laid out on %u pixels%s: %u texts laid out again, %u that did not hold; the texts of the box without a choice, a call, a "
           "secret or a jump: %u boxes for %u; %u hyphens joined; %u bytes changed outside its language's banks\n",
           name, h, layout->width, layout->fixed ? " (classic)" : "", n.flagged, n.unheld, n.boxes_laid_out, n.boxes, n.joined, changed);
    CHECK(wrap && n.flagged > 0 && n.unheld == 0 && changed == 0);
    if (pinned) {
        const expected_layout *x = &e->layout[h];
        CHECK(n.flagged == x->laid_out && n.boxes == x->boxes && n.boxes_laid_out == x->boxes_laid_out && n.joined == x->joined);
    }
    language_wrap_free(wrap);
}

/* Each image of Oracles Combined made by a BPS patch of `dir` on the European ROMs of Ages and Seasons joined: each half's
 * texts read from its three bases, its font the European ROM of its game's in every printed character, its texts those
 * of that ROM in its language but a few, under their identifiers; laid out in its own language (no pack: Combined holds
 * one) at the game's 128 pixels and the wide box's 192, classic and compact, both halves composed, every text laid out
 * again held, none failed, no page lost; a pack's language refused. */
static unsigned check_combined(const char *dir)
{
    rom_file eu[2] = { find(dir, ORACLES_ROM_REVISION_AGES_EU), find(dir, ORACLES_ROM_REVISION_SEASONS_EU) }, us = find(dir, ORACLES_ROM_REVISION_AGES_US);
    unsigned checked = 0;
    char error[256];
    size_t joined_size = 0;
    uint8_t *joined = eu[0].data && eu[1].data ? oracles_bps_join(eu[0].data, eu[0].size, eu[1].data, eu[1].size, &joined_size, error, sizeof error) : NULL;
    DIR *listing = joined ? opendir(dir) : NULL;
    struct dirent *entry;
    static uint32_t ranges[2u * 16384u];
    while (listing && (entry = readdir(listing))) {
        const size_t length = strlen(entry->d_name);
        if (length < 4 || strcmp(entry->d_name + length - 4, ".bps")) continue;
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t patch_size = 0, size = 0;
        uint8_t *patch = oracles_rom_read_file(path, &patch_size, error, sizeof error);
        uint8_t *image = patch ? oracles_bps_apply(joined, joined_size, patch, patch_size, &size, error, sizeof error) : NULL;
        free(patch);
        OraclesRomInfo info;
        const expected_combined *e = NULL;
        if (image && oracles_rom_identify(image, size, &info, error, sizeof error) == 0)
            for (size_t i = 0; i < sizeof combined_expected / sizeof combined_expected[0]; i++)
                if (info.known && combined_expected[i].revision == info.revision) e = &combined_expected[i];
        if (!e) { free(image); continue; }
        checked++;
        char name[32];
        snprintf(name, sizeof name, "combined %s", oracles_language_code(e->language));
        OraclesLanguage own = ORACLES_LANGUAGE_COUNT;
        CHECK(oracles_language_image_own(&info, &own) == 1 && own == e->language);
        const OraclesCompatProfile *halves[2] = { oracles_compat_find(&info), NULL };
        halves[1] = oracles_compat_half(halves[0], 1);
        CHECK(halves[0] && halves[1] && oracles_compat_half_game(halves[0]) == 0);
        if (!halves[0] || !halves[1]) { free(image); continue; }
        /* Each half: its texts, its font. */
        for (unsigned h = 0; h < 2u; h++) {
            size_t half_size = 0;
            const uint8_t *half = oracles_rom_half(image, size, oracles_compat_rom_bank_base(halves[h]), &half_size);
            const OraclesRom rom = { half, half_size }, original = { eu[h].data, eu[h].size };
            OraclesTextSource source, european;
            CHECK(language_source_profile(&rom, halves[h], &source) == 0);
            const OraclesGame game = h ? ORACLES_GAME_SEASONS : ORACLES_GAME_AGES;
            CHECK(language_source_european(&original, game, e->language == ORACLES_LANGUAGE_ENGLISH ? ORACLES_LANGUAGE_ENGLISH_EU : e->language, &european) == 0);
            const unsigned groups = oracles_text_groups(&rom, &source), european_groups = oracles_text_groups(&original, &european);
            unsigned texts = 0, same = 0, unread = 0;
            for (unsigned g = ORACLES_TEXT_DICTIONARY_GROUPS; g < groups; g++) {
                const int count = oracles_text_count(&rom, &source, g), european_count = g < european_groups ? oracles_text_count(&original, &european, g) : 0;
                for (int i = 0; i < count; i++) {
                    uint8_t text[ORACLES_TEXT_MAX], other[ORACLES_TEXT_MAX];
                    size_t text_length = 0, other_length = 0;
                    texts++;
                    if (oracles_text_decode(&rom, &source, g, (unsigned)i, text, sizeof text, &text_length) != ORACLES_OK) { unread++; continue; }
                    same += i < european_count && oracles_text_decode(&original, &european, g, (unsigned)i, other, sizeof other, &other_length) == ORACLES_OK
                            && other_length == text_length && !memcmp(text, other, text_length);
                }
            }
            const OraclesGuestTables *t = oracles_compat_guest_tables(halves[h]);
            const size_t font = oracles_rom_offset(t->font_start.bank, t->font_start.addr), european_font = language_font_european(game);
            const int glyphs = !memcmp(half + font + FONT_FIRST * GLYPH_BYTES, eu[h].data + european_font + FONT_FIRST * GLYPH_BYTES, FONT_GLYPHS * GLYPH_BYTES);
            printf("%s half %u: %u groups, %u texts, %u not read, %u those of the European ROM in its language; its font's printed glyphs the "
                   "European ROM's: %s\n", name, h, groups, texts, unread, same, glyphs ? "yes" : "no");
            CHECK(texts == e->texts[h] && unread == 0 && same == e->same[h] && glyphs);
        }
        /* Laid out: the game's line, then the wide box's, compact and classic. */
        const struct { unsigned width; int fixed; } layouts[] = { { 128u, 0 }, { 192u, 0 }, { 192u, 1 } };
        for (size_t l = 0; l < sizeof layouts / sizeof layouts[0]; l++) {
            OraclesLanguagePages pages = { ranges, 16384u, 0, 0 };
            const OraclesLanguageLayout layout = { layouts[l].width, layouts[l].fixed, 0u, &pages };
            uint8_t *composed = NULL;
            size_t composed_size = 0;
            OraclesLanguageReport report;
            CHECK(oracles_language_compose_english(info.sha1, image, size, &layout, &composed, &composed_size, &report, error, sizeof error) == 0);
            if (!composed) { fprintf(stderr, "%s: %s\n", name, error); continue; }
            unsigned second = 0, sorted = 1;
            for (unsigned p = 0; p < pages.count; p++) {
                second += ranges[2u * p] >= ORACLES_ROM_HALF_SIZE;
                if (p) sorted &= ranges[2u * p] >= ranges[2u * p - 1u];
            }
            printf("%s laid out on %u pixels%s: %u texts laid out again, %u kept, %u that did not hold, %u missing; %u boxes for %u; %u pages, %u "
                   "of them in the second half, %u lost\n", name, layouts[l].width, layouts[l].fixed ? " (classic)" : "", report.laid_out, report.layout_kept,
                   report.layout_failed, report.missing, report.boxes_laid_out, report.boxes, pages.count, second, pages.overflowed);
            CHECK(composed_size == size && report.layout_failed == 0 && report.missing == 0 && pages.overflowed == 0 && second > 0 && second < pages.count && sorted);
            CHECK(report.laid_out + report.english == e->texts[0] + e->texts[1] && report.served == report.laid_out);
            for (unsigned h = 0; h < 2u; h++) check_combined_half(name, e, h, halves[h], image, composed, size, &layout, l == 0);
            free(composed);
        }
        /* Its language is its own: no pack composes into it, no fan game's pack derives for it. */
        language_target target;
        CHECK(language_target_of(info.sha1, &target) == 0 && target.halves == 2u && target.own == e->language && !target.original);
        {
            static const uint8_t text[] = { 'A', 0 };
            OraclesLanguagePack *pack = language_pack_new(ORACLES_GAME_AGES, info.sha1, info.sha1);
            CHECK(pack && language_pack_add(pack, ORACLES_LANGUAGE_GERMAN, 0x0000, ORACLES_LANGUAGE_SOURCE_SAME, text, sizeof text) == 0);
            uint8_t *composed = NULL;
            size_t composed_size = 0;
            OraclesLanguageReport report;
            CHECK(pack && oracles_language_compose(pack, ORACLES_LANGUAGE_GERMAN, image, size, NULL, &composed, &composed_size, &report, error, sizeof error) == -1
                  && !composed);
            free(composed);
            oracles_language_pack_free(pack);
            OraclesRomInfo us_info;
            CHECK(us.data != NULL);
            if (us.data && oracles_rom_identify(us.data, us.size, &us_info, error, sizeof error) == 0) {
                OraclesLanguagePack *base = language_pack_new(ORACLES_GAME_AGES, us_info.sha1, us_info.sha1);
                OraclesLanguagePack *derived = base ? oracles_language_pack_derive(base, us.data, us.size, image, size, error, sizeof error) : NULL;
                CHECK(base && !derived);
                oracles_language_pack_free(derived);
                oracles_language_pack_free(base);
            }
        }
        free(image);
    }
    if (listing) closedir(listing);
    free(joined);
    free(eu[0].data);
    free(eu[1].data);
    free(us.data);
    return checked;
}

static int check_game(const char *dir, const char *work, const expected *e)
{
    const OraclesGame game = e->game;
    const OraclesRomRevision us_revision = e->us, eu_revision = e->eu;
    rom_file us = find(dir, us_revision), eu = find(dir, eu_revision);
    const char *name = game == ORACLES_GAME_AGES ? "ages" : "seasons";
    if (!us.data || !eu.data) { printf("%s: no US and European ROMs in %s: not checked\n", name, dir); free(us.data); free(eu.data); return 0; }
    char error[256];
    language_corrections *used = language_corrections_load(game, error, sizeof error);
    CHECK(used != NULL);
    OraclesLanguagePack *pack = used ? language_pack_extract_with(eu.data, eu.size, us.data, us.size, used, error, sizeof error) : NULL;
    CHECK(pack != NULL);
    if (!pack) { fprintf(stderr, "%s: %s\n", name, error); language_corrections_free(used); free(us.data); free(eu.data); return 1; }
    check_corrections_used(name, used);
    language_corrections_free(used);

    /* The official translations' mistakes corrected (config/language_corrections.tsv) or rewritten (the port's own
     * texts): each text served with the right words, without the wrong ones. */
    for (size_t c = 0; c < sizeof corrections / sizeof corrections[0]; c++) {
        const correction *k = &corrections[c];
        if (k->game != game) continue;
        uint8_t present[128], absent[128];
        size_t present_length = 0, absent_length = 0;
        CHECK(language_text_encode(k->present, strlen(k->present), present, sizeof present, &present_length) == 0);
        CHECK(language_text_encode(k->absent, strlen(k->absent), absent, sizeof absent, &absent_length) == 0);
        for (unsigned id = k->first; id <= k->last; id++) {
            size_t length = 0;
            const uint8_t *text = oracles_language_pack_text(pack, k->language, id, &length);
            const int served = text && holds(text, length, present, present_length - 1u) && !holds(text, length, absent, absent_length - 1u);
            if (!served) fprintf(stderr, "%s %s TX_%04x: not corrected to %s\n", name, oracles_language_code(k->language), id, k->present);
            CHECK(served);
        }
    }

    /* The glyphs: 22 in slots the US font fills with a repeated filler, 13 redrawn. */
    const uint8_t *characters, *bitmaps;
    const unsigned glyphs = oracles_language_pack_glyphs(pack, &characters, &bitmaps);
    const uint8_t *font = us.data + language_font_us(game);
    unsigned fresh = 0;
    for (unsigned g = 0; g < glyphs; g++) {
        unsigned same = 0;
        for (unsigned c = FONT_FIRST; c < 0x100u; c++) same += !memcmp(font + c * GLYPH_BYTES, font + characters[g] * GLYPH_BYTES, GLYPH_BYTES);
        fresh += same > 1u;
    }
    printf("%s: %u glyphs, %u in filler slots\n", name, glyphs, fresh);
    CHECK(glyphs == 35 && fresh == 22);

    static char list[16384];
    const unsigned differences = english_differences(&us, &eu, game, list, sizeof list);
    printf("%s: European English differs from the US on %u texts: %s\n", name, differences, list);
    CHECK(!strcmp(list, e->english_differs));

    const OraclesRom us_rom = { us.data, us.size };
    OraclesTextSource source;
    CHECK(language_source_us(&us_rom, game, &source) == 0);
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) {
        const unsigned count = oracles_language_pack_count(pack, language);
        unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT], long_lines = 0, unheld = 0;
        oracles_language_pack_tally(pack, language, counts);
        list[0] = 0;
        for (unsigned n = 0; n < count; n++) {
            unsigned id = 0;
            size_t length = 0;
            OraclesLanguageSource from;
            const uint8_t *text = oracles_language_pack_nth(pack, language, n, &id, &from, &length);
            if (from != ORACLES_LANGUAGE_SOURCE_SAME) {
                const size_t used = strlen(list);
                snprintf(list + used, sizeof list - used, "%s%04x:%s", used ? " " : "", id, oracles_language_source_name(from));
            }
            if (!text) continue;
            if (oracles_text_longest_line(text, length) > 16u) long_lines++;
            /* A European text or the port's own as it is: the US text's commands, exactly. */
            uint8_t original[ORACLES_TEXT_MAX];
            size_t original_length = 0;
            const unsigned group = (id >> 8) + ORACLES_TEXT_DICTIONARY_GROUPS;
            CHECK(oracles_text_decode(&us_rom, &source, group, id & 0xffu, original, sizeof original, &original_length) == ORACLES_OK);
            if (from != ORACLES_LANGUAGE_SOURCE_ADAPTED && !oracles_text_same_commands(original, original_length, text, length)) unheld++;
        }
        const unsigned served = counts[ORACLES_LANGUAGE_SOURCE_SAME] + counts[ORACLES_LANGUAGE_SOURCE_MOVED] + counts[ORACLES_LANGUAGE_SOURCE_ADAPTED]
                                + counts[ORACLES_LANGUAGE_SOURCE_OWN];
        uint8_t *image = NULL;
        size_t size = 0;
        OraclesLanguageReport report;
        CHECK(oracles_language_compose(pack, language, us.data, us.size, NULL, &image, &size, &report, error, sizeof error) == 0);
        printf("%s %s: %u texts: %u same, %u moved, %u adapted, %u own, %u kept, %u refused, %u missing: %.2f %% translated, %.2f %% official; "
               "%u lines over 16; composed: %u served, %u in English, %u missing, %u bytes\n  not the same: %s\n",
               name, oracles_language_code(language), count, counts[ORACLES_LANGUAGE_SOURCE_SAME], counts[ORACLES_LANGUAGE_SOURCE_MOVED],
               counts[ORACLES_LANGUAGE_SOURCE_ADAPTED], counts[ORACLES_LANGUAGE_SOURCE_OWN], counts[ORACLES_LANGUAGE_SOURCE_KEPT],
               counts[ORACLES_LANGUAGE_SOURCE_REFUSED], counts[ORACLES_LANGUAGE_SOURCE_MISSING], 100.0 * served / (count ? count : 1u),
               100.0 * (served - counts[ORACLES_LANGUAGE_SOURCE_OWN]) / (count ? count : 1u), long_lines, report.served, report.english,
               report.missing, report.bytes, list);
        /* Every US text translated: none refused, none missing, none kept, the European texts as measured. */
        CHECK(count == e->texts && long_lines == 0 && unheld == 0);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_REFUSED] == 0 && counts[ORACLES_LANGUAGE_SOURCE_MISSING] == 0 && counts[ORACLES_LANGUAGE_SOURCE_KEPT] == 0);
        CHECK(counts[ORACLES_LANGUAGE_SOURCE_SAME] == e->same[language]);
        CHECK(report.served == served && report.english == 0 && report.missing == 0);
        free(image);
    }
    check_graphics(name, game, pack, &us, &eu, &graphics_expected[e - games]);
    check_composed(name, game, pack, &us);
    check_layout(name, game, pack, &us);
    check_hook(name, us.data, game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons);
    check_fans(dir, work, pack, &us);
    /* The pack's file: read back, it composes the same image. */
    char path[1024];
    snprintf(path, sizeof path, "%s/%s.pack", work, name);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
    CHECK(back != NULL);
    if (back) {
        uint8_t *a = NULL, *b = NULL;
        size_t a_size = 0, b_size = 0;
        OraclesLanguageReport report;
        CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_FRENCH, us.data, us.size, NULL, &a, &a_size, &report, error, sizeof error) == 0);
        CHECK(oracles_language_compose(back, ORACLES_LANGUAGE_FRENCH, us.data, us.size, NULL, &b, &b_size, &report, error, sizeof error) == 0);
        CHECK(a && b && a_size == b_size && !memcmp(a, b, a_size));
        free(a);
        free(b);
        oracles_language_pack_free(back);
    }
    remove(path);
    oracles_language_pack_free(pack);
    free(us.data);
    free(eu.data);
    return 1;
}

/* Japanese from the Japanese ROM of a game: every US text served (the Japanese text of its representative, a moved
 * one, an adapted one or the port's own), each holding the US text's commands as the Japanese comparison has them and
 * its lines (Seasons TX_1f04 as the Japanese ROM has it, its number in one digit); the texts' structure; the whole
 * Japanese font in the pack and in the composed image, byte for byte the Japanese ROM's; the graphics of the screens
 * the Japanese ROM draws in Japanese, its title's colours and sprites, and nothing else of the US ROM changed but the
 * header's sums (check_japanese_graphics); the wide box keeping the game's lines, and the layout itself keeping every
 * character; the pack's file read back.  The coverage printed. */
typedef struct expected_japanese {
    OraclesGame game;
    OraclesRomRevision us, jp;
    unsigned texts, same, moved, adapted, own;
    const char *structure;   /* japanese_structure's */
    const char *graphics;    /* the US graphics replaced, the title's colours and its sprites' table among them */
    unsigned graphics_left;  /* the bytes of the screens drawn in Japanese the Japanese ROM loads otherwise from the US graphics kept */
    unsigned colours_bank, sprites_bank, sprites_address, sprites_pointer;   /* PALH_03's colours, the title's sprites' new place, the operand naming them */
    const char *grids;       /* the US grid made for Japanese: the secret entry's map and attributes */
    unsigned secret_names;   /* secretTextTable, in bank 2 */
    unsigned kana_entry, kana_tail, kana_called;   /* the US name entry's code (runTextInput), the free tail of bank 2, copyTextCharactersFromHlUntilNull */
    unsigned objects, object_headers, symbols;   /* objectGfxHeaderTable (bank $3f), its headers and the trees' after them, the inventory symbols' */
} expected_japanese;
/* Ages: TX_032c, a placeholder of the Japanese ROM where the US one has a map's name; TX_1208, whose Japanese says the
 * same of TX_1209 (the water going out and coming in). */
static const expected_japanese japanese_expected[] = {
    { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_AGES_US, ORACLES_ROM_REVISION_AGES_JP, 2824, 2786, 8, 6, 24, "TX_032c unserved, TX_1208 twice",
      "00:00a1 02:46d6 02:4e2e 02:7e95 03:4ada 17:4018 29:56ac 29:5a51 29:5b5f 29:5ca9 29:5de7 29:5f33 29:6071 29:61ac 29:62ff 29:644d 29:6589 29:66cc 29:67dd "
      "29:74ff 29:7a39 29:7bc9 29:7c4c 2a:41ed 2a:4352 2a:48a4 30:763a 30:7885 30:7b6a 30:7c7a 30:7de1 31:7163 31:7524 31:7a0a 31:7adf 32:4c88 37:7cba "
      "37:7e5d 37:7edf 37:7f62 38:4470 38:456a 38:470d 38:48b5 38:4930 38:4e0a 38:5274 38:5426 38:55bb 38:5641 38:57cd 38:5982 38:5b12 38:5b90 "
      "38:5d07 38:61ee 38:6260 38:627d 38:636d 38:63cf 38:648d 38:64b8 38:64c9 38:6589 38:664f 3f:7d0a", 0, 0x17, 0x3f, 0x7d0a, 0x4d92, "38:627d 38:636d", 0x4e2e,
      0x46d6, 0x7e95, 0x410e, 0x5a8a, 0xeb, 0x70 },
    { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_SEASONS_US, ORACLES_ROM_REVISION_SEASONS_JP, 1885, 1869, 11, 5, 0, "",
      "00:00a1 02:4696 02:4dee 02:75bb 03:4ada 03:7dd7 16:4018 28:5199 28:5542 28:5646 28:57d2 28:593e 28:5a9b 28:5c23 28:5d56 28:5e99 28:5feb 28:6184 28:628f "
      "29:42b0 30:7e3e 31:4089 31:43a2 31:44b2 31:4619 31:6801 31:6bc2 31:70a8 31:717d 32:4323 37:7bbc 37:7d5f 37:7de1 37:7e64 38:4372 38:446c 38:460f "
      "38:47b7 38:4832 38:4d0c 38:5176 38:5328 38:54bd 38:5543 38:56d4 38:5889 38:5a19 38:5a97 38:5c0e 38:60f5 38:6167 38:6184 38:6274 38:62d6 "
      "38:6394 38:63bf 38:63d0 38:6490 38:6556", 0, 0x16, 0x03, 0x7dd7, 0x4d92, "38:6184 38:6274", 0x4dee, 0x4696, 0x75bb, 0x40ce, 0x5afb, 0xd2, 0x55 },
};

/* Whether the Japanese ROM draws the screen of header h in Japanese: the title and the intro's end, the secrets'
 * screens, the error, the inventory, Ages' maps, the dungeons' names, the secret for the linked game, the file select
 * and its screens, the save menu. */
static int japanese_screen(OraclesGame game, unsigned h)
{
    return h == 0x02 || h == 0x03 || h == 0x05 || h == 0x06 || h == 0x07 || h == 0x08 || h == 0x97 || h == 0x9f || h == 0xba
        || (h >= 0x10 && h <= (game == ORACLES_GAME_AGES ? 0x1fu : 0x1bu)) || (h >= 0xa0 && h <= 0xaf)
        || (game == ORACLES_GAME_AGES && (h == 0x0d || h == 0x0e)) || (game == ORACLES_GAME_SEASONS && h == 0x84);
}

/* The Japanese secrets and names: the cipher and the 64 symbols the Japanese ROM's (the symbols over the US ones, their
 * terminator kept), the names of the secrets' list its own, its first the second's; the secret entry's grid, as the US
 * code reads it (secretSymbols[tile / 2 & $3f] at the cursor's cell, $a3 + its column + 64 its row of the map), shows
 * the 64 symbols once each, in the order of the font, the kana and then the digits, its one blank between them, each
 * cell's attributes the US ones of the cell that showed the same; the name entry's, the Japanese ROM's, its 60 cells as
 * the Japanese routines read them (below).  The places written marked in `allowed`. */
static void check_japanese_secrets(const char *name, OraclesGame game, const rom_file *us, const rom_file *jp, const uint8_t *image, size_t size,
                                   const expected_japanese *e, uint8_t *allowed)
{
    static gfx_load composed, original;
    const size_t cipher = oracles_rom_offset(0x03, 0x4ada), symbols = 0xa1u, names = oracles_rom_offset(0x02, e->secret_names);
    const size_t jp_cipher = language_secret_cipher_japanese(game), jp_symbols = language_secret_symbols_japanese(game), jp_names = language_secret_names_japanese(game);
    CHECK(jp_cipher && jp_symbols && jp_names && size > names + 0x100u);
    CHECK(!memcmp(image + cipher, jp->data + jp_cipher, 0x30) && memcmp(us->data + cipher, jp->data + jp_cipher, 0x30));
    CHECK(!memcmp(image + symbols, jp->data + jp_symbols, 0x40) && image[symbols + 0x40u] == 0);
    memset(allowed + cipher, 1, 0x30);
    memset(allowed + symbols, 1, 0x40);
    unsigned listed = 0, end = 0;
    for (unsigned i = 0; i < 26u; i++) {
        const unsigned p = image[names + 2u * i] | image[names + 2u * i + 1u] << 8, q = jp->data[jp_names + 2u * i] | jp->data[jp_names + 2u * i + 1u] << 8;
        const char *mine = (const char *)image + oracles_rom_offset(0x02, p), *theirs = (const char *)jp->data + oracles_rom_offset(0x02, q);
        if (i == 0) { CHECK(p == ((unsigned)image[names + 2u] | (unsigned)image[names + 3u] << 8)); continue; }
        CHECK(p >= e->secret_names + 52u && !strcmp(mine, theirs));
        listed += !strcmp(mine, theirs);
        if (p + strlen(mine) + 1u - e->secret_names > end) end = (unsigned)(p + strlen(mine) + 1u - e->secret_names);
    }
    memset(allowed + names, 1, end);
    /* The secret entry's grid ($ab; $ac loads the same). */
    const size_t table = language_gfx_headers_us(game);
    CHECK(load_header(image, size, table, 0xab, -1, &composed) == 0 && load_header(us->data, us->size, table, 0xab, -1, &original) == 0);
    const unsigned map_dest = (unsigned)image[composed.at[2] + 3u] << 8 | image[composed.at[2] + 4u], flg_dest = (unsigned)image[composed.at[3] + 3u] << 8 | image[composed.at[3] + 4u];
    const size_t map = (map_dest & 0xfu) * 0x6000u + (map_dest & 0xfff0u) - 0xa0u - 0x8000u, flg = (flg_dest & 0xfu) * 0x6000u + (flg_dest & 0xfff0u) - 0xa0u - 0x8000u;
    uint16_t us_top[0x41], us_bottom[0x41];
    for (unsigned y = 0; y < 5u; y++)
        for (unsigned x = 0; x < 13u; x++) {
            const size_t at = 0xa3u + (x < 8u ? x : x + 1u) + 64u * y;
            const unsigned tile = original.value[map + at], index = tile >= 0x80u ? (tile - 0x80u) / 2u : 0x40u;
            if (index <= 0x40u) { us_top[index] = original.value[flg + at]; us_bottom[index] = original.value[flg + at + 32u]; }
        }
    uint8_t shown[0x41];
    unsigned cells = 0, blanks = 0, ordered = 1, attributes = 0, seen_digit = 0, blank_at = 0;
    for (unsigned y = 0; y < 5u; y++)
        for (unsigned x = 0; x < 13u; x++) {
            const size_t at = 0xa3u + (x < 8u ? x : x + 1u) + 64u * y;
            const unsigned tile = composed.value[map + at], index = tile >= 0x80u ? (tile - 0x80u) / 2u : 0x40u;
            attributes += index <= 0x40u && composed.value[flg + at] == us_top[index] && composed.value[flg + at + 32u] == us_bottom[index];
            if (index == 0x40u) { blanks++; blank_at = cells++; continue; }
            const uint8_t symbol = image[symbols + index];
            const unsigned rank = (symbol >= '0' && symbol <= '9') << 8 | symbol;
            if (cells && cells - 1u != blank_at) {
                const uint8_t before = shown[cells - 1u];
                if ((unsigned)(((before >= '0' && before <= '9') << 8) | before) >= rank) ordered = 0;
            }
            if (symbol >= '0' && symbol <= '9' && !seen_digit) { seen_digit = 1; if (blank_at + 1u != cells || !blanks) ordered = 0; }
            shown[cells++] = symbol;
        }
    unsigned distinct = 0;
    for (unsigned i = 0; i < cells; i++) {
        if (i == blank_at) continue;
        unsigned twice = 0;
        for (unsigned j = 0; j < i; j++) twice |= j != blank_at && shown[j] == shown[i];
        distinct += !twice;
    }
    /* The name entry's grid ($a5), the Japanese ROM's, as the Japanese routines read it (index tile / 2 & $3f at the
     * cursor's cell, $a3 + its column, the columns in groups of 5, + 64 its row): 60 cells of 59 characters, the 46 kana,
     * the 9 small ones, ー, the space twice, ゛ and ゜. */
    CHECK(load_header(image, size, table, 0xa5, -1, &composed) == 0);
    const unsigned name_dest = (unsigned)image[composed.at[3] + 3u] << 8 | image[composed.at[3] + 4u];
    const size_t name_map = (name_dest & 0xfu) * 0x6000u + (name_dest & 0xfff0u) - 0xa0u - 0x8000u;
    unsigned kana_cells = 0, kana_distinct = 0, kana_last = 0;
    uint8_t kana_seen[0x40] = { 0 };
    for (unsigned y = 0; y < 5u; y++)
        for (unsigned x = 0; x < 12u; x++) {
            const unsigned tile = composed.value[name_map + 0xa3u + x + (x >= 10u ? 2u : x >= 5u ? 1u : 0u) + 64u * y], index = tile / 2u & 0x3fu;
            if (tile < 0x80u || tile & 1u) continue;
            kana_cells++;
            kana_distinct += !kana_seen[index];
            kana_seen[index] = 1;
            if (index > kana_last) kana_last = index;
        }
    printf("%s ja secrets: the Japanese cipher and symbols, %u names of the list; the secret entry's grid: %u cells, %u blank, %u symbols, "
           "%s, %u cells with their attributes; the name entry's grid: %u cells, %u characters, the last %02x\n",
           name, listed, cells, blanks, distinct, ordered ? "in the font's order, the digits last" : "out of order", attributes, kana_cells, kana_distinct, kana_last);
    CHECK(listed == 25u && cells == 65u && blanks == 1u && distinct == 64u && ordered && attributes == 65u && kana_cells == 60u && kana_distinct == 59u
          && kana_last == 0x3au);
}

/* The name entry in kana: the US code of the name entry changed at its nine sites alone, each holding the bytes the
 * routines' build gives; the routines in the free tail of bank 2, the rest of the tail its fill; in them, each of the
 * Japanese ROM's blocks, byte for byte, but the addresses set to the US routines, each of those inside the US code of
 * the name entry or copyTextCharactersFromHlUntilNull, or to the tail; the blocks' relative jumps inside their block or
 * to the byte after it, which jumps on (a `jp`, a `ret`); their other addresses in bank 0, a routine the US code calls
 * too, or in WRAM; the tail's other bytes, the port's, counted.  The places written marked in `allowed`. */
static void check_japanese_kana(const char *name, OraclesGame game, const rom_file *us, const rom_file *jp, const uint8_t *image, const expected_japanese *e,
                                uint8_t *allowed)
{
    static language_kana kana;
    const OraclesRom us_rom = { us->data, us->size }, jp_rom = { jp->data, jp->size };
    CHECK(language_kana_make(game, &jp_rom, &us_rom, e->kana_tail, &kana) == 0);
    const size_t entry = oracles_rom_offset(0x02, e->kana_entry), tail = oracles_rom_offset(0x02, e->kana_tail), end = oracles_rom_offset(0x02, 0x8000);
    const uint8_t fill = game == ORACLES_GAME_SEASONS ? 0x02u : 0x00u;
    unsigned sites = 0, changed = 0, outside = 0;
    for (size_t i = entry; i < entry + 0x374u; i++) {
        if (image[i] == us->data[i]) continue;
        changed++;
        int in = 0;
        for (unsigned k = 0; k < LANGUAGE_KANA_SITES; k++) in |= i >= oracles_rom_offset(0x02, kana.site[k]) && i < oracles_rom_offset(0x02, kana.site[k]) + kana.site_bytes[k];
        outside += !in;
    }
    for (unsigned k = 0; k < LANGUAGE_KANA_SITES; k++) {
        const size_t at = oracles_rom_offset(0x02, kana.site[k]);
        sites += at >= entry && at + kana.site_bytes[k] <= entry + 0x374u && !memcmp(image + at, kana.written[k], kana.site_bytes[k])
                 && memcmp(us->data + at, kana.written[k], kana.site_bytes[k]);
        memset(allowed + at, 1, kana.site_bytes[k]);
    }
    unsigned filled = 1;
    for (size_t i = tail + kana.length; i < end; i++) filled &= image[i] == fill && us->data[i] == fill;
    for (size_t i = tail; i < tail + kana.length; i++) filled &= us->data[i] == fill;
    CHECK(!memcmp(image + tail, kana.tail, kana.length));
    memset(allowed + tail, 1, kana.length);
    /* The blocks. */
    unsigned copied = 0, same = 0, relocated = 0, jumps = 0, landing = 0, others = 0, called = 0;
    for (unsigned b = 0; b < LANGUAGE_KANA_BLOCKS; b++) {
        const size_t from = oracles_rom_offset(0x02, kana.block_jp[b]), at = kana.block_at[b] - e->kana_tail;
        copied += kana.block_bytes[b];
        for (unsigned i = 0; i < kana.block_bytes[b]; i++) {
            int set = 0;
            for (unsigned r = 0; r < kana.relocations; r++) set |= at + i == kana.relocated[r] || at + i == kana.relocated[r] + 1u;
            same += set || kana.tail[at + i] == jp->data[from + i];
        }
        if (b >= 5u) continue;   /* the tables */
        for (unsigned i = 0; i < kana.block_bytes[b];) {
            const uint8_t op = kana.tail[at + i];
            const unsigned n = instruction_bytes(op);
            const int address = n == 3u && op != 0x01u && op != 0x11u && op != 0x31u;   /* a jump, a call, a load from or to memory, ld hl */
            if (op == 0x18 || (op & 0xe7u) == 0x20) {
                const int target = (int)i + 2 + (int8_t)kana.tail[at + i + 1u];
                jumps++;
                if (target < 0 || target > (int)kana.block_bytes[b]) landing = 99;
                else if (target == (int)kana.block_bytes[b]) landing += kana.tail[at + kana.block_bytes[b]] != 0xc3u && kana.tail[at + kana.block_bytes[b]] != 0xc9u;
            }
            if (address) {
                const unsigned operand = kana.tail[at + i + 1u] | kana.tail[at + i + 2u] << 8;
                int set = 0;
                for (unsigned r = 0; r < kana.relocations; r++) set |= at + i + 1u == kana.relocated[r];
                if (set) {
                    relocated++;
                    called += (operand >= e->kana_entry && operand < e->kana_entry + 0x374u) || (operand >= e->kana_tail && operand < e->kana_tail + kana.length);
                } else if (operand < 0x4000u) {
                    int used = 0;
                    for (size_t j = entry; j + 3u <= entry + 0x374u && !used; j++)
                        used = (us->data[j] == 0xcdu || us->data[j] == 0xc3u) && us->data[j + 1u] == (uint8_t)operand && us->data[j + 2u] == operand >> 8;
                    others += !used;
                } else others += operand < 0xc000u;
            }
            i += n;
        }
    }
    printf("%s ja kana: %u of the US code's bytes changed, %u outside its %u sites, %u sites as built; the tail 02:%04x, %zu bytes, the rest %s; "
           "%u bytes of %u blocks of the Japanese ROM, %u as it has them or relocated; %u addresses relocated, %u to the US code or the tail; %u "
           "relative jumps, %u landing elsewhere; %u other addresses neither in WRAM nor a routine of bank 0 the US code calls or jumps to; %zu bytes of the port's\n",
           name, changed, outside, LANGUAGE_KANA_SITES, sites, e->kana_tail, kana.length, filled ? "its fill" : "not its fill", copied, LANGUAGE_KANA_BLOCKS,
           same, relocated, called, jumps, landing, others, kana.length - copied);
    CHECK(outside == 0 && sites == LANGUAGE_KANA_SITES && filled && same == copied && relocated == kana.relocations && called == relocated && jumps > 0
          && landing == 0 && others == 0 && copied == 172u && kana.length == 223u);
    /* The values the routines' build must give, pinned: each site's new bytes (the port's, and the tail's addresses), the
     * relocated addresses (textInput_getOutputAddress, func_02_494a twice, textInput_loadCharacterGfx, copyTextCharacters,
     * as the disassembly's symbols name them, and the last glyphs' table in the tail), and the SHA-1 of the whole tail. */
    static const struct { OraclesGame game; const char *sites; unsigned relocated[LANGUAGE_KANA_RELOCATIONS]; const char *tail_sha1; } pins[] = {
        { ORACLES_GAME_AGES, "067f c3957e c3297f 05 04 c3e67e 507f 557f c3347f", { 0x4a37, 0x494a, 0x494a, 0x49a5, 0x49cd, 0x7f6f }, "c965b438dbd7a6c7fff76c26feb635e8a584f827" },
        { ORACLES_GAME_SEASONS, "2c76 c3bb75 c34f76 05 04 c30c76 7676 7b76 c35a76", { 0x49f7, 0x490a, 0x490a, 0x4965, 0x498d, 0x7695 }, "8a6a48a14d412997ab35b4d6ca1fb49791d216e8" },
    };
    char written[64] = "", tail_sha1[41];
    size_t w = 0;
    for (unsigned k = 0; k < LANGUAGE_KANA_SITES; k++) {
        for (unsigned i = 0; i < kana.site_bytes[k]; i++) w += (size_t)snprintf(written + w, sizeof written - w, "%02x", kana.written[k][i]);
        if (k + 1u < LANGUAGE_KANA_SITES) w += (size_t)snprintf(written + w, sizeof written - w, " ");
    }
    oracles_sha1_hex(kana.tail, kana.length, tail_sha1);
    unsigned pinned = 0;
    for (size_t p = 0; p < sizeof pins / sizeof pins[0]; p++) {
        if (pins[p].game != game) continue;
        unsigned targets = 0;
        for (unsigned r = 0; r < kana.relocations; r++)
            targets += (unsigned)(kana.tail[kana.relocated[r]] | kana.tail[kana.relocated[r] + 1u] << 8) == pins[p].relocated[r];
        pinned = !strcmp(written, pins[p].sites) && targets == LANGUAGE_KANA_RELOCATIONS && kana.relocations == LANGUAGE_KANA_RELOCATIONS
                 && !strcmp(tail_sha1, pins[p].tail_sha1);
    }
    printf("%s ja kana: the sites write %s; the tail's SHA-1 %s; %s\n", name, written, tail_sha1, pinned ? "as pinned" : "NOT as pinned");
    CHECK(pinned);
}

/* The Japanese graphics: every header of a screen the Japanese ROM draws in Japanese loads, from the composed image as
 * the US game does, what the Japanese ROM loads over every byte the replaced graphics write, the bytes left those of the
 * US graphics kept (the secret entry's grid); every other header loads what the US ROM loads (the
 * Japanese intro's aside); the inventory's symbols' object header loads what the Japanese ROM's loads, every other
 * object's and tree's what the US one loads; the title's colours are the Japanese ROM's, written over the US
 * ones, and its sprites' table the Japanese ROM's, the furigana among them, in the bank's free tail, the code's pointer
 * set there; the name entry's code and routines (check_japanese_kana); the image changed nowhere else than the header's
 * sums, the font, those tables, that pointer, the name entry's sites and routines and the graphics' entries and bytes. */
static void check_japanese_graphics(const char *name, OraclesGame game, const OraclesLanguagePack *pack, const rom_file *us, const rom_file *jp,
                                    const uint8_t *image, size_t size, const expected_japanese *e)
{
    static gfx_load composed, japanese, original;
    static uint8_t allowed[0x100000];
    char sources[2048] = "";
    const unsigned count = oracles_language_pack_graphic_count(pack, ORACLES_LANGUAGE_JAPANESE);
    for (unsigned n = 0; n < count; n++) {
        OraclesLanguageGraphic g;
        CHECK(oracles_language_pack_graphic(pack, ORACLES_LANGUAGE_JAPANESE, n, &g) == 0);
        snprintf(sources + strlen(sources), sizeof sources - strlen(sources), "%s%02x:%04x", n ? " " : "", g.bank, g.address);
    }
    printf("%s ja graphics replaced: %s\n", name, sources);
    CHECK(!strcmp(sources, e->graphics));
    const size_t us_table = language_gfx_headers_us(game), jp_table = language_gfx_headers_japanese(game), font = language_font_us(game);
    unsigned screens = 0, compared = 0, mismatched = 0, left = 0, others = 0, changed_others = 0, entries = 0, made = 0;
    CHECK(size == 2u * us->size && us->size == sizeof allowed);
    memset(allowed, 0, sizeof allowed);
    allowed[0x148] = allowed[0x14d] = allowed[0x14e] = allowed[0x14f] = 1;
    memset(allowed + font + GLYPH_FIRST * GLYPH_BYTES, 1, GLYPHS_MAX * GLYPH_BYTES);
    for (unsigned h = 0; h < GFX_HEADERS; h++) {
        if (h == 0x04) continue;
        CHECK(load_header(image, size, us_table, h, -1, &composed) == 0);
        CHECK(load_header(us->data, us->size, us_table, h, -1, &original) == 0);
        for (unsigned j = 0; j < composed.count; j++) {
            const uint8_t *x = image + composed.at[j], *y = us->data + composed.at[j];
            CHECK(!memcmp(x + 3, y + 3, 3));   /* the destination and the size stay */
            uint8_t data[2048], original_data[2048];
            size_t consumed = 0;
            const size_t bytes = 16u * ((x[5] & 0x7fu) + 1u), source = oracles_rom_offset(x[0] & 0x3fu, (unsigned)x[1] << 8 | x[2]);
            CHECK(language_gfx_decompress(image, size, source, x[0] >> 6, data, bytes, &consumed) == 0);
            CHECK(language_gfx_decompress(us->data, us->size, oracles_rom_offset(y[0] & 0x3fu, (unsigned)y[1] << 8 | y[2]), y[0] >> 6, original_data, bytes, NULL) == 0);
            if (!memcmp(x, y, 3) && !memcmp(data, original_data, bytes)) continue;
            CHECK(source + consumed <= sizeof allowed);
            memset(allowed + composed.at[j], 1, 3);
            if (source + consumed <= sizeof allowed) memset(allowed + source, 1, consumed);
            entries++;
        }
        if (!japanese_screen(game, h)) {
            others++;
            unsigned changed = 0;
            for (size_t k = 0; k < SPACE; k++) changed += composed.value[k] != original.value[k];
            changed_others += changed != 0;
            continue;
        }
        screens++;
        CHECK(load_header(jp->data, jp->size, jp_table, h, -1, &japanese) == 0);
        for (size_t k = 0; k < SPACE; k++) {
            if (composed.entry[k] < 0) continue;
            CHECK(japanese.value[k] != 0xffff);
            const uint8_t *y = us->data + composed.at[composed.entry[k]];
            char named[16];
            snprintf(named, sizeof named, "%02x:%02x%02x", y[0] & 0x3fu, y[1], y[2]);
            if (strstr(e->grids, named)) made++;
            else if (strstr(sources, named)) { compared++; mismatched += composed.value[k] != japanese.value[k]; }
            else left += composed.value[k] != japanese.value[k];
        }
    }
    /* The title's colours (PALH_03's, at the same place in both ROMs), its sprites' table and the pointer to it. */
    const size_t colours = oracles_rom_offset(e->colours_bank, 0x4018), sprites = oracles_rom_offset(e->sprites_bank, e->sprites_address);
    const size_t pointer = oracles_rom_offset(0x03, e->sprites_pointer), jp_sprites = language_title_sprites_japanese(game);
    const size_t sprites_bytes = 1u + 4u * jp->data[jp_sprites];
    CHECK(!memcmp(image + colours, jp->data + colours, 0x80) && memcmp(us->data + colours, jp->data + colours, 0x80));
    CHECK(jp->data[jp_sprites] == 25u && !memcmp(image + sprites, jp->data + jp_sprites, sprites_bytes));
    CHECK(image[pointer - 1u] == 0x21u && ((unsigned)image[pointer] | (unsigned)image[pointer + 1u] << 8) == e->sprites_address);
    memset(allowed + colours, 1, 0x80);
    memset(allowed + sprites, 1, sprites_bytes);
    allowed[pointer] = allowed[pointer + 1u] = 1;
    check_japanese_secrets(name, game, us, jp, image, size, e, allowed);
    check_japanese_kana(name, game, us, jp, image, e, allowed);
    /* The objects' and the trees' headers: the inventory symbols' loads its 32 blocks as the Japanese ROM's of the same
     * index loads them, the bit that chains the next header kept; every other reads the bytes the US one reads. */
    const size_t objects = oracles_rom_offset(0x3f, e->objects), jp_objects = language_object_headers_japanese(game);
    unsigned same_objects = 0, symbols = 0;
    CHECK(jp_objects != 0);
    for (unsigned i = 0; jp_objects && i < e->object_headers; i++) {
        const uint8_t *x = image + objects + 3u * i, *y = us->data + objects + 3u * i, *j = jp->data + jp_objects + 3u * i;
        const size_t source = oracles_rom_offset(x[0] & 0x3fu, (unsigned)(x[1] & 0x7fu) << 8 | x[2]);
        size_t consumed = 0;
        CHECK(language_gfx_decompress(image, size, source, x[0] >> 6, NULL, 512, &consumed) == 0 && source + consumed <= sizeof allowed);
        if (i != e->symbols) {
            same_objects += !memcmp(x, y, 3) && !memcmp(image + source, us->data + source, consumed);
            continue;
        }
        uint8_t mine[512], theirs[512];
        CHECK(language_gfx_decompress(image, size, source, x[0] >> 6, mine, sizeof mine, NULL) == 0);
        CHECK(language_gfx_decompress(jp->data, jp->size, oracles_rom_offset(j[0] & 0x3fu, (unsigned)(j[1] & 0x7fu) << 8 | j[2]), j[0] >> 6, theirs, sizeof theirs, NULL) == 0);
        symbols = !memcmp(mine, theirs, sizeof mine) && memcmp(x, y, 3) && (x[1] & 0x80u) == (y[1] & 0x80u);
        memset(allowed + objects + 3u * i, 1, 3);
        if (source + consumed <= sizeof allowed) memset(allowed + source, 1, consumed);
    }
    printf("%s ja objects: %u headers, the inventory symbols' (%02x) %s, %u others reading the US bytes\n", name, e->object_headers, e->symbols,
           symbols ? "loading the Japanese ROM's" : "not loading the Japanese ROM's", same_objects);
    CHECK(symbols && same_objects + 1u == e->object_headers);
    unsigned outside = 0;
    for (size_t i = 0; i < us->size; i++) outside += image[i] != us->data[i] && !allowed[i];
    printf("%s ja graphics: %u replaced; %u screens drawn in Japanese, %u entries loading them, %u bytes as the Japanese ROM loads them, %u different, "
           "%u left as the US game loads them, %u of the grids made for Japanese; %u other headers, %u loading otherwise; the title's colours and %u "
           "sprites; %u bytes of the US ROM changed elsewhere\n",
           name, count, screens, entries, compared, mismatched, left, made, others, changed_others, jp->data[jp_sprites], outside);
    CHECK(compared > 0 && mismatched == 0 && left == e->graphics_left && made > 0 && changed_others == 0 && outside == 0);
}

/* The Japanese texts the pack serves, by their addresses in the two ROMs: those of the Japanese ROM no US text takes,
 * and those served for two US texts the US ROM keeps apart, as "TX_GGII unserved" and "TX_GGII twice", the Japanese
 * identifier.  A text shifted against another, which the comparison of commands cannot see between two texts without
 * any, adds to both. */
static void japanese_structure(const OraclesRom *us, const OraclesTextSource *us_source, const OraclesRom *jp, const OraclesTextSource *jp_source,
                               const language_table *table, char *out, size_t capacity)
{
    enum { PAIRS = 8192 };
    static long jp_at[PAIRS], us_at[PAIRS];
    unsigned pairs = 0;
    out[0] = 0;
    const unsigned groups = oracles_text_groups(us, us_source), jp_groups = oracles_text_groups(jp, jp_source);
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < groups; group++) {
        const int count = oracles_text_count(us, us_source, group);
        for (int index = 0; index < count && pairs < PAIRS; index++) {
            const unsigned id = (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index;
            const long at = oracles_text_address(us, us_source, group, (unsigned)index);
            unsigned from = 0;   /* the representative */
            while (from < (unsigned)index && oracles_text_address(us, us_source, group, from) != at) from++;
            unsigned from_id = (id & ~0xffu) | from;
            language_exception x;
            if (language_table_find(table, ORACLES_LANGUAGE_JAPANESE, id, &x)) {
                if (x.source == ORACLES_LANGUAGE_SOURCE_MOVED) from_id = x.id;
                else if (x.source != ORACLES_LANGUAGE_SOURCE_ADAPTED) continue;
            }
            const unsigned from_group = (from_id >> 8) + ORACLES_TEXT_DICTIONARY_GROUPS;
            if (from_group >= jp_groups || (int)(from_id & 0xffu) >= oracles_text_count(jp, jp_source, from_group)) {
                const size_t used = strlen(out);
                snprintf(out + used, capacity - used, "%sTX_%04x missing", used ? ", " : "", from_id);
                continue;
            }
            jp_at[pairs] = oracles_text_address(jp, jp_source, from_group, from_id & 0xffu);
            us_at[pairs++] = at;
        }
    }
    for (unsigned group = ORACLES_TEXT_DICTIONARY_GROUPS; group < jp_groups && group < groups; group++) {
        /* A group the US ROM leaves empty holds the Japanese ROM's placeholders alone. */
        const int count = oracles_text_count(us, us_source, group) > 0 ? oracles_text_count(jp, jp_source, group) : 0;
        for (int index = 0; index < count; index++) {
            const long at = oracles_text_address(jp, jp_source, group, (unsigned)index);
            unsigned served = 0, twice = 0;
            for (unsigned p = 0; p < pairs; p++) {
                if (jp_at[p] != at) continue;
                if (served && us_at[p] != us_at[served - 1u]) twice = 1;
                if (!served) served = p + 1u;
            }
            /* Said once, at the first identifier of the Japanese ROM with that address. */
            int first = 1;
            for (int i = 0; i < index && first; i++) first = oracles_text_address(jp, jp_source, group, (unsigned)i) != at;
            if (!first || (served && !twice)) continue;
            const size_t used = strlen(out);
            snprintf(out + used, capacity - used, "%sTX_%04x %s", used ? ", " : "", (group - ORACLES_TEXT_DICTIONARY_GROUPS) << 8 | (unsigned)index,
                     served ? "twice" : "unserved");
        }
    }
}

/* The characters a text prints, its spaces and line breaks aside, into `out`: their number. */
static size_t printed(const uint8_t *text, size_t length, uint8_t *out)
{
    size_t n = 0;
    for (size_t at = 0; at < length && text[at]; at += text[at] >= 0x10u || text[at] <= 0x01u ? 1u : 2u)
        if (text[at] >= 0x10u && text[at] != ' ') out[n++] = text[at];
    return n;
}

static int check_japanese(const char *dir, const char *work, const expected_japanese *e)
{
    const OraclesGame game = e->game;
    rom_file us = find(dir, e->us), jp = find(dir, e->jp);
    const char *name = game == ORACLES_GAME_AGES ? "ages" : "seasons";
    if (!us.data || !jp.data) { printf("%s: no US and Japanese ROMs in %s: Japanese not checked\n", name, dir); free(us.data); free(jp.data); return 0; }
    char error[256];
    OraclesLanguagePack *pack = oracles_language_pack_extract(jp.data, jp.size, us.data, us.size, error, sizeof error);
    CHECK(pack != NULL);
    if (!pack) { fprintf(stderr, "%s ja: %s\n", name, error); free(us.data); free(jp.data); return 1; }
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_EUROPEAN_END; language++) CHECK(!oracles_language_pack_count(pack, language));
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_JAPANESE; language++) CHECK(!oracles_language_pack_graphic_count(pack, language));

    const OraclesRom us_rom = { us.data, us.size }, jp_rom = { jp.data, jp.size };
    OraclesTextSource source, jp_source;
    CHECK(language_source_us(&us_rom, game, &source) == 0 && language_source_japanese(&jp_rom, game, &jp_source) == 0);
    language_table *table = language_table_load_japanese(game, error, sizeof error);
    CHECK(table != NULL);
    const unsigned count = oracles_language_pack_count(pack, ORACLES_LANGUAGE_JAPANESE);
    unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT], long_lines = 0, unheld = 0;
    oracles_language_pack_tally(pack, ORACLES_LANGUAGE_JAPANESE, counts);
    static char list[16384];
    list[0] = 0;
    for (unsigned n = 0; n < count; n++) {
        unsigned id = 0;
        size_t length = 0;
        OraclesLanguageSource from;
        const uint8_t *text = oracles_language_pack_nth(pack, ORACLES_LANGUAGE_JAPANESE, n, &id, &from, &length);
        if (from != ORACLES_LANGUAGE_SOURCE_SAME) {
            const size_t used = strlen(list);
            snprintf(list + used, sizeof list - used, "%s%04x:%s", used ? " " : "", id, oracles_language_source_name(from));
        }
        if (!text) continue;
        language_exception x;
        const int one_digit = table && language_table_find(table, ORACLES_LANGUAGE_JAPANESE, id, &x) && x.source == ORACLES_LANGUAGE_SOURCE_ADAPTED
                              && x.rule == RULE_ONE_DIGIT;
        if (oracles_text_longest_line(text, length) > 16u + (one_digit ? 1u : 0u)) long_lines++;
        uint8_t original[ORACLES_TEXT_MAX];
        size_t original_length = 0;
        CHECK(oracles_text_decode(&us_rom, &source, (id >> 8) + ORACLES_TEXT_DICTIONARY_GROUPS, id & 0xffu, original, sizeof original, &original_length) == ORACLES_OK);
        if (from != ORACLES_LANGUAGE_SOURCE_ADAPTED && !language_same_commands(ORACLES_LANGUAGE_JAPANESE, original, original_length, text, length)) unheld++;
    }
    const unsigned served = counts[ORACLES_LANGUAGE_SOURCE_SAME] + counts[ORACLES_LANGUAGE_SOURCE_MOVED] + counts[ORACLES_LANGUAGE_SOURCE_ADAPTED]
                            + counts[ORACLES_LANGUAGE_SOURCE_OWN];
    printf("%s ja: %u texts: %u same, %u moved, %u adapted, %u own, %u kept, %u refused, %u missing: %.2f %% translated, %.2f %% official; "
           "%u lines over 16\n  not the same: %s\n",
           name, count, counts[ORACLES_LANGUAGE_SOURCE_SAME], counts[ORACLES_LANGUAGE_SOURCE_MOVED], counts[ORACLES_LANGUAGE_SOURCE_ADAPTED],
           counts[ORACLES_LANGUAGE_SOURCE_OWN], counts[ORACLES_LANGUAGE_SOURCE_KEPT], counts[ORACLES_LANGUAGE_SOURCE_REFUSED],
           counts[ORACLES_LANGUAGE_SOURCE_MISSING], 100.0 * served / (count ? count : 1u), 100.0 * (served - counts[ORACLES_LANGUAGE_SOURCE_OWN]) / (count ? count : 1u),
           long_lines, list);
    /* Every US text in Japanese: none refused, none missing, none kept in English. */
    CHECK(count == e->texts && served == count && long_lines == 0 && unheld == 0);
    CHECK(counts[ORACLES_LANGUAGE_SOURCE_SAME] == e->same && counts[ORACLES_LANGUAGE_SOURCE_MOVED] == e->moved
          && counts[ORACLES_LANGUAGE_SOURCE_ADAPTED] == e->adapted && counts[ORACLES_LANGUAGE_SOURCE_OWN] == e->own);

    /* Seasons TX_1f04, its count of golden beasts in one digit: the Japanese text as the Japanese ROM lays it out. */
    if (game == ORACLES_GAME_SEASONS) {
        uint8_t official[ORACLES_TEXT_MAX];
        size_t official_length = 0, served_length = 0;
        const uint8_t *served = oracles_language_pack_text(pack, ORACLES_LANGUAGE_JAPANESE, 0x1f04, &served_length);
        CHECK(oracles_text_decode(&jp_rom, &jp_source, ORACLES_TEXT_DICTIONARY_GROUPS + 0x1fu, 0x04u, official, sizeof official, &official_length) == ORACLES_OK);
        CHECK(served && served_length == official_length && !memcmp(served, official, official_length));
    }

    /* The texts' structure: each Japanese text served, each for one US text, but those pinned. */
    if (table) {
        static char structure[4096];
        japanese_structure(&us_rom, &source, &jp_rom, &jp_source, table, structure, sizeof structure);
        printf("%s ja structure: %s\n", name, structure[0] ? structure : "every Japanese text served, each for one US text");
        CHECK(!strcmp(structure, e->structure));
    }

    /* The whole font, the Japanese ROM's gfx_font from character $0e. */
    const uint8_t *characters, *bitmaps;
    const unsigned glyphs = oracles_language_pack_glyphs(pack, &characters, &bitmaps);
    const size_t jp_font = language_font_japanese(game), us_font = language_font_us(game);
    CHECK(glyphs == GLYPHS_MAX && characters[0] == GLYPH_FIRST && !memcmp(bitmaps, jp.data + jp_font + GLYPH_FIRST * GLYPH_BYTES, GLYPHS_MAX * GLYPH_BYTES));

    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_JAPANESE, us.data, us.size, NULL, &image, &size, &report, error, sizeof error) == 0);
    if (image) {
        CHECK(report.served == count && report.english == 0 && report.missing == 0);
        CHECK(report.graphics == oracles_language_pack_graphic_count(pack, ORACLES_LANGUAGE_JAPANESE));
        CHECK(!memcmp(image + us_font + GLYPH_FIRST * GLYPH_BYTES, jp.data + jp_font + GLYPH_FIRST * GLYPH_BYTES, GLYPHS_MAX * GLYPH_BYTES));
        CHECK(!memcmp(image + (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + ORACLES_LANGUAGE_INDEX_CODE, "ja", 3));
        printf("%s ja composed: %u texts served, %u bytes; %u graphics, %u bytes\n", name, report.served, report.bytes, report.graphics, report.graphics_bytes);
        check_japanese_graphics(name, game, pack, &us, &jp, image, size, e);
    }
    /* An image that no longer holds a US table the Japanese graphics replace (the title's colours and the free tail its
     * sprites go to, the secrets' symbols and their terminator, their cipher and the names of their list, the US code of
     * the name entry, at its start, inside and at its end, and the free tail of bank 2, inside and at its end), or the US
     * routine the name entry's routines call (copyTextCharactersFromHlUntilNull), or whose code no longer names the US
     * sprites' table where the pointer is, or the US objects' and trees' headers' tables where loadObjectGfx's callers
     * name them, is refused, not written over. */
    uint8_t *other = malloc(us.size);
    CHECK(other != NULL);
    const size_t places[] = { oracles_rom_offset(e->colours_bank, 0x4018), oracles_rom_offset(e->sprites_bank, e->sprites_address + 0x20u),
                              oracles_rom_offset(0x03, e->sprites_pointer), 0xe0u, 0xe1u, oracles_rom_offset(0x03, 0x4ada),
                              oracles_rom_offset(0x02, e->secret_names + 0x40u), oracles_rom_offset(0x3f, 0x41d5), oracles_rom_offset(0x3f, 0x42f6),
                              oracles_rom_offset(0x3f, 0x4300), oracles_rom_offset(0x02, e->kana_entry), oracles_rom_offset(0x02, e->kana_entry + 0x200u),
                              oracles_rom_offset(0x02, e->kana_entry + 0x373u), oracles_rom_offset(0x02, e->kana_tail + 0x100u), oracles_rom_offset(0x02, 0x7fff),
                              oracles_rom_offset(0x02, e->kana_called), oracles_rom_offset(0x02, e->kana_called + 0x1fu) };
    for (unsigned k = 0; other && k < sizeof places / sizeof places[0]; k++) {
        memcpy(other, us.data, us.size);
        other[places[k]] ^= 0x01u;
        uint8_t *refused = NULL;
        size_t refused_size = 0;
        CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_JAPANESE, other, us.size, NULL, &refused, &refused_size, &report, error, sizeof error) != 0 && !refused);
        const char *reason = k >= 15u ? "does not hold the US routine" : k >= 7u && k <= 9u ? "does not name the US object graphics headers"
                           : k == 2u ? "does not name the US table" : "does not hold the US table";
        CHECK(strstr(error, reason) != NULL);
        if (!strstr(error, reason)) fprintf(stderr, "%s ja: a bit at %06zx: %s\n", name, places[k], error);
        free(refused);
    }
    free(other);
    /* A Japanese ROM whose blocks the name entry's routines copy differ, at their first byte or their last: no
     * Japanese graphic made. */
    uint8_t *changed_jp = malloc(jp.size);
    CHECK(changed_jp != NULL);
    static language_kana kana;
    const OraclesRom us_kana = { us.data, us.size };
    CHECK(language_kana_make(game, &jp_rom, &us_kana, e->kana_tail, &kana) == 0);
    unsigned blocks_refused = 0;
    for (unsigned k = 0; changed_jp && k < 2u * LANGUAGE_KANA_BLOCKS; k++) {
        memcpy(changed_jp, jp.data, jp.size);
        changed_jp[oracles_rom_offset(0x02, kana.block_jp[k / 2u] + (k & 1u ? kana.block_bytes[k / 2u] - 1u : 0u))] ^= 0x80u;
        const OraclesRom changed_rom = { changed_jp, jp.size };
        OraclesLanguagePack *none = language_pack_new(game, pack->source, pack->image);
        CHECK(none != NULL);
        if (!none) break;
        blocks_refused += language_gfx_extract_japanese(none, &changed_rom, &us_rom, error, sizeof error) != 0
                          && oracles_language_pack_graphic_count(none, ORACLES_LANGUAGE_JAPANESE) == 0;
        oracles_language_pack_free(none);
    }
    printf("%s ja kana: %u of %u Japanese ROMs with a block changed refused\n", name, blocks_refused, 2u * LANGUAGE_KANA_BLOCKS);
    CHECK(blocks_refused == 2u * LANGUAGE_KANA_BLOCKS);
    free(changed_jp);
    /* The wide box keeps the game's lines in Japanese: the image composed for it is the one composed without it. */
    const OraclesLanguageLayout wide = { 192, 1, 0, NULL };
    uint8_t *laid = NULL;
    size_t laid_size = 0;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_JAPANESE, us.data, us.size, &wide, &laid, &laid_size, &report, error, sizeof error) == 0);
    CHECK(laid && image && report.laid_out == 0 && laid_size == size && !memcmp(laid, image, size));
    /* The layout itself in Japanese, on the wide box's 192 pixels in the composed font: every text laid out again
     * prints the characters it printed, its spaces and line breaks aside, $2d being the katakana's long mark, never a
     * hyphen dropped (Seasons TX_1409 "たいへんだよー|ぼくの"). */
    unsigned changed = 0, wrapped = 0, long_mark = 0;
    const OraclesRom composed = { laid, laid_size };
    const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
    OraclesTextFont font;
    CHECK(laid && oracles_text_font(&composed, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
    oracles_text_font_fixed(&font, &font);
    language_wrap *wrap = laid ? language_wrap_new(&font, wide.width, ORACLES_LANGUAGE_JAPANESE) : NULL;
    CHECK(wrap != NULL);
    for (unsigned n = 0; wrap && n < count; n++) {
        unsigned id = 0;
        size_t length = 0, again_length = 0;
        OraclesLanguageSource from;
        const uint8_t *text = oracles_language_pack_nth(pack, ORACLES_LANGUAGE_JAPANESE, n, &id, &from, &length);
        uint8_t again[ORACLES_TEXT_MAX];
        if (!text || (id >> 8) == 0x09u || language_wrap_text(wrap, text, length, again, sizeof again, &again_length) != WRAP_DONE) continue;
        wrapped++;
        uint8_t before[ORACLES_TEXT_MAX], after[ORACLES_TEXT_MAX];
        const size_t b = printed(text, length, before), a = printed(again, again_length, after);
        if (a != b || memcmp(before, after, a)) {
            if (changed++ < 8u) fprintf(stderr, "%s ja: TX_%04x laid out on 192 pixels prints other characters\n", name, id);
        }
        if (id == 0x1409) long_mark = hyphens(again, again_length) == 2u && hyphens(text, length) == 2u;
    }
    printf("%s ja laid out on 192 pixels (the layout alone, the wide box keeping the game's lines): %u texts laid out again, %u printing other characters\n",
           name, wrapped, changed);
    CHECK(wrapped > 0 && changed == 0);
    if (game == ORACLES_GAME_SEASONS) CHECK(long_mark);
    language_wrap_free(wrap);
    free(laid);
    language_table_free(table);

    char path[1024];
    snprintf(path, sizeof path, "%s/%s-ja.pack", work, name);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
    CHECK(back != NULL);
    if (back && image) {
        uint8_t *again = NULL;
        size_t again_size = 0;
        CHECK(oracles_language_compose(back, ORACLES_LANGUAGE_JAPANESE, us.data, us.size, NULL, &again, &again_size, &report, error, sizeof error) == 0);
        CHECK(again && again_size == size && !memcmp(again, image, size));
        free(again);
    }
    oracles_language_pack_free(back);
    remove(path);
    free(image);
    oracles_language_pack_free(pack);
    free(us.data);
    free(jp.data);
    return 1;
}

/* Polish from the Polish ROM of a game, or from Kaxus's patch of the directory applied to the US ROM (the same image when
 * the directory holds both): every US text served as the Polish ROM has it, five lines of Ages corrected first, each
 * text holding the US text's commands and lines, the same pack by the public extraction; the pack holding Polish alone,
 * the graphics of the screens the Polish ROM translates, and the nine glyphs of the Polish letters, byte for byte the
 * Polish ROM's; nothing else of the US ROM changed in the composed image but those glyphs, the graphics and their
 * headers' entries, and the header's sums; the compact text laid out in Polish, a word broken at a syllable joined
 * again; the pack's file read back.  The coverage printed. */
typedef struct expected_polish {
    OraclesGame game;
    OraclesRomRevision us, pl;
    unsigned texts, corrected;
    expected_layout layout;
    const char *graphics;   /* the US graphics replaced, the table of the title's sprites of PRESS START among them */
    unsigned graphics_left; /* the bytes of the screens translated the Polish ROM loads otherwise from the US graphics kept */
    unsigned press_start_bank, press_start_address;   /* the US table of the title's sprites of PRESS START */
} expected_polish;
static const expected_polish polish_expected[] = {
    { ORACLES_GAME_AGES, ORACLES_ROM_REVISION_AGES_US, ORACLES_ROM_REVISION_AGES_PL, 2824, 5, { 1751, 19, 0, 8666, 6376, 1 },
      "29:5b5f 29:5ca9 29:5de7 29:5f33 29:6071 29:61ac 29:62ff 29:644d 29:6589 29:66cc 30:763a 31:7524 37:7cba 37:7e5d 37:7edf "
      "37:7f62 38:4470 38:456a 38:470d 38:48b5 38:4930 38:4e0a 38:55bb 38:57cd 38:5b12 38:5b90 38:5d07 38:664f 3f:59aa", 456, 0x3f, 0x59aa },
    { ORACLES_GAME_SEASONS, ORACLES_ROM_REVISION_SEASONS_US, ORACLES_ROM_REVISION_SEASONS_PL, 1885, 0, { 1052, 23, 0, 5426, 4096, 0 },
      "03:4e65 28:5542 28:5646 28:57d2 28:593e 28:5c23 28:5d56 28:5e99 28:5feb 28:6184 28:628f 30:7e3e 31:6bc2 37:7bbc 37:7d5f "
      "37:7de1 37:7e64 38:4372 38:446c 38:460f 38:47b7 38:4832 38:4d0c 38:54bd 38:56d4 38:5a19 38:5a97 38:5c0e 38:6556", 677, 0x03, 0x4e65 },
};

/* Whether the Polish ROM translates the screen of header h: the title, the end, the secrets' screens, the error, the
 * dungeons' names, the file select and its screens. */
static int polish_screen(OraclesGame game, unsigned h)
{
    return h == 0x02 || h == 0x03 || h == 0x05 || h == 0x06 || h == 0x07 || (h >= 0x10 && h <= 0x1f) || h == 0x97 || h == 0x9f
        || (h >= 0xa0 && h <= 0xaf) || h == 0xba || (game == ORACLES_GAME_SEASONS && h == 0x84);
}

/* The Polish graphics: every header of a screen the Polish ROM translates loads, from the composed image as the US
 * game does, what the Polish ROM loads over every byte the replaced graphics write, the bytes left those of the US
 * graphics kept (the HUD's, which the Polish ROM redraws without text and other screens load too); every other header
 * loads what the US ROM loads (the Japanese intro's aside, which the US games never load); the title's sprites of PRESS
 * START are the Polish ROM's, their table written over the US one; the image changed nowhere else than the header's
 * sums, the font's nine glyphs, that table and the graphics' entries and bytes. */
static void check_polish_graphics(const char *name, OraclesGame game, const OraclesLanguagePack *pack, const rom_file *us, const rom_file *pl,
                                  const uint8_t *image, size_t size, const expected_polish *e)
{
    static gfx_load composed, polish, original;
    static uint8_t allowed[0x100000];
    char sources[1024] = "";
    const unsigned count = oracles_language_pack_graphic_count(pack, ORACLES_LANGUAGE_POLISH);
    for (unsigned n = 0; n < count; n++) {
        OraclesLanguageGraphic g;
        CHECK(oracles_language_pack_graphic(pack, ORACLES_LANGUAGE_POLISH, n, &g) == 0);
        snprintf(sources + strlen(sources), sizeof sources - strlen(sources), "%s%02x:%04x", n ? " " : "", g.bank, g.address);
    }
    printf("%s pl graphics replaced: %s\n", name, sources);
    CHECK(!strcmp(sources, e->graphics));
    const size_t us_table = language_gfx_headers_us(game), pl_table = language_gfx_headers_polish(game), font = language_font_us(game);
    unsigned screens = 0, compared = 0, mismatched = 0, left = 0, others = 0, changed_others = 0, entries = 0;
    CHECK(size == 2u * us->size && us->size == sizeof allowed);
    memset(allowed, 0, sizeof allowed);
    allowed[0x148] = allowed[0x14d] = allowed[0x14e] = allowed[0x14f] = 1;
    memset(allowed + font + 0x80u * GLYPH_BYTES, 1, 9u * GLYPH_BYTES);
    for (unsigned h = 0; h < GFX_HEADERS; h++) {
        if (h == 0x04) continue;
        CHECK(load_header(image, size, us_table, h, -1, &composed) == 0);
        CHECK(load_header(us->data, us->size, us_table, h, -1, &original) == 0);
        /* The entries that load another graphic than the US one: repointed, or whose graphic was written in place. */
        for (unsigned j = 0; j < composed.count; j++) {
            const uint8_t *x = image + composed.at[j], *y = us->data + composed.at[j];
            CHECK(!memcmp(x + 3, y + 3, 3));   /* the destination and the size stay */
            uint8_t data[2048], original_data[2048];
            size_t consumed = 0;
            const size_t bytes = 16u * ((x[5] & 0x7fu) + 1u), source = oracles_rom_offset(x[0] & 0x3fu, (unsigned)x[1] << 8 | x[2]);
            CHECK(language_gfx_decompress(image, size, source, x[0] >> 6, data, bytes, &consumed) == 0);
            CHECK(language_gfx_decompress(us->data, us->size, oracles_rom_offset(y[0] & 0x3fu, (unsigned)y[1] << 8 | y[2]), y[0] >> 6, original_data, bytes, NULL) == 0);
            if (!memcmp(x, y, 3) && !memcmp(data, original_data, bytes)) continue;
            CHECK(source + consumed <= sizeof allowed);
            memset(allowed + composed.at[j], 1, 3);
            if (source + consumed <= sizeof allowed) memset(allowed + source, 1, consumed);
            entries++;
        }
        if (!polish_screen(game, h)) {
            others++;
            unsigned changed = 0;
            for (size_t k = 0; k < SPACE; k++) changed += composed.value[k] != original.value[k];
            changed_others += changed != 0;
            continue;
        }
        screens++;
        CHECK(load_header(pl->data, pl->size, pl_table, h, -1, &polish) == 0);
        for (size_t k = 0; k < SPACE; k++) {
            if (composed.entry[k] < 0) continue;
            CHECK(polish.value[k] != 0xffff);
            const uint8_t *y = us->data + composed.at[composed.entry[k]];   /* the US graphic the entry named */
            char named[16];
            snprintf(named, sizeof named, "%02x:%02x%02x", y[0] & 0x3fu, y[1], y[2]);
            if (strstr(sources, named)) { compared++; mismatched += composed.value[k] != polish.value[k]; }
            else left += composed.value[k] != polish.value[k];
        }
    }
    const size_t press_start = oracles_rom_offset(e->press_start_bank, e->press_start_address), pl_press_start = language_press_start_polish(game);
    const size_t press_start_bytes = 1u + 4u * pl->data[pl_press_start];
    CHECK(press_start_bytes <= 32u && !memcmp(image + press_start, pl->data + pl_press_start, press_start_bytes));
    memset(allowed + press_start, 1, press_start_bytes);
    unsigned outside = 0;
    for (size_t i = 0; i < us->size; i++) outside += image[i] != us->data[i] && !allowed[i];
    printf("%s pl graphics: %u replaced; %u screens translated, %u entries loading them, %u bytes as the Polish ROM loads them, %u different, "
           "%u left as the US game loads them; %u other headers, %u loading otherwise; %u bytes of the US ROM changed elsewhere\n",
           name, count, screens, entries, compared, mismatched, left, others, changed_others, outside);
    CHECK(count == 29u && compared > 0 && mismatched == 0 && left == e->graphics_left && changed_others == 0 && outside == 0);
}
/* The texts corrected, what each holds then and no more. */
static const correction polish_corrected[] = {
    { AGES, ORACLES_LANGUAGE_POLISH, 0x000b, 0x000b, "a teraźniejszo-\\nścią!", "teraźniejszością" },
    { AGES, ORACLES_LANGUAGE_POLISH, 0x240a, 0x240a, "Goronem-\\ntancerzem!", "Goronem-tancerzem" },
    { AGES, ORACLES_LANGUAGE_POLISH, 0x242a, 0x242a, "Goronem-\\ntancerzem!", "Goronem-tancerzem" },
    { AGES, ORACLES_LANGUAGE_POLISH, 0x3306, 0x3306, "sięgnie\\n", "sięgnie \\n" },
    { AGES, ORACLES_LANGUAGE_POLISH, 0x5805, 0x5805, "naprawiony...", "odrestaurowany" },
};

/* The Polish ROM of `game` made by a patch of `dir` (Kaxus's, by its SHA-1) applied to the US ROM, or an empty one. */
static rom_file polish_patched(const char *dir, OraclesGame game, const rom_file *us)
{
    rom_file made = { NULL, 0 };
    DIR *listing = opendir(dir);
    struct dirent *entry;
    while (listing && us->data && !made.data && (entry = readdir(listing))) {
        const size_t n = strlen(entry->d_name);
        if (n < 4 || strcmp(entry->d_name + n - 4, ".bps")) continue;
        char path[1024], error[256], sha1[41];
        snprintf(path, sizeof path, "%s/%s", dir, entry->d_name);
        size_t size = 0;
        uint8_t *patch = oracles_bps_read_file(path, &size, error, sizeof error);
        if (patch) oracles_sha1_hex(patch, size, sha1);
        if (patch && oracles_language_patch_game(sha1, ORACLES_LANGUAGE_POLISH) == game)
            made.data = oracles_bps_apply(us->data, us->size, patch, size, &made.size, error, sizeof error);
        free(patch);
    }
    if (listing) closedir(listing);
    return made;
}

static int check_polish(const char *dir, const char *work, const expected_polish *e)
{
    const OraclesGame game = e->game;
    const char *name = game == ORACLES_GAME_AGES ? "ages" : "seasons";
    rom_file us = find(dir, e->us), pl = find(dir, e->pl), patched = polish_patched(dir, game, &us);
    if (!us.data || (!pl.data && !patched.data)) {
        printf("%s: no US ROM and Polish ROM or patch in %s: Polish not checked\n", name, dir);
        free(us.data); free(pl.data); free(patched.data);
        return 0;
    }
    /* The patch makes the Polish ROM itself. */
    if (pl.data && patched.data) CHECK(pl.size == patched.size && !memcmp(pl.data, patched.data, pl.size));
    if (!pl.data) { pl = patched; patched.data = NULL; }
    free(patched.data);
    OraclesRomInfo info;
    char error[256];
    CHECK(oracles_rom_identify(pl.data, pl.size, &info, error, sizeof error) == 0 && info.revision == e->pl && language_polish(&info));
    language_corrections *corrections = language_corrections_load_polish(game, error, sizeof error);
    CHECK(corrections != NULL);
    OraclesLanguagePack *pack = corrections ? language_pack_extract_with(pl.data, pl.size, us.data, us.size, corrections, error, sizeof error) : NULL;
    CHECK(pack != NULL);
    if (!pack) {
        fprintf(stderr, "%s pl: %s\n", name, error);
        language_corrections_free(corrections);
        free(us.data); free(pl.data);
        return 1;
    }
    for (OraclesLanguage language = ORACLES_LANGUAGE_ENGLISH_EU; language < ORACLES_LANGUAGE_POLISH; language++)
        CHECK(!oracles_language_pack_count(pack, language) && !oracles_language_pack_graphic_count(pack, language));
    /* The public extraction loads the Polish corrections itself: the same pack. */
    OraclesLanguagePack *public_pack = oracles_language_pack_extract(pl.data, pl.size, us.data, us.size, error, sizeof error);
    char digest[41] = "", public_digest[41] = "";
    oracles_language_pack_digest(pack, digest);
    if (public_pack) oracles_language_pack_digest(public_pack, public_digest);
    CHECK(public_pack && !strcmp(digest, public_digest));
    oracles_language_pack_free(public_pack);
    if (e->corrected) check_corrections_used(name, corrections);
    else CHECK(language_corrections_count(corrections) == 0);

    const OraclesRom us_rom = { us.data, us.size }, pl_rom = { pl.data, pl.size };
    OraclesTextSource source, pl_source;
    CHECK(language_source_us(&us_rom, game, &source) == 0 && language_source_polish(&pl_rom, game, &pl_source) == 0);
    const unsigned count = oracles_language_pack_count(pack, ORACLES_LANGUAGE_POLISH), pl_groups = oracles_text_groups(&pl_rom, &pl_source);
    unsigned counts[ORACLES_LANGUAGE_SOURCE_COUNT], long_lines = 0, unheld = 0, differ = 0;
    oracles_language_pack_tally(pack, ORACLES_LANGUAGE_POLISH, counts);
    for (unsigned n = 0; n < count; n++) {
        unsigned id = 0;
        size_t length = 0, original_length = 0, official_length = 0;
        OraclesLanguageSource from;
        const uint8_t *text = oracles_language_pack_nth(pack, ORACLES_LANGUAGE_POLISH, n, &id, &from, &length);
        if (!text) continue;
        if (oracles_text_longest_line(text, length) > 16u) long_lines++;
        uint8_t original[ORACLES_TEXT_MAX], official[ORACLES_TEXT_MAX];
        CHECK(oracles_text_decode(&us_rom, &source, (id >> 8) + ORACLES_TEXT_DICTIONARY_GROUPS, id & 0xffu, original, sizeof original, &original_length) == ORACLES_OK);
        if (!oracles_text_same_commands(original, original_length, text, length)) unheld++;
        /* The Polish ROM's text of the same identifier, but those corrected. */
        CHECK(language_text_of(&pl_rom, &pl_source, pl_groups, id, official, &official_length) == 0);
        differ += official_length != length || memcmp(official, text, length);
    }
    const unsigned served = counts[ORACLES_LANGUAGE_SOURCE_SAME];
    printf("%s pl: %u texts: %u same, %u moved, %u adapted, %u own, %u kept, %u refused, %u missing: %.2f %% translated; %u corrected; "
           "%u lines over 16\n", name, count, served, counts[ORACLES_LANGUAGE_SOURCE_MOVED], counts[ORACLES_LANGUAGE_SOURCE_ADAPTED],
           counts[ORACLES_LANGUAGE_SOURCE_OWN], counts[ORACLES_LANGUAGE_SOURCE_KEPT], counts[ORACLES_LANGUAGE_SOURCE_REFUSED],
           counts[ORACLES_LANGUAGE_SOURCE_MISSING], 100.0 * served / (count ? count : 1u), differ, long_lines);
    /* Every US text in Polish, the Polish text of its identifier: none refused, none missing, none kept in English. */
    CHECK(count == e->texts && served == count && long_lines == 0 && unheld == 0 && differ == e->corrected);
    for (size_t c = 0; c < sizeof polish_corrected / sizeof polish_corrected[0]; c++) {
        if (polish_corrected[c].game != game) continue;
        uint8_t present[64], absent[64];
        size_t present_length = 0, absent_length = 0, length = 0;
        CHECK(language_text_encode_in(ORACLES_LANGUAGE_POLISH, polish_corrected[c].present, strlen(polish_corrected[c].present), present, sizeof present, &present_length) == 0);
        CHECK(language_text_encode_in(ORACLES_LANGUAGE_POLISH, polish_corrected[c].absent, strlen(polish_corrected[c].absent), absent, sizeof absent, &absent_length) == 0);
        const uint8_t *text = oracles_language_pack_text(pack, ORACLES_LANGUAGE_POLISH, polish_corrected[c].first, &length);
        CHECK(text && holds(text, length, present, present_length - 1u) && !holds(text, length, absent, absent_length - 1u));
    }

    /* The nine glyphs of the Polish letters, the Polish ROM's, where the US font has its accented capitals. */
    const uint8_t *characters, *bitmaps;
    const unsigned glyphs = oracles_language_pack_glyphs(pack, &characters, &bitmaps);
    const size_t pl_font = language_font_polish(game), us_font = language_font_us(game);
    CHECK(glyphs == 9u);
    for (unsigned g = 0; g < glyphs && g < 9u; g++)
        CHECK(characters[g] == 0x80u + g && !memcmp(bitmaps + g * GLYPH_BYTES, pl.data + pl_font + (0x80u + g) * GLYPH_BYTES, GLYPH_BYTES));

    uint8_t *image = NULL;
    size_t size = 0;
    OraclesLanguageReport report;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_POLISH, us.data, us.size, NULL, &image, &size, &report, error, sizeof error) == 0);
    if (image) {
        CHECK(report.served == count && report.english == 0 && report.missing == 0);
        CHECK(report.graphics == oracles_language_pack_graphic_count(pack, ORACLES_LANGUAGE_POLISH));
        CHECK(!memcmp(image + us_font + 0x80u * GLYPH_BYTES, pl.data + pl_font + 0x80u * GLYPH_BYTES, 9u * GLYPH_BYTES));
        CHECK(!memcmp(image + (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u + ORACLES_LANGUAGE_INDEX_CODE, "pl", 3));
        printf("%s pl composed: %u texts served, %u bytes; %u graphics, %u bytes\n", name, report.served, report.bytes, report.graphics, report.graphics_bytes);
        check_polish_graphics(name, game, pack, &us, &pl, image, size, e);
    }
    /* An image that no longer holds the US table of the title's sprites of PRESS START, its first or its last byte, is
     * refused, not written over. */
    uint8_t *other = malloc(us.size);
    CHECK(other != NULL);
    for (unsigned k = 0; other && k < 2u; k++) {
        memcpy(other, us.data, us.size);
        other[oracles_rom_offset(e->press_start_bank, e->press_start_address) + (k ? 0x28u : 0u)] ^= 0x01u;
        uint8_t *refused = NULL;
        size_t refused_size = 0;
        CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_POLISH, other, us.size, NULL, &refused, &refused_size, &report, error, sizeof error) != 0 && !refused);
        CHECK(strstr(error, "does not hold the US table") != NULL);
        free(refused);
    }
    free(other);

    /* The compact text in Polish: every text laid out again holds, as measured; Ages TX_000b's word broken at a syllable
     * by its correction joined again, TX_240a's compound kept whole with its hyphen. */
    const OraclesLanguageLayout layout = { 128, 0, 0, NULL };
    uint8_t *laid = NULL;
    size_t laid_size = 0;
    CHECK(oracles_language_compose(pack, ORACLES_LANGUAGE_POLISH, us.data, us.size, &layout, &laid, &laid_size, &report, error, sizeof error) == 0);
    if (laid) {
        const OraclesRom composed = { laid, laid_size };
        const OraclesGuestTables *t = game == ORACLES_GAME_AGES ? &oracles_guest_tables_ages : &oracles_guest_tables_seasons;
        OraclesTextFont font;
        CHECK(oracles_text_font(&composed, oracles_rom_offset(t->font_start.bank, t->font_start.addr), &font) == ORACLES_OK);
        language_wrap *wrap = language_wrap_new(&font, layout.width, ORACLES_LANGUAGE_POLISH);
        const size_t first = (size_t)ORACLES_IMAGE_LANGUAGE_BANK * 0x4000u;
        const layout_count n = count_layout(laid, laid_size, first, &us_rom, &source, oracles_text_groups(&us_rom, &source), pack, ORACLES_LANGUAGE_POLISH, wrap);
        printf("%s pl laid out for the compact text: %u texts laid out again (%u in the index), %u kept by the table or a call, %u that did not hold; "
               "%u boxes for %u; the texts of the box without a choice, a call, a secret or a jump: %u boxes for %u; %u hyphens joined\n",
               name, report.laid_out, n.flagged, report.layout_kept, report.layout_failed, report.boxes_laid_out, report.boxes, n.boxes_laid_out, n.boxes,
               n.joined);
        CHECK(wrap && n.flagged == report.laid_out && n.unheld == 0);
        CHECK(report.laid_out == e->layout.laid_out && report.layout_kept == e->layout.kept && report.layout_failed == e->layout.failed);
        CHECK(report.boxes == e->layout.boxes && report.boxes_laid_out == e->layout.boxes_laid_out && n.joined == e->layout.joined);
        if (game == ORACLES_GAME_AGES) {
            size_t laid_length = 0;
            const uint8_t *joined = laid_text(laid, laid_size, first, 0x000b, &laid_length);
            CHECK(joined && holds_words(joined, laid_length, "tera\x88niejszo\x83" "ci\x80!"));
        }
        language_wrap_free(wrap);
        free(laid);
    }

    char path[1024];
    snprintf(path, sizeof path, "%s/%s-pl.pack", work, name);
    CHECK(oracles_language_pack_write(pack, path, error, sizeof error) == 0);
    OraclesLanguagePack *back = oracles_language_pack_read(path, error, sizeof error);
    CHECK(back != NULL);
    if (back && image) {
        uint8_t *again = NULL;
        size_t again_size = 0;
        CHECK(oracles_language_compose(back, ORACLES_LANGUAGE_POLISH, us.data, us.size, NULL, &again, &again_size, &report, error, sizeof error) == 0);
        CHECK(again && again_size == size && !memcmp(again, image, size));
        free(again);
    }
    oracles_language_pack_free(back);
    remove(path);
    free(image);
    oracles_language_pack_free(pack);
    language_corrections_free(corrections);
    free(us.data);
    free(pl.data);
    return 1;
}

int main(int argc, char **argv)
{
    const char *dir = getenv("ORACLES_ROM_DIR");
    if (!dir || !*dir) { printf("language-roms: ORACLES_ROM_DIR is not set: skipped\n"); return 77; }
    unsigned checked = 0;
    for (size_t i = 0; i < sizeof games / sizeof games[0]; i++) checked += (unsigned)check_game(dir, argc > 1 ? argv[1] : ".", &games[i]);
    for (size_t i = 0; i < sizeof japanese_expected / sizeof japanese_expected[0]; i++)
        checked += (unsigned)check_japanese(dir, argc > 1 ? argv[1] : ".", &japanese_expected[i]);
    for (size_t i = 0; i < sizeof polish_expected / sizeof polish_expected[0]; i++)
        checked += (unsigned)check_polish(dir, argc > 1 ? argv[1] : ".", &polish_expected[i]);
    const unsigned combined = check_combined(dir);
    printf("Oracles Combined: %u image(s) of its release checked\n", combined);
    checked += combined;
    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    if (!failures && !checked) { printf("language-roms: no game's US and European ROMs in %s: skipped\n", dir); return 77; }
    return failures ? 1 : 0;
}
