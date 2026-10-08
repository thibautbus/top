/* mGBA's Game Boy core behind the core's interface (core_internal.h): the only
 * file of the product that includes mGBA's headers.  It reads mGBA's internal
 * structures (the memory, the registers, the palettes), built with the
 * definitions the vendored library exports (third_party/CMakeLists.txt). */
#include "core_internal.h"
#include "cgb_boot_rom_mgba.h"
#include "colour.h"

#include <mgba/core/config.h>
#include <mgba/core/core.h>
#include <mgba/core/log.h>
#include <mgba/gb/core.h>
#include <mgba/internal/gb/gb.h>
#include <mgba/internal/gb/io.h>
#include <mgba/internal/gb/serialize.h>
#include <mgba/internal/sm83/sm83.h>
#include <mgba-util/audio-buffer.h>
#include <mgba-util/vfs.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef ORACLES_MGBA_VERSION
#error "ORACLES_MGBA_VERSION is defined by the build from config/mgba.json"
#endif

/* mGBA's 131072 Hz output, resampled at the host's rate each frame.  Its mixing is louder than SameBoy's by 2.6 to
 * 2.9 dB (the same passages of Ages and Seasons on both cores): it is brought to SameBoy's level. */
#define MGBA_SAMPLE_RATE 131072u
#define MGBA_GAIN 0.73

/* SameBoy's frames around a screen turned off and on (Core/display.c and memory.c), in mGBA's time: 8 MHz units, two
 * per dot, as SameBoy's.  A frame's length; the time after the last vblank past which the write of LCDC that turns the
 * LCD on ends a frame (ten lines); the time after a scanned vblank within which the first frame after the LCD turns on
 * is not shown (144 lines and 3640 units). */
#define FRAME_UNITS (2 * 70224u)
#define ARTIFICIAL_AFTER (2 * 10 * 456u)
#define REPEAT_WITHIN (2 * 144 * 456u + 3640u)

/* How SameBoy cuts the frames, in mGBA's time (its master cycles, 32 bits, saved in its states: the durations are
 * taken modulo 2^32, a few minutes, far above any of them); saved with the state. */
typedef struct MgbaFrames {
    uint32_t last_vblank;        /* the end of the last frame, of any type */
    uint32_t last_scanned;       /* the last vblank of a scan (LCD on), if scanned_recently */
    uint8_t scanned_recently;    /* a scan ended less than REPEAT_WITHIN ago, as far as the frames since tell */
    uint8_t lcd_turned_on;       /* the next vblank is the first after the LCD turned on */
} MgbaFrames;

typedef struct Mgba {
    struct mCore *core;
    struct mCoreCallbacks callbacks;
    mColor video[ORACLES_SCREEN_PIXELS];
    uint32_t colours[ORACLES_COLOURS];      /* RGB555 to the core's pixels, under the active pipeline */
    unsigned sample_rate_hz;                /* 0: no audio */
    double box_filled;                      /* the source samples gathered into the output sample being made */
    double box_left, box_right;             /* their weighted sums */
    MgbaFrames frames;
    int frame_cut;                          /* an ARTIFICIAL vblank ended the frame inside an instruction */
    /* mGBA's functions under the intermediaries that report the bus to the hooks */
    uint8_t (*load8)(struct SM83Core *, uint16_t);
    void (*store8)(struct SM83Core *, uint16_t, int8_t);
    void (*set_active_region)(struct SM83Core *, uint16_t);
    uint8_t (*cpu_load8)(struct SM83Core *, uint16_t);   /* the one the active region chose */
    void (*dma_service)(struct mTiming *, void *, uint32_t);
    int fetching;                           /* the next fetch is the opcode of an instruction that runs */
    int in_cpu_load8;
} Mgba;

static Mgba *mgba_of(const OraclesCore *core) { return core->backend; }
static struct GB *gb_of(const OraclesCore *core) { return mgba_of(core)->core->board; }
static struct SM83Core *cpu_of(const OraclesCore *core) { return mgba_of(core)->core->cpu; }

/* ---- the log: mGBA's errors to stderr, the rest dropped (it prints to stdout without a logger) -------------------- */

