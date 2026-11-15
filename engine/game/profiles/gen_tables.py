#!/usr/bin/env python3
"""Render a fan game's explicit integration bindings; never infer an original's addresses.

Offline, its output checked in (engine/game/profiles/<name>/). The fan game's
manifest, beside its table, is the sole address source. Unresolved bindings are absent,
not zero-filled function addresses. Generating a table does not register or
enable its compatibility profile.

A manifest with a `half` is one half of Oracles Combined, one image of both games: its banks are relative to the half,
its identity is the images of every language of the release (`rom_sha1` by language, the same in both halves), and what
differs by language is in its `languages` section: one guest table is written per language.

usage: gen_tables.py --manifest MANIFEST.json --name NAME --output tables.c --identity-output identity.h
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path


def struct_block(header: str, name: str) -> str:
    path = Path(__file__).resolve().parents[1] / header
    text = re.sub(r"/\*.*?\*/", "", path.read_text(encoding="utf-8"), flags=re.S)
    return text.split(f"typedef struct {name} {{", 1)[1].split(f"}} {name}", 1)[0]


def fields_by_type(header: str, name: str, known: dict[str, bool]) -> dict[str, set[str]]:
    return parse_fields(struct_block(header, name), name, known)


def parse_fields(block: str, name: str, known: dict[str, bool]) -> dict[str, set[str]]:
    """Every field of a structure, by type.  `known` maps each type the
    generator writes to whether it may be an array; any other type, or an
    array of a type written whole, fails: no field takes the zero of C by
    being of a type nobody reads."""
    out = {kind: set() for kind in known}
    for declaration in block.split(";"):
        declaration = " ".join(declaration.split())
        if not declaration:
            continue
        match = re.fullmatch(r"(const char \*|const char|[A-Za-z_][A-Za-z0-9_]*) ?(.+)", declaration)
        kind = match.group(1).replace(" *", "*").strip() if match else declaration
        if match and "[" in match.group(2) and kind + "[]" in known:
            kind += "[]"   # a table of numbers (the rupee amounts): named empty with its reason, as the texts
        if not match or kind not in known:
            raise ValueError(f"{name}: a field of a type the generator does not write: {declaration!r}")
        for field in match.group(2).split(","):
            array = "[" in field
            field = re.sub(r"\[[^\]]*\]", "", field).strip().lstrip("*").strip()
            if not re.fullmatch(r"[a-z][a-z0-9_]*", field):
                raise ValueError(f"{name}: invalid declaration: {field!r}")
            if array and not known[kind]:
                raise ValueError(f"{name}: an array of {kind} the generator would write whole: {field}")
            out[kind].add(field)
    return out


GUEST_TYPES = {"OraclesGuestSym": False, "uint16_t": False, "uint8_t": False, "char": True, "const char*": True, "uint16_t[]": True, "uint8_t[]": True}
DATA_TYPES = {"OraclesSym": False, "unsigned": False}


def guest_fields() -> dict[str, set[str]]:
    return fields_by_type("guest/guest_tables.h", "OraclesGuestTables", GUEST_TYPES)


def guest_table_fields() -> set[str]:
    return guest_fields()["OraclesGuestSym"]


def guest_table_values() -> dict[str, set[str]]:
    """The fields that are not symbols: sizes, constants, and the item texts."""
    f = guest_fields()
    return {"guest_table_sizes": f["uint16_t"], "guest_table_constants": f["uint8_t"],
            "guest_table_empty": f["char"] | f["const char*"] | f["uint16_t[]"] | f["uint8_t[]"]}


def data_table_fields() -> set[str]:
    """OraclesTables, the ROM data readers' table: each field bound or named empty with its reason."""
    f = fields_by_type("data/oracles_rom.h", "OraclesTables", DATA_TYPES)
    return f["OraclesSym"] | f["unsigned"]


