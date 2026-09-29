/* The BPS applier on patches built here: every action, the checksums, and the refusals. */
#include "bps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

typedef struct patch { uint8_t data[512]; size_t size; } patch;

static void byte(patch *p, uint8_t b) { p->data[p->size++] = b; }
static void number(patch *p, uint64_t v)
{
    for (;;) {
        const uint8_t x = (uint8_t)(v & 0x7fu);
        v >>= 7;
        if (v == 0) { byte(p, (uint8_t)(0x80u | x)); return; }
        byte(p, x);
        v--;
    }
}
static void u32(patch *p, uint32_t v) { for (int i = 0; i < 4; i++) byte(p, (uint8_t)(v >> (8 * i))); }
static void action(patch *p, unsigned kind, size_t length) { number(p, ((uint64_t)(length - 1u) << 2) | kind); }
static void offset(patch *p, long delta) { number(p, delta < 0 ? ((uint64_t)(-delta) << 1) | 1u : (uint64_t)delta << 1); }

/* The footer: the source's, the target's and the patch's checksums. */
static void finish(patch *p, const uint8_t *source, size_t source_size, const uint8_t *target, size_t target_size)
{
    u32(p, oracles_crc32(source, source_size));
    u32(p, oracles_crc32(target, target_size));
    u32(p, oracles_crc32(p->data, p->size));
}

static const uint8_t source[16] = { 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N', 'O', 'P' };
/* The target: source bytes kept, bytes of the patch, a copy from elsewhere in the source going back, and a run. */
static const uint8_t target[20] = { 'A', 'B', 'C', 'D', 'x', 'y', 'M', 'N', 'O', 'E', 'F', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'P', 'Q' };

static void build(patch *p)
{
    memset(p, 0, sizeof *p);
    memcpy(p->data, "BPS1", 4); p->size = 4;
    number(p, sizeof source); number(p, sizeof target); number(p, 3); byte(p, 'm'); byte(p, 'e'); byte(p, 't');
    action(p, 0, 4);                     /* ABCD, as the source there */
    action(p, 1, 2); byte(p, 'x'); byte(p, 'y');
    action(p, 2, 3); offset(p, 12);      /* MNO, from source 12 */
    action(p, 2, 2); offset(p, -11);     /* EF, back to source 4 */
    action(p, 1, 1); byte(p, 'z');
    action(p, 3, 6); offset(p, 11);      /* six z, copied from the z just written: the run overlaps itself */
    action(p, 1, 2); byte(p, 'P'); byte(p, 'Q');
    finish(p, source, sizeof source, target, sizeof target);
}

static int refused(const patch *p, const uint8_t *src, size_t src_size, const char *reason)
{
    char error[128] = "";
    size_t size = 0;
    uint8_t *out = oracles_bps_apply(src, src_size, p->data, p->size, &size, error, sizeof error);
    free(out);
    return out == NULL && strstr(error, reason) != NULL;
}

int main(void)
{
    /* zlib's check value. */
    CHECK(oracles_crc32((const uint8_t *)"123456789", 9) == 0xcbf43926u);

    patch p;
    build(&p);
    char error[128] = "";
    size_t size = 0;
    uint8_t *out = oracles_bps_apply(source, sizeof source, p.data, p.size, &size, error, sizeof error);
    CHECK(out != NULL && size == sizeof target && memcmp(out, target, sizeof target) == 0);
    free(out);

    /* The patch alone: whole, whatever the base. */
    CHECK(oracles_bps_check(p.data, p.size, error, sizeof error) == 0);

    patch bad = p;
    bad.data[bad.size - 1] ^= 1u;            /* the patch's own checksum */
    CHECK(refused(&bad, source, sizeof source, "damaged"));
    CHECK(oracles_bps_check(bad.data, bad.size, error, sizeof error) != 0 && strstr(error, "damaged"));
    uint8_t other[16];
    memcpy(other, source, sizeof other);
    other[0] = 'a';                          /* another base of the same size */
    CHECK(refused(&p, other, sizeof other, "not for this ROM"));
    CHECK(refused(&p, source, sizeof source - 1, "not for this ROM"));

    /* A patch whose target checksum is wrong, its own checksum right. */
    bad = p;
    bad.size -= 8;
    u32(&bad, 0x12345678u);
    u32(&bad, oracles_crc32(bad.data, bad.size));
    CHECK(refused(&bad, source, sizeof source, "checksum does not match"));

    /* A copy before the start of the source. */
    memset(&bad, 0, sizeof bad);
    memcpy(bad.data, "BPS1", 4); bad.size = 4;
    number(&bad, sizeof source); number(&bad, 2); number(&bad, 0);
    action(&bad, 2, 2); offset(&bad, -1);
    finish(&bad, source, sizeof source, target, 2);
    CHECK(refused(&bad, source, sizeof source, "outside"));

    /* A target copy from where nothing is written yet. */
    memset(&bad, 0, sizeof bad);
    memcpy(bad.data, "BPS1", 4); bad.size = 4;
    number(&bad, sizeof source); number(&bad, 2); number(&bad, 0);
    action(&bad, 3, 2); offset(&bad, 0);
    finish(&bad, source, sizeof source, target, 2);
    CHECK(refused(&bad, source, sizeof source, "outside"));

    /* Actions that stop before the image is whole. */
    memset(&bad, 0, sizeof bad);
    memcpy(bad.data, "BPS1", 4); bad.size = 4;
    number(&bad, sizeof source); number(&bad, 8); number(&bad, 0);
    action(&bad, 0, 4);
    finish(&bad, source, sizeof source, source, 8);
    CHECK(refused(&bad, source, sizeof source, "ends before"));

    /* Not a BPS patch at all, and one cut before its footer. */
    const uint8_t ips[] = "PATCH....EOF.......";
    patch not_bps = { { 0 }, sizeof ips };
    memcpy(not_bps.data, ips, sizeof ips);
    CHECK(refused(&not_bps, source, sizeof source, "not a BPS patch"));
    CHECK(oracles_bps_check(not_bps.data, not_bps.size, error, sizeof error) != 0 && strstr(error, "not a BPS patch"));

    if (failures) return 1;
    puts("test_bps: ok");
    return 0;
}
