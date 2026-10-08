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
