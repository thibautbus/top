/* Mods in Lua, without a ROM: a package loads with its
 * modules and gets its identity; the sandbox offers no file, clock, address or
 * randomness and walks tables in a fixed order; the work of C functions is
 * bounded; an endless loop, a key without order, a finalizer, a bad sprite, a
 * bad room or a second keeper refuse the package with the
 * reason and the mod's line; a conversation that fails (a character the font
 * lacks, options too long, memory exhausted) stops the mod and not the
 * process; the guest's catalogue of routines accepts only what it lists; and
 * mods loaded together are sorted by name under one identity, and refused
 * when two share a name, one of the game's NPCs, or a room for their houses;
 * mod.description gives one line; and the launcher's mods folder lists its
 * mods in the order of their names, each with its description, its houses in
 * each game and a refusal with the loader's reason. */
#include "mod_folder.h"
#include "mod_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _WIN32
#include <direct.h>
#define make_dir(path) _mkdir(path)
#define remove_dir(path) _rmdir(path)
#else
#include <unistd.h>
#define make_dir(path) mkdir(path, 0777)
#define remove_dir(path) rmdir(path)
#endif

static int failures;

#define CHECK(condition) do { if (!(condition)) { failures++; fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #condition); } } while (0)

/* A package of one main.lua, and optionally a second file. */
static OraclesMod *load(const char *id, const char *main_lua, const char *other_name, const char *other, char *error, size_t capacity)
{
    OraclesModFile files[2] = { { "main.lua", main_lua, strlen(main_lua) }, { other_name, other, other ? strlen(other) : 0 } };
    error[0] = 0;
    return oracles_mod_load(id, files, other ? 2 : 1, ORACLES_GAME_AGES, NULL, 0, error, capacity);
}

/* A package whose main.lua is `source` is refused, with `reason` in the message. */
static void refused(const char *id, const char *source, const char *reason)
{
    char error[512];
    OraclesMod *mod = load(id, source, NULL, NULL, error, sizeof error);
    CHECK(mod == NULL);
    if (mod) oracles_mod_free(mod);
    else if (!strstr(error, reason)) { failures++; fprintf(stderr, "FAIL %s: \"%s\" does not say \"%s\"\n", id, error, reason); }
}

/* The conversation of a mod's one NPC, run without a game until it ends: whether the mod stopped, and why. */
static void conversation(const char *id, const char *body, const char *reason)
{
    char source[2048], error[512];
    snprintf(source, sizeof source, "mod.npc(\"someone\", { existing = { ages = \"3/f8\" } }, function(talk)\n%s\nend)\n", body);
    OraclesMod *mod = load(id, source, NULL, NULL, error, sizeof error);
    CHECK(mod != NULL);
    if (!mod) { fprintf(stderr, "%s: %s\n", id, error); return; }
    int running = mod_lua_start_conversation(mod, 0) == 0;
    for (unsigned frame = 0; running && frame < 600; frame++) running = mod_lua_conversation_frame(mod, frame & 1u ? 0x10u : 0u);
    if (!reason) CHECK(!mod->faulted);
    else if (!mod->faulted || !strstr(mod->error, reason)) { failures++; fprintf(stderr, "FAIL %s: \"%s\" does not say \"%s\"\n", id, mod->error, reason); }
    oracles_mod_free(mod);
}

