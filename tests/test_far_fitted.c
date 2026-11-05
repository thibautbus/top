/* The far view fitted to the screen without a ROM: the rule over the screens of the devices the project runs on (each
 * the size its maker gives, held landscape), the sizes the view takes, each level's surface on those screens with
 * far=fixed and a shape named (as the session, the harness and Display take it), the camera framing the fitted bands outdoors on
 * a synthetic core, and the refusal of a savestate between far's fixed and fitted sizes, naming the setting. */
#include "camera.h"
#include "compositor.h"
#include "guest_struct_offsets.h"
#include "smooth_camera.h"
#include "view.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

typedef struct screen {
    const char *devices;
    unsigned w, h;
    OraclesEnhancedFit fit;
    unsigned size_w, size_h;           /* the far view's surface there */
    unsigned computed_w, computed_h;   /* the size the scale gives, kept or not; 0 where far fills the screen */
} screen;

static const screen screens[] = {
    { "RG DS, RG405V, RG35XX, Miyoo Mini Plus", 640, 480, ORACLES_ENHANCED_FIT_SMALL, 480, 360, 320, 240 },
    { "RG34XX (3:2)", 720, 480, ORACLES_ENHANCED_FIT_SMALL, 480, 360, 360, 240 },
    { "RG Cube (1:1)", 720, 720, ORACLES_ENHANCED_FIT_SMALL, 480, 360, 360, 360 },
    { "Miyoo Flip", 750, 560, ORACLES_ENHANCED_FIT_SMALL, 480, 360, 374, 280 },
    { "RG406H", 960, 720, ORACLES_ENHANCED_FIT_FILLS, 480, 360, 0, 0 },
    { "RG DS Plus", 1024, 768, ORACLES_ENHANCED_FIT_FILLS, 480, 360, 0, 0 },
    { "Retroid Pocket Nova, RG477M, Retroid Pocket Mini", 1280, 960, ORACLES_ENHANCED_FIT_FITTED, 426, 320, 426, 320 },
    { "Retroid Pocket Mini V2, Retroid Pocket Classic, AYN Thor's bottom screen", 1240, 1080, ORACLES_ENHANCED_FIT_FITTED, 412, 360, 412, 360 },
    { "the same, held upright", 1080, 1240, ORACLES_ENHANCED_FIT_FITTED, 412, 360, 412, 360 },
    { "Retroid Pocket 4 Pro", 1334, 750, ORACLES_ENHANCED_FIT_UNLISTED, 480, 270, 444, 250 },
    { "Steam Deck (16:10)", 1280, 800, ORACLES_ENHANCED_FIT_UNLISTED, 480, 270, 426, 266 },
    { "TrimUI Smart Pro", 1280, 720, ORACLES_ENHANCED_FIT_UNLISTED, 480, 270, 426, 240 },
    { "Retroid Pocket 5, AYN Odin 2, 2 Mini, 2 Portal, 3, AYN Thor's top screen, RG556", 1920, 1080, ORACLES_ENHANCED_FIT_FILLS, 480, 270, 0, 0 },
    { "Galaxy S24", 2340, 1080, ORACLES_ENHANCED_FIT_FILLS, 480, 270, 0, 0 },
    { "Pixel 9", 2424, 1080, ORACLES_ENHANCED_FIT_SMALL, 480, 270, 404, 180 },
    { "a 1440x1080 monitor", 1440, 1080, ORACLES_ENHANCED_FIT_FILLS, 480, 360, 0, 0 },
    { "a 1600x1200 monitor", 1600, 1200, ORACLES_ENHANCED_FIT_FILLS, 480, 360, 0, 0 },
    { "a 5:4 monitor", 1280, 1024, ORACLES_ENHANCED_FIT_UNLISTED, 480, 360, 426, 340 },
    { "a 16:10 monitor", 1920, 1200, ORACLES_ENHANCED_FIT_FILLS, 480, 270, 0, 0 },
    { "a 1440p monitor", 2560, 1440, ORACLES_ENHANCED_FIT_FILLS, 480, 270, 0, 0 },
    { "a 2560x1600 monitor", 2560, 1600, ORACLES_ENHANCED_FIT_UNLISTED, 480, 270, 426, 266 },
};

