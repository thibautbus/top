#include "state.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAGIC "ORACLESST"
#define MAGIC_LENGTH 9u
#define MAX_FIELD (64u * 1024u * 1024u)

static void set_error(char *error, size_t capacity, const char *message)
{
    if (error && capacity) snprintf(error, capacity, "%s", message);
}

typedef struct writer {
    uint8_t *data;
    size_t size;
    size_t capacity;
    int failed;
} writer;

static void put(writer *w, const void *bytes, size_t count)
{
    if (w->failed) return;
    if (w->size + count > w->capacity) {
        size_t capacity = w->capacity ? w->capacity * 2 : 4096;
        while (capacity < w->size + count) capacity *= 2;
        uint8_t *data = realloc(w->data, capacity);
        if (!data) { w->failed = 1; return; }
        w->data = data;
        w->capacity = capacity;
    }
    memcpy(w->data + w->size, bytes, count);
    w->size += count;
}

static void put_u32(writer *w, uint32_t value)
{
    const uint8_t bytes[4] = { (uint8_t)value, (uint8_t)(value >> 8), (uint8_t)(value >> 16), (uint8_t)(value >> 24) };
    put(w, bytes, 4);
}

static void put_field(writer *w, const void *data, size_t size)
{
    if (size > MAX_FIELD) { w->failed = 1; return; }
    put_u32(w, (uint32_t)size);
    if (size) put(w, data, size);
}

int oracles_state_serialize(OraclesCore *core, const OraclesStateInfo *info,
                            uint8_t **out, size_t *out_size, char *error, size_t error_capacity)
{
    *out = NULL;
    *out_size = 0;
    const size_t guest_size = oracles_core_state_size(core);
    uint8_t *guest = malloc(guest_size ? guest_size : 1);
    if (!guest || oracles_core_save_state(core, guest, guest_size) != 0) {
        free(guest);
        set_error(error, error_capacity, "the core could not serialise its state");
        return -1;
    }
    writer w = { NULL, 0, 0, 0 };
    const char *version = oracles_core_version(core);
    put(&w, MAGIC, MAGIC_LENGTH);
    put_u32(&w, ORACLES_STATE_FORMAT);
    put_field(&w, version, strlen(version));
    put_field(&w, info->game, strlen(info->game));
    put_field(&w, info->rom_sha1, strlen(info->rom_sha1));
    put_field(&w, info->mods, strlen(info->mods));
    put_field(&w, guest, guest_size);
    put_field(&w, info->host_state, info->host_state_size);
    put_field(&w, info->mods_storage, info->mods_storage_size);
    free(guest);
    if (w.failed) { free(w.data); set_error(error, error_capacity, "out of memory"); return -1; }
    *out = w.data;
    *out_size = w.size;
    return 0;
}

typedef struct reader {
    const uint8_t *data;
    size_t size;
    size_t position;
} reader;

static int get_u32(reader *r, uint32_t *value)
{
    if (r->size - r->position < 4) return 0;
    const uint8_t *b = r->data + r->position;
    *value = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    r->position += 4;
    return 1;
}

/* A length-prefixed field, copied into a NUL-terminated heap buffer. */
static int get_field(reader *r, uint8_t **data, size_t *size)
{
    uint32_t length;
    if (!get_u32(r, &length) || length > MAX_FIELD || r->size - r->position < length) return 0;
    uint8_t *buffer = malloc((size_t)length + 1);
    if (!buffer) return 0;
    memcpy(buffer, r->data + r->position, length);
    buffer[length] = 0;
    r->position += length;
    *data = buffer;
    *size = length;
    return 1;
}

static int check_field(reader *r, const char *expected, const char *what, char *error, size_t capacity)
{
    uint8_t *value = NULL;
    size_t size = 0;
    if (!get_field(r, &value, &size)) { set_error(error, capacity, "the savestate is truncated"); return 0; }
    const int match = strcmp((const char *)value, expected) == 0;
    if (!match && error && capacity)
        snprintf(error, capacity, "the savestate was made with another %s (%s, now %s)", what,
                 value[0] ? (const char *)value : "none", expected[0] ? expected : "none");
    free(value);
    return match;
}

