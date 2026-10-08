/* Smooth camera reducer for the Enhanced profile.
 *
 * A pure, host-only reducer: an authenticated observation of Link in world
 * coordinates goes in, a camera position in 1/256-pixel fixed point comes out,
 * with a dead zone, bounded look-ahead from filtered velocity, deterministic
 * damping and recentring, and a serialisable state.  It reads no SCX/SCY, no
 * Link position guessed from sprites, and never writes to the guest; the host
 * feeds it from the guest bus (engine/enhanced/camera.c).
 *
 * This reducer was developed and tested before the engine ran on SameBoy, and
 * is kept verbatim so its pinned config digest and wire format stay valid;
 * only its observation source changed, to the guest bus.  The
 * struct layouts are wire contracts, not serialisation formats.  Vendored
 * from the pre-SameBoy tree; the edits since are the second pinned
 * configuration (`oracles_e11_config_profile`), with its own digest, and the
 * way that configuration alone comes to rest (`oracles_e11_braking_offset`). */
#ifndef ORACLES_ENHANCED_WORLD_SMOOTH_CAMERA_E11_H
#define ORACLES_ENHANCED_WORLD_SMOOTH_CAMERA_E11_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define ORACLES_E11_F256 256
#define ORACLES_E11_I32_MIN INT32_MIN
#define ORACLES_E11_I32_MAX INT32_MAX
#define ORACLES_E11_I64_MIN INT64_MIN
#define ORACLES_E11_I64_MAX INT64_MAX
#define ORACLES_E11_U64_MAX UINT64_MAX

/* The viewports a pinned configuration may frame: the normal band's, and the
 * bands of the view's levels outdoors (camera.c; compositor.h's sizes:
 * 213 near in 4:3, 320 and 384 medium, 480 far), framed at their middle.  The
 * digest names the rest of the configuration, which they share. */
#define ORACLES_E11_VIEWPORT 256
#define ORACLES_E11_ZOOM_VIEWPORT 480
static inline bool oracles_e11_viewport_valid(int32_t viewport_width, int32_t framing) {
    static const int32_t widths[] = { 213, ORACLES_E11_VIEWPORT, 320, 384, ORACLES_E11_ZOOM_VIEWPORT };
    bool known = false;
    for (size_t i = 0; i < sizeof widths / sizeof widths[0]; i++) known = known || viewport_width == widths[i] * ORACLES_E11_F256;
    return known && framing * 2 == viewport_width;
}

#define ORACLES_E11_CONFIG_SCHEMA "camera-smooth-horizontal-v1"
#define ORACLES_E11_STATE_SCHEMA "camera-smooth-horizontal-state-v1"
#define ORACLES_E11_CONFIG_DIGEST \
    "61ea5c9d265a8fb4b1e75268b417b49fec993cc4c5e346f8d6a355bd66d4b067"
/* A second pinned configuration, after the first sessions played:
 * a dead zone of two pixels instead of sixteen, half the top speed and
 * acceleration, a longer look-ahead: the camera follows Link continuously
 * instead of standing still while he crosses the dead zone and catching up
 * in bursts.  When Link stops, this configuration pins its target to the
 * point where its braking ends and never recentres, so the camera comes to
 * rest without the backward twitch the decaying look-ahead gave it.  The
 * schema names that behaviour and how the stop is told from a slow walk
 * (v4: v2 had the same values and the old rest, v3 the same rest but told
 * from a single still frame, which stalled the camera under one pixel a
 * frame).  Its digest is the SHA-256 of the same compact JSON of its
 * fields, keys sorted, as the first one's. */
#define ORACLES_E11_CONFIG_SCHEMA_V4 "camera-smooth-horizontal-v4"
#define ORACLES_E11_CONFIG_DIGEST_V4 \
    "de4d24170ef03e38465c12767cc3ef7902cf8a94a32a00d9f13b72192c87194f"

/* Link's position arrives in whole pixels, so a speed under one pixel a frame
 * reaches the reducer as a step of one followed by still frames: at a quarter
 * of a pixel a frame there are three of them in a row.  A single still frame
 * is therefore not a stop, and the rest is pronounced only after this many in
 * a row -- the first count no walk of a quarter pixel a frame or faster can
 * produce.  Until then the look-ahead stays frozen at the value it had while
 * moving, so the target never slides back under a camera still gliding. */
#define ORACLES_E11_REST_FRAMES 4u

/* IDs are intentionally bounded at the C boundary.  Python's canonical
 * contract accepts arbitrary non-empty strings; the C ABI closes this input
 * to the portable JSON-safe subset so its digest has no hidden escaping rules.
 */
#define ORACLES_E11_ID_CAP 96u
#define ORACLES_E11_DIGEST_CAP 65u
#define ORACLES_E11_STATE_WIRE_SIZE 306u
#define ORACLES_E11_STATE_WIRE_MAX ORACLES_E11_STATE_WIRE_SIZE

typedef enum OraclesE11Error {
    ORACLES_E11_OK = 0,
    ORACLES_E11_INVALID_ARGUMENT = 1,
    ORACLES_E11_INVALID_CONFIG = 2,
    ORACLES_E11_INVALID_STATE = 3,
    ORACLES_E11_INVALID_OBSERVATION = 4,
    ORACLES_E11_OVERFLOW = 5,
    ORACLES_E11_BUFFER_TOO_SMALL = 6,
    ORACLES_E11_SERIALIZATION = 7,
    ORACLES_E11_CONFIG_MISMATCH = 8
} OraclesE11Error;

typedef enum OraclesE11Status {
    ORACLES_E11_UNINITIALIZED = 0,
    ORACLES_E11_TRACKING = 1,
    ORACLES_E11_DESYNCHRONIZED = 2
} OraclesE11Status;

typedef enum OraclesE11Event {
    ORACLES_E11_NORMAL_VBLANK = 0,
    ORACLES_E11_EVENT_A = 1,
    ORACLES_E11_EVENT_K = 2,
    ORACLES_E11_EVENT_X = 3,
    ORACLES_E11_PRESENTATION = 4
} OraclesE11Event;

/* These aliases mirror the Python enum's intentionally equivalent spellings. */
#define ORACLES_E11_ACTIVATE ORACLES_E11_EVENT_A
#define ORACLES_E11_K_BOUNDARY ORACLES_E11_EVENT_K
#define ORACLES_E11_EXPORT_X ORACLES_E11_EVENT_X

typedef enum OraclesE11FollowRecenter {
    ORACLES_E11_FOLLOW = 0,
    ORACLES_E11_RECENTER = 1
} OraclesE11FollowRecenter;

typedef enum OraclesE11Constraint {
    ORACLES_E11_CONSTRAINT_NONE = 0,
    ORACLES_E11_DOMAIN_BOUND = 1,
    ORACLES_E11_VISIBILITY_FALLBACK = 2
} OraclesE11Constraint;