static int size_is(OraclesEnhancedSize s, unsigned w, unsigned h) { return s.width == w && s.height == h; }

static void rule(void)
{
    unsigned listed[ORACLES_ENHANCED_FITTED_SIZES] = { 0 };
    for (size_t i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        const screen *s = &screens[i];
        const OraclesEnhancedFitting f = oracles_enhanced_far_fitting(s->w, s->h);
        const int ok = f.fit == s->fit && f.size.width == s->size_w && f.size.height == s->size_h
                       && f.computed.width == s->computed_w && f.computed.height == s->computed_h;
        if (!ok) fprintf(stderr, "%ux%u (%s): fit %d, %ux%u, computed %ux%u\n", s->w, s->h, s->devices, (int)f.fit, f.size.width, f.size.height,
                         f.computed.width, f.computed.height);
        CHECK(ok);
        /* Far's fixed size where it is not fitted: the screen's shape, as the launcher takes it. */
        CHECK(f.fit == ORACLES_ENHANCED_FIT_FITTED || (f.size.width == oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, f.aspect).width
                                                       && f.size.height == oracles_enhanced_view_size(ORACLES_ENHANCED_FAR, f.aspect).height));
        int fitted = 0;
        CHECK(oracles_enhanced_size_known(f.size, &fitted) && fitted == (f.fit == ORACLES_ENHANCED_FIT_FITTED));
        for (unsigned n = 0; n < ORACLES_ENHANCED_FITTED_SIZES; n++)
            if (f.fit == ORACLES_ENHANCED_FIT_FITTED && f.size.width == oracles_enhanced_fitted_size(n).width && f.size.height == oracles_enhanced_fitted_size(n).height) listed[n]++;
    }
    /* Every size the view takes fitted is a screen's of the table. */
    for (unsigned n = 0; n < ORACLES_ENHANCED_FITTED_SIZES; n++) CHECK(listed[n] > 0);
    /* The scales, and the part of the screen far covers. */
    OraclesEnhancedFitting f = oracles_enhanced_far_fitting(1280, 960);
    CHECK(f.scale == 3 && f.far_scale == 2 && f.far_percent == 56 && f.aspect == ORACLES_ENHANCED_4_3);
    f = oracles_enhanced_far_fitting(1240, 1080);
    CHECK(f.scale == 3 && f.far_scale == 2 && f.far_percent == 51 && f.aspect == ORACLES_ENHANCED_4_3);
    f = oracles_enhanced_far_fitting(1920, 1080);
    CHECK(f.far_scale == 4 && f.far_percent == 100 && f.aspect == ORACLES_ENHANCED_16_9);
    f = oracles_enhanced_far_fitting(0, 0);
    CHECK(f.fit == ORACLES_ENHANCED_FIT_FILLS && f.size.width == 480 && f.size.height == 270);
    /* The sizes the view takes: the levels' and the fitted ones, no other. */
    int fitted = 1;
    CHECK(oracles_enhanced_size_known((OraclesEnhancedSize){ 480, 360 }, &fitted) && !fitted);
    CHECK(oracles_enhanced_size_known((OraclesEnhancedSize){ 213, 160 }, &fitted) && !fitted);
    CHECK(!oracles_enhanced_size_known((OraclesEnhancedSize){ 444, 250 }, NULL) && !oracles_enhanced_size_known((OraclesEnhancedSize){ 426, 266 }, NULL));
    /* The smooth camera frames their bands at their middle. */
    CHECK(oracles_e11_viewport_valid(426 * ORACLES_E11_F256, 213 * ORACLES_E11_F256) && oracles_e11_viewport_valid(412 * ORACLES_E11_F256, 206 * ORACLES_E11_F256));
    CHECK(!oracles_e11_viewport_valid(444 * ORACLES_E11_F256, 222 * ORACLES_E11_F256));
    /* The log says what far takes and why. */
    char words[192];
    oracles_enhanced_far_fitting_words(1280, 960, 0, words, sizeof words);
    CHECK(!strcmp(words, "the far view fitted to the 1280x960 screen: 426x320 at 3x, where 480x360 covers 56% of it at 2x"));
    oracles_enhanced_far_fitting_words(1334, 750, 0, words, sizeof words);
    CHECK(!strcmp(words, "the far view at 480x270: the 1334x750 screen at 3x gives 444x250, not a size the view takes"));
    oracles_enhanced_far_fitting_words(640, 480, 0, words, sizeof words);
    CHECK(!strcmp(words, "the far view at 480x360: the 640x480 screen at 2x gives 320x240, under 400x224 or taller than wide"));
    oracles_enhanced_far_fitting_words(1920, 1080, 0, words, sizeof words);
    CHECK(!strcmp(words, "the far view at 480x270: it covers 100% of the 1920x1080 screen at 4x"));
    /* With a screen margin, far fits to the band it leaves: on 1280x960, Small's 120 lines leave 1280x840, where 426x320
     * at three times does not fit, and far keeps 480x360 at twice, as it does without fitting; the shape stays the
     * screen's.  1240x1080 likewise; a margin that leaves the lines the fitted size needs keeps it fitted. */
    f = oracles_enhanced_far_fitting_band(1280, 960, 120);
    CHECK(f.fit == ORACLES_ENHANCED_FIT_UNLISTED && size_is(f.size, 480, 360) && size_is(f.computed, 426, 280) && f.far_scale == 2 && f.aspect == ORACLES_ENHANCED_4_3);
    f = oracles_enhanced_far_fitting_band(1240, 1080, 48);
    CHECK(f.fit == ORACLES_ENHANCED_FIT_UNLISTED && size_is(f.size, 480, 360) && size_is(f.computed, 412, 344));
    f = oracles_enhanced_far_fitting_band(1280, 960, 0);
    CHECK(f.fit == ORACLES_ENHANCED_FIT_FITTED && size_is(f.size, 426, 320));
    CHECK(size_is(oracles_enhanced_screen_view_size(ORACLES_ENHANCED_FAR, ORACLES_ENHANCED_ASPECTS, 0, 1280, 960, 120), 480, 360));
    CHECK(size_is(oracles_enhanced_screen_view_size(ORACLES_ENHANCED_FAR, ORACLES_ENHANCED_ASPECTS, 0, 1280, 960, 0), 426, 320));
    oracles_enhanced_far_fitting_words(1280, 960, 120, words, sizeof words);
    CHECK(!strcmp(words, "the far view at 480x360: the 1280x840 band the margin leaves on the 1280x960 screen at 3x gives 426x280, not a size the view takes"));
}