static void log_message(struct mLogger *logger, int category, enum mLogLevel level, const char *format, va_list args)
{
    (void)logger;
    if (!(level & (mLOG_FATAL | mLOG_ERROR))) return;
    fprintf(stderr, "mgba: %s: ", mLogCategoryName(category));
    vfprintf(stderr, format, args);
    fputc('\n', stderr);
}

static struct mLogger logger = { log_message, NULL };

/* ---- the frame -------------------------------------------------------------------------------------------------- */

/* mGBA's pixels widen each RGB555 channel to 8 bits by repeating its high bits: the colour is recovered exactly, then
 * converted as SameBoy converts it (colour.c). */
static uint16_t rgb555_of(mColor c)
{
    return (uint16_t)(((c >> 3) & 0x1fu) | (((c >> 11) & 0x1fu) << 5) | (((c >> 19) & 0x1fu) << 10));
}

/* mGBA's global time counts only with its debugger built: the master cycles, which every build keeps, wrap instead. */
static uint32_t now_of(const OraclesCore *core) { return (uint32_t)mTimingCurrentTime(&gb_of(core)->timing); }

static void white_frame(OraclesCore *core)
{
    const uint32_t white = mgba_of(core)->colours[0x7fffu];
    for (unsigned i = 0; i < ORACLES_SCREEN_PIXELS; i++) core->pixels[i] = white;
}

static void vblank(OraclesCore *core, OraclesVblankType type)
{
    mgba_of(core)->frames.last_vblank = now_of(core);
    if (core->hooks.on_vblank) core->hooks.on_vblank(core->hooks_opaque, type);
}

/* The end of a frame: before the CPU takes the vblank interrupt (mGBA runs it at the instruction boundary first).  Its
 * type and image as SameBoy gives them: white with the LCD off; the first frame after the LCD turns on not shown (the
 * previous image kept) when it comes soon enough after the last scanned one, white otherwise. */
static void on_frame_ended(void *context)
{
    OraclesCore *core = context;
    Mgba *m = mgba_of(core);
    const uint32_t now = now_of(core);
    const int recent = m->frames.scanned_recently && (uint32_t)(now - m->frames.last_scanned) < REPEAT_WITHIN;
    OraclesVblankType type = ORACLES_VBLANK_NORMAL;
    if (!(gb_of(core)->memory.io[GB_REG_LCDC] & 0x80u)) {
        type = ORACLES_VBLANK_LCD_OFF;
        white_frame(core);
        m->frames.scanned_recently = (uint8_t)recent;
    } else {
        if (m->frames.lcd_turned_on) {
            m->frames.lcd_turned_on = 0;
            if (recent) type = ORACLES_VBLANK_REPEAT;
            else white_frame(core);
        } else {
            for (unsigned i = 0; i < ORACLES_SCREEN_PIXELS; i++) core->pixels[i] = m->colours[rgb555_of(m->video[i])];
        }
        m->frames.last_scanned = now;
        m->frames.scanned_recently = 1;
    }
    vblank(core, type);
}

/* ---- the bus: intermediaries under mGBA's memory functions ------------------------------------------------------- */

/* The core of a CPU: the context of the callbacks this file adds, the only ones. */
static OraclesCore *core_of_cpu(struct SM83Core *cpu)
{
    return ((struct GB *)cpu->master)->coreCallbacks.vector[0].context;
}

/* The write of LCDC, cut as SameBoy cuts its frames: turning the LCD on long enough after the last vblank ends a frame
 * there, white (the LCD still off), and the frames of a screen turned off follow the last vblank, not the write. */
static void write_lcdc(OraclesCore *core, struct SM83Core *cpu, uint8_t value)
{
    Mgba *m = mgba_of(core);
    struct GB *gb = gb_of(core);
    const int was_on = (gb->memory.io[GB_REG_LCDC] & 0x80u) != 0, on = (value & 0x80u) != 0;
    if (!was_on && on && (uint32_t)(now_of(core) - m->frames.last_vblank) > ARTIFICIAL_AFTER) {
        white_frame(core);
        vblank(core, ORACLES_VBLANK_ARTIFICIAL);
        m->frame_cut = 1;
        cpu->nextEvent = cpu->cycles;   /* the run stops at the end of this instruction */
    }
    m->store8(cpu, GB_BASE_IO | GB_REG_LCDC, (int8_t)value);
    if (!was_on && on) m->frames.lcd_turned_on = 1;
    if (was_on && !on) {
        const uint32_t since = (uint32_t)(now_of(core) - m->frames.last_vblank);
        mTimingDeschedule(&gb->timing, &gb->video.frameEvent);
        mTimingSchedule(&gb->timing, &gb->video.frameEvent, since < FRAME_UNITS ? (int32_t)(FRAME_UNITS - since) : 0);
    }
}

