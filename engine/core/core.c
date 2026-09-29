#include "core.h"
#include "cgb_boot_rom.h"

#include "gb.h"

#include <stdlib.h>
#include <string.h>

/* About a quarter of a second of audio at 48 kHz; the host drains it every frame. */
#define AUDIO_RING_FRAMES 16384u

struct OraclesCore {
    GB_gameboy_t *gb;
    void *extension;
    int colour_correction;
    uint32_t pixels[ORACLES_SCREEN_PIXELS];
    int16_t audio[AUDIO_RING_FRAMES * 2];
    size_t audio_head;
    size_t audio_count;
};

static uint32_t encode_rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void on_sample(GB_gameboy_t *gb, GB_sample_t *sample)
{
    OraclesCore *core = GB_get_user_data(gb);
    if (core->audio_count >= AUDIO_RING_FRAMES) return; /* the host fell behind: drop */
    const size_t index = (core->audio_head + core->audio_count) % AUDIO_RING_FRAMES;
    core->audio[index * 2] = sample->left;
    core->audio[index * 2 + 1] = sample->right;
    core->audio_count++;
}

OraclesCore *oracles_core_create(const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options)
{
    if (!rom || rom_size < 0x8000u) return NULL;
    OraclesCore *core = calloc(1, sizeof *core);
    if (!core) return NULL;
    /* Deterministic power-on state: the harness relies on it, players do not notice. */
    GB_random_set_enabled(false);
    core->gb = GB_init(GB_alloc(), GB_MODEL_CGB_E);
    if (!core->gb) { free(core); return NULL; }
    GB_set_user_data(core->gb, core);
    GB_load_boot_rom_from_buffer(core->gb, oracles_cgb_boot_rom, oracles_cgb_boot_rom_size);
    GB_load_rom_from_buffer(core->gb, rom, rom_size);
    GB_set_rgb_encode_callback(core->gb, encode_rgb);
    oracles_core_set_colour_correction(core, options && options->colour_correction);
    GB_set_pixels_output(core->gb, core->pixels);
    if (options && options->sample_rate_hz) {
        GB_set_sample_rate(core->gb, options->sample_rate_hz);
        /* The hardware's own high-pass: no DC offset, so discontinuities do not pop. */
        GB_set_highpass_filter_mode(core->gb, GB_HIGHPASS_ACCURATE);
        GB_apu_set_sample_callback(core->gb, on_sample);
    }
    /* SameBoy makes a key bounce for a few thousand cycles after each change, and draws the bounce from the APU's cycle
     * counter, which the audio sample rate empties sooner or later: with it the emulated state depends on the host's
     * audio, and a windowed session is not the game its replay is.  Off, unless a route recorded with it asks. */
    GB_set_emulate_joypad_bouncing(core->gb, false);
    GB_set_key_mask(core->gb, 0);
    return core;
}

void oracles_core_destroy(OraclesCore *core)
{
    if (!core) return;
    GB_free(core->gb);
    free(core->gb);
    free(core);
}

void oracles_core_set_joypad_bouncing(OraclesCore *core, int enabled)
{
    GB_set_emulate_joypad_bouncing(core->gb, enabled != 0);
}

void oracles_core_set_keys(OraclesCore *core, unsigned key_mask)
{
    GB_set_key_mask(core->gb, (GB_key_mask_t)(key_mask & 0xffu));
}

void oracles_core_set_colour_correction(OraclesCore *core, int enabled)
{
    core->colour_correction = enabled != 0;
    GB_set_color_correction_mode(core->gb, enabled ? GB_COLOR_CORRECTION_MODERN_BALANCED : GB_COLOR_CORRECTION_DISABLED);
}

int oracles_core_colour_correction(const OraclesCore *core)
{
    return core->colour_correction;
}

uint32_t oracles_core_convert_rgb555(const OraclesCore *core, uint16_t colour)
{
    /* SameBoy takes a mutable handle, and reads it only. */
    return GB_convert_rgb15((GB_gameboy_t *)core->gb, colour, false);
}

void oracles_core_set_sample_rate(OraclesCore *core, unsigned sample_rate_hz)
{
    if (sample_rate_hz) GB_set_sample_rate(core->gb, sample_rate_hz);
}

void oracles_core_run_frame(OraclesCore *core)
{
    GB_run_frame(core->gb);
}

const uint32_t *oracles_core_pixels(const OraclesCore *core)
{
    return core->pixels;
}

size_t oracles_core_take_audio(OraclesCore *core, int16_t *out, size_t max_samples)
{
    size_t frames = max_samples / 2;
    if (frames > core->audio_count) frames = core->audio_count;
    for (size_t i = 0; i < frames; i++) {
        const size_t index = (core->audio_head + i) % AUDIO_RING_FRAMES;
        out[i * 2] = core->audio[index * 2];
        out[i * 2 + 1] = core->audio[index * 2 + 1];
    }
    core->audio_head = (core->audio_head + frames) % AUDIO_RING_FRAMES;
    core->audio_count -= frames;
    return frames * 2;
}

size_t oracles_core_sram_size(OraclesCore *core)
{
    const int size = GB_save_battery_size(core->gb);
    return size > 0 ? (size_t)size : 0;
}

int oracles_core_load_sram(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    if (size != oracles_core_sram_size(core)) return -1;
    GB_load_battery_from_buffer(core->gb, buffer, size);
    return 0;
}

int oracles_core_save_sram(OraclesCore *core, uint8_t *buffer, size_t size)
{
    if (size != oracles_core_sram_size(core)) return -1;
    return GB_save_battery_to_buffer(core->gb, buffer, size) == 0 ? 0 : -1;
}

size_t oracles_core_state_size(OraclesCore *core)
{
    return GB_get_save_state_size(core->gb);
}

int oracles_core_save_state(OraclesCore *core, uint8_t *buffer, size_t size)
{
    if (size != GB_get_save_state_size(core->gb)) return -1;
    GB_save_state_to_buffer(core->gb, buffer);
    return 0;
}

int oracles_core_load_state(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    return GB_load_state_from_buffer(core->gb, buffer, size) == 0 ? 0 : -1;
}

#ifndef ORACLES_SAMEBOY_VERSION
#error "ORACLES_SAMEBOY_VERSION is defined by the build from config/sameboy.json"
#endif

const char *oracles_core_version(void)
{
    return "sameboy-" ORACLES_SAMEBOY_VERSION;
}

struct GB_gameboy_s *oracles_core_gb(OraclesCore *core)
{
    return core->gb;
}

void oracles_core_set_extension(OraclesCore *core, void *extension) { core->extension = extension; }
void *oracles_core_extension(OraclesCore *core) { return core->extension; }