/* The view's surface on a screen, as the session, the harness and Display take it: the screen's shape for every level
 * of every screen of the table, far fitted where the rule fits it; far=fixed keeps far's size in the screen's shape; a
 * shape named keeps the level's size in it whatever the screen; no screen is 16:9. */
static void screen_view_sizes(void)
{
    for (size_t i = 0; i < sizeof screens / sizeof screens[0]; i++) {
        const screen *s = &screens[i];
        const OraclesEnhancedAspect shape = oracles_enhanced_screen_aspect(s->w, s->h);
        for (unsigned l = 0; l < ORACLES_ENHANCED_LEVELS; l++) {
            const OraclesEnhancedSize level = oracles_enhanced_view_size((OraclesEnhancedLevel)l, shape);
            const OraclesEnhancedSize size = oracles_enhanced_screen_view_size((OraclesEnhancedLevel)l, ORACLES_ENHANCED_ASPECTS, 0, s->w, s->h, 0);
            CHECK(l == ORACLES_ENHANCED_FAR ? size_is(size, s->size_w, s->size_h) : size_is(size, level.width, level.height));
            const OraclesEnhancedSize fixed = oracles_enhanced_screen_view_size((OraclesEnhancedLevel)l, ORACLES_ENHANCED_ASPECTS, 1, s->w, s->h, 0);
            CHECK(size_is(fixed, level.width, level.height));
            for (unsigned a = 0; a < ORACLES_ENHANCED_ASPECTS; a++) {
                const OraclesEnhancedSize named = oracles_enhanced_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a);
                CHECK(size_is(oracles_enhanced_screen_view_size((OraclesEnhancedLevel)l, (OraclesEnhancedAspect)a, 0, s->w, s->h, 0), named.width, named.height));
            }
        }
    }
    CHECK(size_is(oracles_enhanced_screen_view_size(ORACLES_ENHANCED_FAR, ORACLES_ENHANCED_ASPECTS, 0, 0, 0, 0), 480, 270));
    CHECK(size_is(oracles_enhanced_screen_view_size(ORACLES_ENHANCED_NEAR, ORACLES_ENHANCED_ASPECTS, 0, 0, 0, 0), 256, 144));
    /* The shape's rule at its edges: 3:2 and a square are 4:3, 16:10 and 5:3 are 16:9, a screen held upright by its turn. */
    CHECK(oracles_enhanced_screen_aspect(720, 480) == ORACLES_ENHANCED_4_3 && oracles_enhanced_screen_aspect(720, 720) == ORACLES_ENHANCED_4_3);
    CHECK(oracles_enhanced_screen_aspect(1280, 800) == ORACLES_ENHANCED_16_9 && oracles_enhanced_screen_aspect(1280, 768) == ORACLES_ENHANCED_16_9);
    CHECK(oracles_enhanced_screen_aspect(1080, 1240) == ORACLES_ENHANCED_4_3 && oracles_enhanced_screen_aspect(1080, 2400) == ORACLES_ENHANCED_16_9);
}