typedef enum OraclesE11Availability {
    ORACLES_E11_AVAILABLE = 0,
    ORACLES_E11_AUTHENTICATED_ABSENT = 1,
    ORACLES_E11_UNAVAILABLE = 2
} OraclesE11Availability;

typedef enum OraclesE11Domain {
    ORACLES_E11_DOMAIN_NONE = -1,
    ORACLES_E11_EXTERIOR = 0,
    ORACLES_E11_INTERIOR = 1,
    ORACLES_E11_DUNGEON = 2,
    ORACLES_E11_ERA = 3,
    ORACLES_E11_UNDERGROUND = 4
} OraclesE11Domain;

typedef struct OraclesE11Config {
    char schema[sizeof(ORACLES_E11_CONFIG_SCHEMA)];
    int32_t viewport_width;
    int32_t viewport_height;
    int32_t framing;
    int32_t dead_half_width;
    int32_t lookahead_max;
    int32_t horizon_ticks;
    int32_t velocity_filter_divisor;
    int32_t lookahead_step;
    int32_t max_speed;
    int32_t max_accel;
    int32_t idle_delay;
    int32_t rest_tolerance;
    int32_t convergence_bound;
    char digest[ORACLES_E11_DIGEST_CAP];
} OraclesE11Config;

typedef struct OraclesE11Observation {
    char session[ORACLES_E11_ID_CAP];
    uint64_t epoch;
    uint64_t ordinal;
    uint64_t tick;
    char snapshot_identity[ORACLES_E11_ID_CAP];
    int32_t link_x;
    int32_t link_y;
    int32_t bounds_origin_x;
    int32_t bounds_origin_y;
    int64_t bounds_width;
    int64_t bounds_height;
    OraclesE11Domain domain;
    OraclesE11Availability availability;
    OraclesE11Event event;
    /* Empty means “derive the canonical authenticated digest”.  A non-empty
     * value is checked against that digest, preventing a caller from making a
     * contradictory duplicate look identical merely by changing its hash. */
    char digest[ORACLES_E11_DIGEST_CAP];
} OraclesE11Observation;

typedef struct OraclesE11State {
    OraclesE11Status status;
    char config_digest[ORACLES_E11_DIGEST_CAP];
    char session[ORACLES_E11_ID_CAP];
    uint64_t epoch;
    OraclesE11Domain domain;
    uint64_t segment;
    int32_t camera_pos;
    int64_t applied_velocity;
    int64_t filtered_link_velocity;
    int64_t lookahead;
    bool last_link_present;
    int32_t last_link_x;
    int32_t last_link_y;
    bool last_ordinal_present;
    uint64_t last_ordinal;
    char last_observation_digest[ORACLES_E11_DIGEST_CAP];
    uint64_t idle_ticks;
    OraclesE11FollowRecenter follow_recenter;
    OraclesE11Constraint last_constraint;
    bool forced_displacement;
} OraclesE11State;

typedef struct OraclesE11Result {
    OraclesE11Error error;
    OraclesE11State state;
} OraclesE11Result;

static inline void oracles_e11_observation_initial(OraclesE11Observation *observation) {
    if (observation != NULL) memset(observation, 0, sizeof(*observation));
    if (observation != NULL) {
        observation->domain = ORACLES_E11_EXTERIOR;
        observation->availability = ORACLES_E11_AVAILABLE;
        observation->event = ORACLES_E11_NORMAL_VBLANK;
    }
}

/* ---------- Small checked primitives ---------- */

static inline bool oracles_e11_i64_add(int64_t a, int64_t b, int64_t *out) {
    if (out == NULL) return false;
    if ((b > 0 && a > INT64_MAX - b) || (b < 0 && a < INT64_MIN - b)) return false;
    *out = a + b;
    return true;
}

static inline bool oracles_e11_i64_sub(int64_t a, int64_t b, int64_t *out) {
    if (out == NULL) return false;
    if ((b > 0 && a < INT64_MIN + b) || (b < 0 && a > INT64_MAX + b)) return false;
    *out = a - b;
    return true;
}

static inline bool oracles_e11_i64_mul(int64_t a, int64_t b, int64_t *out) {
    if (out == NULL) return false;
    if (a == 0 || b == 0) { *out = 0; return true; }
    if (a == -1) { if (b == INT64_MIN) return false; *out = -b; return true; }
    if (b == -1) { if (a == INT64_MIN) return false; *out = -a; return true; }
    if (a > 0) {
        if (b > 0) { if (a > INT64_MAX / b) return false; }
        else if (b < INT64_MIN / a) return false;
    } else if (b > 0) {
        if (a < INT64_MIN / b) return false;
    } else {
        if (a < INT64_MAX / b) return false;
    }
    *out = a * b;
    return true;
}

static inline bool oracles_e11_round_half_away(int64_t value, int64_t denominator,
                                                int64_t *out) {
    uint64_t magnitude;
    uint64_t quotient;
    uint64_t remainder;
    uint64_t rounded;
    if (out == NULL || denominator <= 0) return false;
    magnitude = value < 0 ? (uint64_t)(-(value + 1)) + 1u : (uint64_t)value;
    quotient = magnitude / (uint64_t)denominator;
    remainder = magnitude % (uint64_t)denominator;
    if (remainder * 2u >= (uint64_t)denominator) quotient++;
    rounded = quotient;
    if (value < 0) {
        if (rounded > (uint64_t)INT64_MAX + 1u) return false;
        if (rounded == (uint64_t)INT64_MAX + 1u) *out = INT64_MIN;
        else *out = -(int64_t)rounded;
    } else {
        if (rounded > (uint64_t)INT64_MAX) return false;
        *out = (int64_t)rounded;
    }
    return true;
}

static inline bool oracles_e11_clamp(int64_t value, int64_t low, int64_t high,
                                     int64_t *out) {
    if (out == NULL || low > high) return false;
    *out = value < low ? low : (value > high ? high : value);
    return true;
}

static inline bool oracles_e11_i64_abs(int64_t value, int64_t *out) {
    if (out == NULL || value == INT64_MIN) return false;
    *out = value < 0 ? -value : value;
    return true;
}

static inline bool oracles_e11_u64_inc(uint64_t value, uint64_t *out) {
    if (out == NULL || value == UINT64_MAX) return false;
    *out = value + 1u;
    return true;
}

static inline bool oracles_e11_copy_text(char *dst, size_t capacity, const char *src) {
    size_t length;
    if (dst == NULL || src == NULL || capacity == 0u) return false;
    length = strlen(src);
    if (length == 0u || length >= capacity) return false;
    memcpy(dst, src, length + 1u);
    return true;
}

