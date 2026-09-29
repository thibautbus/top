#ifndef ORACLES_SHA1_H
#define ORACLES_SHA1_H

#include <stddef.h>
#include <stdint.h>

/* SHA-1 of a buffer, as 40 lowercase hexadecimal characters plus NUL. */
void oracles_sha1_hex(const uint8_t *data, size_t size, char out[41]);

#endif