static void store8(struct SM83Core *cpu, uint16_t address, int8_t value)
{
    OraclesCore *core = core_of_cpu(cpu);
    if (core->hooks.on_write && cpu->memory.accessSource != mACCESS_DMA) core->hooks.on_write(core->hooks_opaque, address, (uint8_t)value);
    if (address == (GB_BASE_IO | GB_REG_LCDC)) write_lcdc(core, cpu, (uint8_t)value);
    else mgba_of(core)->store8(cpu, address, value);
}

static uint8_t load8(struct SM83Core *cpu, uint16_t address)
{
    OraclesCore *core = core_of_cpu(cpu);
    const uint8_t value = mgba_of(core)->load8(cpu, address);
    if (core->hooks.on_read) core->hooks.on_read(core->hooks_opaque, address, cpu->sp);
    return value;
}

/* The fetches of opcodes and operands.  mGBA's own may set the active region and fetch again: that nested fetch is
 * the same read, reported once. */
static uint8_t cpu_load8(struct SM83Core *cpu, uint16_t address)
{
    OraclesCore *core = core_of_cpu(cpu);
    Mgba *m = mgba_of(core);
    if (m->in_cpu_load8) return m->cpu_load8(cpu, address);
    m->in_cpu_load8 = 1;
    const uint8_t value = m->cpu_load8(cpu, address);
    m->in_cpu_load8 = 0;
    if (core->hooks.on_read) core->hooks.on_read(core->hooks_opaque, address, cpu->sp);
    if (m->fetching) {
        m->fetching = 0;
        if (core->hooks.on_execute) core->hooks.on_execute(core->hooks_opaque, address, value, cpu->sp);
    }
    return value;
}

static void set_active_region(struct SM83Core *cpu, uint16_t address)
{
    Mgba *m = mgba_of(core_of_cpu(cpu));
    m->set_active_region(cpu, address);
    m->cpu_load8 = cpu->memory.cpuLoad8;
    cpu->memory.cpuLoad8 = cpu_load8;
}

/* The OAM DMA reads its source directly, outside load8: its read is reported here, one byte per event. */
static void dma_service(struct mTiming *timing, void *context, uint32_t cycles_late)
{
    struct GB *gb = context;
    OraclesCore *core = core_of_cpu(gb->cpu);
    const uint16_t source = gb->memory.dmaSource;
    mgba_of(core)->dma_service(timing, context, cycles_late);
    if (core->hooks.on_read) core->hooks.on_read(core->hooks_opaque, source, gb->cpu->sp);
}

/* The write intermediary is there for the whole life of the core: LCDC's writes cut the frames, hooks or not. */
static void install_bus(OraclesCore *core)
{
    Mgba *m = mgba_of(core);
    struct SM83Core *cpu = cpu_of(core);
    struct GB *gb = gb_of(core);
    m->load8 = cpu->memory.load8;
    m->store8 = cpu->memory.store8;
    m->set_active_region = cpu->memory.setActiveRegion;
    m->cpu_load8 = cpu->memory.cpuLoad8;
    m->dma_service = gb->memory.dmaEvent.callback;
    cpu->memory.store8 = store8;
}

/* The read intermediaries, only while a hook reads them: every read of the bus goes through them. */
static void hooks_changed(OraclesCore *core)
{
    Mgba *m = mgba_of(core);
    struct SM83Core *cpu = cpu_of(core);
    struct GB *gb = gb_of(core);
    const int reads = core->hooks.on_read || core->hooks.on_execute;
    if (reads && cpu->memory.load8 != load8) {
        cpu->memory.load8 = load8;
        cpu->memory.setActiveRegion = set_active_region;
        m->cpu_load8 = cpu->memory.cpuLoad8;
        cpu->memory.cpuLoad8 = cpu_load8;
        gb->memory.dmaEvent.callback = dma_service;
    } else if (!reads && cpu->memory.load8 == load8) {
        cpu->memory.load8 = m->load8;
        cpu->memory.setActiveRegion = m->set_active_region;
        cpu->memory.cpuLoad8 = m->cpu_load8;
        gb->memory.dmaEvent.callback = m->dma_service;
    }
}