static const OraclesCompatProfile *ages_profile(void)
{
    const OraclesRomInfo info = { .game = ORACLES_GAME_AGES, .revision = ORACLES_ROM_REVISION_AGES_US, .known = 1 };
    return oracles_compat_find(&info);
}

/* A synthetic core running `jr -2`, whose WRAM the test writes as the game would. */
typedef struct rig { uint8_t *rom; OraclesCore *core; OraclesGuest *guest; } rig;
static int rig_start(rig *r)
{
    const size_t size = 1024u * 1024u;
    r->rom = calloc(size, 1);
    memcpy(r->rom + 0x134, "ZELDA NAYRU", 11);
    r->rom[0x143] = 0xc0; r->rom[0x147] = 0x1b; r->rom[0x148] = 0x05; r->rom[0x149] = 0x02;
    r->rom[0x100] = 0x00; r->rom[0x101] = 0xc3; r->rom[0x102] = 0x50; r->rom[0x103] = 0x01;
    r->rom[0x150] = 0x18; r->rom[0x151] = 0xfe;
    const OraclesCoreOptions options = { 0, 0, ORACLES_CORE_SAMEBOY, 0 };
    r->core = oracles_core_create(r->rom, size, &options);
    r->guest = r->core ? oracles_guest_attach(r->core, ages_profile()) : NULL;
    return r->guest != NULL;
}
static void rig_stop(rig *r)
{
    if (r->guest) oracles_guest_detach(r->guest);
    if (r->core) oracles_core_destroy(r->core);
    free(r->rom);
}

static OraclesEnhancedView *view_of(rig *r, unsigned w, unsigned h)
{
    OraclesEnhancedView *v = oracles_enhanced_view_start(r->core, r->guest, NULL, 0);
    if (v) oracles_enhanced_view_set_size(v, (OraclesEnhancedSize){ w, h });
    return v;
}

