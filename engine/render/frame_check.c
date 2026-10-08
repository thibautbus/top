#include "frame_check.h"

#include <stdlib.h>
#include <string.h>

#define IO_LCDC 0x40u
#define IO_SCY 0x42u
#define IO_SCX 0x43u
#define IO_WY 0x4au
#define IO_WX 0x4bu
#define IO_DMA 0x46u
#define IO_HDMA5 0x55u
#define IO_BGPD 0x69u
#define IO_OBPD 0x6bu
#define PER_LINE_WRITES 16u   /* SCX/SCY writes during the scan that prove a per-line handler ran */

struct OraclesFrameCheck {
    OraclesGuest *guest;
    OraclesCore *core;
    uint32_t frame;
    OraclesPpuRegs at_last_vblank;    /* registers as they stood when the previous vblank hook ran */
    int have_last;
    int lcd_was_off;                  /* LCDC bit 7 low at the previous vblank */
    uint8_t vram[0x4000];
    uint8_t oam[160];
    uint8_t bg_palettes[64];
    uint8_t obj_palettes[64];
    uint32_t colours[ORACLES_PPU_COLOURS];   /* the core's conversion of every RGB555 colour under the pipeline below */
    int colours_pipeline;                    /* -1 until built; the player's colour correction switches it */
    uint32_t native[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    uint32_t core_pixels[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];
    OraclesFrameStats stats;
    char samples_dir[4096];
    unsigned samples_per_class;
    unsigned samples_written[ORACLES_FRAME_CLASSES];
    uint32_t next_sample_frame[ORACLES_FRAME_CLASSES];
};

const char *oracles_frame_class_name(OraclesFrameClass c)
{
    static const char *const names[ORACLES_FRAME_CLASSES] = {
        "normal", "lcd-off", "lcd-on-first", "lcd-0-1", "lcd-5-6", "mid-scan", "no-scan"
    };
    return c < ORACLES_FRAME_CLASSES ? names[c] : "?";
}

static void write_ppm(const char *path, const uint32_t *pixels)
{
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%u %u\n255\n", ORACLES_PPU_WIDTH, ORACLES_PPU_HEIGHT);
    for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++) {
        const uint8_t rgb[3] = { (uint8_t)(pixels[i] >> 16), (uint8_t)(pixels[i] >> 8), (uint8_t)pixels[i] };
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static void write_samples(OraclesFrameCheck *c, OraclesFrameClass cls, int mismatch, const uint8_t late[ORACLES_PPU_HEIGHT])
{
    if (!c->samples_dir[0] || c->samples_written[cls] >= c->samples_per_class) return;
    /* Every class gets its mismatches first, then frames about ten seconds apart. */
    if (!mismatch && c->frame < c->next_sample_frame[cls]) return;
    char path[4096 + 64];
    static uint32_t diff[ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT];   /* 92 KiB: not on the callback's stack */
    for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++)   /* magenta: a mismatch; grey: a difference on a line not compared */
        diff[i] = c->native[i] == c->core_pixels[i] ? 0xff000000u : late[i / ORACLES_PPU_WIDTH] ? 0xff808080u : 0xffff00ffu;
    snprintf(path, sizeof path, "%s/%s-%06u-native.ppm", c->samples_dir, oracles_frame_class_name(cls), c->frame);
    write_ppm(path, c->native);
    snprintf(path, sizeof path, "%s/%s-%06u-core.ppm", c->samples_dir, oracles_frame_class_name(cls), c->frame);
    write_ppm(path, c->core_pixels);
    snprintf(path, sizeof path, "%s/%s-%06u-diff.ppm", c->samples_dir, oracles_frame_class_name(cls), c->frame);
    write_ppm(path, diff);
    c->samples_written[cls]++;
    c->next_sample_frame[cls] = c->frame + 600; /* about ten seconds apart */
}

/* The registers of each scanned line, from the state at the previous vblank
 * and the journal of the period: writes during vblank (LY >= 144) set line
 * 0; a write during a line's hblank (mode 0) takes effect on the next line,
 * a write during its OAM scan or drawing (modes 2, 3) on that line.
 *
 * A scroll write stamped with the drawing (mode 3) normally falls on its
 * first dot, before the fetcher has read anything, and holds for the whole
 * line.  A handler that runs after another one busy in the previous line's
 * hblank starts late: its write falls while the line is drawn, and the core
 * mixes both values (the tiles already read keep the old one, the next take
 * the new tile column with the fine scroll latched at the line's start).
 * Where in the line it falls is not in the journal (SameBoy calls the write
 * callback before it catches its PPU up), so such a line, a scroll write in
 * mode 3 after a write in the previous line's hblank that changes the value,
 * is marked in `late` and left out of the comparison.  Ages' underwater waves do it on line 16,
 * whose handler (lcdInterrupt, behaviour 0) follows the status bar's.
 *
 * "Before the fetcher has read anything" is SameBoy's timing.  mGBA starts
 * drawing a line sooner (its first pixel five dots into mode 3, plus the fine
 * scroll, video.c's _endMode2), and a per-line handler's write that SameBoy
 * takes on its first dot may reach mGBA after its first pixel or two.  The
 * other cores' pipelines decide which of the line's first pixels take such a
 * write, and the journal cannot tell: a line whose SCX or SCY changes at the
 * start of its drawing, otherwise compared, is marked in `first_tile` and
 * compared from its second tile on (from pixel 8). */
static void build_lines(OraclesFrameCheck *c, OraclesPpuInput *in, uint8_t late[ORACLES_PPU_HEIGHT], uint8_t first_tile[ORACLES_PPU_HEIGHT],
                        int *mid_scan, unsigned *scan_scroll_writes, int *lcd_switched_on)
{
    OraclesPpuRegs regs = c->at_last_vblank;
    size_t count = 0;
    const OraclesGuestRegWrite *journal = oracles_guest_journal(c->guest, &count);
    *mid_scan = 0;
    *scan_scroll_writes = 0;
    *lcd_switched_on = 0;
    memset(late, 0, ORACLES_PPU_HEIGHT);
    memset(first_tile, 0, ORACLES_PPU_HEIGHT);
    uint8_t hblank_write[ORACLES_PPU_HEIGHT] = { 0 };
    for (size_t i = 0; i < count; i++) if (journal[i].ly < ORACLES_PPU_HEIGHT && journal[i].stat_mode == 0) hblank_write[journal[i].ly] = 1;
    /* A rising edge of LCDC bit 7 anywhere in the period: the core paints the first frame after it white. */
    {
        uint8_t lcdc = regs.lcdc;
        for (size_t i = 0; i < count; i++) {
            if (journal[i].reg != IO_LCDC) continue;
            if (!(lcdc & 0x80u) && (journal[i].value & 0x80u)) *lcd_switched_on = 1;
            lcdc = journal[i].value;
        }
    }
    /* First the vblank-period writes, in order, then a walk over the scan lines. */
    for (size_t i = 0; i < count; i++) {
        const OraclesGuestRegWrite *w = &journal[i];
        if (w->ly < ORACLES_PPU_HEIGHT) continue;
        switch (w->reg) {
            case IO_LCDC: regs.lcdc = w->value; break;
            case IO_SCY: regs.scy = w->value; break;
            case IO_SCX: regs.scx = w->value; break;
            case IO_WY: regs.wy = w->value; break;
            case IO_WX: regs.wx = w->value; break;
            default: break;
        }
    }
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
        /* apply the writes that take effect on this line */
        for (size_t i = 0; i < count; i++) {
            const OraclesGuestRegWrite *w = &journal[i];
            if (w->ly >= ORACLES_PPU_HEIGHT) continue;
            const unsigned effective = w->stat_mode == 0 ? (unsigned)w->ly + 1u : w->ly;
            if (effective != ly) continue;
            const int scroll_changes = (w->reg == IO_SCX && w->value != regs.scx) || (w->reg == IO_SCY && w->value != regs.scy);
            if (w->stat_mode == 3 && scroll_changes) {
                if (ly > 0 && hblank_write[ly - 1]) late[ly] = 1;
                else first_tile[ly] = 1;
            }
            switch (w->reg) {
                case IO_LCDC: regs.lcdc = w->value; break;
                case IO_SCY: regs.scy = w->value; (*scan_scroll_writes)++; break;
                case IO_SCX: regs.scx = w->value; (*scan_scroll_writes)++; break;
                case IO_WY: regs.wy = w->value; break;
                case IO_WX: regs.wx = w->value; break;
                case IO_DMA: case IO_HDMA5: case IO_BGPD: case IO_OBPD: *mid_scan = 1; break;
                default: break;
            }
        }
        in->lines[ly] = regs;
    }
}

