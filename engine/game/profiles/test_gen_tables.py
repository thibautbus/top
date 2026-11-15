"""ROM-free checks of the fan games' table generator."""
import copy
import json
import unittest
from pathlib import Path

from gen_tables import FAN_GAMES, GUEST_TYPES, generate, generate_identity, parse_fields, resolve, symbol

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[2]


def manifests():
    for name, manifest in FAN_GAMES:
        yield name, manifest, json.loads((ROOT / manifest).read_text())


class TableGenerationTests(unittest.TestCase):
    def test_aliases_and_absence(self):
        spec = {"guest_table": {"first": {"bank": 3, "address": "0xd000"},
                                "second": "guest_table.first"}}
        self.assertEqual(symbol(spec, "guest_table.second"), "{ 0x03, 0xd000 }")
        self.assertEqual(symbol(spec, None), "{ ORACLES_GUEST_ABSENT, 0 }")

    def test_no_implicit_fallback(self):
        with self.assertRaises(KeyError):
            resolve({"guest_table": {}}, "guest_table.missing")
        with self.assertRaises(ValueError):
            resolve({"guest_table": {"a": "guest_table.a"}}, "guest_table.a")

    def test_checked_in_tables_match_their_manifests(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                self.assertEqual(generate(spec, name, manifest), ((ROOT / manifest).parent / "tables.c").read_text())
                self.assertEqual(generate_identity(spec, name, manifest), ((ROOT / manifest).parent / "identity.h").read_text())

    def test_unknown_keys_are_refused(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                noted = copy.deepcopy(spec)
                first = next(iter(noted["guest_table"]))
                noted["guest_table"][first] = {"bank": 0, "address": "0x0100", "note": "how it was found"}
                with self.assertRaisesRegex(ValueError, "unknown key note"):
                    generate(noted, name, manifest)
                noted = copy.deepcopy(spec)
                noted["report"] = {}
                with self.assertRaisesRegex(ValueError, "unknown section report"):
                    generate(noted, name, manifest)

    def test_guest_table_is_exhaustive(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                missing = copy.deepcopy(spec)
                missing["guest_table"].pop(next(iter(missing["guest_table"])))
                with self.assertRaises(ValueError):
                    generate(missing, name, manifest)
                extra = copy.deepcopy(spec)
                extra["guest_table"]["not_a_guest_field"] = None
                with self.assertRaises(ValueError):
                    generate(extra, name, manifest)

    def test_absent_symbols_need_their_reason(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                forgotten = copy.deepcopy(spec)
                forgotten["guest_table"]["draw_all_sprites"] = None   # unbound, named nowhere
                with self.assertRaises(ValueError):
                    generate(forgotten, name, manifest)
                named = copy.deepcopy(forgotten)
                named["guest_table_absent"]["draw_all_sprites"] = ""
                with self.assertRaises(ValueError):
                    generate(named, name, manifest)
                stale = copy.deepcopy(spec)
                stale["guest_table_absent"]["draw_all_sprites"] = "bound all the same"
                with self.assertRaises(ValueError):
                    generate(stale, name, manifest)

    def test_other_fields_are_exhaustive(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                for section in ("guest_table_sizes", "guest_table_constants", "guest_table_empty"):
                    missing = copy.deepcopy(spec)
                    missing[section].pop(next(iter(missing[section])))
                    with self.assertRaises(ValueError):
                        generate(missing, name, manifest)
                for invalid in ("0x100", -1, None):
                    out_of_range = copy.deepcopy(spec)
                    out_of_range["guest_table_constants"]["roomflag_visited"] = invalid
                    with self.assertRaises(ValueError):
                        generate(out_of_range, name, manifest)


class FieldTypeTests(unittest.TestCase):
    def test_unknown_type_and_whole_arrays_fail(self):
        self.assertEqual(parse_fields("OraclesGuestSym a, b; uint8_t c; char d[4][5];", "T", GUEST_TYPES)["OraclesGuestSym"], {"a", "b"})
        for block in ("unsigned e;", "uint32_t f;", "OraclesGuestSym g[2];", "int16_t h[3];"):
            with self.assertRaises(ValueError):
                parse_fields(block, "T", GUEST_TYPES)
        # A table of numbers is a kind of its own, which the manifest must name empty with its reason.
        self.assertEqual(parse_fields("uint8_t i[3]; uint16_t j[2];", "T", GUEST_TYPES)["uint8_t[]"], {"i"})
        self.assertEqual(parse_fields("uint8_t i[3]; uint16_t j[2];", "T", GUEST_TYPES)["uint16_t[]"], {"j"})

    def test_data_table_is_exhaustive(self):
        for name, manifest, spec in manifests():
            with self.subTest(name):
                for section in ("data_tables", "data_tables_empty"):
                    missing = copy.deepcopy(spec)
                    missing[section].pop(next(iter(missing[section])))
                    with self.assertRaises(ValueError):
                        generate(missing, name, manifest)
                both = copy.deepcopy(spec)
                both["data_tables_empty"]["tileset_data"] = "twice"
                with self.assertRaises(ValueError):
                    generate(both, name, manifest)


def halves():
    return {spec["half"]: (name, manifest, spec) for name, manifest, spec in manifests() if "half" in spec}


class CombinedHalfTests(unittest.TestCase):
    def test_the_halves_share_their_images(self):
        both = halves()
        self.assertEqual(sorted(both), ["ages", "seasons"])
        ages, seasons = both["ages"][2], both["seasons"][2]
        self.assertEqual(list(ages["rom_sha1"]), ["en", "fr", "de", "es", "it"])
        self.assertEqual(ages["rom_sha1"], seasons["rom_sha1"])
        self.assertEqual(ages["rom_size"], seasons["rom_size"])
        for common in ("current_game", "swap_game_request", "swap_game"):
            self.assertEqual(ages["half_switch"][common], seasons["half_switch"][common])

    def test_the_cartridge_ram_is_its_own_space(self):
        sram = {"guest_table": {"run": {"space": "sram", "bank": 0x41, "address": "0xa333"}}}
        self.assertEqual(symbol(sram, "guest_table.run"), "{ 0x41, 0xa333 }")
        for item in ({"bank": 0x41, "address": "0xa333"}, {"space": "sram", "bank": 0x01, "address": "0xa333"},
                     {"space": "sram", "bank": 0x41, "address": "0x6333"}, {"space": "sram", "bank": 0x41, "address": "0xc000"}):
            with self.assertRaises(ValueError):
                symbol({"guest_table": {"run": item}}, "guest_table.run")
        name, manifest, spec = halves()["seasons"]
        moved = copy.deepcopy(spec)
        moved["guest_table"]["update_all_objects"].pop("space")
        with self.assertRaises(ValueError):
            generate(moved, name, manifest)

    def test_the_item_texts_are_given_or_named_empty(self):
        for name, manifest, spec in halves().values():
            with self.subTest(name):
                table = generate(spec, name, manifest)
                self.assertIn('"Slingshot"', table)
                self.assertIn('"snd_selectitem"', table)
                both = copy.deepcopy(spec)
                both["guest_table_empty"]["item_names"] = "given and empty at once"
                with self.assertRaises(ValueError):
                    generate(both, name, manifest)
                neither = copy.deepcopy(spec)
                neither["guest_table_texts"].pop("item_names")
                with self.assertRaises(ValueError):
                    generate(neither, name, manifest)
                for field, bad in (("item_labels", ["X"] * 31), ("item_names", ["Rod; of"] * 32), ("sound_names", ["SND_X"] * 256), ("ring_names", [""] * 64)):
                    wrong = copy.deepcopy(spec)
                    wrong["guest_table_texts"][field] = bad
                    wrong["guest_table_empty"].pop(field, None)
                    with self.assertRaises(ValueError):
                        generate(wrong, name, manifest)

    def test_one_guest_table_per_language(self):
        for name, manifest, spec in halves().values():
            with self.subTest(name):
                tables = generate(spec, name, manifest)
                self.assertEqual(tables.count("const OraclesGuestTables "), len(spec["rom_sha1"]))
                self.assertEqual(tables.count("const OraclesTables "), 1)
                for language, section in spec["languages"].items():
                    table = tables.split(f"oracles_guest_tables_{name}_{language} = {{", 1)[1].split("};", 1)[0]
                    split = int(section["guest_table_constants"]["text_offset_split_index"], 0)
                    self.assertIn(f".text_offset_split_index = 0x{split:02x},", table)

    def test_language_sections_are_whole(self):
        for name, manifest, spec in halves().values():
            with self.subTest(name):
                missing = copy.deepcopy(spec)
                missing["languages"].pop("it")
                with self.assertRaises(ValueError):
                    generate(missing, name, manifest)
                reordered = copy.deepcopy(spec)
                reordered["languages"] = dict(reversed(list(reordered["languages"].items())))
                with self.assertRaises(ValueError):
                    generate(reordered, name, manifest)
                uneven = copy.deepcopy(spec)
                uneven["languages"]["fr"]["guest_table_constants"]["roomflag_item"] = "0x20"
                with self.assertRaises(ValueError):
                    generate(uneven, name, manifest)
                twice = copy.deepcopy(spec)
                twice["guest_table_constants"]["text_offset_split_index"] = "0x30"
                with self.assertRaises(ValueError):
                    generate(twice, name, manifest)
                noted = copy.deepcopy(spec)
                noted["languages"]["en"]["text"]["note"] = "how it was found"
                with self.assertRaisesRegex(ValueError, "unknown key note"):
                    generate(noted, name, manifest)

    def test_a_half_s_keys_belong_to_a_half(self):
        for name, manifest, spec in manifests():
            if "half" in spec:
                continue
            with self.subTest(name):
                for key, value in (("languages", {}), ("half_switch", {}), ("rom_sha1", {"en": spec["rom_sha1"]})):
                    other = copy.deepcopy(spec)
                    other[key] = value
                    with self.assertRaises(ValueError):
                        generate(other, name, manifest)
        name, manifest, spec = halves()["ages"]
        for key, value in (("half", "both"), ("rom_size", 4194304), ("rom_sha1", "9158458fe61ccba2ed35d4ce7e567865a2accd4a")):
            other = copy.deepcopy(spec)
            other[key] = value
            with self.assertRaises(ValueError):
                generate(other, name, manifest)

    def test_the_half_s_identity(self):
        for half, (name, manifest, spec) in halves().items():
            with self.subTest(half):
                identity = generate_identity(spec, name, manifest)
                base = 0x100 if half == "seasons" else 0
                self.assertIn(f"#define ORACLES_{name.upper()}_ROM_BANK_BASE 0x{base:03x}u", identity)
                self.assertIn(f"#define ORACLES_{name.upper()}_IMAGE_OFFSET 0x{base * 0x4000:06x}u", identity)
                for language, sha1 in spec["rom_sha1"].items():
                    self.assertIn(f'#define ORACLES_{name.upper()}_SHA1_{language.upper()} "{sha1}"', identity)


if __name__ == "__main__":
    unittest.main()
