/* The files a player keeps a ROM or a patch in: the file itself, or a .zip holding it.
 *
 * A zip the player keeps a ROM in holds one file ending in .gbc or .gb, a zip of a patch one ending in .bps (any
 * case), stored or deflated; the others beside it (a readme, macOS's __MACOSX/ folder) are left aside.  Zip64 and
 * encrypted entries are refused, and so are 7z and RAR archives, which the port does not read. */
#ifndef ORACLES_ZIP_H
#define ORACLES_ZIP_H

#include <stddef.h>
#include <stdint.h>

/* What a reader takes out of a zip: a ROM (.gbc, .gb) or a BPS patch (.bps). */
typedef enum OraclesZipKind { ORACLES_ZIP_ROM, ORACLES_ZIP_PATCH } OraclesZipKind;

/* Whether `data` begins as a zip file does (a local file header). */
int oracles_zip_is_zip(const uint8_t *data, size_t size);

/* Whether the zip `zip` holds a patch rather than a ROM, read from its directory alone, nothing decompressed: 1 when
 * the ROMs and patches it names are all .bps files, 0 when a ROM is among them, -1 when it names neither or its
 * directory cannot be read. */
int oracles_zip_holds_patch(const uint8_t *zip, size_t size);

/* Whether the file at `path` is a patch rather than a ROM, nothing decompressed: 1 when its name ends in .bps, it
 * begins as a BPS patch does, or it is a zip that holds a patch; 0 for a zip that holds a ROM, or another file read;
 * -1 when it cannot tell: a zip that names neither or whose directory cannot be read, a file it does not read (a 7z or
 * RAR archive, a file larger than a patch, one that does not open). */
int oracles_zip_is_patch_file(const char *path);

/* The bytes of the zip's one file of kind `kind`, at most `max_size` of them, their CRC-32 checked; the caller frees
 * them.  NULL and the reason otherwise. */
uint8_t *oracles_zip_extract(const uint8_t *zip, size_t size, OraclesZipKind kind, size_t max_size, size_t *out_size,
                             char *error, size_t error_capacity);

/* The file at `path` read whole, at most `max_size` bytes, or for a zip the one file of kind `kind` inside it; the
 * caller frees it.  NULL and the reason otherwise. */
uint8_t *oracles_zip_read_file(const char *path, OraclesZipKind kind, size_t max_size, size_t *size, char *error,
                               size_t error_capacity);

#endif
