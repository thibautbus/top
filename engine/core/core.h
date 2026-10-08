/* The execution core: an emulator running the user's ROM, SameBoy or mGBA;
 * only engine/core/ knows which.
 *
 * This is the only authority on gameplay.  The host reads from it (pixels,
 * audio, SRAM, memory and registers through the guest bus) and drives it one
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

/* Same bit layout as SameBoy's joypad mask; every core takes it. */
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

typedef enum OraclesCoreKind {
    ORACLES_CORE_SAMEBOY = 0,   /* the reference: accurate */
    ORACLES_CORE_MGBA = 1       /* about seven times lighter, for small devices */
} OraclesCoreKind;

typedef struct OraclesCoreOptions {
    /* Audio sample rate; 0 disables audio generation. */
    unsigned sample_rate_hz;
    /* 0: raw RGB555 to RGB888, the pinned Faithful pipeline.
     * 1: SameBoy's "modern balanced" colour correction, closer to a real LCD. */
    int colour_correction;
    /* The core to create. */
    OraclesCoreKind kind;
} OraclesCoreOptions;

typedef struct OraclesCore OraclesCore;

/* Boots the embedded free CGB boot ROM with the given ROM image (copied), on
 * a CGB.  Returns NULL if the ROM cannot be loaded or the core is not built. */
OraclesCore *oracles_core_create(const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options);
void oracles_core_destroy(OraclesCore *core);

OraclesCoreKind oracles_core_kind(const OraclesCore *core);

/* SameBoy's emulation of joypad bouncing, off by default: it makes the emulated state depend on the audio sample rate
 * and a session would not be the game its replay is.  On only to replay a route recorded with it, before its first frame.
 * Returns 0, or -1 when the core has no such emulation (mGBA). */
int oracles_core_set_joypad_bouncing(OraclesCore *core, int enabled);

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
/* Runs to the end of the next frame, which every vblank ends, whatever its type (OraclesVblankType). */
void oracles_core_run_frame(OraclesCore *core);

/* 160x144 numeric 0xAARRGGBB pixels of the last completed frame: from the
 * on_vblank hook on, the frame that hook announces. */
const uint32_t *oracles_core_pixels(const OraclesCore *core);

/* Moves up to max_samples interleaved stereo int16 samples produced since the
 * last call into out; returns the number of samples written (always even). */
size_t oracles_core_take_audio(OraclesCore *core, int16_t *out, size_t max_samples);

/* Cartridge RAM (battery-backed): raw bytes for the Oracle games, which have
 * no clock, the same on every core. */
size_t oracles_core_sram_size(OraclesCore *core);
int oracles_core_load_sram(OraclesCore *core, const uint8_t *buffer, size_t size);
int oracles_core_save_sram(OraclesCore *core, uint8_t *buffer, size_t size);

/* Guest state as the core serialises it (its own versioned format); a state
 * belongs to the core that wrote it.  Taken and loaded between frames. */
size_t oracles_core_state_size(OraclesCore *core);
int oracles_core_save_state(OraclesCore *core, uint8_t *buffer, size_t size);
int oracles_core_load_state(OraclesCore *core, const uint8_t *buffer, size_t size);
/* Version string of the instance's core ("sameboy-1.0.3"), part of every
 * composite savestate. */
const char *oracles_core_version(const OraclesCore *core);

/* ---- memory and registers, for the guest bus ------------------------------------ */

/* The core's own memory, live: what the game reads and writes.  Each region
 * has the same size and layout on every core. */