static const char sandbox[] =
    "assert(os == nil and io == nil and load == nil and dofile == nil and loadfile == nil and debug == nil)\n"
    "assert(package == nil and collectgarbage == nil and coroutine == nil and pcall == nil and xpcall == nil and rawset == nil)\n"
    "assert(math.random == nil and math.randomseed == nil and string.dump == nil and (\"x\").dump == nil)\n"
    "local keys = {}\n"
    "for key in pairs({ z = 1, [2] = 1, a = 1, [1] = 1, [true] = 1, [false] = 1 }) do keys[#keys + 1] = tostring(key) end\n"
    "assert(table.concat(keys, \",\") == \"1,2,a,z,false,true\", table.concat(keys, \",\"))\n"
    "assert(next({ b = 1, a = 2 }) == \"a\")\n"
    "assert(tostring({}) == \"table\" and tostring(print) == \"function\")\n"
    "assert(string.format(\"%s %s\", {}, print) == \"table function\")\n"
    "assert((\"a,b\"):gsub(\",\", \";\") == \"a;b\" and (\"x=12\"):match(\"(%d+)\") == \"12\")\n"
    "local helper = require(\"helper\")\n"
    "assert(helper.value == 42 and require(\"helper\") == helper)\n"
    "local size = mod.sprite(\"dot\", { palette = { k = \"#000000\" }, pixels = [[\n  k.\n  .k\n  k.\n]] })\n"
    "assert(size.width == 2 and size.height == 3)\n"
    "assert(game.sound(\"snd_getseed\") == \"snd_getseed\" and game.treasure(\"gasha_seed\") == \"gasha_seed\")\n"
    "local Class = {} Class.__index = Class\n"
    "local object = setmetatable({}, Class)\n"
    "function Class:hello() return \"hi\" end\n"
    "assert(object:hello() == \"hi\")\n"
    "local Derived = setmetatable({}, { __index = Class }) Derived.__index = Derived\n"
    "function Derived:bye() return \"bye\" end\n"
    "local child = setmetatable({}, Derived)\n"
    "assert(child:hello() == \"hi\" and child:bye() == \"bye\")\n"
    "mod.house(\"shop\", { ages = { room = \"0/55\", col = 7, row = 4 } })\n"
    "mod.npc(\"keeper\", { house = \"shop\" }, function(talk) end)\n";

/* A mod that loads, or NULL with the reason printed. */
static OraclesMod *quick(const char *id, const char *main_lua)
{
    char error[512];
    OraclesMod *mod = load(id, main_lua, NULL, NULL, error, sizeof error);
    if (!mod) { failures++; fprintf(stderr, "FAIL %s: %s\n", id, error); }
    return mod;
}

static const char talker[] = "mod.npc(\"a\", { existing = { ages = \"3/f8\" } }, function(talk) end)\n";
static const char house[] = "mod.house(\"h\", { ages = { room = \"0/55\", col = 7, row = 4 } })\nmod.npc(\"k\", { house = \"h\" }, function(talk) end)\n";

/* Mods loaded together: sorted by name under one identity, their keys passed through without a game; refused when two
 * share a name or talk as one NPC; their houses refused in one room before the ROM is read. */
static void sets(void)
{
    char error[512];
    OraclesMod *pair[2] = { quick("zeta", talker), quick("alpha", "") };
    OraclesModSet *set = pair[0] && pair[1] ? oracles_mod_set_create(pair, 2, error, sizeof error) : NULL;
    CHECK(set != NULL);
    if (set) {
        const char *identity = oracles_mod_set_identity(set);
        CHECK(strncmp(identity, "alpha@", 6) == 0 && strstr(identity, ",zeta@") != NULL && strlen(identity) == 46u + 1u + 45u);
        CHECK(oracles_mod_set_count(set) == 2 && strcmp(oracles_mod_identity(oracles_mod_set_mod(set, 0)), identity) != 0);
        CHECK(oracles_mod_set_frame(set, 0, 0x11u) == 0x11u && !oracles_mod_set_busy(set) && oracles_mod_set_house_count(set) == 0);
        oracles_mod_set_free(set);
    }
    OraclesMod *twins[2] = { quick("same", ""), quick("same", "-- another\n") };
    CHECK(twins[0] && twins[1] && oracles_mod_set_create(twins, 2, error, sizeof error) == NULL && strstr(error, "two mods are named same"));
    OraclesMod *rivals[2] = { quick("one", talker), quick("two", talker) };
    CHECK(rivals[0] && rivals[1] && oracles_mod_set_create(rivals, 2, error, sizeof error) == NULL && strstr(error, "both talk as the NPC of 3/f8"));
    OraclesMod *builders[2] = { quick("east", house), quick("west", house) };
    if (builders[0] && builders[1]) {
        uint8_t *image = NULL;
        size_t size = 0;
        CHECK(mod_compose(builders, 2, NULL, 0, &image, &size, error, sizeof error) != 0 && image == NULL && strstr(error, "are both in 0/55"));
        CHECK(mod_compose(builders, 0, NULL, 0, &image, &size, error, sizeof error) == 0 && image == NULL);   /* no house: the ROM itself */
    }
    for (unsigned i = 0; i < 2; i++) oracles_mod_free(builders[i]);
}

/* mod.storage: a conversation fills it, the game's save encodes it into a slot, a load gives it back; the set's text
 * keeps the slots of a mod not loaded; what cannot be saved stops the mod, with the reason. */