static void on_vblank(void *opaque, OraclesVblankType type)
{
    OraclesFrameCheck *c = opaque;
    const uint8_t *io = oracles_guest_io(c->guest);
    const OraclesPpuRegs now = { io[IO_LCDC], io[IO_SCY], io[IO_SCX], io[IO_WY], io[IO_WX] };
    const int lcd_on_now = (now.lcdc & 0x80u) != 0;

    OraclesFrameClass cls;
    int compare = 1;
    OraclesPpuInput in;
    int mid_scan = 0, lcd_switched_on = 0;
    unsigned scan_scroll_writes = 0;
    uint8_t late[ORACLES_PPU_HEIGHT], first_tile[ORACLES_PPU_HEIGHT];
    /* Snapshot what the finished scan used. */
    memcpy(c->vram, oracles_guest_vram(c->guest, 0), 0x4000);
    memcpy(c->oam, oracles_guest_oam(c->guest), 160);
    memcpy(c->bg_palettes, oracles_guest_bg_palettes(c->guest), 64);
    memcpy(c->obj_palettes, oracles_guest_obj_palettes(c->guest), 64);
    memcpy(c->core_pixels, oracles_core_pixels(c->core), sizeof c->core_pixels);
    /* The colour pipeline is the player's choice (F2): the native image follows it as the core does. */
    if (c->colours_pipeline != oracles_core_colour_correction(c->core)) {
        c->colours_pipeline = oracles_core_colour_correction(c->core);
        for (unsigned i = 0; i < ORACLES_PPU_COLOURS; i++) c->colours[i] = oracles_core_convert_rgb555(c->core, (uint16_t)i);
    }
    in.vram = c->vram; in.oam = c->oam; in.bg_palettes = c->bg_palettes; in.obj_palettes = c->obj_palettes;
    in.colours = c->colours;
    build_lines(c, &in, late, first_tile, &mid_scan, &scan_scroll_writes, &lcd_switched_on);

    const uint8_t behaviour = oracles_guest_read8(c->guest, oracles_guest_tables(c->guest)->lcd_interrupt_behaviour);
    if (type != ORACLES_VBLANK_NORMAL && type != ORACLES_VBLANK_LCD_OFF) { cls = ORACLES_FRAME_NO_SCAN; compare = 0; }
    else if (type == ORACLES_VBLANK_LCD_OFF || !lcd_on_now) cls = ORACLES_FRAME_LCD_OFF;
    else if (c->lcd_was_off || lcd_switched_on || !c->have_last) cls = ORACLES_FRAME_LCD_ON_FIRST;
    /* lcdInterrupt (bank0.s): behaviours 0 and 1 write SCX or SCY on every
     * line from wBigBuffer; 2, 3 and 4 are the status-bar switch of normal
     * play (3 sets LCDC to $a7, 4 clears WX and WY after a textbox); 5 (ring
     * menu) and 6 rearm LYC and rewrite registers more than once per frame.
     * All of them are register writes the journal replays; the classes only
     * separate the report.  The class "lcd-0-1" is claimed only when seen: the
     * per-line handler leaves dozens of SCX/SCY writes during the scan; a
     * behaviour byte of 0 or 1 without them (the boot logos, before the game
     * initialises it) is a normal frame. */
    else if (mid_scan) { cls = ORACLES_FRAME_MID_SCAN; compare = 0; }
    else if (behaviour <= 1 && scan_scroll_writes >= PER_LINE_WRITES) cls = ORACLES_FRAME_LCD01;
    else if (behaviour == 5 || behaviour == 6) cls = ORACLES_FRAME_LCD2_6;
    else cls = ORACLES_FRAME_NORMAL;

    if (cls == ORACLES_FRAME_LCD_OFF || cls == ORACLES_FRAME_LCD_ON_FIRST) {
        for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++) c->native[i] = ORACLES_PPU_WHITE;
    } else if (cls != ORACLES_FRAME_NO_SCAN) {
        oracles_ppu_render(&in, c->native);
    }
    int window = 0;
    for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT && !window; ly++)
        if ((in.lines[ly].lcdc & 0x80u) && (in.lines[ly].lcdc & 0x20u) && in.lines[ly].wy <= ly && in.lines[ly].wx <= 166u) window = 1;
    if (window && compare) c->stats.window_frames++;

    c->stats.frames[cls]++;
    if (cls == ORACLES_FRAME_MID_SCAN) {
        /* Not compared, but measured: how far the core's image is from the committed state. */
        uint32_t pixels = 0;
        for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++) if (c->native[i] != c->core_pixels[i]) pixels++;
        c->stats.mid_scan_pixels += pixels;
        write_samples(c, cls, pixels != 0, late);
    }
    if (compare) {
        int mismatch = 0;
        for (unsigned ly = 0; ly < ORACLES_PPU_HEIGHT; ly++) {
            c->stats.compared_lines++;
            if (late[ly]) { c->stats.late_scroll_lines++; continue; }
            const size_t from = first_tile[ly] ? 8u : 0u, row = (size_t)ly * ORACLES_PPU_WIDTH + from;
            if (first_tile[ly]) c->stats.first_tile_lines++;
            if (memcmp(c->native + row, c->core_pixels + row, (ORACLES_PPU_WIDTH - from) * sizeof c->native[0])) mismatch = 1;
        }
        if (mismatch) {
            if (c->stats.mismatches[cls] == 0) c->stats.first_mismatch[cls] = c->frame;
            c->stats.mismatches[cls]++;
        }
        write_samples(c, cls, mismatch, late);
    }

    c->at_last_vblank = now;
    c->have_last = 1;
    c->lcd_was_off = !lcd_on_now;
    oracles_guest_journal_clear(c->guest);
}