def check_exhaustive(section: str, expected: set[str], actual: set[str]) -> None:
    if actual != expected:
        missing = sorted(expected - actual)
        extra = sorted(actual - expected)
        raise ValueError(f"{section} fields differ: missing={missing}, extra={extra}")


def resolve(spec: dict, value: object, seen: tuple = ()) -> dict | None:
    if value is None or isinstance(value, dict):
        return value
    if not isinstance(value, str) or value in seen:
        raise ValueError(f"invalid or cyclic binding: {value!r}")
    section, name = value.split(".", 1)
    source = spec[section]
    if isinstance(source, list):
        target = next(item for item in source if item["name"] == name)
    else:
        target = source
        for part in name.split("."):   # a nested contract: static_contracts.expanded_tilesets.tileset_data
            target = target[part]
    return resolve(spec, target, seen + (value,))


# Oracles Combined runs code from the cartridge RAM: the first 8 KiB of its half's bank $41, copied to RAM bank 1 and
# run at $a000-$bfff.  An entry there is in the `sram` space: its address is where it runs, its bank the one copied.
SRAM_CODE_BANK = 0x41
SRAM_CODE = range(0xa000, 0xc000)


def symbol(spec: dict, value: object) -> str:
    item = resolve(spec, value)
    if item is None:
        return "{ ORACLES_GUEST_ABSENT, 0 }"
    bank = item["bank"]
    address = item["address"]
    address = int(address, 0) if isinstance(address, str) else address
    if not isinstance(bank, int) or not 0 <= bank < 255:
        raise ValueError("invalid bank")
    if not isinstance(address, int) or not 0 <= address <= 65535:
        raise ValueError("invalid address")
    if (item.get("space") == "sram") != (address in SRAM_CODE) or (address in SRAM_CODE and bank != SRAM_CODE_BANK):
        raise ValueError(f"an address in the cartridge RAM is in the sram space, copied from bank {SRAM_CODE_BANK:02x}, and only it: {item}")
    return f"{{ 0x{bank:02x}, 0x{address:04x} }}"


# The fan games whose table is checked in: (name, manifest from the repository's root, beside its table).
FAN_GAMES = [("moonrise", "engine/game/profiles/moonrise-regalia/manifest.json"),
             ("temple", "engine/game/profiles/temple-of-seasons/manifest.json"),
             ("kinomi", "engine/game/profiles/gifts-of-kinomi/manifest.json"),
             ("combined_ages", "engine/game/profiles/oracles-combined/ages/manifest.json"),
             ("combined_seasons", "engine/game/profiles/oracles-combined/seasons/manifest.json")]


# What a manifest may hold: its sections, and in them addresses, sizes, names and one-line reasons.  Any other key is
# refused, so that nothing but what the generator reads (the report of how an address was found, say) enters one.
TOP_KEYS = {"id", "rom_sha1", "rom_size", "static_contracts", "fields", "ghost_fields", "sites", "guest_table",
            "guest_table_absent", "guest_table_sizes", "guest_table_constants", "guest_table_empty", "guest_table_texts", "data_tables",
            "data_tables_empty", "half", "half_switch", "languages"}
# The texts a manifest may give, each a list of strings by identifier: the item hotkeys' label and name of each item a
# button can hold (gen_guest_tables.py's item_labels, from the game's TREASURE_* constants), the sounds' names (its
# constant_names, from SND_* and MUS_*: the hotkeys play SND_SELECTITEM by name); their counts, lengths and letters.
TEXT_FIELDS = {"item_labels": (32, 4, r"[A-Za-z ]*"), "item_names": (32, 32, r"[A-Za-z ]*"), "sound_names": (256, 40, r"[a-z0-9_]*")}
HALVES = ("ages", "seasons")
HALF_SIZE = 0x400000
# What a half's language section holds: the guest table's constants that differ by language, and the European text's
# third base (getTextAddress: the split from which a text is read from it, and its bank and address).
LANGUAGE_KEYS = {"guest_table_constants", "text"}
TEXT_KEYS = {"split_index_2", "offset_3"}
SWITCH_KEYS = {"current_game", "swap_game_request", "swap_game", "main_thread_start"}
ENTRY_KEYS = {"bank", "address", "length", "name", "space"}
CONTRACT_KEYS = ENTRY_KEYS | {"width", "height", "routines", "tileset_data", "room_tilesets_group_table", "animate",
                              "next_frame", "load_data", "binding"}


