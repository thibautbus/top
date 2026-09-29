/* The launcher drawn: the home screen (the state of ui_home_nav.h at the
 * places of ui_home_layout.h) or a page (ui_page.h), with its
 * colours and transitions (the title's fade and slide, the motifs'
 * cross-fade and their slide behind a page, the highlight, the toast). */
#ifndef ORACLES_UI_HOME_H
#define ORACLES_UI_HOME_H

#include "ui_anim.h"
#include "ui_draw.h"
#include "ui_home_nav.h"
#include "ui_version.h"

typedef struct OraclesUiHome {
    OraclesUiTween hero_opacity[ORACLES_HOME_HEROES], hero_shift[ORACLES_HOME_HEROES];
    OraclesUiTween motif[ORACLES_UI_MOTIF_COUNT];
    OraclesUiTween motif_shift, motif_layer;    /* the motifs slide left and fade to half behind a page */
    OraclesUiTween highlight[ORACLES_HOME_MAX_ITEMS];
    OraclesUiTween toast;
    char toast_text[256];
    double toast_until_ms;       /* when the toast starts to fade out; 0: none shown */
} OraclesUiHome;

/* The screen at rest on the navigation's state. */
void oracles_ui_home_start(OraclesUiHome *home, const OraclesHomeNav *nav);
/* Starts the transitions towards the navigation's state after a change. */
void oracles_ui_home_follow(OraclesUiHome *home, const OraclesHomeNav *nav, double now_ms);
/* A message shown for a few seconds above the help bar. */
void oracles_ui_home_toast(OraclesUiHome *home, const char *text, double now_ms);

/* Draws the screen; returns 1 while a transition runs, for the caller to draw again. */
int oracles_ui_home_draw(OraclesUiDraw *draw, OraclesUiHome *home, const OraclesHomeNav *nav, double now_ms);
/* The next time the screen changes without input (a toast fading out), or a negative value. */
double oracles_ui_home_due_ms(const OraclesUiHome *home);

typedef enum OraclesUiHomeHitKind {
    ORACLES_UI_HIT_NONE,
    ORACLES_UI_HIT_ITEM,       /* a menu item: `index` */
    ORACLES_UI_HIT_ENTRY,      /* an entry of the left stack: `entry` */
    ORACLES_UI_HIT_BACK,       /* the help bar's hint that goes back */
    ORACLES_UI_HIT_ROW,        /* a row of a page: `index`, and one of its options: `option`, or -1 */
    ORACLES_UI_HIT_CELL        /* a place of Controls: `column`, `row`, and the Item hotkeys' Off or On: `option`, or -1 */
} OraclesUiHomeHitKind;

typedef struct OraclesUiHomeHit {
    OraclesUiHomeHitKind kind;
    unsigned index;
    int option;
    OraclesHomeEntry entry;
    int column, row;
} OraclesUiHomeHit;

/* What lies under a point of the scene. */
OraclesUiHomeHit oracles_ui_home_hit(const OraclesHomeNav *nav, float x, float y);

#endif
