/* The one ROM or patch of a .zip file.
 *
 * A zip the player keeps a ROM in holds one file ending in .gbc, .gb or .bps (any case), stored or deflated; the
 * others beside it (a readme, macOS's __MACOSX/ folder) are left aside.  Zip64 and encrypted entries are refused. */
#ifndef ORACLES_ZIP_H
#define ORACLES_ZIP_H

#include <stddef.h>
#include <stdint.h>

/* Whether `data` begins as a zip file does (a local file header). */
int oracles_zip_is_zip(const uint8_t *data, size_t size);

/* A file's bytes as read, `data` of `*size`: themselves, or for a zip the one file inside it, `data` freed and its size
 * in `*size`.  NULL and the reason when the zip is refused, `data` freed. */
uint8_t *oracles_zip_unwrap(uint8_t *data, size_t *size, size_t max_size, char *error, size_t error_capacity);

/* The bytes of the zip's one ROM or patch, at most `max_size` of them, their CRC-32 checked; the caller frees them.
 * NULL and the reason otherwise. */
uint8_t *oracles_zip_extract(const uint8_t *zip, size_t size, size_t max_size, size_t *out_size, char *error,
                             size_t error_capacity);

#endif