OraclesFrameCheck *oracles_frame_check_start(OraclesGuest *guest, OraclesCore *core,
                                             const char *samples_dir, unsigned samples_per_class)
{
    OraclesFrameCheck *c = calloc(1, sizeof *c);
    if (!c) return NULL;
    c->guest = guest;
    c->core = core;
    if (samples_dir) snprintf(c->samples_dir, sizeof c->samples_dir, "%s", samples_dir);
    c->samples_per_class = samples_per_class;
    c->colours_pipeline = -1;
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) c->stats.first_mismatch[i] = UINT32_MAX;
    for (unsigned i = 0; i < ORACLES_PPU_WIDTH * ORACLES_PPU_HEIGHT; i++) c->native[i] = ORACLES_PPU_WHITE;
    oracles_guest_set_vblank_hook(guest, on_vblank, c);
    return c;
}

void oracles_frame_check_stop(OraclesFrameCheck *c)
{
    if (!c) return;
    oracles_guest_set_vblank_hook(c->guest, NULL, NULL);
    free(c);
}

void oracles_frame_check_frame_begin(void *opaque, uint32_t frame)
{
    OraclesFrameCheck *c = opaque;
    c->frame = frame;
}

void oracles_frame_check_reset(OraclesFrameCheck *c)
{
    oracles_guest_journal_clear(c->guest);
    c->have_last = 0;   /* the next vblank is a first frame: not compared against a state that is gone */
}