typedef enum OraclesCoreRegion {
    ORACLES_CORE_WRAM,           /* 0x8000 bytes, the 8 banks; bank: the one SVBK maps at $d000 */
    ORACLES_CORE_VRAM,           /* 0x4000 bytes, the 2 banks; bank: the one VBK maps */
    ORACLES_CORE_OAM,            /* 0xa0 bytes */
    ORACLES_CORE_HRAM,           /* 0x7f bytes, $ff80-$fffe */
    ORACLES_CORE_IO,             /* 0x80 bytes, $ff00-$ff7f: the core's own register file, raw.  LCDC, STAT, LY, LYC,
                                  * SCY, SCX, WY, WX and SVBK are the same on every core; a register a core computes when
                                  * it is read (DIV, VBK, HDMA5, JOYP) is not */
    ORACLES_CORE_ROM,            /* the whole ROM; bank: the one mapped at $4000 */
    ORACLES_CORE_CART_RAM,       /* the cartridge's RAM; bank: the one mapped at $a000 */
    ORACLES_CORE_BG_PALETTES,    /* 0x40 bytes, the 32 background colours, RGB555 little-endian */
    ORACLES_CORE_OBJ_PALETTES    /* 0x40 bytes, the 32 object colours */
} OraclesCoreRegion;

/* A pointer to the region, its size and its bank (either may be NULL); NULL
 * and 0 for a region the core does not have. */
uint8_t *oracles_core_memory(OraclesCore *core, OraclesCoreRegion region, size_t *size, uint16_t *bank);

/* A read of the bus without side effects (no register read clears anything). */
uint8_t oracles_core_peek(OraclesCore *core, uint16_t address);

typedef struct OraclesCoreRegisters {
    uint16_t af, bc, de, hl, sp, pc;
} OraclesCoreRegisters;

/* The CPU's registers, copied; written back whole.  Coherent, and writable,
 * between frames and from on_execute (the call transaction moves SP and sets A
 * and C there), where PC is the core's own (the opcode's address or the next
 * byte's: use the pc on_execute receives).  From on_read and on_write, a core
 * may be in the middle of an instruction: their values depend on the core. */
OraclesCoreRegisters oracles_core_registers(const OraclesCore *core);
void oracles_core_set_registers(OraclesCore *core, const OraclesCoreRegisters *registers);

/* ---- hooks ------------------------------------------------------------------------ */

/* The kind of the frame that ends, as SameBoy names them; the values are SameBoy's. */
typedef enum OraclesVblankType {
    ORACLES_VBLANK_NORMAL = 0,       /* a frame the LCD scanned */
    ORACLES_VBLANK_LCD_OFF = 1,      /* a frame's time passed with the LCD off */
    ORACLES_VBLANK_ARTIFICIAL = 2,   /* the write of LCDC that turns the LCD back on more than ten lines after the last
                                      * vblank: it ends a frame there, in the middle of the instruction */
    ORACLES_VBLANK_REPEAT = 3,       /* the first frame scanned after the LCD turns on, which a CGB does not show: the
                                      * screen keeps the previous image */
    ORACLES_VBLANK_SKIPPED = 4       /* a frame skipped (unused: no turbo) */
} OraclesVblankType;

/* What the core calls while it runs, each with the opaque pointer given with
 * them; a NULL member is not called, and costs nothing.
 *   on_execute: once for each instruction the CPU really executes, after the
 *     on_read of its opcode and before it runs, with SP (the guest reads it at
 *     every instruction); never for an instruction an interrupt pre-empts.
 *   on_read: each read of the bus by the CPU (opcodes and operands included)
 *     and by the OAM DMA and HDMA transfers, after it is made, with SP; the
 *     ghost instance's key is made of them.
 *   on_write: each write of the bus by the CPU, with its value, before it is
 *     made; the DMA transfers' writes are not reported.
 *   on_vblank: at the end of each frame, its image ready: for a NORMAL,
 *     LCD_OFF or REPEAT frame before the CPU takes the vblank interrupt, for
 *     an ARTIFICIAL one from the write of LCDC. */
typedef struct OraclesCoreHooks {
    void (*on_execute)(void *opaque, uint16_t pc, uint8_t opcode, uint16_t sp);
    void (*on_read)(void *opaque, uint16_t address, uint16_t sp);
    void (*on_write)(void *opaque, uint16_t address, uint8_t value);
    void (*on_vblank)(void *opaque, OraclesVblankType type);
} OraclesCoreHooks;

/* Replaces the hooks (copied); NULL removes them all. */
void oracles_core_set_hooks(OraclesCore *core, const OraclesCoreHooks *hooks, void *opaque);

#ifdef __cplusplus
}
#endif

#endif