static void views_and_savestates(void)
{
    rig r;
    memset(&r, 0, sizeof r);
    CHECK(rig_start(&r));
    if (!r.guest) { rig_stop(&r); return; }
    OraclesEnhancedView *nova = view_of(&r, 426, 320), *mini = view_of(&r, 412, 360), *far_4_3 = view_of(&r, 480, 360);
    OraclesEnhancedView *far_16_9 = view_of(&r, 480, 270), *near = view_of(&r, 256, 144), *unlisted = view_of(&r, 444, 250);
    CHECK(nova && mini && far_4_3 && far_16_9 && near && unlisted);
    if (!nova || !mini || !far_4_3 || !far_16_9 || !near || !unlisted) goto done;
    CHECK(oracles_enhanced_view_width(nova) == 426 && oracles_enhanced_view_height(nova) == 320);
    CHECK(oracles_enhanced_view_width(mini) == 412 && oracles_enhanced_view_height(mini) == 360);
    CHECK(oracles_enhanced_view_width(unlisted) == 256 && oracles_enhanced_view_height(unlisted) == 144);   /* refused: the view keeps its size */

    /* Outdoors the whole band is shown, Link at its middle, an odd half included (213). */
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    uint8_t *w0 = oracles_guest_wram_writable(r.guest, 0), *w1 = oracles_guest_wram_writable(r.guest, 1);
    uint8_t *link = w1 + (ORACLES_OBJECTS_BASE - 0xd000u);
    #define W0(sym) w0[(sym).addr - 0xc000u]
    W0(t->game_state) = 2; W0(t->cutscene_index) = 1; W0(t->scroll_mode) = 1; W0(t->screen_transition_state) = 2;
    W0(t->active_group) = 0; W0(t->active_room) = 0x45; W0(t->room_is_large) = 0;
    #undef W0
    link[ORACLES_OBJ_ENABLED] = 1; link[ORACLES_OBJ_STATE] = 1; link[ORACLES_OBJ_XH] = 80; link[ORACLES_OBJ_YH] = 64;
    OraclesEnhancedView *const fitted[2] = { nova, mini };
    for (unsigned i = 0; i < 2u; i++) {
        OraclesEnhancedMode mode = ORACLES_ENHANCED_FRAMED;
        int32_t cam = 0, cam_y = 0;
        for (unsigned n = 0; n < 40u; n++) { oracles_core_run_frame(r.core); oracles_enhanced_view_compose(fitted[i], &mode, &cam, &cam_y); }
        unsigned shown_w = 0, shown_h = 0;
        oracles_enhanced_view_shown(fitted[i], &shown_w, &shown_h);
        const int32_t w = (int32_t)oracles_enhanced_view_width(fitted[i]), band = (int32_t)oracles_enhanced_view_height(fitted[i]) - 16;
        CHECK(mode == ORACLES_ENHANCED_WORLD && shown_w == (unsigned)w && shown_h == (unsigned)band);
        CHECK(cam == 5 * 160 + 80 - w / 2 && cam_y == 4 * 128 + 64 - band / 2);
    }

    /* A state loads only on its own surface; the refusal names the setting that shows the one it was taken on. */
    uint8_t wire[1024];
    size_t written = 0;
    char why[256];
    CHECK(oracles_enhanced_view_save_state(nova, wire, sizeof wire, &written) == 0);
    CHECK(oracles_enhanced_view_check_state(nova, wire, written, why, sizeof why) == 0);
    CHECK(oracles_enhanced_view_check_state(far_4_3, wire, written, why, sizeof why) != 0);
    CHECK(!strcmp(why, "the savestate was taken on the far view fitted to the screen, 426x320, and this session shows the far view, 480x360 in 4:3: "
                       "it loads only on its own surface; far=screen and aspect=auto in the settings show that surface on the screen it was taken on"));
    CHECK(oracles_enhanced_view_check_state(mini, wire, written, why, sizeof why) != 0);   /* another screen's */
    CHECK(!strcmp(why, "the savestate was taken on the far view fitted to the screen, 426x320, and this session shows the far view fitted to the screen, "
                       "412x360: it loads only on its own surface"));
    CHECK(oracles_enhanced_view_check_state(near, wire, written, why, sizeof why) != 0 && !strstr(why, "settings"));
    /* The pause's words: the view it was taken in, and Display's choice that shows it.  A state of far fitted to
     * 1280x960 in far's fixed size: Fitted where far=fixed kept it, Screen margin where Small's 120 lines did, the
     * shape where aspect= named it, else another screen (a monitor far never fits). */
    char message[256];
    const OraclesEnhancedFarChoice fixed = { 1, 0, 0, 0, 0 }, margin = { 0, 0, 1280, 960, 120 }, named = { 0, 1, 0, 0, 0 }, monitor = { 0, 0, 1920, 1080, 0 };
    oracles_enhanced_view_state_message(far_4_3, wire, written, &fixed, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320, fitted to its screen: choose Fitted in Display \xe2\x80\xba Picture \xe2\x80\xba Far size from the home screen"));
    oracles_enhanced_view_state_message(far_4_3, wire, written, &margin, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320: choose Off in Display \xe2\x80\xba Picture \xe2\x80\xba Screen margin from the home screen"));
    oracles_enhanced_view_state_message(far_4_3, wire, written, &named, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320, fitted to its screen: aspect=auto in settings.txt shows it"));
    oracles_enhanced_view_state_message(far_4_3, wire, written, &monitor, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320, fitted to another screen"));
    oracles_enhanced_view_state_message(far_4_3, wire, written, NULL, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320, fitted to another screen"));
    oracles_enhanced_view_state_message(mini, wire, written, NULL, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 426\xc3\x97" "320, fitted to another screen"));
    oracles_enhanced_view_state_message(near, wire, written, NULL, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far: choose Far in Display's View from the home screen"));
    CHECK(oracles_enhanced_view_save_state(far_4_3, wire, sizeof wire, &written) == 0);
    CHECK(oracles_enhanced_view_check_state(nova, wire, written, why, sizeof why) != 0);
    CHECK(!strcmp(why, "the savestate was taken on the far view, 480x360 in 4:3, and this session shows the far view fitted to the screen, 426x320: "
                       "it loads only on its own surface; far=fixed in the settings shows that surface"));
    oracles_enhanced_view_state_message(nova, wire, written, NULL, message, sizeof message);   /* a 1.1.0 state of Far on a 1280x960 screen */
    CHECK(!strcmp(message, "Refused: taken in Far at 480\xc3\x97" "360: choose Fixed in Display \xe2\x80\xba Picture \xe2\x80\xba Far size from the home screen"));
    CHECK(oracles_enhanced_view_load_state(nova, wire, written) != 0);
    CHECK(oracles_enhanced_view_save_state(far_16_9, wire, sizeof wire, &written) == 0);
    CHECK(oracles_enhanced_view_check_state(mini, wire, written, why, sizeof why) != 0 && strstr(why, "; aspect=16:9 in the settings shows that surface"));
    oracles_enhanced_view_state_message(mini, wire, written, NULL, message, sizeof message);
    CHECK(!strcmp(message, "Refused: taken in Far at 480\xc3\x97" "270: aspect=16:9 in settings.txt shows it"));
done:
    oracles_enhanced_view_stop(nova); oracles_enhanced_view_stop(mini); oracles_enhanced_view_stop(far_4_3);
    oracles_enhanced_view_stop(far_16_9); oracles_enhanced_view_stop(near); oracles_enhanced_view_stop(unlisted);
    rig_stop(&r);
}

int main(void)
{
    rule();
    screen_view_sizes();
    views_and_savestates();
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_far_fitted: ok\n");
    return 0;
}
