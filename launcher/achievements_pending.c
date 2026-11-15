/* The unlocks waiting for RetroAchievements (achievements_pending.h). */
#include "achievements_pending.h"

#include "achievements_files.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse(const char *line, OraclesAchievementsPendingUnlock *u)
{
    OraclesAchievementsPendingUnlock got;
    memset(&got, 0, sizeof got);
    if (sscanf(line, "%63s %32s %" SCNu32 " %" SCNd64, got.user, got.hash, &got.id, &got.when) != 4 || !got.id || strlen(got.hash) != 32) return 0;
    *u = got;
    return 1;
}

size_t oracles_achievements_pending_read(const char *path, OraclesAchievementsPendingUnlock *out, size_t capacity)
{
    FILE *f = path && path[0] ? fopen(path, "r") : NULL;
    if (!f) return 0;
    char line[256];
    size_t count = 0;
    while (count < capacity && fgets(line, sizeof line, f))
        if (line[0] != '#' && parse(line, &out[count])) count++;
    fclose(f);
    return count;
}

static int write_all(const char *path, const OraclesAchievementsPendingUnlock *u, size_t count)
{
    size_t capacity = 256 + count * 128, used = 0;
    char *text = malloc(capacity);
    if (!text) return 0;
    used += (size_t)snprintf(text, capacity, "# RetroAchievements: the unlocks not yet acknowledged, sent again at the next login.\n"
                                             "# account game-hash achievement earned-at (seconds since 1970)\n");
    for (size_t i = 0; i < count && used < capacity; i++)
        used += (size_t)snprintf(text + used, capacity - used, "%s %s %" PRIu32 " %" PRId64 "\n", u[i].user, u[i].hash, u[i].id, u[i].when);
    const int ok = used < capacity && oracles_achievements_write_file(path, text, used, 1);
    free(text);
    return ok;
}

int oracles_achievements_pending_add(const char *path, const OraclesAchievementsPendingUnlock *unlock)
{
    if (!path || !path[0]) return 0;
    OraclesAchievementsPendingUnlock *all = malloc((ORACLES_ACHIEVEMENTS_PENDING_MAX + 1) * sizeof *all);
    if (!all) return 0;
    const int locked = oracles_achievements_lock(path);
    size_t count = oracles_achievements_pending_read(path, all, ORACLES_ACHIEVEMENTS_PENDING_MAX);
    int ok = 1, known = 0;
    for (size_t i = 0; i < count; i++) known |= all[i].id == unlock->id && !strcmp(all[i].user, unlock->user);
    if (!known) {
        if (count == ORACLES_ACHIEVEMENTS_PENDING_MAX) { memmove(all, all + 1, (count - 1) * sizeof *all); count--; }
        all[count++] = *unlock;
        ok = write_all(path, all, count);
    }
    if (locked) oracles_achievements_unlock(path);
    free(all);
    return ok;
}

int oracles_achievements_pending_remove(const char *path, const char *user, uint32_t id)
{
    if (!path || !path[0]) return 0;
    OraclesAchievementsPendingUnlock *all = malloc(ORACLES_ACHIEVEMENTS_PENDING_MAX * sizeof *all);
    if (!all) return 0;
    const int locked = oracles_achievements_lock(path);
    const size_t count = oracles_achievements_pending_read(path, all, ORACLES_ACHIEVEMENTS_PENDING_MAX);
    size_t kept = 0;
    for (size_t i = 0; i < count; i++)
        if (!(all[i].id == id && !strcmp(all[i].user, user))) all[kept++] = all[i];
    const int ok = kept == count || write_all(path, all, kept);
    if (locked) oracles_achievements_unlock(path);
    free(all);
    return ok;
}

void oracles_achievements_pending_follow(const char *path, const char *user, const char *hash, const OraclesAchievementsEvent *event, int64_t now)
{
    if (event->kind == ORACLES_ACHIEVEMENTS_UNLOCKED && !event->delivered) {
        OraclesAchievementsPendingUnlock unlock;
        memset(&unlock, 0, sizeof unlock);
        snprintf(unlock.user, sizeof unlock.user, "%s", user);
        snprintf(unlock.hash, sizeof unlock.hash, "%s", hash);
        unlock.id = event->id;
        unlock.when = now;
        if (!oracles_achievements_pending_add(path, &unlock)) fprintf(stderr, "oracles: achievements: the unlock of %u cannot be kept in %s\n", event->id, path);
    } else if (event->kind == ORACLES_ACHIEVEMENTS_AWARD_SETTLED && (event->delivered || !event->token_refused)) {
        if (!event->delivered) fprintf(stderr, "oracles: achievements: the server refused the unlock of %u: %s\n", event->id, event->text ? event->text : "");
        oracles_achievements_pending_remove(path, user, event->id);
    }
}

unsigned oracles_achievements_pending_resend(OraclesAchievements *a, const char *path, const char *user, const char *token, int64_t now)
{
    OraclesAchievementsPendingUnlock *all = malloc(ORACLES_ACHIEVEMENTS_PENDING_MAX * sizeof *all);
    if (!all) return 0;
    const size_t count = oracles_achievements_pending_read(path, all, ORACLES_ACHIEVEMENTS_PENDING_MAX);
    unsigned sent = 0;
    for (size_t i = 0; i < count; i++) {
        if (strcmp(all[i].user, user) != 0) continue;
        const int64_t ago = now > all[i].when ? now - all[i].when : 1;
        oracles_achievements_resend(a, user, token, all[i].hash, all[i].id, ago > UINT32_MAX ? UINT32_MAX : (uint32_t)ago);
        sent++;
    }
    free(all);
    return sent;
}
