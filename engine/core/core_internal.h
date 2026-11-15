/* What core.c shares with the implementation of each core (core_sameboy.c, core_mgba.c):
 * the instance every core fills, and the table of its functions.  core.c
 * dispatches the public API (core.h) through that table; the audio ring and
 * the hooks are common.  Not a public interface: the diagnostics that read an
 * emulator itself include it, and read it through backend. */
#ifndef ORACLES_CORE_INTERNAL_H
#define ORACLES_CORE_INTERNAL_H

#include "core.h"

/* About a quarter of a second of audio at 48 kHz; the host drains it every frame. */
#define ORACLES_CORE_AUDIO_FRAMES 16384u
/* The boot ROM turns the sound circuits on in its third frame: their DACs, at rest, move the output by a step the
 * high-pass brings back within a few frames, heard as a pop a second before the boot sound (on both cores).  The audio
 * of the frames until then is silence; the boot sound starts in frame 66. */
#define ORACLES_CORE_POWER_ON_QUIET_FRAMES 16u
/* A loaded state leaves out part of the emulator's audio output (on SameBoy, its high-pass and its channels' levels),
 * which starts again from the core's own: a step heard as a pop, louder than the one at power on.  The audio of the
 * frames after a load fades in, from the last sample the host had; after the host cleared its audio (the pause), from
 * silence. */
#define ORACLES_CORE_FADE_IN_FRAMES 3u

typedef struct OraclesCoreOps {
    void (*destroy)(OraclesCore *core);
    int (*set_joypad_bouncing)(OraclesCore *core, int enabled);
    void (*set_keys)(OraclesCore *core, unsigned key_mask);
    void (*set_colour_correction)(OraclesCore *core, int enabled);
    uint32_t (*convert_rgb555)(const OraclesCore *core, uint16_t colour);
    void (*set_sample_rate)(OraclesCore *core, unsigned sample_rate_hz);
    void (*run_frame)(OraclesCore *core);
    size_t (*sram_size)(OraclesCore *core);
    int (*load_sram)(OraclesCore *core, const uint8_t *buffer, size_t size);
    int (*save_sram)(OraclesCore *core, uint8_t *buffer, size_t size);
    size_t (*state_size)(OraclesCore *core);
    int (*save_state)(OraclesCore *core, uint8_t *buffer, size_t size);
    int (*load_state)(OraclesCore *core, const uint8_t *buffer, size_t size);
    uint8_t *(*memory)(OraclesCore *core, OraclesCoreRegion region, size_t *size, uint16_t *bank);
    uint8_t (*peek)(OraclesCore *core, uint16_t address);
    void (*write_vram)(OraclesCore *core, size_t offset, const uint8_t *bytes, size_t count);
    OraclesCoreRegisters (*registers)(const OraclesCore *core);
    void (*set_registers)(OraclesCore *core, const OraclesCoreRegisters *registers);
    /* Called after core->hooks changed: arms the core's own callbacks for the members set, disarms the others. */
    void (*hooks_changed)(OraclesCore *core);
    const char *version;
} OraclesCoreOps;

struct OraclesCore {
    const OraclesCoreOps *ops;
    OraclesCoreKind kind;
    void *backend;                     /* the emulator's own instance: SameBoy's GB_gameboy_t, or mGBA's state */
    void *extension;                   /* free for such a diagnostic: SameBoy's callbacks find the core as their user data */
    int colour_correction;
    OraclesCoreHooks hooks;
    void *hooks_opaque;
    uint32_t pixels[ORACLES_SCREEN_PIXELS];
    int16_t audio[ORACLES_CORE_AUDIO_FRAMES * 2];
    size_t audio_head;
    size_t audio_count;
    /* The frames run since power on, counted to ORACLES_CORE_POWER_ON_QUIET_FRAMES; that many once a state is loaded. */
    unsigned power_on_frames;
    unsigned fade_in_frames;           /* the frames of the fade in still to run */
    int16_t fade_from[2];              /* the sample it fades in from, left and right */
};

/* One stereo frame into the ring; dropped when the host fell behind. */
static inline void oracles_core_push_audio(OraclesCore *core, int16_t left, int16_t right)
{
    if (core->audio_count >= ORACLES_CORE_AUDIO_FRAMES) return;
    const size_t index = (core->audio_head + core->audio_count) % ORACLES_CORE_AUDIO_FRAMES;
    core->audio[index * 2] = left;
    core->audio[index * 2 + 1] = right;
    core->audio_count++;
}

/* Each core's creation: fills `core` (calloc'ed by core.c, kind and ops set) or returns -1. */
int oracles_core_sameboy_init(OraclesCore *core, const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options);
extern const OraclesCoreOps oracles_core_sameboy_ops;
int oracles_core_mgba_init(OraclesCore *core, const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options);
extern const OraclesCoreOps oracles_core_mgba_ops;

#endif