def refuse_unknown_keys(spec: dict) -> None:
    def entries(value: object, allowed: set[str], path: str) -> None:
        if isinstance(value, dict):
            unknown = sorted(set(value) - allowed)
            if unknown:
                raise ValueError(f"unknown key {unknown[0]} at {path}: a manifest holds addresses, sizes, names and reasons")
            for key, item in value.items():
                entries(item, allowed, f"{path}.{key}")
        elif isinstance(value, list):
            for i, item in enumerate(value):
                entries(item, allowed, f"{path}[{i}]")
    unknown = sorted(set(spec) - TOP_KEYS)
    if unknown:
        raise ValueError(f"unknown section {unknown[0]}: a manifest holds addresses, sizes, names and reasons")
    for section in ("fields", "ghost_fields", "sites"):
        entries(spec.get(section, []), ENTRY_KEYS, section)
    for section in ("guest_table", "data_tables"):
        for field, value in spec.get(section, {}).items():
            entries(value, ENTRY_KEYS, f"{section}.{field}")
    for contract, value in spec.get("static_contracts", {}).items():
        entries(value, CONTRACT_KEYS, f"static_contracts.{contract}")
    for field, value in spec.get("half_switch", {}).items():
        entries(value, ENTRY_KEYS, f"half_switch.{field}")
    for language, section in spec.get("languages", {}).items():
        unknown = sorted(set(section) - LANGUAGE_KEYS) + sorted(set(section.get("text", {})) - TEXT_KEYS)
        if unknown:
            raise ValueError(f"unknown key {unknown[0]} at languages.{language}: a manifest holds addresses, sizes, names and reasons")
        entries(section.get("text", {}).get("offset_3"), ENTRY_KEYS, f"languages.{language}.text.offset_3")


def check_half(spec: dict) -> list[str]:
    """A half of Oracles Combined: its name, its images by language, its switch's symbols, and its language sections,
    one per image, each overriding the same constants, which the main section leaves out.  Its languages, in order."""
    if spec.get("half") not in HALVES:
        raise ValueError(f"invalid half: {spec.get('half')!r}")
    sha1 = spec.get("rom_sha1")
    if not isinstance(sha1, dict) or not sha1 or any(not re.fullmatch(r"[a-z]{2}", k) for k in sha1):
        raise ValueError("a half's rom_sha1 is the image of each language, by language")
    if spec.get("rom_size") != 2 * HALF_SIZE:
        raise ValueError("a half's rom_size is the whole image's")
    if set(spec.get("half_switch", {})) != SWITCH_KEYS:
        raise ValueError(f"half_switch fields differ from {sorted(SWITCH_KEYS)}")
    for value in spec["half_switch"].values():
        symbol(spec, value)
    languages = spec.get("languages", {})
    if list(languages) != list(sha1):
        raise ValueError("a half's languages are those of its images, in the same order")
    overridden = [set(section.get("guest_table_constants", {})) for section in languages.values()]
    if any(fields != overridden[0] for fields in overridden):
        raise ValueError("every language overrides the same constants")
    both = overridden[0] & set(spec.get("guest_table_constants", {}))
    if both:
        raise ValueError(f"a constant set by language is left out of the main section: {sorted(both)}")
    for language, section in languages.items():
        text = section.get("text", {})
        if set(text) != TEXT_KEYS:
            raise ValueError(f"languages.{language}.text fields differ from {sorted(TEXT_KEYS)}")
        constant(text["split_index_2"], f"languages.{language}.text.split_index_2")
        symbol(spec, text["offset_3"])
    return list(sha1)