/* Field `wanted` of a savestate (0 core version, 1 game, 2 ROM, 3 mods, 4 guest state, 5 host state, 6 mods' storage), read in place. */
static int peek_field(const uint8_t *data, size_t size, unsigned wanted, const uint8_t **field_data, size_t *field_size)
{
    reader r = { data, size, MAGIC_LENGTH };
    uint32_t format, length = 0;
    if (size < MAGIC_LENGTH || memcmp(data, MAGIC, MAGIC_LENGTH) != 0 || !get_u32(&r, &format) || format != ORACLES_STATE_FORMAT) return -1;
    for (unsigned field = 0; field <= wanted; field++) {
        if (!get_u32(&r, &length) || length > MAX_FIELD || r.size - r.position < length) return -1;
        if (field == wanted) { *field_data = length ? r.data + r.position : NULL; *field_size = length; }
        r.position += length;
    }
    return 0;
}

int oracles_state_host_state(const uint8_t *data, size_t size, const uint8_t **host_state, size_t *host_state_size)
{
    *host_state = NULL;
    *host_state_size = 0;
    return peek_field(data, size, 5u, host_state, host_state_size);
}

int oracles_state_mods_storage(const uint8_t *data, size_t size, const uint8_t **storage, size_t *storage_size)
{
    *storage = NULL;
    *storage_size = 0;
    const uint8_t *host = NULL;
    size_t host_size = 0;
    if (peek_field(data, size, 5u, &host, &host_size) != 0) return -1;
    if (peek_field(data, size, 6u, storage, storage_size) != 0) { *storage = NULL; *storage_size = 0; }   /* a state from before the field */
    return 0;
}

int oracles_state_mods(const uint8_t *data, size_t size, char *mods, size_t capacity)
{
    const uint8_t *field = NULL;
    size_t length = 0;
    if (capacity) mods[0] = 0;
    if (peek_field(data, size, 3u, &field, &length) != 0 || length >= capacity) return -1;
    if (length) memcpy(mods, field, length);
    mods[length] = 0;
    return 0;
}

int oracles_state_deserialize(OraclesCore *core, const OraclesStateInfo *info,
                              const uint8_t *data, size_t size,
                              uint8_t **host_state, size_t *host_state_size,
                              char *error, size_t error_capacity)
{
    *host_state = NULL;
    *host_state_size = 0;
    reader r = { data, size, 0 };
    uint32_t format;
    if (size < MAGIC_LENGTH || memcmp(data, MAGIC, MAGIC_LENGTH) != 0) {
        set_error(error, error_capacity, "not a savestate of The Oracles Project");
        return -1;
    }
    r.position = MAGIC_LENGTH;
    if (!get_u32(&r, &format)) { set_error(error, error_capacity, "the savestate is truncated"); return -1; }
    if (format != ORACLES_STATE_FORMAT) { set_error(error, error_capacity, "the savestate has another format version"); return -1; }
    if (!check_field(&r, oracles_core_version(core), "core version", error, error_capacity)
        || !check_field(&r, info->game, "game", error, error_capacity)
        || !check_field(&r, info->rom_sha1, "ROM", error, error_capacity)
        || !check_field(&r, info->mods, "set of mods and gameplay options", error, error_capacity)) return -1;
    uint8_t *guest = NULL, *host = NULL;
    size_t guest_size = 0, host_size = 0;
    if (!get_field(&r, &guest, &guest_size) || !get_field(&r, &host, &host_size)) {
        free(guest); free(host);
        set_error(error, error_capacity, "the savestate is truncated");
        return -1;
    }
    if (oracles_core_load_state(core, guest, guest_size) != 0) {
        free(guest); free(host);
        set_error(error, error_capacity, "the core refused the guest state");
        return -1;
    }
    free(guest);
    if (host_size == 0) { free(host); host = NULL; }
    *host_state = host;
    *host_state_size = host_size;
    return 0;
}
