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


if __name__ == "__main__":
    unittest.main()
