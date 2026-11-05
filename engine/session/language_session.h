/* A session's language (--language, --eu-rom, --language-pack), for the launcher and the harness alike: its pack read
 * or extracted, composed into the image, served by the guest, and said in the report.  The files are read here, with
 * the C library; engine/language opens only the pack it is given. */
#ifndef ORACLES_LANGUAGE_SESSION_H
#define ORACLES_LANGUAGE_SESSION_H

#include "guest.h"
#include "language.h"

#include <stdio.h>

typedef struct OraclesLanguageSession OraclesLanguageSession;

/* The language `code` for the image `info` describes, read before any composition: the US ROM of Ages or Seasons, or a
 * fan game made on it from the US ROM at `base_path` (NULL for an original, or a fan game's image read whole: its US ROM
 * is then the one beside its European ROM).  Its pack read from `pack_path`, else
 * extracted from the European ROM `european` of the game (the file, or a directory where it is found by its SHA-1),
 * for a fan game from that game's pack and the two images (oracles_language_pack_derive), and written to `pack_path`
 * when one is given (a pack that cannot be written serves all the same: oracles_language_session_note says so).  A pack
 * read that is not this version's, of this game or whole is extracted again when the European ROM is given, refused
 * otherwise.  *out is NULL for English.  0, or -1 with the reason. */
int oracles_language_session_start(OraclesLanguageSession **out, const char *code, const char *european, const char *pack_path,
                                   const char *base_path, const OraclesRomInfo *info, const uint8_t *rom, size_t rom_size, char *error,
                                   size_t capacity);
/* The texts laid out again when the language is composed, by `layout` (language.h, OraclesLanguageLayout): the compact
 * text on a line of the game's text box, 128 pixels; the classic or the compact text on a line of the host's wide box,
 * 192.  For English (US), *session NULL, a session of its own, whose index serves the US texts laid out again.  The US
 * ROM of Ages or Seasons in any language, or a fan game made on it in its English alone and in the game's box alone
 * (`info`, as read).  0, or -1 with the reason. */
int oracles_language_session_lay_out(OraclesLanguageSession **session, const OraclesRomInfo *info, const OraclesLanguageLayout *layout, char *error,
                                     size_t capacity);
/* The language composed into the image (after the mods' houses), which it replaces. */
int oracles_language_session_compose(OraclesLanguageSession *session, uint8_t **rom, size_t *rom_size, char *error, size_t capacity);
/* Arms the guest's transaction on the composed image. */
int oracles_language_session_attach(OraclesLanguageSession *session, OraclesGuest *guest, char *error, size_t capacity);
/* What the player is told of a session that plays: the pack it extracted could not be written (it is extracted again
 * at the next start); "" for nothing, and without a language. */
const char *oracles_language_session_note(const OraclesLanguageSession *session);
/* "language-fr@SHA1", the language and its pack's digest, part of a savestate's identity: a state taken with another
 * pack (extracted again from other tables) is refused; then the layout's ("text-compact@SHA1" for the compact text in
 * the game's box, "text-classic-192x2@SHA1" or "text-compact-192x2@SHA1" in the wide box), a state taken with another
 * layout refused too; "" without a language or a layout. */
const char *oracles_language_session_identity(const OraclesLanguageSession *session);
/* Where the pages the layout laid out again lie in the image composed (OraclesLanguagePages), *count pairs of physical
 * offsets, from a page's first byte to past its last, in increasing order; NULL before the composition or without a
 * layout.  The host's wide box centres every other page (oracles_guest_set_text_box_pages). */
const uint32_t *oracles_language_session_laid_pages(const OraclesLanguageSession *session, unsigned *count);
/* Each apart: the language's, and the layout's, which a change of the text's settings alone changes. */
const char *oracles_language_session_language_identity(const OraclesLanguageSession *session);
const char *oracles_language_session_layout_identity(const OraclesLanguageSession *session);
/* The layout's word for an image of `revision` laid out by `layout`, as a session's identity holds it. */
void oracles_language_layout_word(const OraclesLanguageLayout *layout, OraclesRomRevision revision, char *out, size_t capacity);
/* The language and its pack, and the texts the game looked up (oracles_guest_language_counts): the end of a session's
 * report; `prefix` starts the line. */
void oracles_language_session_report(const OraclesLanguageSession *session, const OraclesGuest *guest, const char *prefix, FILE *out);
/* The graphics that carry text the composition wrote in the language (0 before it, or without a language). */
unsigned oracles_language_session_graphics(const OraclesLanguageSession *session);
/* Whether the session serves a translation: a pack's language, not English (US) laid out alone. */
int oracles_language_session_translated(const OraclesLanguageSession *session);
/* What the composition did (NULL without a language). */
const OraclesLanguageReport *oracles_language_session_composed(const OraclesLanguageSession *session);
void oracles_language_session_stop(OraclesLanguageSession *session);

#endif
