/* SameBoy behind the core's interface (core_internal.h): the only file of the
 * product that includes gb.h. */
#include "core_internal.h"
#include "cgb_boot_rom.h"

#include "gb.h"

#include <stdlib.h>

#ifndef ORACLES_SAMEBOY_VERSION
#error "ORACLES_SAMEBOY_VERSION is defined by the build from config/sameboy.json"
#endif

/* The vblank types are SameBoy's own values. */
_Static_assert(ORACLES_VBLANK_NORMAL == (int)GB_VBLANK_TYPE_NORMAL_FRAME && ORACLES_VBLANK_LCD_OFF == (int)GB_VBLANK_TYPE_LCD_OFF
               && ORACLES_VBLANK_ARTIFICIAL == (int)GB_VBLANK_TYPE_ARTIFICIAL && ORACLES_VBLANK_REPEAT == (int)GB_VBLANK_TYPE_REPEAT
               && ORACLES_VBLANK_SKIPPED == (int)GB_VBLANK_TYPE_SKIPPED_FRAME, "the vblank types are SameBoy's");

static GB_gameboy_t *gb_of(const OraclesCore *core) { return core->backend; }

static uint32_t encode_rgb(GB_gameboy_t *gb, uint8_t r, uint8_t g, uint8_t b)
{
    (void)gb;
    return 0xff000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
}

static void on_sample(GB_gameboy_t *gb, GB_sample_t *sample)
{
    oracles_core_push_audio(GB_get_user_data(gb), sample->left, sample->right);
}

int oracles_core_sameboy_init(OraclesCore *core, const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options)
{
    /* Deterministic power-on state: the harness relies on it, players do not notice. */
    GB_random_set_enabled(false);
    GB_gameboy_t *gb = GB_init(GB_alloc(), GB_MODEL_CGB_E);
    if (!gb) return -1;
    core->backend = gb;
    /* SameBoy's user data is the core: the callbacks below find it there, and so do the diagnostics that read SameBoy. */
    GB_set_user_data(gb, core);
    GB_load_boot_rom_from_buffer(gb, oracles_cgb_boot_rom, oracles_cgb_boot_rom_size);
    GB_load_rom_from_buffer(gb, rom, rom_size);
    GB_set_rgb_encode_callback(gb, encode_rgb);
    oracles_core_set_colour_correction(core, options && options->colour_correction);
    GB_set_pixels_output(gb, core->pixels);
    if (options && options->sample_rate_hz) {
        GB_set_sample_rate(gb, options->sample_rate_hz);
        /* The hardware's own high-pass: no DC offset, so discontinuities do not pop. */
        GB_set_highpass_filter_mode(gb, GB_HIGHPASS_ACCURATE);
        GB_apu_set_sample_callback(gb, on_sample);
    }
    /* SameBoy makes a key bounce for a few thousand cycles after each change, and draws the bounce from the APU's cycle
     * counter, which the audio sample rate empties sooner or later: with it the emulated state depends on the host's
     * audio, and a windowed session is not the game its replay is.  Off, unless a route recorded with it asks. */
    GB_set_emulate_joypad_bouncing(gb, false);
    GB_set_key_mask(gb, 0);
    return 0;
}

static void destroy(OraclesCore *core)
{
    GB_free(gb_of(core));
    free(gb_of(core));
}

static int set_joypad_bouncing(OraclesCore *core, int enabled)
{
    GB_set_emulate_joypad_bouncing(gb_of(core), enabled != 0);
    return 0;
}

static void set_keys(OraclesCore *core, unsigned key_mask) { GB_set_key_mask(gb_of(core), (GB_key_mask_t)key_mask); }

static void set_colour_correction(OraclesCore *core, int enabled)
{
    GB_set_color_correction_mode(gb_of(core), enabled ? GB_COLOR_CORRECTION_MODERN_BALANCED : GB_COLOR_CORRECTION_DISABLED);
}

static uint32_t convert_rgb555(const OraclesCore *core, uint16_t colour)
{
    /* SameBoy takes a mutable handle, and reads it only. */
    return GB_convert_rgb15(gb_of(core), colour, false);
}

static void set_sample_rate(OraclesCore *core, unsigned sample_rate_hz) { GB_set_sample_rate(gb_of(core), sample_rate_hz); }

static void run_frame(OraclesCore *core) { GB_run_frame(gb_of(core)); }

static size_t sram_size(OraclesCore *core)
{
    const int size = GB_save_battery_size(gb_of(core));
    return size > 0 ? (size_t)size : 0;
}

static int load_sram(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    GB_load_battery_from_buffer(gb_of(core), buffer, size);
    return 0;
}