def constant(value: object, field: str) -> int:
    value = int(value, 0) if isinstance(value, str) else value
    if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= 255:
        raise ValueError(f"invalid constant: {field}")
    return value


def generate(spec: dict, name: str, source: str) -> str:
    refuse_unknown_keys(spec)
    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        raise ValueError(f"invalid name: {name}")
    if "half" not in spec:
        if {"half_switch", "languages"} & set(spec) or not isinstance(spec.get("rom_sha1", ""), str):
            raise ValueError("half_switch, languages and a rom_sha1 by language belong to a half of Oracles Combined")
        return guest_table(spec, name, source, name) + data_table(spec, name)
    languages = check_half(spec)
    out = ""
    for language in languages:
        merged = dict(spec, guest_table_constants={**spec["guest_table_constants"], **spec["languages"][language].get("guest_table_constants", {})})
        out += guest_table(merged, f"{name}_{language}", source, name if not out else None)
    return out + data_table(spec, name)


def guest_table(spec: dict, name: str, source: str, header: str | None) -> str:
    check_exhaustive("guest table", guest_table_fields(), set(spec["guest_table"]))
    # A symbol left unbound is named in guest_table_absent with its reason: none is absent by being forgotten.
    absent = spec.get("guest_table_absent", {})
    check_exhaustive("guest table absent", {f for f, v in spec["guest_table"].items() if resolve(spec, v) is None}, set(absent))
    for field, reason in absent.items():
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"an absent symbol needs its reason: {field}")
    # Every other field is named too: a size, a constant, or left empty with its
    # reason; none takes the zero of C by being forgotten.
    texts = spec.get("guest_table_texts", {})
    for section, expected in guest_table_values().items():
        check_exhaustive(section, expected - set(texts) if section == "guest_table_empty" else expected, set(spec.get(section, {})))
    lines = [f'/* Generated from {source}. */',
             '#include "guest_tables.h"', '#include "oracles_rom.h"',
             '#include "profile.h"', ''] if header else ['']
    lines += [f'const OraclesGuestTables oracles_guest_tables_{name} = {{']
    for field, value in spec["guest_table"].items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", field):
            raise ValueError(f"invalid field: {field}")
        lines.append(f"    .{field} = {symbol(spec, value)},")
    for field, size in spec["guest_table_sizes"].items():
        if not isinstance(size, int) or not 0 < size <= 65535:
            raise ValueError(f"invalid size: {field}")
        lines.append(f"    .{field} = {size},")
    for field, value in spec["guest_table_constants"].items():
        lines.append(f"    .{field} = 0x{constant(value, field):02x},")
    for field, values in texts.items():
        if field not in TEXT_FIELDS:
            raise ValueError(f"not a text field: {field}")
        count, longest, letters = TEXT_FIELDS[field]
        if not isinstance(values, list) or len(values) != count or any(not isinstance(v, str) or len(v) > longest or not re.fullmatch(letters, v) for v in values):
            raise ValueError(f"{field}: {count} texts of {letters}, {longest} at most each")
        lines.append(f"    .{field} = {{ " + ", ".join(f'"{v}"' for v in values) + " },")
    for field, reason in spec["guest_table_empty"].items():
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"an empty field needs its reason: {field}")
    return '\n'.join(lines + ['};', ''])


def data_table(spec: dict, name: str) -> str:
    check_exhaustive("data table", data_table_fields(), set(spec["data_tables"]) | set(spec["data_tables_empty"]))
    both = set(spec["data_tables"]) & set(spec["data_tables_empty"])
    if both:
        raise ValueError(f"data table fields both bound and empty: {sorted(both)}")
    lines = ['', f'const OraclesTables oracles_tables_{name} = {{']
    for field, value in spec["data_tables"].items():
        if resolve(spec, value) is None:
            raise ValueError(f"a data table field is bound or named empty, never null: {field}")
        lines.append(f"    .{field} = {symbol(spec, value)},")
    for field, reason in spec["data_tables_empty"].items():
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"an empty field needs its reason: {field}")
    lines += ['};', '']
    return '\n'.join(lines)


