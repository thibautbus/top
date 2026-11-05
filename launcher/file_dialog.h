/* The system's dialogs the launcher opens: SDL 3's file dialog (Windows'
 * own, macOS's, the desktop portal or zenity under Linux, Android's document
 * picker), and the system's file manager on a folder (SDL_OpenURL, which
 * macOS opens as `open` does).  Without a dialog the caller says that a file
 * dropped on the window does the same. */
#ifndef ORACLES_LAUNCHER_FILE_DIALOG_H
#define ORACLES_LAUNCHER_FILE_DIALOG_H

#include <stddef.h>

struct SDL_Window;
union SDL_Event;

typedef enum OraclesDialogResult {
    ORACLES_DIALOG_CHOSEN,
    ORACLES_DIALOG_CANCELLED,
    ORACLES_DIALOG_UNAVAILABLE,    /* no dialog on this system, or one that could not open */
    ORACLES_DIALOG_FAILED          /* a file chosen that could not be copied into the application's folder (Android) */
} OraclesDialogResult;

/* What a dialog chooses: a ROM, a fan game's BPS patch, an Oracle's European ROM, a save to import (read where it is,
 * never copied), or where a copy of the save goes, or of the game with mods' save (Android's document picker,
 * oracles_choose_export). */
typedef enum OraclesDialogKind {
    ORACLES_DIALOG_ROM,
    ORACLES_DIALOG_PATCH,
    ORACLES_DIALOG_EU_ROM,
    ORACLES_DIALOG_SAVE,
    ORACLES_DIALOG_EXPORT,
    ORACLES_DIALOG_EXPORT_MODS
} OraclesDialogKind;

/* Opens the system's dialog for a file of `kind` for game `game`, starting in `folder` (may be empty), over `window`,
 * and returns at once: the answer comes later, as an event of SDL's queue (oracles_dialog_answer), from whatever
 * thread the system answers on. */
void oracles_choose_file(struct SDL_Window *window, OraclesDialogKind kind, const char *folder, int game);

/* Android's document picker making a document named `name` for a copy of game `game`'s save (`kind`
 * ORACLES_DIALOG_EXPORT) or of its game with mods' save (ORACLES_DIALOG_EXPORT_MODS), and returning at once: the answer
 * comes as oracles_choose_file's does, of that kind, its path the document's content:// address.  UNAVAILABLE at once
 * elsewhere, where Open folder shows the save. */
void oracles_choose_export(const char *name, OraclesDialogKind kind, int game);

/* 1 when `event` is a dialog's answer: its result, the kind and the game it was opened for and the path chosen (empty
 * unless CHOSEN), *copied set when that path is a copy made for this answer (Android), which the caller removes if it
 * refuses the file; the answer's memory is released.  0 for any other event. */
int oracles_dialog_answer(const union SDL_Event *event, OraclesDialogResult *result, OraclesDialogKind *kind, int *game,
                          char *path, size_t capacity, int *copied);

/* A file a dialog chose, as the page names it: its name and its folder; on Android, a content:// address's name as the
 * picker shows it, and the folder oracles_dialog_uri_folder reads in the address. */
void oracles_dialog_describe(const char *path, char *name, size_t name_capacity, char *folder, size_t folder_capacity);
/* The folder of the shared storage a content:// address says its document is in, from the storage's root: "Download"
 * for "primary:Download/save.sav", for the downloads' "raw:/storage/emulated/0/Download/save.sav" and for their other
 * documents, and for "file:///storage/emulated/0/Download/save.sav"; empty when the address names none (a cloud
 * drive's, a file at the storage's root) or is neither.  A '%' without two hexadecimal digits stays as it is. */
void oracles_dialog_uri_folder(const char *uri, char *folder, size_t capacity);

/* Keeps `size` bytes as `name` in `folder` (created if need be) without ever writing over a file: a file of that name
 * holding the same bytes is taken as it is (*fresh 0); else the first free name of "name", "name (2)"... before the
 * extension, written under a temporary name then renamed (*fresh 1).  1 with its path, else 0 with SDL's reason. */
int oracles_dialog_store_copy(const char *folder, const char *name, const void *data, size_t size, char *path, size_t capacity,
                              int *fresh);

/* Opens a folder in the system's file manager. Returns 0, or -1 with SDL's reason in *error. */
int oracles_open_folder(const char *folder, char *error, size_t capacity);

#endif