static int16_t clamp16(double v) { return (int16_t)(v > 32767.0 ? 32767 : v < -32768.0 ? -32768 : (v < 0 ? v - 0.5 : v + 0.5)); }

/* The frame's audio at the host's rate: each output sample the mean of the source samples its period covers, the
 * ones at its edges in part.  That box filter keeps out most of what lies above the output's band, which a
 * point-sampling interpolator folds back as a metallic hiss (mGBA's cosine one: four times SameBoy's energy above
 * 6 kHz), at a cost of a few operations a source sample (mGBA's sinc one: 2.5 M instructions a frame). */
static void take_audio(OraclesCore *core)
{
    Mgba *m = mgba_of(core);
    struct mAudioBuffer *produced = m->core->getAudioBuffer(m->core);
    if (!m->sample_rate_hz) { mAudioBufferClear(produced); return; }
    const double period = (double)MGBA_SAMPLE_RATE / m->sample_rate_hz;   /* source samples per output sample */
    const double scale = MGBA_GAIN / period;
    int16_t samples[2 * 256];
    size_t frames;
    while ((frames = mAudioBufferRead(produced, samples, 256)) > 0) {
        for (size_t i = 0; i < frames; i++) {
            const double left = samples[2 * i], right = samples[2 * i + 1];
            double weight = 1.0;
            while (m->box_filled + weight >= period) {
                const double part = period - m->box_filled;
                oracles_core_push_audio(core, clamp16((m->box_left + part * left) * scale), clamp16((m->box_right + part * right) * scale));
                weight -= part;
                m->box_filled = 0;
                m->box_left = m->box_right = 0;
            }
            m->box_left += weight * left;
            m->box_right += weight * right;
            m->box_filled += weight;
        }
    }
}

/* ---- creation --------------------------------------------------------------------------------------------------- */

int oracles_core_mgba_init(OraclesCore *core, const uint8_t *rom, size_t rom_size, const OraclesCoreOptions *options)
{
    mLogSetDefaultLogger(&logger);
    Mgba *m = calloc(1, sizeof *m);
    if (!m) return -1;
    m->core = GBCoreCreate();
    if (!m->core || !m->core->init(m->core)) { free(m->core); free(m); return -1; }
    core->backend = m;
    mCoreInitConfig(m->core, NULL);
    /* A CGB whatever the cartridge, as SameBoy is created: mGBA also drops an unknown boot ROM while it detects the
     * model, and its reset reads one key for each kind of cartridge. */
    static const char *const model_keys[] = { "gb.model", "sgb.model", "cgb.model", "cgb.hybridModel", "cgb.sgbModel" };
    for (size_t i = 0; i < sizeof model_keys / sizeof model_keys[0]; i++) mCoreConfigSetValue(&m->core->config, model_keys[i], "CGB");
    m->core->setVideoBuffer(m->core, m->video, ORACLES_SCREEN_WIDTH);
    if (!m->core->loadROM(m->core, VFileMemChunk(rom, rom_size))) goto fail;   /* copied */
    if (!m->core->loadBIOS(m->core, VFileFromConstMemory(oracles_cgb_boot_rom_mgba, oracles_cgb_boot_rom_mgba_size), 0)) goto fail;
    m->callbacks.context = core;
    m->callbacks.videoFrameEnded = on_frame_ended;
    m->core->addCoreCallbacks(m->core, &m->callbacks);
    oracles_core_set_colour_correction(core, options && options->colour_correction);
    m->sample_rate_hz = options ? options->sample_rate_hz : 0;
    m->core->reset(m->core);
    if (gb_of(core)->model != GB_MODEL_CGB || !gb_of(core)->biosVf) goto fail;   /* the boot ROM kept, on a CGB */
    install_bus(core);
    m->frames.last_vblank = now_of(core);
    m->core->setKeys(m->core, 0);
    return 0;
fail:
    m->core->deinit(m->core);
    free(m);
    core->backend = NULL;
    return -1;
}

static void destroy(OraclesCore *core)
{
    Mgba *m = mgba_of(core);
    m->core->deinit(m->core);
    free(m);
}

