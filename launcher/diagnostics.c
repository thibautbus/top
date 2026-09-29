#include "diagnostics.h"

#include "guest.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct OraclesDiagnostics {
    OraclesGuest *guest;
    FILE *out;
    char buffer[1 << 16];             /* the lines of the frame, written once at its end */
    size_t used;
    uint32_t frame;
    unsigned last_keys;
    unsigned vblanks[4];
    unsigned frames_done;
    unsigned flag_writes;
    size_t journal_writes;
    unsigned frame_flag_writes;       /* flag writes of the current frame, summarised at frame end */
    uint16_t frame_flag_first;
    uint8_t frame_flag_value;
    unsigned last_slot;               /* objectCreateInteraction and getFreeInteractionSlot both report the same slot */
    uint8_t palette_mode;
    int palette_mode_known;
};

static void say(OraclesDiagnostics *d, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    const int n = vsnprintf(d->buffer + d->used, sizeof d->buffer - d->used, format, args);
    va_end(args);
    if (n < 0) return;
    if ((size_t)n >= sizeof d->buffer - d->used) {
        /* Full: write what is gathered, then the line itself. */
        fwrite(d->buffer, 1, d->used, d->out);
        d->used = 0;
        va_start(args, format);
        vfprintf(d->out, format, args);
        va_end(args);
        return;
    }
    d->used += (size_t)n;
}

static void on_event(void *opaque, const OraclesGuestEvent *e)
{
    OraclesDiagnostics *d = opaque;
    const OraclesGuestTables *t = oracles_guest_tables(d->guest);
    switch (e->type) {
        case ORACLES_EVENT_FRAME_DONE:
            d->frames_done++;
            break;
        case ORACLES_EVENT_TILE_SUBSTITUTIONS:
        case ORACLES_EVENT_TILE_SUBSTITUTIONS_DONE:
        case ORACLES_EVENT_CHECK_ROOM_PACK:
        case ORACLES_EVENT_OBJECT_GFX_LOAD:
        case ORACLES_EVENT_OBJECT_GFX_LOAD_DONE:
            break;   /* ghost instance only */
        case ORACLES_EVENT_ANIMATIONS:
            break;   /* the Enhanced view's count of the game's animation steps */
        case ORACLES_EVENT_OBJECTS_UPDATED:
        case ORACLES_EVENT_STATUS_BAR_CHECKED:
        case ORACLES_EVENT_STATUS_BAR_CHECK:
        case ORACLES_EVENT_EQUIPPED_GFX_LOADED:
        case ORACLES_EVENT_USE_ITEMS:
        case ORACLES_EVENT_ITEM_STARTED:
            break;   /* the end of the room's settling, for the numbering of its objects */
        case ORACLES_EVENT_FRAME_DRAWN:
            break;   /* the transition transaction's point */
        case ORACLES_EVENT_FILE_OPERATION:
            break;   /* a mod's storage follows the game's files */
        case ORACLES_EVENT_VBLANK:
            if (e->value < 4) d->vblanks[e->value]++;
            break;
        case ORACLES_EVENT_ROOM_ENTER:
            say(d, "[%u] room enter: group %u room %02x (large %u, state modifier %u)\n", e->frame,
                    oracles_guest_read8(d->guest, t->active_group), oracles_guest_read8(d->guest, t->loading_room),
                    oracles_guest_read8(d->guest, t->room_is_large), oracles_guest_read8(d->guest, t->room_state_modifier));
            break;
        case ORACLES_EVENT_ROOM_INITIALIZED: {
            unsigned interactions = 0, enemies = 0, parts = 0;
            for (unsigned i = 0; i < 16; i++) {
                if (oracles_guest_object(d->guest, i, 1)[0]) interactions++;
                if (oracles_guest_object(d->guest, i, 2)[0]) enemies++;
                if (oracles_guest_object(d->guest, i, 3)[0]) parts++;
            }
            say(d, "[%u] room initialized: %u interactions, %u enemies, %u parts\n", e->frame, interactions, enemies, parts);
            break;
        }
        case ORACLES_EVENT_INTERACTION_CREATED:
            say(d, "[%u] interaction slot %02x created (b=%02x c=%02x)\n", e->frame, e->h, e->b, e->c);
            break;
        case ORACLES_EVENT_ENEMY_CREATED:
            say(d, "[%u] enemy slot %02x created\n", e->frame, e->h);
            break;
        case ORACLES_EVENT_PART_CREATED:
            say(d, "[%u] part slot %02x created\n", e->frame, e->h);
            break;
        case ORACLES_EVENT_RANDOM_PLACEMENT:
        case ORACLES_EVENT_RANDOM_PLACEMENT_DONE:
        case ORACLES_EVENT_OBJECT_DRAW:
        case ORACLES_EVENT_OBJECT_DRAW_DONE:
        case ORACLES_EVENT_TERRAIN_EFFECT:
        case ORACLES_EVENT_OAM_BLOCK:
        case ORACLES_EVENT_OAM_BLOCK_DONE:
            break;   /* the neighbour objects' numbering and sprite tags, every frame */
        case ORACLES_EVENT_ENEMY_KILLED:
            say(d, "[%u] enemy slot %02x killed\n", e->frame, e->address >> 8);
            break;
        case ORACLES_EVENT_TRANSITION:
            say(d, "[%u] screen transition decided, direction %u\n", e->frame, e->value & 3u);
            break;
        case ORACLES_EVENT_WARP:
            say(d, "[%u] warp applied: group %u room %02x\n", e->frame,
                    oracles_guest_read8(d->guest, t->active_group), oracles_guest_read8(d->guest, t->loading_room));
            break;
        case ORACLES_EVENT_TEXT:
            say(d, "[%u] text %02x%02x\n", e->frame, e->b, e->c);
            break;
        case ORACLES_EVENT_TEXT_CHOICE:
            say(d, "[%u] text choice %u\n", e->frame, e->value);
            break;
        case ORACLES_EVENT_FLAG_WRITE:
            /* Flags are written in bursts (a save writes hundreds): one line per frame, at frame end. */
            d->flag_writes++;
            if (d->frame_flag_writes++ == 0) { d->frame_flag_first = e->address; d->frame_flag_value = e->value; }
            break;
        case ORACLES_EVENT_TREASURE:
            say(d, "[%u] treasure %02x (c=%02x)\n", e->frame, e->a, e->c);
            break;
        case ORACLES_EVENT_MENU:
            say(d, "[%u] menu %02x\n", e->frame, e->a);
            break;
        case ORACLES_EVENT_SAVE:
            say(d, "[%u] save file\n", e->frame);
            break;
        case ORACLES_EVENT_INPUT: {
            const unsigned keys = oracles_guest_read8(d->guest, t->keys_pressed);
            if (keys != d->last_keys) { say(d, "[%u] keys %02x\n", e->frame, keys); d->last_keys = keys; }
            break;
        }
    }
}

