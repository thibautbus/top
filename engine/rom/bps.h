/* Applying a BPS patch to the user's ROM, in memory (the format of beat, byuu's
 * specification: "BPS1", the source, target and metadata sizes, the actions,
 * then the CRC32 of the source, of the target and of the patch).
 *
 * A fan game distributed as a patch (Moonrise Regalia) is played from the
 * base ROM and the patch the player chose: the image is built at each start,
 * never written.  Every copy is bounded, and the three checksums must hold:
 * a patch made for another base, cut short or damaged is refused, with the
 * reason, before anything plays. */
#ifndef ORACLES_BPS_H
#define ORACLES_BPS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The CRC32 of zlib and of the format (reflected, polynomial 0xEDB88320). */
uint32_t oracles_crc32(const uint8_t *data, size_t size);

/* A BPS patch whose own checksum holds, whatever its base: 0, or -1 with the reason.  The launcher says so of the
 * patch chosen before it has a base to apply it to. */
int oracles_bps_check(const uint8_t *patch, size_t patch_size, char *error, size_t error_capacity);
int oracles_bps_check_file(const char *patch_path, char *error, size_t error_capacity);

/* Applies `patch` to `source`.  Returns the target, which the caller frees,
 * and its size; NULL with the reason otherwise. */
uint8_t *oracles_bps_apply(const uint8_t *source, size_t source_size, const uint8_t *patch, size_t patch_size,
                           size_t *target_size, char *error, size_t error_capacity);

/* Reads the base ROM and the patch and applies it: the image a session plays.
 * Returns it, which the caller frees, and its size; NULL with the reason. */
uint8_t *oracles_bps_apply_files(const char *base_path, const char *patch_path, size_t *size,
                                 char *error, size_t error_capacity);

#ifdef __cplusplus
}
#endif

#endif
