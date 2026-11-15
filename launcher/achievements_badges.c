/* The badges' images kept on disk (achievements_badges.h). */
#include "achievements_badges.h"

#include <stdio.h>
#include <string.h>

int oracles_achievements_badge_path(const char *directory, const char *name, int locked, char *out, size_t capacity)
{
    const size_t length = strlen(name);
    if (!directory[0] || !length || length > 16) return 0;
    for (const char *c = name; *c; c++)
        if (!((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9') || *c == '_' || *c == '-')) return 0;
    const int written = snprintf(out, capacity, "%s/%s%s.png", directory, name, locked ? "_lock" : "");
    return written > 0 && (size_t)written < capacity;
}

/* The image is written by the transport, or by the client where the transport has no save. */
static void answered(void *opaque, int http_status, const uint8_t *body, size_t length)
{
    OraclesAchievementsBadgeJob *job = opaque;
    (void)body; (void)length;
    job->done = 1;
    if (http_status > 0 && http_status != 200) fprintf(stderr, "oracles: achievements: no badge for %s (HTTP %d)\n", job->path, http_status);
}

typedef struct walk {
    OraclesAchievements *a;
    const char *directory;
    OraclesAchievementsBadges *badges;
} walk;

static void want(walk *w, const char *name, int locked, const char *url)
{
    OraclesAchievementsBadges *b = w->badges;
    if (b->count == ORACLES_ACHIEVEMENTS_BADGES_MAX) return;
    OraclesAchievementsBadgeJob *job = &b->jobs[b->count];
    if (!oracles_achievements_badge_path(w->directory, name, locked, job->path, sizeof job->path)) return;
    for (unsigned i = 0; i < b->count; i++) if (!strcmp(b->jobs[i].path, job->path)) return;   /* two achievements, one badge */
    job->done = 0;
    b->count++;
    oracles_achievements_save(w->a, url, job->path, answered, job);   /* none asked for when the file is there */
}

static void badge(void *opaque, const char *name, const char *unlocked_url, const char *locked_url)
{
    want(opaque, name, 0, unlocked_url);
    want(opaque, name, 1, locked_url);
}

void oracles_achievements_fetch_badges(OraclesAchievements *a, const char *directory, OraclesAchievementsBadges *badges)
{
    walk w = { a, directory, badges };
    const unsigned before = badges->count;
    oracles_achievements_badges(a, badge, &w);
    if (badges->count > before) fprintf(stderr, "oracles: achievements: %u badge images looked for in %s\n", badges->count - before, directory);
}

void oracles_achievements_fetch_badge(OraclesAchievements *a, const char *directory, OraclesAchievementsBadges *badges, const char *name, const char *unlocked_url,
                                      const char *locked_url)
{
    walk w = { a, directory, badges };
    badge(&w, name, unlocked_url, locked_url);
}