static void observe_palette_mix(OraclesDiagnostics *d, uint32_t frame)
{
    const OraclesGuestTables *t = oracles_guest_tables(d->guest);
    const uint8_t mode = oracles_guest_read8(d->guest, t->palette_thread_mode);
    if ((!d->palette_mode_known || d->palette_mode != 8u) && mode == 8u) {
        say(d, "[%u] palette mix: palette_thread_mode entered 8; active group %u room %02x, scroll_mode %u, transition_state %u, tileset_palette %u, loaded_tileset_palette %u\n",
            frame, oracles_guest_read8(d->guest, t->active_group), oracles_guest_read8(d->guest, t->active_room),
            oracles_guest_read8(d->guest, t->scroll_mode), oracles_guest_read8(d->guest, t->screen_transition_state),
            oracles_guest_read8(d->guest, t->tileset_palette), oracles_guest_read8(d->guest, t->loaded_tileset_palette));
    }
    d->palette_mode = mode;
    d->palette_mode_known = 1;
}

OraclesDiagnostics *oracles_diagnostics_start(OraclesGuest *guest, FILE *out)
{
    OraclesDiagnostics *d = calloc(1, sizeof *d);
    if (!d) return NULL;
    d->guest = guest;
    d->out = out;
    /* The panel's lines of a frame are gathered in its own buffer and written
     * once at the end of the frame: stderr itself keeps its buffering. */
    oracles_guest_set_event_sink(d->guest, on_event, d);
    say(d, "oracles: diagnostics on, hooks armed\n");
    return d;
}

void oracles_diagnostics_frame_begin(void *opaque, uint32_t frame)
{
    OraclesDiagnostics *d = opaque;
    d->frame = frame;
    oracles_guest_set_frame(d->guest, frame);
    observe_palette_mix(d, frame);
}

void oracles_diagnostics_frame_end(void *opaque, uint32_t frame)
{
    OraclesDiagnostics *d = opaque;
    observe_palette_mix(d, frame);
    size_t count = 0;
    oracles_guest_journal(d->guest, &count);
    d->journal_writes += count;
    oracles_guest_journal_clear(d->guest);
    if (d->frame_flag_writes) {
        say(d, "[%u] %u flag write%s, first %04x = %02x\n", frame, d->frame_flag_writes,
                d->frame_flag_writes > 1 ? "s" : "", d->frame_flag_first, d->frame_flag_value);
        d->frame_flag_writes = 0;
    }
    /* One flush per frame: a console that blocks on every line would stall the game. */
    if (d->used) { fwrite(d->buffer, 1, d->used, d->out); d->used = 0; }
    fflush(d->out);
}

void oracles_diagnostics_stop(OraclesDiagnostics *d)
{
    if (d->used) { fwrite(d->buffer, 1, d->used, d->out); d->used = 0; }
    unsigned overflow = 0, purged = 0;
    oracles_guest_dropped_returns(d->guest, &overflow, &purged);
    fprintf(d->out, "oracles: diagnostics: %u game frames, vblanks normal %u / lcd off %u / artificial %u / repeat %u, %u flag writes, %zu journaled register writes (%zu dropped), return captures lost %u, purged %u\n",
            d->frames_done, d->vblanks[0], d->vblanks[1], d->vblanks[2], d->vblanks[3], d->flag_writes, d->journal_writes,
            oracles_guest_journal_dropped(d->guest), overflow, purged);
    oracles_guest_set_event_sink(d->guest, NULL, NULL);
    free(d);
}