int oracles_frame_check_verdict(const OraclesFrameCheck *c, unsigned expected_classes, char *reason, size_t capacity)
{
    const OraclesFrameStats *s = &c->stats;
    uint32_t total = 0;
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) total += s->frames[i];
    if (reason && capacity) reason[0] = '\0';
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) {
        const int compared = i != ORACLES_FRAME_MID_SCAN && i != ORACLES_FRAME_NO_SCAN;
        if (compared && s->mismatches[i]) {
            if (reason) snprintf(reason, capacity, "%u mismatches in class %s", s->mismatches[i], oracles_frame_class_name((OraclesFrameClass)i));
            return 0;
        }
        if ((expected_classes & (1u << i)) && s->frames[i] == 0) {
            if (reason) snprintf(reason, capacity, "expected class %s is empty", oracles_frame_class_name((OraclesFrameClass)i));
            return 0;
        }
    }
    if (total && 100.0 * s->frames[ORACLES_FRAME_MID_SCAN] / total > 2.0) {
        if (reason) snprintf(reason, capacity, "mid-scan frames above 2%% of the vblanks");
        return 0;
    }
    if (total && 100.0 * (s->frames[ORACLES_FRAME_MID_SCAN] + s->frames[ORACLES_FRAME_NO_SCAN]) / total > 10.0) {
        if (reason) snprintf(reason, capacity, "frames not compared above 10%% of the vblanks");
        return 0;
    }
    if (s->compared_lines && 100.0 * s->late_scroll_lines / s->compared_lines > 1.0) {
        if (reason) snprintf(reason, capacity, "lines with a late scroll write above 1%% of the compared frames' lines");
        return 0;
    }
    return 1;
}