static void storage(void)
{
    char error[512];
    OraclesMod *mod = quick("keeper", "mod.npc(\"a\", { existing = { ages = \"3/f8\" } }, function(talk)\n"
                                     "  local s = mod.storage\n"
                                     "  s.visits = (s.visits or 0) + 1\n"
                                     "  s.name, s.ratio, s.done = \"Link\", 0.1, true\n"
                                     "  s.list = { 3, 2, { deep = \"x\" } }\n"
                                     "end)\n");
    if (!mod) return;
    for (unsigned visit = 1; visit <= 2; visit++) {
        CHECK(mod_lua_start_conversation(mod, 0) == 0);
        while (mod_lua_conversation_frame(mod, 0)) {}
        mod_store_file_operation(mod, 1, 2);       /* the game saves file 3 */
        mod_store_file_operation(mod, 2, 0);       /* loads file 1, empty */
        mod_store_file_operation(mod, 2, 2);       /* and file 3 again */
    }
    CHECK(!mod->faulted && mod->slot == 2 && mod->stored[2] && !mod->stored[0]);
    static const char encoded_visits[] = "{s4:donets4:list{i1;i3;i2;i2;i3;{s4:deeps1:x}}s4:names4:Links5:ratiod3fb999999999999a;s6:visitsi2;}";
    CHECK(mod->stored[2] && mod->stored_size[2] == sizeof encoded_visits - 1 && memcmp(mod->stored[2], encoded_visits, sizeof encoded_visits - 1) == 0);   /* keys sorted */

    /* The set's text: the slots, and those of a mod not loaded, kept and written back. */
    OraclesMod *one[1] = { mod };
    OraclesModSet *set = oracles_mod_set_create(one, 1, error, sizeof error);
    CHECK(set != NULL);
    if (!set) return;
    static const char file[] = "oracles-mod-storage 1\nslot absent 1 9\n{}x\n";
    CHECK(oracles_mod_set_storage_load(set, (const uint8_t *)file, sizeof file - 1, 0, error, sizeof error) != 0);   /* a bad length */
    static const char good[] = "oracles-mod-storage 1\nslot absent 1 2\n{}\nslot keeper 0 9\n{s1:ai7;}\n";
    CHECK(oracles_mod_set_storage_load(set, (const uint8_t *)good, sizeof good - 1, 0, error, sizeof error) == 0);
    CHECK(mod->stored[0] && mod->stored_size[0] == 9 && !mod->stored[2]);   /* the file replaces the slots */
    size_t size = 0;
    uint8_t *text = oracles_mod_set_storage_save(set, 0, &size, error, sizeof error);
    CHECK(text && size == sizeof good - 1 && strstr((const char *)text, "slot absent 1 2\n{}\n") && strstr((const char *)text, "slot keeper 0 9\n{s1:ai7;}\n"));
    free(text);
    /* A savestate's text: the live table and the file loaded come back. */
    mod_store_file_operation(mod, 2, 0);
    text = oracles_mod_set_storage_save(set, 1, &size, error, sizeof error);
    CHECK(text && strstr((const char *)text, "live keeper 0 9\n{s1:ai7;}\n") && !strstr((const char *)text, "absent"));
    mod_store_file_operation(mod, 0, 1);           /* a new file 2: empty, the live table too */
    CHECK(mod->slot == 1 && text && oracles_mod_set_storage_load(set, text, size, 1, error, sizeof error) == 0 && mod->slot == 0);
    free(text);
    size_t live = 0;
    char *encoded = mod_store_encode_live(mod, &live, error, sizeof error);
    CHECK(encoded && live == 9 && memcmp(encoded, "{s1:ai7;}", 9) == 0);
    free(encoded);
    /* A text that does not read is refused whole: the slots and the live table stay as they were. */
    {
        const size_t before = mod->stored_size[0];
        static const char broken[] = "oracles-mod-storage 1\nslot keeper 0 9\n{s1:ai7;}\nlive keeper 0 4\n{s1:\n";
        CHECK(before && oracles_mod_set_storage_load(set, (const uint8_t *)broken, sizeof broken - 1, 1, error, sizeof error) != 0);
        CHECK(mod->stored_size[0] == before);
    }
    /* The slots of a mod not loaded follow the file operations: a copy of file 2 onto file 3, then file 2 erased. */
    static const char absent[] = "oracles-mod-storage 1\nslot absent 1 9\n{s1:bi1;}\n";
    CHECK(oracles_mod_set_storage_load(set, (const uint8_t *)absent, sizeof absent - 1, 0, error, sizeof error) == 0);
    const unsigned changes = oracles_mod_set_storage_changes(set);
    mod_set_file_operation(set, 2, 1);
    mod_set_file_operation(set, 1, 2);
    mod_set_file_operation(set, 3, 1);
    uint8_t *after = oracles_mod_set_storage_save(set, 0, &size, error, sizeof error);
    CHECK(after && strstr((const char *)after, "slot absent 2 9\n{s1:bi1;}\n") && !strstr((const char *)after, "slot absent 1 ")
          && oracles_mod_set_storage_changes(set) > changes);
    free(after);
    /* A stopped mod's slots follow too: a new file empties its slot, a copy copies the slot saved. */
    mod_fault(mod, "stopped for the test");
    mod_set_file_operation(set, 2, 2);             /* file 3 (the visits) loaded */
    mod_set_file_operation(set, 1, 0);             /* and saved onto file 1: a copy */
    CHECK(mod->stored[0] && mod->stored[2] && mod->stored_size[0] == mod->stored_size[2]);
    mod_set_file_operation(set, 0, 2);             /* file 3 made anew */
    CHECK(!mod->stored[2] && mod->stored[0]);
    oracles_mod_set_free(set);

    /* What cannot be saved stops the mod when the game saves, with the reason; its slots keep what they held. */
    static const struct { const char *body; const char *reason; } bad[] = {
        { "mod.storage.f = print", "only booleans, numbers, strings and tables" },
        { "mod.storage[1.5] = 1", "keys are whole numbers or strings" },
        { "mod.storage.big = string.rep(\"x\", 20000)", "more than 16384 bytes" },
        { "local t = mod.storage for i = 1, 20 do t.next = {} t = t.next end", "more than 16 deep" },
        { "mod.storage.n = 0/0", "not finite" },
    };
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; i++) {
        char source[512];
        snprintf(source, sizeof source, "mod.npc(\"a\", { existing = { ages = \"3/f8\" } }, function(talk) %s end)\n", bad[i].body);
        OraclesMod *m = quick("bad", source);
        if (!m) continue;
        CHECK(mod_lua_start_conversation(m, 0) == 0);
        while (mod_lua_conversation_frame(m, 0)) {}
        mod_store_file_operation(m, 1, 0);
        if (!m->faulted || !strstr(m->error, bad[i].reason) || m->stored[0]) { failures++; fprintf(stderr, "FAIL storage %zu: \"%s\"\n", i, m->error); }
        oracles_mod_free(m);
    }
}