static int save_sram(OraclesCore *core, uint8_t *buffer, size_t size)
{
    return GB_save_battery_to_buffer(gb_of(core), buffer, size) == 0 ? 0 : -1;
}

static size_t state_size(OraclesCore *core) { return GB_get_save_state_size(gb_of(core)); }

static int save_state(OraclesCore *core, uint8_t *buffer, size_t size)
{
    (void)size;
    GB_save_state_to_buffer(gb_of(core), buffer);
    return 0;
}

static int load_state(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    return GB_load_state_from_buffer(gb_of(core), buffer, size) == 0 ? 0 : -1;
}

static uint8_t *memory(OraclesCore *core, OraclesCoreRegion region, size_t *size, uint16_t *bank)
{
    static const GB_direct_access_t access[] = {
        [ORACLES_CORE_WRAM] = GB_DIRECT_ACCESS_RAM,           [ORACLES_CORE_VRAM] = GB_DIRECT_ACCESS_VRAM,
        [ORACLES_CORE_OAM] = GB_DIRECT_ACCESS_OAM,            [ORACLES_CORE_HRAM] = GB_DIRECT_ACCESS_HRAM,
        [ORACLES_CORE_IO] = GB_DIRECT_ACCESS_IO,              [ORACLES_CORE_ROM] = GB_DIRECT_ACCESS_ROM,
        [ORACLES_CORE_CART_RAM] = GB_DIRECT_ACCESS_CART_RAM,  [ORACLES_CORE_BG_PALETTES] = GB_DIRECT_ACCESS_BGP,
        [ORACLES_CORE_OBJ_PALETTES] = GB_DIRECT_ACCESS_OBP,
    };
    if ((unsigned)region >= sizeof access / sizeof access[0]) {
        if (size) *size = 0;
        if (bank) *bank = 0;
        return NULL;
    }
    return GB_get_direct_access(gb_of(core), access[region], size, bank);
}

static uint8_t peek(OraclesCore *core, uint16_t address) { return GB_safe_read_memory(gb_of(core), address); }

static OraclesCoreRegisters registers(const OraclesCore *core)
{
    const GB_registers_t *r = GB_get_registers(gb_of(core));
    const OraclesCoreRegisters out = { r->af, r->bc, r->de, r->hl, r->sp, r->pc };
    return out;
}

static void set_registers(OraclesCore *core, const OraclesCoreRegisters *in)
{
    GB_registers_t *r = GB_get_registers(gb_of(core));
    r->af = in->af; r->bc = in->bc; r->de = in->de; r->hl = in->hl; r->sp = in->sp; r->pc = in->pc;
}

/* ---- hooks: SameBoy's callbacks, armed only for the hooks set ---------------------- */

static void on_execute(GB_gameboy_t *gb, uint16_t pc, uint8_t opcode)
{
    OraclesCore *core = GB_get_user_data(gb);
    core->hooks.on_execute(core->hooks_opaque, pc, opcode, GB_get_registers(gb)->sp);
}

static uint8_t on_read(GB_gameboy_t *gb, uint16_t address, uint8_t data)
{
    OraclesCore *core = GB_get_user_data(gb);
    core->hooks.on_read(core->hooks_opaque, address, GB_get_registers(gb)->sp);
    return data;
}

static bool on_write(GB_gameboy_t *gb, uint16_t address, uint8_t value)
{
    OraclesCore *core = GB_get_user_data(gb);
    core->hooks.on_write(core->hooks_opaque, address, value);
    return true;
}

static void on_vblank(GB_gameboy_t *gb, GB_vblank_type_t type)
{
    OraclesCore *core = GB_get_user_data(gb);
    core->hooks.on_vblank(core->hooks_opaque, (OraclesVblankType)type);
}

static void hooks_changed(OraclesCore *core)
{
    GB_gameboy_t *gb = gb_of(core);
    GB_set_execution_callback(gb, core->hooks.on_execute ? on_execute : NULL);
    GB_set_read_memory_callback(gb, core->hooks.on_read ? on_read : NULL);
    GB_set_write_memory_callback(gb, core->hooks.on_write ? on_write : NULL);
    GB_set_vblank_callback(gb, core->hooks.on_vblank ? on_vblank : NULL);
}

const OraclesCoreOps oracles_core_sameboy_ops = {
    destroy, set_joypad_bouncing, set_keys, set_colour_correction, convert_rgb555, set_sample_rate, run_frame,
    sram_size, load_sram, save_sram, state_size, save_state, load_state, memory, peek, registers, set_registers,
    hooks_changed, "sameboy-" ORACLES_SAMEBOY_VERSION,
};