static inline bool oracles_e11_text_valid(const char *value, size_t capacity) {
    size_t i;
    if (value == NULL || capacity == 0u || value[0] == '\0') return false;
    for (i = 0u; i < capacity; ++i) {
        unsigned char c = (unsigned char)value[i];
        if (c == '\0') return true;
        if (!(c == '-' || c == '_' || c == '.' || (c >= '0' && c <= '9') ||
              (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return false;
}

static inline bool oracles_e11_digest_valid(const char *value) {
    size_t i;
    if (value == NULL || value[64] != '\0') return false;
    for (i = 0u; i < 64u; ++i) {
        const char c = value[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    }
    return true;
}

/* ---------- Bounded SHA-256 used only for canonical observation identity ---------- */

typedef struct OraclesE11Sha256 {
    uint32_t h[8];
    uint64_t bits;
    uint8_t block[64];
    size_t used;
} OraclesE11Sha256;

static inline uint32_t oracles_e11_rotr(uint32_t x, unsigned n) {
    return (x >> n) | (x << (32u - n));
}

static inline void oracles_e11_sha_transform(OraclesE11Sha256 *ctx) {
    static const uint32_t k[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
    };
    uint32_t w[64];
    uint32_t a,b,c,d,e,f,g,h;
    size_t i;
    for (i = 0u; i < 16u; ++i) {
        const size_t p = i * 4u;
        w[i] = ((uint32_t)ctx->block[p] << 24u) | ((uint32_t)ctx->block[p + 1u] << 16u) |
               ((uint32_t)ctx->block[p + 2u] << 8u) | (uint32_t)ctx->block[p + 3u];
    }
    for (i = 16u; i < 64u; ++i) {
        const uint32_t s0 = oracles_e11_rotr(w[i - 15u], 7u) ^ oracles_e11_rotr(w[i - 15u], 18u) ^ (w[i - 15u] >> 3u);
        const uint32_t s1 = oracles_e11_rotr(w[i - 2u], 17u) ^ oracles_e11_rotr(w[i - 2u], 19u) ^ (w[i - 2u] >> 10u);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    a=ctx->h[0]; b=ctx->h[1]; c=ctx->h[2]; d=ctx->h[3]; e=ctx->h[4]; f=ctx->h[5]; g=ctx->h[6]; h=ctx->h[7];
    for (i = 0u; i < 64u; ++i) {
        const uint32_t s1 = oracles_e11_rotr(e, 6u) ^ oracles_e11_rotr(e, 11u) ^ oracles_e11_rotr(e, 25u);
        const uint32_t ch = (e & f) ^ (~e & g);
        const uint32_t temp1 = h + s1 + ch + k[i] + w[i];
        const uint32_t s0 = oracles_e11_rotr(a, 2u) ^ oracles_e11_rotr(a, 13u) ^ oracles_e11_rotr(a, 22u);
        const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const uint32_t temp2 = s0 + maj;
        h=g; g=f; f=e; e=d + temp1; d=c; c=b; b=a; a=temp1 + temp2;
    }
    ctx->h[0]+=a; ctx->h[1]+=b; ctx->h[2]+=c; ctx->h[3]+=d;
    ctx->h[4]+=e; ctx->h[5]+=f; ctx->h[6]+=g; ctx->h[7]+=h;
}

static inline void oracles_e11_sha_init(OraclesE11Sha256 *ctx) {
    static const uint32_t initial[8] = {
        0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,
        0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u
    };
    memcpy(ctx->h, initial, sizeof(initial));
    ctx->bits = 0u;
    ctx->used = 0u;
}

static inline void oracles_e11_sha_update(OraclesE11Sha256 *ctx, const char *data, size_t length) {
    while (length != 0u) {
        const size_t take = (length < sizeof(ctx->block) - ctx->used) ? length : sizeof(ctx->block) - ctx->used;
        memcpy(ctx->block + ctx->used, data, take);
        ctx->used += take;
        data += take;
        length -= take;
        ctx->bits += (uint64_t)take * 8u;
        if (ctx->used == sizeof(ctx->block)) {
            oracles_e11_sha_transform(ctx);
            ctx->used = 0u;
        }
    }
}

static inline void oracles_e11_sha_final(OraclesE11Sha256 *ctx, char output[ORACLES_E11_DIGEST_CAP]) {
    size_t i;
    const uint64_t bits = ctx->bits;
    static const char hex[] = "0123456789abcdef";
    ctx->block[ctx->used++] = 0x80u;
    while (ctx->used != 56u) {
        if (ctx->used == 64u) { oracles_e11_sha_transform(ctx); ctx->used = 0u; }
        ctx->block[ctx->used++] = 0u;
    }
    for (i = 0u; i < 8u; ++i) ctx->block[56u + i] = (uint8_t)(bits >> (56u - i * 8u));
    oracles_e11_sha_transform(ctx);
    for (i = 0u; i < 8u; ++i) {
        const uint32_t word = ctx->h[i];
        output[i * 8u] = hex[(word >> 28u) & 0xfu]; output[i * 8u + 1u] = hex[(word >> 24u) & 0xfu];
        output[i * 8u + 2u] = hex[(word >> 20u) & 0xfu]; output[i * 8u + 3u] = hex[(word >> 16u) & 0xfu];
        output[i * 8u + 4u] = hex[(word >> 12u) & 0xfu]; output[i * 8u + 5u] = hex[(word >> 8u) & 0xfu];
        output[i * 8u + 6u] = hex[(word >> 4u) & 0xfu]; output[i * 8u + 7u] = hex[word & 0xfu];
    }
    output[64u] = '\0';
}

static inline const char *oracles_e11_domain_name(OraclesE11Domain domain) {
    switch (domain) {
        case ORACLES_E11_EXTERIOR: return "exterior";
        case ORACLES_E11_INTERIOR: return "interior";
        case ORACLES_E11_DUNGEON: return "dungeon";
        case ORACLES_E11_ERA: return "era";
        case ORACLES_E11_UNDERGROUND: return "underground";
        default: return NULL;
    }
}

static inline const char *oracles_e11_availability_name(OraclesE11Availability availability) {
    switch (availability) {
        case ORACLES_E11_AVAILABLE: return "available";
        case ORACLES_E11_AUTHENTICATED_ABSENT: return "authenticated-absent";
        case ORACLES_E11_UNAVAILABLE: return "unavailable";
        default: return NULL;
    }
}

static inline const char *oracles_e11_event_name(OraclesE11Event event) {
    switch (event) {
        case ORACLES_E11_NORMAL_VBLANK: return "normal-vblank";
        case ORACLES_E11_EVENT_A: return "a";
        case ORACLES_E11_EVENT_K: return "k";
        case ORACLES_E11_EVENT_X: return "x";
        case ORACLES_E11_PRESENTATION: return "presentation";
        default: return NULL;
    }
}

static inline bool oracles_e11_observation_digest(const OraclesE11Observation *observation,
                                                   char output[ORACLES_E11_DIGEST_CAP]) {
    char json[1024];
    int length;
    const char *domain;
    const char *availability;
    const char *event;
    OraclesE11Sha256 sha;
    if (observation == NULL || output == NULL ||
        !oracles_e11_text_valid(observation->session, sizeof(observation->session)) ||
        !oracles_e11_text_valid(observation->snapshot_identity, sizeof(observation->snapshot_identity))) return false;
    domain = oracles_e11_domain_name(observation->domain);
    availability = oracles_e11_availability_name(observation->availability);
    event = oracles_e11_event_name(observation->event);
    if (domain == NULL || availability == NULL || event == NULL) return false;
    length = snprintf(json, sizeof(json),
        "{\"availability\":\"%s\",\"bounds\":{\"height\":%" PRId64 ",\"origin\":{\"x\":%" PRId32 ",\"y\":%" PRId32 "},\"width\":%" PRId64 "},\"domain\":\"%s\",\"epoch\":%" PRIu64 ",\"event\":\"%s\",\"link\":{\"x\":%" PRId32 ",\"y\":%" PRId32 "},\"ordinal\":%" PRIu64 ",\"session\":\"%s\",\"snapshot_identity\":\"%s\",\"tick\":%" PRIu64 "}",
        availability, observation->bounds_height, observation->bounds_origin_x, observation->bounds_origin_y,
        observation->bounds_width, domain, observation->epoch, event, observation->link_x, observation->link_y,
        observation->ordinal, observation->session, observation->snapshot_identity, observation->tick);
    if (length < 0 || (size_t)length >= sizeof(json)) return false;
    oracles_e11_sha_init(&sha);
    oracles_e11_sha_update(&sha, json, (size_t)length);
    oracles_e11_sha_final(&sha, output);
    return true;
}

/* ---------- Contract construction, validation, and explicit wire format ---------- */

static inline void oracles_e11_config_default(OraclesE11Config *config) {
    if (config == NULL) return;
    memset(config, 0, sizeof(*config));
    (void)oracles_e11_copy_text(config->schema, sizeof(config->schema), ORACLES_E11_CONFIG_SCHEMA);
    config->viewport_width = 256 * ORACLES_E11_F256;
    config->viewport_height = 128 * ORACLES_E11_F256;
    config->framing = 128 * ORACLES_E11_F256;
    config->dead_half_width = 16 * ORACLES_E11_F256;
    config->lookahead_max = 16 * ORACLES_E11_F256;
    config->horizon_ticks = 8;
    config->velocity_filter_divisor = 4;
    config->lookahead_step = ORACLES_E11_F256 / 2;
    config->max_speed = 4 * ORACLES_E11_F256;
    config->max_accel = ORACLES_E11_F256 / 4;
    config->idle_delay = 30;
    config->rest_tolerance = ORACLES_E11_F256 / 16;
    config->convergence_bound = 180;
    (void)oracles_e11_copy_text(config->digest, sizeof(config->digest), ORACLES_E11_CONFIG_DIGEST);
}

static inline void oracles_e11_config_profile(OraclesE11Config *config, unsigned profile) {
    if (config == NULL) return;
    oracles_e11_config_default(config);
    if (profile != 2u) return;
    (void)oracles_e11_copy_text(config->schema, sizeof(config->schema), ORACLES_E11_CONFIG_SCHEMA_V4);
    config->dead_half_width = 2 * ORACLES_E11_F256;
    config->lookahead_max = 24 * ORACLES_E11_F256;
    config->max_speed = 2 * ORACLES_E11_F256;
    config->max_accel = ORACLES_E11_F256 / 8;
    (void)oracles_e11_copy_text(config->digest, sizeof(config->digest), ORACLES_E11_CONFIG_DIGEST_V4);
}

static inline bool oracles_e11_config_valid_v4(const OraclesE11Config *config) {
    return strncmp(config->schema, ORACLES_E11_CONFIG_SCHEMA_V4, sizeof(config->schema)) == 0 &&
        oracles_e11_viewport_valid(config->viewport_width, config->framing) && config->viewport_height == 128 * ORACLES_E11_F256 &&
        config->dead_half_width == 2 * ORACLES_E11_F256 &&
        config->lookahead_max == 24 * ORACLES_E11_F256 && config->horizon_ticks == 8 &&
        config->velocity_filter_divisor == 4 && config->lookahead_step == ORACLES_E11_F256 / 2 &&
        config->max_speed == 2 * ORACLES_E11_F256 && config->max_accel == ORACLES_E11_F256 / 8 &&
        config->idle_delay == 30 && config->rest_tolerance == ORACLES_E11_F256 / 16 &&
        config->convergence_bound == 180 && oracles_e11_digest_valid(config->digest) &&
        strncmp(config->digest, ORACLES_E11_CONFIG_DIGEST_V4, sizeof(config->digest)) == 0;
}

static inline bool oracles_e11_config_rests_where_it_brakes(const OraclesE11Config *config) {
    return strncmp(config->schema, ORACLES_E11_CONFIG_SCHEMA_V4, sizeof(config->schema)) == 0;
}

static inline bool oracles_e11_config_valid(const OraclesE11Config *config) {
    if (config == NULL) return false;
    if (oracles_e11_config_valid_v4(config)) return true;
    return strncmp(config->schema, ORACLES_E11_CONFIG_SCHEMA, sizeof(config->schema)) == 0 &&
        oracles_e11_viewport_valid(config->viewport_width, config->framing) && config->viewport_height == 128 * ORACLES_E11_F256 &&
        config->dead_half_width == 16 * ORACLES_E11_F256 &&
        config->lookahead_max == 16 * ORACLES_E11_F256 && config->horizon_ticks == 8 &&
        config->velocity_filter_divisor == 4 && config->lookahead_step == ORACLES_E11_F256 / 2 &&
        config->max_speed == 4 * ORACLES_E11_F256 && config->max_accel == ORACLES_E11_F256 / 4 &&
        config->idle_delay == 30 && config->rest_tolerance == ORACLES_E11_F256 / 16 &&
        config->convergence_bound == 180 && oracles_e11_digest_valid(config->digest) &&
        strncmp(config->digest, ORACLES_E11_CONFIG_DIGEST, sizeof(config->digest)) == 0;
}

static inline void oracles_e11_state_initial(OraclesE11State *state,
                                             const OraclesE11Config *config) {
    if (state == NULL) return;
    memset(state, 0, sizeof(*state));
    state->status = ORACLES_E11_UNINITIALIZED;
    state->domain = ORACLES_E11_DOMAIN_NONE;
    state->follow_recenter = ORACLES_E11_FOLLOW;
    if (config != NULL) memcpy(state->config_digest, config->digest, sizeof(state->config_digest));
}

static inline bool oracles_e11_state_valid(const OraclesE11State *state) {
    bool history;
    if (state == NULL || !oracles_e11_digest_valid(state->config_digest) ||
        state->domain < ORACLES_E11_DOMAIN_NONE || state->domain > ORACLES_E11_UNDERGROUND ||
        state->last_constraint < ORACLES_E11_CONSTRAINT_NONE || state->last_constraint > ORACLES_E11_VISIBILITY_FALLBACK ||
        state->follow_recenter < ORACLES_E11_FOLLOW || state->follow_recenter > ORACLES_E11_RECENTER) return false;
    if (state->session[0] != '\0' && !oracles_e11_text_valid(state->session, sizeof(state->session))) return false;
    if (state->last_observation_digest[0] != '\0' && !oracles_e11_digest_valid(state->last_observation_digest)) return false;
    if (state->last_link_present != state->last_ordinal_present) return false;
    history = state->session[0] != '\0' || state->domain != ORACLES_E11_DOMAIN_NONE || state->last_link_present ||
              state->last_ordinal_present || state->last_observation_digest[0] != '\0';
    if (state->status == ORACLES_E11_UNINITIALIZED)
        return !history && state->domain == ORACLES_E11_DOMAIN_NONE;
    if (state->status == ORACLES_E11_TRACKING)
        return state->session[0] != '\0' && state->domain != ORACLES_E11_DOMAIN_NONE && state->last_link_present &&
               state->last_ordinal_present && state->last_observation_digest[0] != '\0';
    if (state->status == ORACLES_E11_DESYNCHRONIZED)
        return !history || (state->session[0] != '\0' && state->domain != ORACLES_E11_DOMAIN_NONE &&
                            state->last_link_present && state->last_ordinal_present);
    return false;
}

static inline size_t oracles_e11_put_u8(uint8_t *buffer, size_t at, uint8_t value) { buffer[at] = value; return at + 1u; }
static inline size_t oracles_e11_put_u32(uint8_t *buffer, size_t at, uint32_t value) {
    buffer[at++] = (uint8_t)(value >> 24u); buffer[at++] = (uint8_t)(value >> 16u);
    buffer[at++] = (uint8_t)(value >> 8u); buffer[at++] = (uint8_t)value; return at;
}
static inline size_t oracles_e11_put_u64(uint8_t *buffer, size_t at, uint64_t value) {
    unsigned shift;
    for (shift = 56u; ; shift -= 8u) { buffer[at++] = (uint8_t)(value >> shift); if (shift == 0u) break; }
    return at;
}
static inline size_t oracles_e11_put_i64(uint8_t *buffer, size_t at, int64_t value) { return oracles_e11_put_u64(buffer, at, (uint64_t)value); }
static inline uint8_t oracles_e11_get_u8(const uint8_t *buffer, size_t *at) { return buffer[(*at)++]; }
static inline uint32_t oracles_e11_get_u32(const uint8_t *buffer, size_t *at) {
    uint32_t value = ((uint32_t)buffer[(*at)++] << 24u); value |= ((uint32_t)buffer[(*at)++] << 16u);
    value |= ((uint32_t)buffer[(*at)++] << 8u); value |= buffer[(*at)++]; return value;
}
static inline uint64_t oracles_e11_get_u64(const uint8_t *buffer, size_t *at) {
    uint64_t value = 0u; unsigned i; for (i = 0u; i < 8u; ++i) value = (value << 8u) | buffer[(*at)++]; return value;
}

/* Version 1 wire fields are written one-by-one in network byte order. */
static inline OraclesE11Error oracles_e11_state_serialize(const OraclesE11State *state,
                                                          uint8_t *buffer, size_t capacity,
                                                          size_t *written) {
    size_t at = 0u;
    if (state == NULL || buffer == NULL || written == NULL || !oracles_e11_state_valid(state)) return ORACLES_E11_INVALID_STATE;
    if (capacity < ORACLES_E11_STATE_WIRE_MAX) return ORACLES_E11_BUFFER_TOO_SMALL;
#define E11_COPY_FIELD(field) do { memcpy(buffer + at, (field), sizeof(field)); at += sizeof(field); } while (0)
    buffer[at++] = 'E'; buffer[at++] = '1'; buffer[at++] = '1'; buffer[at++] = 'S'; buffer[at++] = 1u;
    at = oracles_e11_put_u8(buffer, at, (uint8_t)state->status); E11_COPY_FIELD(state->config_digest);
    E11_COPY_FIELD(state->session); at = oracles_e11_put_u64(buffer, at, state->epoch);
    at = oracles_e11_put_u8(buffer, at, (uint8_t)(state->domain + 1)); at = oracles_e11_put_u64(buffer, at, state->segment);
    at = oracles_e11_put_u32(buffer, at, (uint32_t)state->camera_pos); at = oracles_e11_put_i64(buffer, at, state->applied_velocity);
    at = oracles_e11_put_i64(buffer, at, state->filtered_link_velocity); at = oracles_e11_put_i64(buffer, at, state->lookahead);
    at = oracles_e11_put_u8(buffer, at, state->last_link_present ? 1u : 0u);
    at = oracles_e11_put_u32(buffer, at, (uint32_t)state->last_link_x); at = oracles_e11_put_u32(buffer, at, (uint32_t)state->last_link_y);
    at = oracles_e11_put_u8(buffer, at, state->last_ordinal_present ? 1u : 0u); at = oracles_e11_put_u64(buffer, at, state->last_ordinal);
    E11_COPY_FIELD(state->last_observation_digest); at = oracles_e11_put_u64(buffer, at, state->idle_ticks);
    at = oracles_e11_put_u8(buffer, at, (uint8_t)state->follow_recenter); at = oracles_e11_put_u8(buffer, at, (uint8_t)state->last_constraint);
    at = oracles_e11_put_u8(buffer, at, state->forced_displacement ? 1u : 0u);
    *written = at;
#undef E11_COPY_FIELD
    return ORACLES_E11_OK;
}

static inline OraclesE11Error oracles_e11_state_restore(const uint8_t *buffer, size_t length,
                                                        const OraclesE11Config *config,
                                                        OraclesE11State *state) {
    size_t at = 0u;
    if (buffer == NULL || state == NULL || config == NULL || !oracles_e11_config_valid(config)) return ORACLES_E11_INVALID_ARGUMENT;
    if (length != ORACLES_E11_STATE_WIRE_SIZE || buffer[0] != 'E' || buffer[1] != '1' || buffer[2] != '1' || buffer[3] != 'S' || buffer[4] != 1u)
        return ORACLES_E11_SERIALIZATION;
    at = 5u; memset(state, 0, sizeof(*state));
#define E11_READ_FIELD(field) do { if (at + sizeof(field) > length) return ORACLES_E11_SERIALIZATION; memcpy((field), buffer + at, sizeof(field)); at += sizeof(field); } while (0)
    state->status = (OraclesE11Status)oracles_e11_get_u8(buffer, &at); E11_READ_FIELD(state->config_digest);
    E11_READ_FIELD(state->session); state->epoch = oracles_e11_get_u64(buffer, &at);
    state->domain = (OraclesE11Domain)((int)oracles_e11_get_u8(buffer, &at) - 1); state->segment = oracles_e11_get_u64(buffer, &at);
    state->camera_pos = (int32_t)oracles_e11_get_u32(buffer, &at); state->applied_velocity = (int64_t)oracles_e11_get_u64(buffer, &at);
    state->filtered_link_velocity = (int64_t)oracles_e11_get_u64(buffer, &at); state->lookahead = (int64_t)oracles_e11_get_u64(buffer, &at);
    {
        const uint8_t last_link_present = oracles_e11_get_u8(buffer, &at);
        if (last_link_present > 1u) return ORACLES_E11_SERIALIZATION;
        state->last_link_present = last_link_present == 1u;
    }
    state->last_link_x = (int32_t)oracles_e11_get_u32(buffer, &at); state->last_link_y = (int32_t)oracles_e11_get_u32(buffer, &at);
    {
        const uint8_t last_ordinal_present = oracles_e11_get_u8(buffer, &at);
        if (last_ordinal_present > 1u) return ORACLES_E11_SERIALIZATION;
        state->last_ordinal_present = last_ordinal_present == 1u;
    }
    state->last_ordinal = oracles_e11_get_u64(buffer, &at);
    E11_READ_FIELD(state->last_observation_digest); state->idle_ticks = oracles_e11_get_u64(buffer, &at);
    state->follow_recenter = (OraclesE11FollowRecenter)oracles_e11_get_u8(buffer, &at); state->last_constraint = (OraclesE11Constraint)oracles_e11_get_u8(buffer, &at);
    {
        const uint8_t forced_displacement = oracles_e11_get_u8(buffer, &at);
        if (forced_displacement > 1u) return ORACLES_E11_SERIALIZATION;
        state->forced_displacement = forced_displacement == 1u;
    }
#undef E11_READ_FIELD
    if (at != length || memcmp(state->config_digest, config->digest, sizeof(state->config_digest)) != 0 || !oracles_e11_state_valid(state)) return ORACLES_E11_SERIALIZATION;
    return ORACLES_E11_OK;
}

/* ---------- Reducer ---------- */

/* Distance the camera still travels when it sheds one acceleration step per
 * frame from `velocity` to zero: the sum of the velocities to come, not of
 * the current one (with v=256 and a=32 the next frame moves 224 units). */
static inline bool oracles_e11_braking_offset(int64_t velocity, const OraclesE11Config *config, int64_t *offset) {
    int64_t remaining, magnitude, distance = 0;
    const int64_t direction = velocity < 0 ? -1 : 1;
    if (config == NULL || offset == NULL || config->max_accel <= 0 ||
        !oracles_e11_i64_abs(velocity, &magnitude) || magnitude > config->max_speed) return false;
    remaining = magnitude;
    while (remaining != 0) {
        const int64_t deceleration = remaining < config->max_accel ? remaining : config->max_accel;
        int64_t step;
        if (!oracles_e11_i64_sub(remaining, deceleration, &remaining) ||
            !oracles_e11_i64_mul(remaining, direction, &step) ||
            !oracles_e11_i64_add(distance, step, &distance)) return false;
    }
    *offset = distance;
    return true;
}

static inline bool oracles_e11_bounds(const OraclesE11Observation *observation,
                                      const OraclesE11Config *config,
                                      int64_t *low, int64_t *high) {
    int64_t right;
    if (!oracles_e11_i64_add((int64_t)observation->bounds_origin_x, observation->bounds_width, &right) ||
        !oracles_e11_i64_sub(right, config->viewport_width, high)) return false;
    *low = observation->bounds_origin_x;
    return *high >= *low;
}

static inline bool oracles_e11_visible(int64_t camera, int32_t link_x,
                                       const OraclesE11Config *config) {
    int64_t right;
    return oracles_e11_i64_add(camera, config->viewport_width, &right) && camera <= link_x && link_x <= right;
}

static inline bool oracles_e11_valid_normal(const OraclesE11Observation *observation,
                                            const OraclesE11Config *config) {
    int64_t right, bottom, low, high, link_low, link_high, overlap_low, overlap_high;
    if (observation->event != ORACLES_E11_NORMAL_VBLANK || observation->availability != ORACLES_E11_AVAILABLE) return false;
    if (!oracles_e11_i64_add((int64_t)observation->bounds_origin_x, observation->bounds_width, &right) ||
        !oracles_e11_i64_add((int64_t)observation->bounds_origin_y, observation->bounds_height, &bottom) ||
        observation->link_x < observation->bounds_origin_x || observation->link_x >= right ||
        observation->link_y < observation->bounds_origin_y || observation->link_y >= bottom ||
        !oracles_e11_bounds(observation, config, &low, &high) ||
        !oracles_e11_i64_sub(observation->link_x, config->viewport_width, &link_low)) return false;
    link_high = observation->link_x;
    overlap_low = low > link_low ? low : link_low;
    overlap_high = high < link_high ? high : link_high;
    return overlap_low <= overlap_high;
}

static inline OraclesE11State oracles_e11_desync(const OraclesE11State *state,
                                                 OraclesE11Constraint constraint) {
    OraclesE11State result = *state;
    result.status = ORACLES_E11_DESYNCHRONIZED;
    result.applied_velocity = 0; result.filtered_link_velocity = 0; result.lookahead = 0;
    result.idle_ticks = 0; result.follow_recenter = ORACLES_E11_FOLLOW;
    result.last_constraint = constraint; result.forced_displacement = false;
    return result;
}

static inline OraclesE11Error oracles_e11_bootstrap(const OraclesE11State *old,
                                                    const OraclesE11Observation *observation,
                                                    const char digest[ORACLES_E11_DIGEST_CAP],
                                                    const OraclesE11Config *config,
                                                    uint64_t segment, OraclesE11State *out) {
    int64_t target, low, high, camera;
    if (!oracles_e11_bounds(observation, config, &low, &high) ||
        !oracles_e11_i64_sub(observation->link_x, config->framing, &target) ||
        !oracles_e11_clamp(target, low, high, &camera) || camera < INT32_MIN || camera > INT32_MAX ||
        !oracles_e11_visible(camera, observation->link_x, config)) return ORACLES_E11_OVERFLOW;
    *out = *old; out->status = ORACLES_E11_TRACKING; memcpy(out->config_digest, config->digest, sizeof(out->config_digest));
    (void)oracles_e11_copy_text(out->session, sizeof(out->session), observation->session); out->epoch = observation->epoch;
    out->domain = observation->domain; out->segment = segment; out->camera_pos = (int32_t)camera;
    out->applied_velocity = 0; out->filtered_link_velocity = 0; out->lookahead = 0;
    out->last_link_present = true; out->last_link_x = observation->link_x; out->last_link_y = observation->link_y;
    out->last_ordinal_present = true; out->last_ordinal = observation->ordinal; memcpy(out->last_observation_digest, digest, sizeof(out->last_observation_digest));
    out->idle_ticks = 0; out->follow_recenter = ORACLES_E11_FOLLOW;
    out->last_constraint = camera == target ? ORACLES_E11_CONSTRAINT_NONE : ORACLES_E11_DOMAIN_BOUND;
    out->forced_displacement = false;
    return ORACLES_E11_OK;
}

static inline int64_t oracles_e11_stopping_speed(int64_t distance, const OraclesE11Config *config) {
    int64_t low = 0, high = config->max_speed;
    while (low < high) {
        int64_t span, span_plus_one, middle;
        int64_t steps;
        int64_t first, triangular, deceleration, braking;
        if (!oracles_e11_i64_sub(high, low, &span) ||
            !oracles_e11_i64_add(span, 1, &span_plus_one) ||
            !oracles_e11_i64_add(low, span_plus_one / 2, &middle)) return -1;
        steps = (middle + config->max_accel - 1) / config->max_accel;
        if (!oracles_e11_i64_mul(steps, middle, &first) || !oracles_e11_i64_mul(steps, steps - 1, &triangular)) return -1;
        triangular /= 2;
        if (!oracles_e11_i64_mul(config->max_accel, triangular, &deceleration) || !oracles_e11_i64_sub(first, deceleration, &braking)) return -1;
        if (braking <= distance) low = middle; else high = middle - 1;
    }
    return low;
}

static inline OraclesE11Result oracles_e11_reduce(const OraclesE11State *state,
                                                  const OraclesE11Observation *observation,
                                                  const OraclesE11Config *config) {
    OraclesE11Result result;
    OraclesE11State current;
    char digest[ORACLES_E11_DIGEST_CAP];
    int64_t low, high, measured, filtered_delta, filtered, product, lookahead_product, lookahead;
    uint64_t idle_ticks;
    int64_t target, raw_desired, desired, focus, predicted, dead_low, dead_high, error, desired_velocity;
    int64_t available, safe, applied, candidate, abs_error;
    int64_t braking_offset, stop_target, nominal_target;
    uint64_t next_ordinal;
    uint64_t next_segment;
    bool domain_constraint, at_rest, outside_dead_zone;
    if (state == NULL || observation == NULL || config == NULL || !oracles_e11_config_valid(config) || !oracles_e11_state_valid(state)) {
        if (state == NULL || observation == NULL || config == NULL) result.error = ORACLES_E11_INVALID_ARGUMENT;
        else if (!oracles_e11_config_valid(config)) result.error = ORACLES_E11_INVALID_CONFIG;
        else result.error = ORACLES_E11_INVALID_STATE;
        if (state != NULL) result.state = *state; else oracles_e11_state_initial(&result.state, config);
        return result;
    }
    if (strcmp(state->config_digest, config->digest) != 0) {
        result.error = ORACLES_E11_CONFIG_MISMATCH;
        result.state = *state;
        return result;
    }
    result.error = ORACLES_E11_OK; current = *state;
    if (!oracles_e11_text_valid(observation->session, sizeof(observation->session)) ||
        !oracles_e11_text_valid(observation->snapshot_identity, sizeof(observation->snapshot_identity)) ||
        observation->domain < ORACLES_E11_EXTERIOR || observation->domain > ORACLES_E11_UNDERGROUND ||
        observation->availability < ORACLES_E11_AVAILABLE || observation->availability > ORACLES_E11_UNAVAILABLE ||
        observation->event < ORACLES_E11_NORMAL_VBLANK || observation->event > ORACLES_E11_PRESENTATION ||
        observation->bounds_width <= 0 || observation->bounds_height <= 0 ||
        !oracles_e11_observation_digest(observation, digest) ||
        (observation->digest[0] != '\0' && (!oracles_e11_digest_valid(observation->digest) || strcmp(observation->digest, digest) != 0))) {
        result.error = ORACLES_E11_INVALID_OBSERVATION; result.state = *state; return result;
    }
    if (observation->event != ORACLES_E11_NORMAL_VBLANK) { result.state = current; return result; }
    if (current.status == ORACLES_E11_UNINITIALIZED) {
        if (!oracles_e11_valid_normal(observation, config) || oracles_e11_bootstrap(&current, observation, digest, config, 0u, &result.state) != ORACLES_E11_OK)
            result.state = oracles_e11_desync(&current, ORACLES_E11_VISIBILITY_FALLBACK);
        return result;
    }
    if (current.status == ORACLES_E11_DESYNCHRONIZED) {
        if (current.session[0] != '\0' && strcmp(observation->session, current.session) != 0) { result.state = current; return result; }
        if (!oracles_e11_valid_normal(observation, config) || (current.last_ordinal_present &&
            ((observation->epoch == current.epoch && (observation->domain != current.domain || observation->ordinal <= current.last_ordinal)) ||
             (observation->epoch < current.epoch) || (observation->epoch > current.epoch && observation->domain != current.domain)))) {
            result.state = current; return result;
        }
        if (current.last_ordinal_present && !oracles_e11_u64_inc(current.segment, &next_segment)) {
            result.error = ORACLES_E11_OVERFLOW; result.state = current; return result;
        }
        if (oracles_e11_bootstrap(&current, observation, digest, config, current.last_ordinal_present ? next_segment : current.segment, &result.state) != ORACLES_E11_OK)
            result.state = current;
        return result;
    }
    if (strcmp(observation->session, current.session) != 0) { result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result; }
    if (observation->epoch < current.epoch) { result.state = current; return result; }
    if (observation->epoch > current.epoch) {
        if (observation->domain != current.domain) {
            result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
        }
        if (!oracles_e11_valid_normal(observation, config) ||
            !oracles_e11_u64_inc(current.segment, &next_segment) ||
            oracles_e11_bootstrap(&current, observation, digest, config, next_segment, &result.state) != ORACLES_E11_OK)
            result.state = oracles_e11_desync(&current, ORACLES_E11_VISIBILITY_FALLBACK);
        return result;
    }
    if (observation->ordinal < current.last_ordinal) { result.state = current; return result; }
    if (observation->ordinal == current.last_ordinal) {
        result.state = strcmp(digest, current.last_observation_digest) == 0 ? current : oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
        return result;
    }
    if (!oracles_e11_u64_inc(current.last_ordinal, &next_ordinal) || observation->ordinal != next_ordinal) { result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result; }
    if (!oracles_e11_valid_normal(observation, config)) {
        result.state = oracles_e11_desync(&current, ORACLES_E11_VISIBILITY_FALLBACK); return result;
    }
    if (observation->domain != current.domain) {
        result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
    }
    if (!oracles_e11_bounds(observation, config, &low, &high) ||
        !oracles_e11_i64_sub(observation->link_x, current.last_link_x, &measured) ||
        !oracles_e11_i64_sub(measured, current.filtered_link_velocity, &filtered_delta) ||
        !oracles_e11_round_half_away(filtered_delta, config->velocity_filter_divisor, &product) ||
        !oracles_e11_i64_add(current.filtered_link_velocity, product, &filtered) ||
        !oracles_e11_i64_mul(filtered, config->horizon_ticks, &product) ||
        !oracles_e11_i64_mul(product, config->lookahead_step, &lookahead_product) ||
        !oracles_e11_round_half_away(lookahead_product, ORACLES_E11_F256, &lookahead) ||
        !oracles_e11_clamp(lookahead, -config->lookahead_max, config->lookahead_max, &lookahead)) {
        result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
    }
    at_rest = measured >= -config->rest_tolerance && measured <= config->rest_tolerance;
    if (at_rest) { if (!oracles_e11_u64_inc(current.idle_ticks, &idle_ticks)) { result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result; } }
    else idle_ticks = 0u;
    if (at_rest && oracles_e11_config_rests_where_it_brakes(config)) {
        if (idle_ticks == (uint64_t)ORACLES_E11_REST_FRAMES) {
            /* Link has stopped: pin the target to the point where the
             * camera's braking ends.  Letting the filtered look-ahead decay
             * here would move the target back under a still-moving camera,
             * the visible forward-then-backward twitch. */
            if (!oracles_e11_braking_offset(current.applied_velocity, config, &braking_offset) ||
                !oracles_e11_i64_add(current.camera_pos, braking_offset, &stop_target) ||
                !oracles_e11_i64_sub(observation->link_x, config->framing, &nominal_target) ||
                !oracles_e11_i64_sub(stop_target, nominal_target, &lookahead)) {
                result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
            }
        } else {
            /* Before the rest is pronounced, the look-ahead Link had while
             * moving; after it, the serialised offset from his framed
             * position to that fixed stop, which a state saved at rest
             * resumes.  Both are simply the previous frame's. */
            lookahead = current.lookahead;
        }
    }
    {
        OraclesE11FollowRecenter mode = current.follow_recenter;
        if (!at_rest || oracles_e11_config_rests_where_it_brakes(config)) mode = ORACLES_E11_FOLLOW;
        else if (idle_ticks >= (uint64_t)config->idle_delay) mode = ORACLES_E11_RECENTER;
        if (!oracles_e11_i64_sub(observation->link_x, config->framing, &target) ||
            !oracles_e11_i64_add(target, mode == ORACLES_E11_RECENTER ? 0 : lookahead, &raw_desired) ||
            !oracles_e11_clamp(raw_desired, low, high, &desired) ||
            !oracles_e11_i64_add(current.camera_pos, config->framing, &focus) ||
            !oracles_e11_i64_add(observation->link_x, lookahead, &predicted) ||
            !oracles_e11_i64_sub(focus, config->dead_half_width, &dead_low) ||
            !oracles_e11_i64_add(focus, config->dead_half_width, &dead_high)) {
            result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
        }
        domain_constraint = desired != raw_desired;
        outside_dead_zone = predicted < dead_low || predicted > dead_high;
        if (mode == ORACLES_E11_FOLLOW && !outside_dead_zone) desired = current.camera_pos;
        if (!oracles_e11_i64_sub(desired, current.camera_pos, &error) ||
            !oracles_e11_clamp(error, -config->max_speed, config->max_speed, &desired_velocity)) {
            result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
        }
        if (error != 0) {
            if (error > 0) {
                if (!oracles_e11_i64_sub(high, current.camera_pos, &available)) {
                    result.error = ORACLES_E11_OVERFLOW;
                    result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
                    return result;
                }
            } else if (!oracles_e11_i64_sub(current.camera_pos, low, &available)) {
                result.error = ORACLES_E11_OVERFLOW;
                result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
                return result;
            }
            if (available < 0) available = 0;
            if (!oracles_e11_i64_abs(error, &abs_error)) {
                result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result;
            }
            safe = oracles_e11_stopping_speed((abs_error < available ? abs_error : available), config);
            if (safe < 0) { result.error = ORACLES_E11_OVERFLOW; result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE); return result; }
            desired_velocity = error > 0 ? (desired_velocity < safe ? desired_velocity : safe) : -( (-desired_velocity < safe) ? -desired_velocity : safe);
        }
        if (!oracles_e11_i64_sub(desired_velocity, current.applied_velocity, &product)) {
            result.error = ORACLES_E11_OVERFLOW;
            result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
            return result;
        }
        if (product > config->max_accel || product < -config->max_accel) {
            if (!oracles_e11_i64_add(current.applied_velocity,
                                     product > 0 ? config->max_accel : -config->max_accel,
                                     &applied)) {
                result.error = ORACLES_E11_OVERFLOW;
                result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
                return result;
            }
        } else applied = desired_velocity;
        if (!oracles_e11_clamp(applied, -config->max_speed, config->max_speed, &applied) ||
            !oracles_e11_i64_add(current.camera_pos, applied, &candidate) || candidate < low || candidate > high || candidate < INT32_MIN || candidate > INT32_MAX) {
            result.state = oracles_e11_desync(&current, ORACLES_E11_DOMAIN_BOUND); return result;
        }
        if (!oracles_e11_i64_sub(candidate, current.camera_pos, &applied)) {
            result.error = ORACLES_E11_OVERFLOW;
            result.state = oracles_e11_desync(&current, ORACLES_E11_CONSTRAINT_NONE);
            return result;
        }
        {
            int64_t velocity_delta;
            const bool visible = oracles_e11_visible(candidate, observation->link_x, config);
            if (!oracles_e11_i64_sub(applied, current.applied_velocity, &velocity_delta) ||
                velocity_delta > config->max_accel || velocity_delta < -config->max_accel ||
                !visible) {
                result.state = oracles_e11_desync(&current, !visible ? ORACLES_E11_VISIBILITY_FALLBACK : ORACLES_E11_CONSTRAINT_NONE); return result;
            }
        }
        result.state = current; result.state.status = ORACLES_E11_TRACKING; result.state.camera_pos = (int32_t)candidate;
        result.state.applied_velocity = applied; result.state.filtered_link_velocity = filtered; result.state.lookahead = lookahead;
        result.state.last_link_x = observation->link_x; result.state.last_link_y = observation->link_y; result.state.last_ordinal = observation->ordinal;
        memcpy(result.state.last_observation_digest, digest, sizeof(result.state.last_observation_digest)); result.state.idle_ticks = idle_ticks;
        result.state.follow_recenter = mode; result.state.last_constraint = domain_constraint ? ORACLES_E11_DOMAIN_BOUND : ORACLES_E11_CONSTRAINT_NONE;
        result.state.forced_displacement = measured != 0 && domain_constraint;
    }
    return result;
}

#endif /* ORACLES_ENHANCED_WORLD_SMOOTH_CAMERA_E11_H */
