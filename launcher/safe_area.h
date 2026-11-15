/* The window's safe area, the part of it the system does not cover (a status bar shown, a display cutout, a rounded
 * corner, the gesture areas), as SDL reports it, said on the standard error: what a player's log needs to tell what
 * the system took of a screen. */
#ifndef ORACLES_LAUNCHER_SAFE_AREA_H
#define ORACLES_LAUNCHER_SAFE_AREA_H

#include "touch_controls.h"

#include <stddef.h>

struct SDL_Window;
union SDL_Event;

/* One line: SDL's safe area in the window, the window's size in points and pixels, its state, its display's bounds
 * and usable bounds, the screen margin with what it was read from and the band it leaves (oracles_touch_band), and on
 * Android what the activity's window insets are by kind (OraclesActivity.safeArea). `who` and `when` name the moment
 * ("home screen", "opened"). */
void oracles_safe_area_report(struct SDL_Window *window, const char *who, const char *when, const OraclesTouchEdges *edges);
/* The same for an event that may change it: the safe area's, the display's or its scale's, the window's display
 * turned, a resize and the focus where the window is always fullscreen (Android); a change of the safe area
 * elsewhere, when it is not the whole window.  A change of the safe area, of the display, of its scale or of its
 * orientation reads `edges` again, for their margin, before: 1 then, else 0. */
int oracles_safe_area_event(struct SDL_Window *window, const char *who, const union SDL_Event *event, OraclesTouchEdges *edges);
/* What screen_margin= `margin` (an OraclesScreenMargin) keeps clear of on the display of `window`: with off, nothing is
 * read and the band is the whole screen.  Otherwise the display cutout's insets at the window's top and bottom, in
 * pixels (OraclesActivity.cutoutInsets): a notch or a curved edge above or below the picture; neither the gesture areas
 * nor the bars, hidden in fullscreen, count, nor a cutout at a side, which a centred frame leaves clear; 0 and 0
 * elsewhere than on Android.  And the display's pixels per dp: on Android its DisplayMetrics' density
 * (OraclesActivity.displayDensity), elsewhere its content scale.  JNI calls: read when a screen opens, when the margin
 * changes and when SDL says the safe area or the display changed, never at each frame. */
void oracles_safe_area_edges(struct SDL_Window *window, int margin, OraclesTouchEdges *edges);
/* Android: the text a static method of the activity returns ("()Ljava/lang/String;"), else an empty one. */
void oracles_android_activity_text(const char *method, char *out, size_t capacity);
/* Android: the window kept out of screenshots and of the recent applications' thumbnails (FLAG_SECURE), or back; elsewhere
 * nothing. */
void oracles_android_secure_window(int on);

#endif
