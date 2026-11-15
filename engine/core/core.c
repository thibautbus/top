/* The public API of the core (core.h), dispatched to the core the instance
 * runs (core_internal.h); the audio ring and the hooks are the same for every
 * core. */
#include "core_internal.h"

#include <stdlib.h>
#include <string.h>

OraclesCore *oracles_core_create(const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options)
{
    if (!rom || rom_size < 0x8000u) return NULL;
    const OraclesCoreKind kind = options ? options->kind : ORACLES_CORE_SAMEBOY;
    if (kind != ORACLES_CORE_SAMEBOY && kind != ORACLES_CORE_MGBA) return NULL;
    OraclesCore *core = calloc(1, sizeof *core);
    if (!core) return NULL;
    core->kind = kind;
    core->ops = kind == ORACLES_CORE_MGBA ? &oracles_core_mgba_ops : &oracles_core_sameboy_ops;
    const int failed = kind == ORACLES_CORE_MGBA ? oracles_core_mgba_init(core, rom, rom_size, options)
                                                 : oracles_core_sameboy_init(core, rom, rom_size, options);
    if (failed) { free(core); return NULL; }
    return core;
}

void oracles_core_destroy(OraclesCore *core)
{
    if (!core) return;
    core->ops->destroy(core);
    free(core);
}

OraclesCoreKind oracles_core_kind(const OraclesCore *core) { return core->kind; }

int oracles_core_set_joypad_bouncing(OraclesCore *core, int enabled)
{
    return core->ops->set_joypad_bouncing(core, enabled);
}

void oracles_core_set_keys(OraclesCore *core, unsigned key_mask) { core->ops->set_keys(core, key_mask & 0xffu); }

void oracles_core_set_colour_correction(OraclesCore *core, int enabled)
{
    core->colour_correction = enabled != 0;
    core->ops->set_colour_correction(core, core->colour_correction);
}

int oracles_core_colour_correction(const OraclesCore *core) { return core->colour_correction; }

uint32_t oracles_core_convert_rgb555(const OraclesCore *core, uint16_t colour)
{
    return core->ops->convert_rgb555(core, colour);
}

void oracles_core_set_sample_rate(OraclesCore *core, unsigned sample_rate_hz)
{
    if (sample_rate_hz) core->ops->set_sample_rate(core, sample_rate_hz);
}

/* The samples the frame added after `queued`, the `frame`th of a fade in: from fade_from, the last sample the host had,
 * to their own, by a gain cubed, low while the pop of a load lasts. */
static void fade_audio(OraclesCore *core, size_t queued, unsigned frame)
{
    const size_t count = core->audio_count - queued;
    for (size_t i = 0; i < count; i++) {
        const double t = (frame + (double)i / count) / ORACLES_CORE_FADE_IN_FRAMES;
        const double gain = t * t * t;
        const size_t index = (core->audio_head + queued + i) % ORACLES_CORE_AUDIO_FRAMES;
        for (unsigned side = 0; side < 2; side++) {
            int16_t *sample = &core->audio[index * 2 + side];
            *sample = (int16_t)(*sample * gain + core->fade_from[side] * (1 - gain));
        }
    }
}

void oracles_core_run_frame(OraclesCore *core)
{
    const size_t queued = core->audio_count;
    core->ops->run_frame(core);
    if (core->power_on_frames < ORACLES_CORE_POWER_ON_QUIET_FRAMES) {
        core->power_on_frames++;
        for (size_t i = queued; i < core->audio_count; i++) {
            const size_t index = (core->audio_head + i) % ORACLES_CORE_AUDIO_FRAMES;
            core->audio[index * 2] = core->audio[index * 2 + 1] = 0;
        }
    }
    else if (core->fade_in_frames) {
        fade_audio(core, queued, ORACLES_CORE_FADE_IN_FRAMES - core->fade_in_frames--);
    }
}

const uint32_t *oracles_core_pixels(const OraclesCore *core) { return core->pixels; }