def generate_identity(spec: dict, name: str, source: str) -> str:
    if "half" in spec:
        return generate_half_identity(spec, name, source)
    sha1 = spec.get("rom_sha1")
    size = spec.get("rom_size")
    if not isinstance(sha1, str) or not re.fullmatch(r"[0-9a-fA-F]{40}", sha1):
        raise ValueError("invalid ROM SHA-1")
    if not isinstance(size, int) or size <= 0:
        raise ValueError("invalid ROM size")
    macro = f"ORACLES_{name.upper()}"
    return f"""/* Generated from {source}. */
#ifndef {macro}_IDENTITY_H
#define {macro}_IDENTITY_H

#define {macro}_SHA1 "{sha1.lower()}"
#define {macro}_ROM_SIZE {size}u

#endif
"""


def generate_half_identity(spec: dict, name: str, source: str) -> str:
    """A half's images, where the half lies in them and maps its banks, the cartridge RAM its code runs from, the
    symbols of the switch between the halves, and the European text's third base of each language."""
    languages = check_half(spec)
    macro = f"ORACLES_{name.upper()}"
    index = HALVES.index(spec["half"])
    lines = [f"/* Generated from {source}. */", f"#ifndef {macro}_IDENTITY_H", f"#define {macro}_IDENTITY_H", ""]
    for language in languages:
        sha1 = spec["rom_sha1"][language]
        if not isinstance(sha1, str) or not re.fullmatch(r"[0-9a-fA-F]{40}", sha1):
            raise ValueError("invalid ROM SHA-1")
        lines.append(f'#define {macro}_SHA1_{language.upper()} "{sha1.lower()}"')
    lines += [f"#define {macro}_ROM_SIZE {spec['rom_size']}u", f"#define {macro}_HALF {index}u",
              f"#define {macro}_ROM_BANK_BASE 0x{index * 0x100:03x}u", f"#define {macro}_IMAGE_OFFSET 0x{index * HALF_SIZE:06x}u",
              f"#define {macro}_HALF_SIZE 0x{HALF_SIZE:06x}u", f"#define {macro}_SRAM_CODE_BANK 0x{SRAM_CODE_BANK:02x}u",
              f"#define {macro}_SRAM_CODE_RAM_BANK 1u", ""]
    for field in sorted(SWITCH_KEYS):
        lines.append(f"#define {macro}_{field.upper()} {symbol(spec, spec['half_switch'][field])}")
    lines.append("")
    for language in languages:
        text = spec["languages"][language]["text"]
        lines.append(f"#define {macro}_TEXT_SPLIT_INDEX_2_{language.upper()} 0x{constant(text['split_index_2'], 'split_index_2'):02x}u")
        lines.append(f"#define {macro}_TEXT_OFFSET_3_{language.upper()} {symbol(spec, text['offset_3'])}")
    return "\n".join(lines + ["", "#endif", ""])


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True, help="relative to the repository's root")
    parser.add_argument("--name", required=True, help="the tables' and macros' name: oracles_guest_tables_NAME")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--identity-output", type=Path)
    args = parser.parse_args()
    spec = json.loads(args.manifest.read_text(encoding="utf-8"))
    source = args.manifest.as_posix()
    output = generate(spec, args.name, source)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(output, encoding="utf-8")
    if args.identity_output:
        args.identity_output.parent.mkdir(parents=True, exist_ok=True)
        args.identity_output.write_text(generate_identity(spec, args.name, source), encoding="utf-8")


if __name__ == "__main__":
    main()
