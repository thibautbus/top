/* A game's save brought in from another file, and a copy of it taken out (Cartridge's Save row).  A file comes in as
 * the save of a game when it is a save of the game's engine, Ages' or Seasons' (a fan game's, its Oracle's): an
 * SRAM image of the size the game's has, one copy of a file at least valid (save.h).  An Oracle's has its language set
 * to English, which a save of the European ROM played in German, Italian or Spanish needs; a fan game's is taken as it
 * is.  The save it replaces is kept in a Backups folder beside it, under its name and the time, never over another
 * file, and its savestate moved there with it, as are the save of the game with mods, its savestate and its mods'
 * storage, which the next game with mods then starts again from the new save; the new one is written under a temporary
 * name, then renamed over the save. */
#ifndef ORACLES_LAUNCHER_SAVE_TRANSFER_H
#define ORACLES_LAUNCHER_SAVE_TRANSFER_H

#include "save.h"

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/* The most a file chosen as a save is read of: a save and a byte, which tells a larger file. */
#define ORACLES_SAVE_TRANSFER_READ (ORACLES_SAVE_SIZE + 1u)

/* The file at `path` (on Android, its content:// address) into `out`, ORACLES_SAVE_TRANSFER_READ bytes, *size the bytes
 * read: a file larger than a save (a video, a disc image chosen by mistake) refused unread, with "larger than a save"
 * in `error`, and never more than ORACLES_SAVE_TRANSFER_READ bytes read of one whose size is not known.  1, or 0 with
 * the reason in `error`. */
int oracles_save_transfer_read(const char *path, uint8_t *out, size_t *size, char *error, size_t error_capacity);

/* The `size` bytes of a file as the save of a game on `engine`, into `out` (ORACLES_SAVE_SIZE bytes): 1 when they are
 * one, *english then 1 when `oracle` set a language other than English to it; 0 when they are not one. */
int oracles_save_transfer_check(const uint8_t *data, size_t size, OraclesGame engine, int oracle, uint8_t *out, int *english);

/* What an import kept in Backups, each path empty when there was no such file: the save it replaced, its savestate, and
 * the game with mods' save, savestate and mods' storage. */
#define ORACLES_SAVE_TRANSFER_PATH 4352
typedef struct OraclesSaveKept {
    char save[ORACLES_SAVE_TRANSFER_PATH], state[ORACLES_SAVE_TRANSFER_PATH];
    char mods_save[ORACLES_SAVE_TRANSFER_PATH], mods_state[ORACLES_SAVE_TRANSFER_PATH], mods_store[ORACLES_SAVE_TRANSFER_PATH];
} OraclesSaveKept;

/* The save at `save` copied into its folder's Backups, as "NAME YYYY-MM-DD HH-MM.sav" at the local time `now` (or with
 * " (2)"... before the extension when a file of that name is there and holds other bytes); then moved there likewise
 * the files that hold the SRAM it replaces: its savestate (NAME.state beside NAME.sav, whichever core took it,
 * "NAME YYYY-MM-DD HH-MM.state"), and the game with mods' save, savestate and mods' storage (NAME.mods.sav,
 * NAME.mods.state, NAME.mods.store, "NAME.mods YYYY-MM-DD HH-MM.sav"...); then `data` written as the save.  *kept says
 * where each went.  1, or 0 with the reason in `error`: the save replaced only once all are in Backups, those moved put
 * back when it cannot be. */
int oracles_save_transfer_import(const char *save, const uint8_t *data, size_t size, time_t now, OraclesSaveKept *kept, char *error,
                                 size_t error_capacity);

/* The file beside the save at `save` that `extension` names: NAME.mods.sav for ".mods.sav" beside NAME.sav. */
void oracles_save_transfer_beside(const char *save, const char *extension, char *out, size_t capacity);

/* The file an export of the save at `save` copies: the save, or with `mods` the game with mods' save beside it
 * (NAME.mods.sav); and 1 when that save is there. */
void oracles_save_transfer_exported(const char *save, int mods, char *out, size_t capacity);
int oracles_save_transfer_has_mods_save(const char *save);

/* 1 when the game of the save at `save` played with mods has a file an import moves to Backups: its save, savestate or
 * mods' storage (NAME.mods.sav, NAME.mods.state, NAME.mods.store). */
int oracles_save_transfer_has_mods(const char *save);

/* The save at `save` copied to `destination` (on Android, the content:// address the document picker made).  1, or 0
 * with the reason in `error`. */
int oracles_save_transfer_export(const char *save, const char *destination, char *error, size_t error_capacity);

#endif