const uint32_t *oracles_frame_check_native(const OraclesFrameCheck *c) { return c->native; }
const OraclesFrameStats *oracles_frame_check_stats(const OraclesFrameCheck *c) { return &c->stats; }

void oracles_frame_check_report(const OraclesFrameCheck *c, FILE *out)
{
    const OraclesFrameStats *s = &c->stats;
    uint32_t total = 0, unsupported = 0;
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) total += s->frames[i];
    unsupported = s->frames[ORACLES_FRAME_MID_SCAN];
    fprintf(out, "renderer: %u vblanks, %u with the window scanned\n", total, s->window_frames);
    for (unsigned i = 0; i < ORACLES_FRAME_CLASSES; i++) {
        const int compared = i != ORACLES_FRAME_MID_SCAN && i != ORACLES_FRAME_NO_SCAN;
        fprintf(out, "  %-13s %7u frames", oracles_frame_class_name((OraclesFrameClass)i), s->frames[i]);
        if (compared) {
            fprintf(out, ", %u mismatches", s->mismatches[i]);
            if (s->mismatches[i]) fprintf(out, " (first at frame %u)", s->first_mismatch[i]);
        } else if (i == ORACLES_FRAME_MID_SCAN && s->frames[i]) {
            fprintf(out, ", not compared; %.1f differing pixels per frame on average", (double)s->mid_scan_pixels / s->frames[i]);
        } else {
            fprintf(out, ", not compared");
        }
        fputc('\n', out);
    }
    fprintf(out, "  mid-scan frames: %.2f%% of the vblanks (ceiling 2%%); not compared with no-scan: %.2f%% (ceiling 10%%)\n",
            total ? 100.0 * unsupported / total : 0.0, total ? 100.0 * (unsupported + s->frames[ORACLES_FRAME_NO_SCAN]) / total : 0.0);
    fprintf(out, "  lines with a late scroll write, not compared: %u, %.3f%% of the compared frames' lines (ceiling 1%%)\n",
            s->late_scroll_lines, s->compared_lines ? 100.0 * s->late_scroll_lines / s->compared_lines : 0.0);
    fprintf(out, "  lines whose scroll changed at the start of their drawing, compared from pixel 8: %u\n", s->first_tile_lines);
}
