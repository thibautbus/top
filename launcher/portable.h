/* The folder of the player's data (settings.txt, the mods folder, the language packs: whatever the launcher keeps beside
 * settings.txt), and the portable mode, which keeps it beside the executable rather than in the user's folder.  The
 * portable mode is on while a file portable.txt is beside the executable (portable.txt.txt too, the name Windows gives
 * a text document named portable.txt when it hides the extensions); its data is then in the folder user-data beside
 * it.  ORACLES_SETTINGS_DIR, when set, names the folder before either; without it nor the mode, the caller asks the
 * system's (SDL_GetPrefPath, which makes it, and so is never asked in the portable mode).  The mode is a desktop's:
 * Android has none, and in a macOS application (its executable inside a .app) or an AppImage (inside its image, read
 * only) the file is ignored, which the log says. */
#ifndef ORACLES_LAUNCHER_PORTABLE_H
#define ORACLES_LAUNCHER_PORTABLE_H

#include <stddef.h>

#define ORACLES_PORTABLE_MARKER "portable.txt"
#define ORACLES_PORTABLE_FOLDER "user-data"

/* 1 where the portable mode can be on for the executable in `exe_dir` (SDL_GetBasePath, with its trailing separator). */
int oracles_portable_available(const char *exe_dir);

/* 1 when the portable mode is on: available, and portable.txt (or portable.txt.txt), a file, beside the executable. */
int oracles_portable_marked(const char *exe_dir);

typedef enum OraclesUserDirKind {
    ORACLES_USER_DIR_FORCED,     /* ORACLES_SETTINGS_DIR's */
    ORACLES_USER_DIR_PORTABLE,   /* user-data beside the executable */
    ORACLES_USER_DIR_SYSTEM,     /* the system's, which the caller asks for: `out` is left empty */
    ORACLES_USER_DIR_NONE        /* a path longer than `capacity` */
} OraclesUserDirKind;

/* The folder of the player's data, with a trailing separator, into `out`: `forced` (ORACLES_SETTINGS_DIR) when set
 * and not empty; else user-data beside the executable in the portable mode, made if it is not there; else, and when
 * the portable mode's folder cannot be made or written (an executable in Program Files), which the standard error
 * says, the system's. */
OraclesUserDirKind oracles_user_dir(const char *forced, const char *exe_dir, char *out, size_t capacity);

/* What the last oracles_user_dir chose: the run's folder, which a marker put or removed since does not change. */
OraclesUserDirKind oracles_user_dir_chosen(void);

/* What turning the portable mode on or off did. */
typedef enum OraclesPortableResult {
    ORACLES_PORTABLE_COPIED,          /* turned: the data copied into the folder the next launch uses, which had none */
    ORACLES_PORTABLE_NOTHING_TO_COPY, /* turned: neither folder had data */
    ORACLES_PORTABLE_KEPT,            /* turned: the folder the next launch uses had data of its own, nothing copied */
    ORACLES_PORTABLE_UNCHANGED,       /* already so */
    ORACLES_PORTABLE_READ_ONLY,       /* the executable's folder, or user-data, cannot be written: nothing changed */
    ORACLES_PORTABLE_COPY_FAILED,     /* the data could not be copied: nothing changed */
    ORACLES_PORTABLE_UNAVAILABLE      /* no portable mode here (oracles_portable_available), or ORACLES_SETTINGS_DIR set */
} OraclesPortableResult;

/* The portable mode turned on (`on`) or off for the executable in `exe_dir`, `system_dir` the system's folder of the
 * player's data (SDL_GetPrefPath), each with its trailing separator; `next_dir` the folder the next launch uses.  On,
 * portable.txt is written beside the executable, and the data of `system_dir` copied into user-data when user-data
 * has no settings.txt; off, every portable.txt is removed, and the data of user-data copied into `system_dir` when it
 * has none.  The data is the launcher's: settings.txt, the mods folder and the language packs, never a ROM or a save;
 * a link is copied as a link, a relative one made absolute (on Windows, a link or a junction is refused).  It is
 * copied, never moved: the folder left keeps its own.  It goes first into a folder .copying inside the folder it is
 * copied to, made only if it is not there (another launcher's), then out of it, settings.txt last, without replacing a
 * language pack, a mod or a mods folder that is a link that folder already has; a mods there that is no folder fails
 * it.  A .copying that a copy cut short left is removed first, its data copied again.  When the copy, or the marker's
 * change after it, fails, what it moved is removed and the mode left as it was.  `error`, on READ_ONLY and
 * COPY_FAILED, says what failed. */
OraclesPortableResult oracles_portable_set(int on, const char *exe_dir, const char *system_dir, char *next_dir, size_t next_capacity, char *error,
                                           size_t capacity);

/* oracles_portable_set for this executable (SDL_GetBasePath) and the system's folder (SDL_GetPrefPath); `next_dir`
 * the folder the next launch uses, which oracles_settings_portable_turned (settings.h) takes. */
OraclesPortableResult oracles_portable_switch(int on, char *next_dir, size_t next_capacity, char *error, size_t capacity);

/* A .copying a copy cut short left in the folder `dir`, at a launch, in the folder it reads: one that holds settings.txt
 * is a whole copy cut while it was moved out, and its move is finished; another is removed. */
void oracles_portable_tidy(const char *dir);

#endif
