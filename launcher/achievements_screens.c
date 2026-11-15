/* What the player sees of a session's RetroAchievements (achievements_screens.h). */
#include "achievements_screens.h"

#include <stdio.h>
#include <string.h>

int oracles_achievements_suspension_kind(const OraclesAchievements *a)
{
    const char *why = oracles_achievements_suspension(a);
    if (!why) return ORACLES_ACHIEVEMENTS_RUNNING;
    return strcmp(why, ORACLES_ACHIEVEMENTS_MODS_REASON) == 0 ? ORACLES_ACHIEVEMENTS_BY_MOD : ORACLES_ACHIEVEMENTS_BY_CHEAT;
}

void oracles_achievements_tell(OraclesAchievementsToasts *toasts, OraclesAchievementsTold *told, OraclesAchievements *a, const char *game,
                               const OraclesAchievementsEvent *event)
{
    switch (event->kind) {
        case ORACLES_ACHIEVEMENTS_GAME_LOADED: {
            const int suspended = oracles_achievements_suspension_kind(a);
            oracles_achievements_toast_start(toasts, game, event->unlocked, event->count, suspended);
            told->suspension = suspended != ORACLES_ACHIEVEMENTS_RUNNING;
            told->loaded = 1;
            break;
        }
        case ORACLES_ACHIEVEMENTS_UNLOCKED:
            oracles_achievements_toast_unlock(toasts, event->text, event->points, event->badge);
            told->offline = 0;
            break;
        case ORACLES_ACHIEVEMENTS_GAME_COMPLETED: {
            unsigned count, unlocked, points, points_unlocked;
            oracles_achievements_progress(a, &count, &unlocked, &points, &points_unlocked);
            oracles_achievements_toast_start(toasts, game, count, count, 0);
            break;
        }
        case ORACLES_ACHIEVEMENTS_OFFLINE:
            if (!told->offline) oracles_achievements_toast_offline(toasts);
            told->offline = 1;
            break;
        case ORACLES_ACHIEVEMENTS_LOGIN_FAILED:
        case ORACLES_ACHIEVEMENTS_GAME_FAILED:
            /* No server as the game starts: no achievement this session, said once; the game goes on. */
            if (event->unreachable && !told->loaded && !told->unreachable) oracles_achievements_toast_unreachable(toasts);
            if (event->unreachable) told->unreachable = 1;
            break;
        default: break;
    }
}

void oracles_achievements_tell_suspension(OraclesAchievementsToasts *toasts, OraclesAchievementsTold *told, const OraclesAchievements *a)
{
    const int suspended = oracles_achievements_suspension_kind(a);
    if (!told->loaded || !suspended || told->suspension) return;
    if (suspended == ORACLES_ACHIEVEMENTS_BY_MOD) oracles_achievements_toast_mod(toasts);
    else oracles_achievements_toast_cheat(toasts);
    told->suspension = 1;
}

typedef struct making {
    OraclesAchievementsList *list;
    OraclesAchievementsDate date;
} making;

static void list_entry(void *opaque, const OraclesAchievementsEntry *e)
{
    const making *m = opaque;
    OraclesAchievementsList *l = m->list;
    if (l->count == ORACLES_ACHIEVEMENTS_MAX) return;
    OraclesAchievementsItem *item = &l->items[l->count++];
    memset(item, 0, sizeof *item);
    oracles_ui_copy_whole(item->title, sizeof item->title, e->title);
    oracles_ui_copy_whole(item->description, sizeof item->description, e->description);
    /* A line break of the server's is a space: the lines break where they fit. */
    for (char *c = item->description; *c; c++) if ((unsigned char)*c < 0x20u) *c = ' ';
    snprintf(item->badge, sizeof item->badge, "%s", e->badge ? e->badge : "");
    item->points = e->points;
    item->unlocked = e->unlocked;
    if (e->unlocked && e->unlock_time > 0 && m->date) m->date(e->unlock_time, item->date, sizeof item->date);
    if (!e->unlocked && e->progress && e->progress[0]) snprintf(item->progress, sizeof item->progress, "%s", e->progress);
    item->percent = e->percent;
    item->rarity = e->rarity;
    item->tag = e->type <= ORACLES_ACHIEVEMENT_TAG_WIN ? e->type : ORACLES_ACHIEVEMENT_TAG_NONE;
}

int oracles_achievements_list_make(OraclesAchievements *a, const char *badges, OraclesAchievementsDate date, OraclesAchievementsList *list)
{
    list->count = list->unlocked = list->points = list->points_unlocked = 0;
    list->generation++;   /* the screens lay it out again */
    if (!oracles_achievements_watching(a)) return 0;
    unsigned count;
    oracles_achievements_progress(a, &count, &list->unlocked, &list->points, &list->points_unlocked);
    snprintf(list->badges, sizeof list->badges, "%.*s", (int)sizeof list->badges - 1, badges ? badges : "");
    making m = { list, date };
    oracles_achievements_each(a, list_entry, &m);
    return 1;
}

static void count_entry(void *opaque, const OraclesAchievementsEntry *e)
{
    OraclesAchievementsList *l = opaque;
    l->points += e->points;
    if (e->unlocked) { l->unlocked++; l->points_unlocked += e->points; }
}

void oracles_achievements_catalogue_list(const OraclesAchievementsCatalogue *catalogue, const char *badges, OraclesAchievementsList *list)
{
    list->count = list->unlocked = list->points = list->points_unlocked = 0;
    list->generation++;
    snprintf(list->badges, sizeof list->badges, "%.*s", (int)sizeof list->badges - 1, badges ? badges : "");
    oracles_achievements_catalogue_each(catalogue, count_entry, list);
    making m = { list, NULL };
    oracles_achievements_catalogue_each(catalogue, list_entry, &m);
}