static int set_joypad_bouncing(OraclesCore *core, int enabled)
{
    (void)core;
    return enabled ? -1 : 0;   /* mGBA has no such emulation */
}

static void set_keys(OraclesCore *core, unsigned key_mask)
{
    /* Opposite directions held together, as SameBoy gives them to a game that reads the directions alone (joypad.c,
     * GB_update_joyp): right releases left, up releases down.  mGBA would release both of a pair (io.c,
     * _readKeysFiltered), and a route that holds them would part.  SameBoy leaves a pair alone when the game reads
     * the directions and the buttons together, which pollInput never does: it reads one group, then the other (the
     * Oracles, and the same bytes in the three fan games). */
    if (key_mask & ORACLES_KEY_RIGHT) key_mask &= ~(unsigned)ORACLES_KEY_LEFT;
    if (key_mask & ORACLES_KEY_UP) key_mask &= ~(unsigned)ORACLES_KEY_DOWN;
    /* Ours (right, left, up, down, A, B, select, start) to mGBA's (A, B, select, start, right, left, up, down). */
    mgba_of(core)->core->setKeys(mgba_of(core)->core, ((key_mask & 0x0fu) << 4) | ((key_mask >> 4) & 0x0fu));
}

static void set_colour_correction(OraclesCore *core, int enabled) { oracles_colour_fill(mgba_of(core)->colours, enabled); }

static uint32_t convert_rgb555(const OraclesCore *core, uint16_t colour) { return mgba_of(core)->colours[colour & 0x7fffu]; }

static void set_sample_rate(OraclesCore *core, unsigned sample_rate_hz)
{
    Mgba *m = mgba_of(core);
    if (!m->sample_rate_hz) return;   /* created without audio */
    m->sample_rate_hz = sample_rate_hz;
}

/* To the end of the frame: mGBA's run, or, with an execute hook, one instruction at a time, the events due processed
 * at each boundary as SM83Tick does first, so that the fetch that follows is known to be an instruction's opcode (no
 * interrupt pending, the CPU not halted).  Either way the same emulation: only where the events are processed differs,
 * never when. */
static void run_frame(OraclesCore *core)
{
    Mgba *m = mgba_of(core);
    struct GB *gb = gb_of(core);
    struct SM83Core *cpu = cpu_of(core);
    const uint32_t start = gb->video.frameCounter;
    m->frame_cut = 0;
    if (!core->hooks.on_execute) {
        while (gb->video.frameCounter == start && !m->frame_cut) SM83Run(cpu);
    } else {
        while (gb->video.frameCounter == start && !m->frame_cut) {
            if (cpu->executionState == SM83_CORE_FETCH) {
                while (cpu->cycles >= cpu->nextEvent) cpu->irqh.processEvents(cpu);
                if (gb->video.frameCounter != start) break;
                m->fetching = !cpu->irqPending && !cpu->halted;
            }
            do SM83Tick(cpu); while (cpu->executionState != SM83_CORE_FETCH);
            m->fetching = 0;
        }
    }
    take_audio(core);
}

static size_t sram_size(OraclesCore *core) { return gb_of(core)->sramSize; }

static int load_sram(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    if (!gb_of(core)->memory.sram) return -1;
    memcpy(gb_of(core)->memory.sram, buffer, size);
    return 0;
}

static int save_sram(OraclesCore *core, uint8_t *buffer, size_t size)
{
    if (!gb_of(core)->memory.sram) return -1;
    memcpy(buffer, gb_of(core)->memory.sram, size);
    return 0;
}

/* mGBA's state, then the cartridge RAM, which it leaves out and SameBoy puts in, then how the frames are cut. */
#define FRAMES_BYTES 10u

static size_t state_size(OraclesCore *core) { return sizeof(struct GBSerializedState) + sram_size(core) + FRAMES_BYTES; }

static void put32(uint8_t *at, uint32_t v) { for (unsigned i = 0; i < 4; i++) at[i] = (uint8_t)(v >> (8 * i)); }
static uint32_t get32(const uint8_t *at) { uint32_t v = 0; for (unsigned i = 0; i < 4; i++) v |= (uint32_t)at[i] << (8 * i); return v; }

