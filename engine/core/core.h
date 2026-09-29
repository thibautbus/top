/* The execution core: SameBoy running the user's ROM.
 *
 * This is the only authority on gameplay.  The host reads from it (pixels,
 * audio, SRAM, and later memory through the guest bus) and drives it one
 * frame at a time; nothing here knows about windows, files or SDL. */
#ifndef ORACLES_CORE_H
#define ORACLES_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ORACLES_SCREEN_WIDTH 160u
#define ORACLES_SCREEN_HEIGHT 144u
#define ORACLES_SCREEN_PIXELS (ORACLES_SCREEN_WIDTH * ORACLES_SCREEN_HEIGHT)

/* Same bit layout as the Game Boy joypad mask used by SameBoy. */
enum {
    ORACLES_KEY_RIGHT = 1u << 0,
    ORACLES_KEY_LEFT = 1u << 1,
    ORACLES_KEY_UP = 1u << 2,
    ORACLES_KEY_DOWN = 1u << 3,
    ORACLES_KEY_A = 1u << 4,
    ORACLES_KEY_B = 1u << 5,
    ORACLES_KEY_SELECT = 1u << 6,
    ORACLES_KEY_START = 1u << 7
};

typedef struct OraclesCoreOptions {
    /* Audio sample rate; 0 disables audio generation. */
    unsigned sample_rate_hz;
    /* 0: raw RGB555 to RGB888, the pinned Faithful pipeline.
     * 1: SameBoy's "modern balanced" colour correction, closer to a real LCD. */
    int colour_correction;
} OraclesCoreOptions;

typedef struct OraclesCore OraclesCore;

/* Boots the embedded free CGB boot ROM with the given ROM image (copied).
 * Returns NULL if the ROM cannot be loaded. */
OraclesCore *oracles_core_create(const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options);
void oracles_core_destroy(OraclesCore *core);

/* SameBoy's emulation of joypad bouncing, off by default: it makes the emulated state depend on the audio sample rate
 * and a session would not be the game its replay is.  On only to replay a route recorded with it, before its first frame. */
void oracles_core_set_joypad_bouncing(OraclesCore *core, int enabled);

void oracles_core_set_keys(OraclesCore *core, unsigned key_mask);
/* Switches the colour pipeline at any time; the next frame uses it. */
void oracles_core_set_colour_correction(OraclesCore *core, int enabled);
int oracles_core_colour_correction(const OraclesCore *core);
/* The pixel the core's palette encoder gives an RGB555 colour under the
 * active pipeline: the native renderer converts through it, so that its
 * image equals the core's whatever the player chose. */
uint32_t oracles_core_convert_rgb555(const OraclesCore *core, uint16_t colour);
/* Changes the number of samples produced per second, for audio rate control:
 * a small deviation from the device rate keeps its queue at a steady level. */
void oracles_core_set_sample_rate(OraclesCore *core, unsigned sample_rate_hz);
void oracles_core_run_frame(OraclesCore *core);

/* 160x144 numeric 0xAARRGGBB pixels of the last completed frame. */
const uint32_t *oracles_core_pixels(const OraclesCore *core);

/* Moves up to max_samples interleaved stereo int16 samples produced since the
 * last call into out; returns the number of samples written (always even). */
size_t oracles_core_take_audio(OraclesCore *core, int16_t *out, size_t max_samples);

/* Cartridge RAM (battery-backed) as SameBoy saves it: raw bytes for the
 * Oracle games, which have no clock. */
size_t oracles_core_sram_size(OraclesCore *core);
int oracles_core_load_sram(OraclesCore *core, const uint8_t *buffer, size_t size);
int oracles_core_save_sram(OraclesCore *core, uint8_t *buffer, size_t size);

/* Guest state as SameBoy serialises it (its own versioned format). */
size_t oracles_core_state_size(OraclesCore *core);
int oracles_core_save_state(OraclesCore *core, uint8_t *buffer, size_t size);
int oracles_core_load_state(OraclesCore *core, const uint8_t *buffer, size_t size);
/* Version string of the core, part of every composite savestate. */
const char *oracles_core_version(void);

/* The underlying SameBoy instance, for the harness and the guest bus.
 * The core keeps itself as SameBoy's user data; an extension (the guest)
 * hangs off the core rather than replacing that pointer. */
struct GB_gameboy_s *oracles_core_gb(OraclesCore *core);
void oracles_core_set_extension(OraclesCore *core, void *extension);
void *oracles_core_extension(OraclesCore *core);

#ifdef __cplusplus
}
#endif

#endif