/* The call transaction's rules, mode by mode, on a ROM image made here: the entries the rules read are written where
 * Ages' tables place them (a collection behaviour, a treasure's objects, a rupee value), none of them the game's own.
 * Which treasure has which mode is read in the player's ROM at the time of a call; the rules are the game's. */
static uint8_t call_rom[0x100000];

static uint8_t *at(OraclesGuestSym sym, unsigned offset)
{
    return call_rom + (sym.bank ? (size_t)sym.bank * 0x4000u + (sym.addr - 0x4000u) : sym.addr) + offset;
}

static int gives(const OraclesGuestTables *t, const OraclesGuestRom *rom, uint8_t treasure, uint8_t parameter)
{
    const OraclesGuestCall call = { ORACLES_ROUTINE_GIVE_TREASURE, treasure, parameter };
    return oracles_guest_call_allowed(t, rom, &call);
}

static void calls(void)
{
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    const OraclesGuestRom rom = { call_rom, sizeof call_rom }, none = { NULL, 0 };
    uint8_t sword = 0, bracelet = 0, flute = 0;
    for (unsigned k = 0; k < ORACLES_GUEST_TREASURES; k++) {
        if (!strcmp(t->treasure_names[k], "sword")) sword = (uint8_t)k;
        if (!strcmp(t->treasure_names[k], "bracelet")) bracelet = (uint8_t)k;
        if (!strcmp(t->treasure_names[k], "flute")) flute = (uint8_t)k;
    }
    CHECK(sword && bracelet && flute && sword < t->treasure_collection_behaviour_count && flute < t->treasure_object_count);
    /* Each mode's bound, the mode read in the second byte's low nibble (its high bit says whether a sound plays). */
    static const struct { uint8_t mode, parameter; int allowed; } modes[] = {
        { 0x0, 0, 1 }, { 0x0, 1, 0 }, { 0x1, 7, 1 }, { 0x1, 8, 0 }, { 0x2, 0, 0 }, { 0x3, 0, 0 }, { 0x4, 0x10, 1 },
        { 0x4, 0x1a, 0 }, { 0x4, 0, 0 }, { 0x5, 3, 0 }, { 0x6, 0, 0 }, { 0x7, 0, 0 }, { 0x9, 0x3f, 1 }, { 0x9, 0x40, 0 },
        { 0xa, 4, 0 }, { 0xb, 7, 1 }, { 0xb, 8, 0 }, { 0xc, 0x50, 1 }, { 0xc, 0x51, 0 }, { 0xd, 0x99, 1 }, { 0xe, 20, 1 },
        { 0xe, 21, 0 }, { 0xf, 0x05, 1 },
    };
    for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
        at(t->treasure_collection_behaviours, 3u * sword)[1] = (uint8_t)(0x80u | modes[i].mode);
        if (gives(t, &rom, sword, modes[i].parameter) != modes[i].allowed) {
            failures++;
            fprintf(stderr, "FAIL mode $%x with $%02x: expected %s\n", modes[i].mode, modes[i].parameter, modes[i].allowed ? "allowed" : "refused");
        }
    }
    const OraclesGuestCall lose = { ORACLES_ROUTINE_LOSE_TREASURE, sword, 0 };
    CHECK(oracles_guest_call_allowed(t, &rom, &lose));
    at(t->treasure_collection_behaviours, 3u * sword)[1] = 0x06;   /* a dungeon's treasure is never lost by a mod either */
    CHECK(!oracles_guest_call_allowed(t, &rom, &lose) && oracles_guest_treasure_mode(t, &rom, sword) == 6);
    /* A level or a companion: only a parameter the game gives the treasure with, in its entry of treasureObjectData or in
     * the list its entry points to, which ends where the next list begins. */
    at(t->treasure_collection_behaviours, 3u * bracelet)[1] = 0x08;
    at(t->treasure_collection_behaviours, 3u * flute)[1] = 0x08;
    at(t->treasure_object_data, 4u * bracelet)[0] = 0x0a, at(t->treasure_object_data, 4u * bracelet)[1] = 0x02;
    const uint16_t list = (uint16_t)(t->treasure_object_lists_end.addr - 12u);
    uint8_t *flute_entry = at(t->treasure_object_data, 4u * flute);
    flute_entry[0] = 0x80, flute_entry[1] = (uint8_t)list, flute_entry[2] = (uint8_t)(list >> 8);
    uint8_t *subids = at((OraclesGuestSym){ t->treasure_object_data.bank, list }, 0);
    subids[1] = 0x0b, subids[5] = 0x0c;
    CHECK(gives(t, &rom, bracelet, 2) && !gives(t, &rom, bracelet, 1) && !gives(t, &rom, bracelet, 3));
    CHECK(gives(t, &rom, flute, 0x0b) && gives(t, &rom, flute, 0x0c) && !gives(t, &rom, flute, 0x0d) && !gives(t, &rom, flute, 1));
    subids[9] = 0x0d;
    CHECK(gives(t, &rom, flute, 0x0d));
    uint8_t *sword_entry = at(t->treasure_object_data, 4u * sword);   /* another list starting inside the flute's ends it */
    sword_entry[0] = 0x80, sword_entry[1] = (uint8_t)(list + 8u), sword_entry[2] = (uint8_t)((list + 8u) >> 8);
    CHECK(!gives(t, &rom, flute, 0x0d) && gives(t, &rom, flute, 0x0c));
    /* The rupee values: getRupeeValue's BCD words; a payment's value only below their count. */
    at(t->rupee_values, 2u * 4u)[0] = 0x10, at(t->rupee_values, 2u * 4u)[1] = 0x00;
    at(t->rupee_values, 2u * 5u)[0] = 0x99, at(t->rupee_values, 2u * 5u)[1] = 0x09;
    at(t->rupee_values, 2u * 6u)[0] = 0x0a;
    CHECK(oracles_guest_rupee_amount(t, &rom, 4) == 10 && oracles_guest_rupee_amount(t, &rom, 5) == 999 && oracles_guest_rupee_amount(t, &rom, 6) < 0);
    CHECK(oracles_guest_rupee_amount(t, &rom, t->rupee_value_count) < 0);
    const OraclesGuestCall rupees = { ORACLES_ROUTINE_REMOVE_RUPEES, t->rupee_value_count, 0 };
    const OraclesGuestCall silence = { ORACLES_ROUTINE_PLAY_SOUND, 0, 0 };
    const OraclesGuestCall unknown = { (OraclesGuestRoutine)99, 0, 0 };
    CHECK(!oracles_guest_call_allowed(t, &rom, &rupees) && !oracles_guest_call_allowed(t, &rom, &silence) && !oracles_guest_call_allowed(t, &rom, &unknown));
    /* Without an image, or a treasure past the table, no treasure is given. */
    at(t->treasure_collection_behaviours, 3u * sword)[1] = 0x00;
    CHECK(gives(t, &rom, sword, 0) && !gives(t, &none, sword, 0) && oracles_guest_treasure_mode(t, &rom, 0x7f) < 0);
}