static int save_state(OraclesCore *core, uint8_t *buffer, size_t size)
{
    /* Between two instructions: mGBA would otherwise run the CPU to the next one, and saving would change the game. */
    if (cpu_of(core)->executionState != SM83_CORE_FETCH) return -1;
    struct mCore *m = mgba_of(core)->core;
    if (!m->saveState(m, buffer)) return -1;
    const size_t sram = sram_size(core);
    if (save_sram(core, buffer + sizeof(struct GBSerializedState), sram) != 0) return -1;
    uint8_t *frames = buffer + size - FRAMES_BYTES;
    const MgbaFrames *f = &mgba_of(core)->frames;
    put32(frames, f->last_vblank);
    put32(frames + 4, f->last_scanned);
    frames[8] = f->scanned_recently;
    frames[9] = f->lcd_turned_on;
    return 0;
}

static int load_state(OraclesCore *core, const uint8_t *buffer, size_t size)
{
    if (size != state_size(core)) return -1;
    struct mCore *m = mgba_of(core)->core;
    if (!m->loadState(m, buffer)) return -1;
    if (load_sram(core, buffer + sizeof(struct GBSerializedState), sram_size(core)) != 0) return -1;
    const uint8_t *frames = buffer + size - FRAMES_BYTES;
    MgbaFrames *f = &mgba_of(core)->frames;
    f->last_vblank = get32(frames);
    f->last_scanned = get32(frames + 4);
    f->scanned_recently = frames[8] != 0;
    f->lcd_turned_on = frames[9] != 0;
    return 0;
}

static uint8_t *memory(OraclesCore *core, OraclesCoreRegion region, size_t *size, uint16_t *bank)
{
    struct GB *gb = gb_of(core);
    uint8_t *data = NULL;
    size_t length = 0;
    int current = 0;
    switch (region) {
        case ORACLES_CORE_WRAM: data = gb->memory.wram; length = GB_SIZE_WORKING_RAM; current = gb->memory.wramCurrentBank; break;
        case ORACLES_CORE_VRAM: data = gb->video.vram; length = GB_SIZE_VRAM; current = gb->video.vramCurrentBank; break;
        case ORACLES_CORE_OAM: data = gb->video.oam.raw; length = GB_SIZE_OAM; break;
        case ORACLES_CORE_HRAM: data = gb->memory.hram; length = GB_SIZE_HRAM; break;
        case ORACLES_CORE_IO: data = gb->memory.io; length = GB_SIZE_IO; break;
        case ORACLES_CORE_ROM: data = gb->memory.rom; length = gb->memory.romSize; current = gb->memory.currentBank; break;
        case ORACLES_CORE_CART_RAM: data = gb->memory.sram; length = gb->sramSize; current = gb->memory.sramCurrentBank; break;
        /* 32 background colours then 32 object colours, each as the game wrote its two bytes (little-endian hosts). */
        case ORACLES_CORE_BG_PALETTES: data = (uint8_t *)gb->video.palette; length = 0x40; break;
        case ORACLES_CORE_OBJ_PALETTES: data = (uint8_t *)gb->video.palette + 0x40; length = 0x40; break;
    }
    if (!data) length = 0, current = 0;
    if (size) *size = length;
    if (bank) *bank = (uint16_t)current;
    return data;
}

static uint8_t peek(OraclesCore *core, uint16_t address) { return GBView8(cpu_of(core), address, -1); }

static OraclesCoreRegisters registers(const OraclesCore *core)
{
    const struct SM83Core *cpu = cpu_of(core);
    const OraclesCoreRegisters out = { cpu->af, cpu->bc, cpu->de, cpu->hl, cpu->sp, cpu->pc };
    return out;
}

static void set_registers(OraclesCore *core, const OraclesCoreRegisters *in)
{
    struct SM83Core *cpu = cpu_of(core);
    cpu->af = in->af; cpu->bc = in->bc; cpu->de = in->de; cpu->hl = in->hl; cpu->sp = in->sp;
    if (cpu->pc != in->pc) {
        cpu->pc = in->pc;
        cpu->memory.setActiveRegion(cpu, cpu->pc);
    }
}


const OraclesCoreOps oracles_core_mgba_ops = {
    destroy, set_joypad_bouncing, set_keys, set_colour_correction, convert_rgb555, set_sample_rate, run_frame,
    sram_size, load_sram, save_sram, state_size, save_state, load_state, memory, peek, registers, set_registers,
    hooks_changed, "mgba-" ORACLES_MGBA_VERSION,
};