void oracles_core_audio_cleared(OraclesCore *core)
{
    /* The samples not taken yet are dropped with the host's, and the last one, which a load fades from, is silence. */
    core->audio_count = 0;
    const size_t last = (core->audio_head + ORACLES_CORE_AUDIO_FRAMES - 1) % ORACLES_CORE_AUDIO_FRAMES;
    core->audio[last * 2] = core->audio[last * 2 + 1] = 0;
    core->fade_from[0] = core->fade_from[1] = 0;
    core->fade_in_frames = ORACLES_CORE_FADE_IN_FRAMES;
}

size_t oracles_core_take_audio(OraclesCore *core, int16_t *out, size_t max_samples)
{
    size_t frames = max_samples / 2;
    if (frames > core->audio_count) frames = core->audio_count;
    for (size_t i = 0; i < frames; i++) {
        const size_t index = (core->audio_head + i) % ORACLES_CORE_AUDIO_FRAMES;
        out[i * 2] = core->audio[index * 2];
        out[i * 2 + 1] = core->audio[index * 2 + 1];
    }
    core->audio_head = (core->audio_head + frames) % ORACLES_CORE_AUDIO_FRAMES;
    core->audio_count -= frames;
    return frames * 2;
}

size_t oracles_core_sram_size(OraclesCore *core) { return core->ops->sram_size(core); }

int oracles_core_load_sram(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    if (size != oracles_core_sram_size(core)) return -1;
    return core->ops->load_sram(core, buffer, size);
}

int oracles_core_save_sram(OraclesCore *core, uint8_t *buffer, size_t size)
{
    if (size != oracles_core_sram_size(core)) return -1;
    return core->ops->save_sram(core, buffer, size);
}

size_t oracles_core_state_size(OraclesCore *core) { return core->ops->state_size(core); }

int oracles_core_save_state(OraclesCore *core, uint8_t *buffer, size_t size)
{
    if (size != oracles_core_state_size(core)) return -1;
    return core->ops->save_state(core, buffer, size);
}

int oracles_core_load_state(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    const int result = core->ops->load_state(core, buffer, size);
    if (result == 0) {
        /* The ring keeps the samples already taken: its last one, taken or not, is the last the host has. */
        const size_t last = (core->audio_head + core->audio_count + ORACLES_CORE_AUDIO_FRAMES - 1)
                            % ORACLES_CORE_AUDIO_FRAMES;
        core->fade_from[0] = core->audio[last * 2];
        core->fade_from[1] = core->audio[last * 2 + 1];
        core->power_on_frames = ORACLES_CORE_POWER_ON_QUIET_FRAMES;
        core->fade_in_frames = ORACLES_CORE_FADE_IN_FRAMES;
    }
    return result;
}

const char *oracles_core_version(const OraclesCore *core) { return core->ops->version; }

const char *oracles_core_kind_version(OraclesCoreKind kind)
{
    return (kind == ORACLES_CORE_MGBA ? &oracles_core_mgba_ops : &oracles_core_sameboy_ops)->version;
}

uint8_t *oracles_core_memory(OraclesCore *core, OraclesCoreRegion region, size_t *size, uint16_t *bank)
{
    return core->ops->memory(core, region, size, bank);
}

uint8_t oracles_core_peek(OraclesCore *core, uint16_t address) { return core->ops->peek(core, address); }

void oracles_core_write_vram(OraclesCore *core, size_t offset, const uint8_t *bytes, size_t count) { core->ops->write_vram(core, offset, bytes, count); }

OraclesCoreRegisters oracles_core_registers(const OraclesCore *core) { return core->ops->registers(core); }

void oracles_core_set_registers(OraclesCore *core, const OraclesCoreRegisters *registers)
{
    core->ops->set_registers(core, registers);
}

void oracles_core_set_hooks(OraclesCore *core, const OraclesCoreHooks *hooks, void *opaque)
{
    if (hooks) core->hooks = *hooks;
    else memset(&core->hooks, 0, sizeof core->hooks);
    core->hooks_opaque = hooks ? opaque : NULL;
    core->ops->hooks_changed(core);
}