static void write_file(const char *dir, const char *name, const char *text)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *out = fopen(path, "wb");
    CHECK(out != NULL);
    if (!out) return;
    fputs(text, out);
    fclose(out);
}

/* What folder() makes, removed before it runs (a run before may have left it) and after. */
static void clear_folder(const char *root)
{
    static const char *const files[] = { "valid/main.lua", "bare/main.lua", "absent-main/helper.lua", "ages-room/main.lua", "notes.txt" };
    static const char *const dirs[] = { "valid", "bare", "absent-main", "ages-room", ".hidden", "" };
    char path[1100];
    for (size_t i = 0; i < sizeof files / sizeof files[0]; i++) { snprintf(path, sizeof path, "%s/mods/%s", root, files[i]); remove(path); }
    for (size_t i = 0; i < sizeof dirs / sizeof dirs[0]; i++) { snprintf(path, sizeof path, "%s/mods/%s", root, dirs[i]); remove_dir(path); }
}

/* The mods folder in `root` (made here): a mod with a description and a house in each game, one without a
 * description, one refused for its missing main.lua, a file and a hidden directory that are no mods. */
static void folder(const char *root)
{
    char folder[1024], dir[1100];
    OraclesModEntry entries[8];
    snprintf(folder, sizeof folder, "%s/mods", root);
    make_dir(root);
    clear_folder(root);
    CHECK(oracles_mod_folder_list(folder, ORACLES_GAME_AGES, entries, 8) == 0);   /* absent: no mod */
    make_dir(folder);
    CHECK(oracles_mod_folder_list(folder, ORACLES_GAME_AGES, entries, 8) == 0);   /* empty */
    snprintf(dir, sizeof dir, "%s/valid", folder);
    make_dir(dir);
    write_file(dir, "main.lua", "mod.description(\"A house in each game\")\n"
                                "mod.house(\"h\", { ages = { room = \"0/55\", col = 7, row = 4 }, seasons = { room = \"0/e9\", col = 3, row = 2 } })\n"
                                "mod.npc(\"k\", { house = \"h\" }, function(talk) end)\n");
    snprintf(dir, sizeof dir, "%s/bare", folder);
    make_dir(dir);
    write_file(dir, "main.lua", "mod.npc(\"a\", { existing = { ages = \"3/f8\" } }, function(talk) end)\n");
    snprintf(dir, sizeof dir, "%s/absent-main", folder);
    make_dir(dir);
    write_file(dir, "helper.lua", "return 1\n");
    snprintf(dir, sizeof dir, "%s/.hidden", folder);
    make_dir(dir);
    write_file(folder, "notes.txt", "not a mod\n");

    size_t count = oracles_mod_folder_list(folder, ORACLES_GAME_AGES, entries, 8);
    CHECK(count == 3);
    if (count == 3) {
        CHECK(!strcmp(entries[0].name, "absent-main") && !strcmp(entries[1].name, "bare") && !strcmp(entries[2].name, "valid"));
        CHECK(!strcmp(entries[0].refusal, "Refused: mod absent-main has no main.lua") && !entries[0].description[0]);
        CHECK(!entries[0].houses[0] && !entries[0].houses[1]);
        CHECK(!entries[1].refusal[0] && !entries[1].description[0] && !entries[1].houses[0] && !entries[1].houses[1]);
        CHECK(!entries[2].refusal[0] && !strcmp(entries[2].description, "A house in each game") && entries[2].houses[0] == 1 && entries[2].houses[1] == 1);
    }
    CHECK(oracles_mod_folder_list(folder, ORACLES_GAME_AGES, entries, 2) == 2 && !strcmp(entries[1].name, "bare"));   /* the first by name */

    /* The refusal is the one of the game shown. */
    snprintf(dir, sizeof dir, "%s/ages-room", folder);
    make_dir(dir);
    write_file(dir, "main.lua", "mod.npc(\"a\", { existing = { seasons = \"zz\" } }, function(talk) end)\n");
    count = oracles_mod_folder_list(folder, ORACLES_GAME_AGES, entries, 8);
    CHECK(count == 4 && !strcmp(entries[1].name, "ages-room") && !entries[1].refusal[0]);
    count = oracles_mod_folder_list(folder, ORACLES_GAME_SEASONS, entries, 8);
    CHECK(count == 4 && strstr(entries[1].refusal, "Refused: mod ages-room: main.lua:1:") == entries[1].refusal);

    /* The package the folder reads is the one a session loads. */
    snprintf(dir, sizeof dir, "%s/valid", folder);
    char error[512];
    OraclesMod *mod = oracles_mod_folder_load(dir, ORACLES_GAME_SEASONS, NULL, 0, error, sizeof error);
    CHECK(mod != NULL && oracles_mod_house_count(mod) == 1 && !strncmp(oracles_mod_identity(mod), "valid@", 6));
    oracles_mod_free(mod);
    clear_folder(root);
}

