/* The item hotkeys' lines of settings.txt, without SDL. */
#include "hotkey_lines.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c) do { if (!(c)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); } } while (0)

int main(void)
{
    unsigned game = 9, n = 9;
    OraclesHotkeySlot slot;
    CHECK(oracles_hotkey_line_parse("hotkey_ages_1", "b:0a:--", &game, &n, &slot) && game == 0 && n == 0);
    CHECK(slot.set && slot.item == 0x0a && slot.variant == ORACLES_HOTKEY_NO_VARIANT && slot.target == ORACLES_INVENTORY_SLOT_B);
    CHECK(oracles_hotkey_line_parse("hotkey_seasons_4", "a:19:01", &game, &n, &slot) && game == 1 && n == 3);
    CHECK(slot.set && slot.item == 0x19 && slot.variant == 0x01 && slot.target == ORACLES_INVENTORY_SLOT_A);
    CHECK(oracles_hotkey_line_parse("hotkey_ages_2", "-", &game, &n, &slot) && !slot.set);
    /* a line of something else, a slot that does not exist, a malformed value: not a hotkey, the slot keeps what it had */
    static const char *const refused[][2] = { { "key_a", "X" }, { "hotkey_ages_5", "b:0a:--" }, { "hotkey_ages_0", "b:0a:--" }, { "hotkey_moonrise_1", "b:0a:--" },
        { "hotkey_ages_1", "c:0a:--" }, { "hotkey_ages_1", "b:00:--" }, { "hotkey_ages_1", "b:0g:--" }, { "hotkey_ages_1", "b:0a:-" }, { "hotkey_ages_1", "b:0a:--x" }, { "hotkey_ages_1", "" } };
    for (size_t i = 0; i < sizeof refused / sizeof refused[0]; i++) CHECK(!oracles_hotkey_line_parse(refused[i][0], refused[i][1], &game, &n, &slot));
    /* what is written is read back the same */
    const OraclesHotkeySlot slots[3] = { { 1, 0x17, ORACLES_HOTKEY_NO_VARIANT, ORACLES_INVENTORY_SLOT_A }, { 1, 0x11, 0x03, ORACLES_INVENTORY_SLOT_B }, { 0, 0, 0, 0 } };
    for (unsigned i = 0; i < 3u; i++) {
        char line[64];
        CHECK(oracles_hotkey_line_format(line, sizeof line, 1, i, &slots[i]) > 0);
        char *equals = strchr(line, '=');
        CHECK(equals != NULL);
        if (!equals) continue;
        *equals = 0;
        CHECK(oracles_hotkey_line_parse(line, equals + 1, &game, &n, &slot) && game == 1 && n == i);
        CHECK(slot.set == slots[i].set && (!slot.set || (slot.item == slots[i].item && slot.variant == slots[i].variant && slot.target == slots[i].target)));
    }
    char small[8];
    CHECK(oracles_hotkey_line_format(small, sizeof small, 0, 0, &slots[0]) == 0);
    if (failures) { fprintf(stderr, "%d failure(s)\n", failures); return 1; }
    printf("test_hotkey_lines: ok\n");
    return 0;
}