int main(int argc, char **argv)
{
    char error[512];

    /* A package, its module and its identity: the directory's name and a SHA-1 of its files. */
    OraclesMod *mod = load("good", sandbox, "helper.lua", "return { value = 42 }\n", error, sizeof error);
    CHECK(mod != NULL);
    if (!mod) fprintf(stderr, "good: %s\n", error);
    char first[128] = "";
    if (mod) {
        snprintf(first, sizeof first, "%s", oracles_mod_identity(mod));
        CHECK(strncmp(first, "good@", 5) == 0 && strlen(first) == 45);
        CHECK(!oracles_mod_busy(mod) && oracles_mod_error(mod) == NULL && oracles_mod_house_count(mod) == 1);
        CHECK(oracles_mod_frame(mod, 0, 0x11u) == 0x11u);   /* without a game attached, a frame passes the keys through */
        oracles_mod_free(mod);
    }
    mod = load("good", sandbox, "helper.lua", "return { value = 42 }\r\n", error, sizeof error);
    CHECK(mod != NULL && strcmp(oracles_mod_identity(mod), first) == 0);   /* a CRLF checkout is the same mod */
    if (mod) oracles_mod_free(mod);
    mod = load("good", sandbox, "helper.lua", "return { value = 42 } -- changed\n", error, sizeof error);
    CHECK(mod != NULL && strcmp(oracles_mod_identity(mod), first) != 0);   /* any change to a file changes the identity */
    if (mod) oracles_mod_free(mod);

    /* Refusals, with their reason; an error of the engine's primitives names the mod's line. */
    refused("loop", "while true do end\n", "more Lua instructions");
    refused("format", "local s = string.format(\"%p\", {})\n", "%p");
    refused("keys", "for key in pairs({ [{}] = 1 }) do end\n", "no fixed order");
    refused("gc", "setmetatable({}, { __gc = function() end })\n", "__gc");
    refused("late-mode", "local mt = {}\nsetmetatable({}, mt)\nmt.__mode = \"k\"\n", "main.lua:3:");
    refused("meta-meta", "local mm = setmetatable({}, {})\nlocal t = setmetatable({}, mm)\nmm.__mode = \"v\"\n", "main.lua:3:");
    refused("hidden-meta", "local mm = setmetatable({}, { __metatable = \"no\" })\nsetmetatable({}, mm)\n", "main.lua:2:");
    refused("memory", "local s = string.rep(\"x\", 64 * 1024 * 1024)\n", "memory");
    refused("move", "table.move({}, 1, 3e8, 1)\n", "more work than a frame allows");
    refused("pattern", "local s = (\"a\"):rep(40)\nlocal f = s:find((\"a-\"):rep(12) .. \"b\")\n", "main.lua:2:");
    refused("sprite", "mod.sprite(\"bad\", { palette = { k = \"#000000\" }, pixels = [[\n  kk\n  k\n]] })\n", "row 2");
    refused("room", "mod.npc(\"a\", { existing = { ages = \"zz\" } }, function(talk) end)\n", "G/RR");
    refused("sound", "\n\nlocal JINGLE = game.sound(\"snd_nope\")\n", "main.lua:3:");
    refused("keepers", "mod.house(\"h\", { ages = { room = \"0/55\", col = 7, row = 4 } })\n"
                       "mod.npc(\"a\", { house = \"h\" }, function(talk) end)\nmod.npc(\"b\", { house = \"h\" }, function(talk) end)\n", "keeper already");
    refused("column", "mod.house(\"h\", { ages = { room = \"0/55\", col = 9, row = 4 } })\n", "main.lua:1:");
    refused("module", "require(\"../main\")\n", "a module is a file of the mod");
    refused("syntax", "this is not lua\n", "main.lua");
    refused("not-running", "game.pay(10)\n", "the game is not running yet");
    /* A house of Seasons only: none in Ages, and its keeper waits for the other game. */
    mod = load("seasons-only", "mod.house(\"h\", { seasons = { room = \"0/e9\", col = 3, row = 2 } })\nmod.npc(\"k\", { house = \"h\" }, function(talk) end)\n",
               NULL, NULL, error, sizeof error);
    CHECK(mod != NULL && oracles_mod_house_count(mod) == 0);
    if (mod) oracles_mod_free(mod);
    OraclesModFile helper_only = { "helper.lua", "return 1\n", 9 };
    CHECK(oracles_mod_load("nomain", &helper_only, 1, ORACLES_GAME_AGES, NULL, 0, error, sizeof error) == NULL && strstr(error, "no main.lua"));
    CHECK(load("Bad Name", "", NULL, NULL, error, sizeof error) == NULL && strstr(error, "a-z, 0-9"));

    /* mod.description: one line, given once. */
    mod = load("described", "mod.description(\"A claw game\")\n", NULL, NULL, error, sizeof error);
    CHECK(mod != NULL && !strcmp(oracles_mod_description(mod), "A claw game"));
    if (mod) oracles_mod_free(mod);
    mod = load("undescribed", "\n", NULL, NULL, error, sizeof error);
    CHECK(mod != NULL && !strcmp(oracles_mod_description(mod), "") && oracles_mod_npc_count(mod) == 0);
    if (mod) oracles_mod_free(mod);
    refused("described-twice", "mod.description(\"a\")\nmod.description(\"b\")\n", "main.lua:2: mod.description: a mod has one description");
    refused("two-lines", "mod.description(\"a\\nb\")\n", "one line of 100 bytes at most");
    refused("long", "mod.description(string.rep(\"x\", 101))\n", "one line of 100 bytes at most");
    refused("not-text", "mod.description({})\n", "main.lua:1: mod.description");

    /* Conversations run without a game: a failure stops the mod, never the process. */
    conversation("talks", "talk:say(\"Hello there!\")\nassert(talk:random(6) >= 1)", NULL);
    conversation("font", "talk:say(\"Ô rôdeur\")", "no 'Ô'");
    conversation("ask", "talk:ask(\"Which?\", { \"A dog\", \"A man\", \"A Moblin\" })", "a line of the box holds 16");
    conversation("scene-rect", "talk:play({ update = function() return false end, draw = function(self, g) g:rect(-30000, -30000, 60000, 60000, \"#ffffff\") end })", NULL);
    conversation("oom", "local keep = {}\ntalk:play({ update = function() keep[#keep + 1] = string.rep(\"x\", 200000) .. #keep end, draw = function() end })", "memory");
    conversation("copies", "local s = string.rep(\"x\", 1000000)\nfor i = 1, 150000 do local t = s .. \"y\" end", "more work than a frame allows");
    conversation("bomb", "local s = (\"a\"):rep(40)\nreturn s:find((\"a-\"):rep(12) .. \"b\")", "too long");

    /* The catalogue of the game's routines (the call transaction): a treasure only with a parameter the game bounds. */
    calls();
    const OraclesGuestTables *t = &oracles_guest_tables_ages;
    CHECK(!strcmp(t->ring_names[1], "power_ring_l1") && !t->treasure_names[0x10][0]);

    sets();
    storage();
    if (argc > 1) folder(argv[1]);

    if (failures) fprintf(stderr, "%d failure(s)\n", failures);
    else printf("mods: all checks passed\n");
    return failures ? 1 : 0;
}
