#!/usr/bin/env python3
"""Render a fan game's explicit integration bindings; never infer an original's addresses.

Offline, its output checked in (engine/game/profiles/<name>/). The fan game's
manifest, beside its table, is the sole address source. Unresolved bindings are absent,
not zero-filled function addresses. Generating a table does not register or
enable its compatibility profile.

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
    return f"{{ 0x{bank:02x}, 0x{address:04x} }}"


# The fan games whose table is checked in: (name, manifest from the repository's root, beside its table).
FAN_GAMES = [("moonrise", "engine/game/profiles/moonrise-regalia/manifest.json"),
             ("temple", "engine/game/profiles/temple-of-seasons/manifest.json"),
             ("kinomi", "engine/game/profiles/gifts-of-kinomi/manifest.json")]


# What a manifest may hold: its sections, and in them addresses, sizes, names and one-line reasons.  Any other key is
# refused, so that nothing but what the generator reads (the report of how an address was found, say) enters one.
TOP_KEYS = {"id", "rom_sha1", "rom_size", "static_contracts", "fields", "ghost_fields", "sites", "guest_table",
            "guest_table_absent", "guest_table_sizes", "guest_table_constants", "guest_table_empty", "data_tables",
            "data_tables_empty"}
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


def generate(spec: dict, name: str, source: str) -> str:
    refuse_unknown_keys(spec)
    check_exhaustive("guest table", guest_table_fields(), set(spec["guest_table"]))
    # A symbol left unbound is named in guest_table_absent with its reason: none is absent by being forgotten.
    absent = spec.get("guest_table_absent", {})
    check_exhaustive("guest table absent", {f for f, v in spec["guest_table"].items() if resolve(spec, v) is None}, set(absent))
    for field, reason in absent.items():
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"an absent symbol needs its reason: {field}")
    # Every other field is named too: a size, a constant, or left empty with its
    # reason; none takes the zero of C by being forgotten.
    for section, expected in guest_table_values().items():
        check_exhaustive(section, expected, set(spec.get(section, {})))
    if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
        raise ValueError(f"invalid name: {name}")
    lines = [f'/* Generated from {source}. */',
             '#include "guest_tables.h"', '#include "oracles_rom.h"',
             '#include "profile.h"', '',
             f'const OraclesGuestTables oracles_guest_tables_{name} = {{']
    for field, value in spec["guest_table"].items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", field):
            raise ValueError(f"invalid field: {field}")
        lines.append(f"    .{field} = {symbol(spec, value)},")
    for field, size in spec["guest_table_sizes"].items():
        if not isinstance(size, int) or not 0 < size <= 65535:
            raise ValueError(f"invalid size: {field}")
        lines.append(f"    .{field} = {size},")
    for field, value in spec["guest_table_constants"].items():
        value = int(value, 0) if isinstance(value, str) else value
        if not isinstance(value, int) or isinstance(value, bool) or not 0 <= value <= 255:
            raise ValueError(f"invalid constant: {field}")
        lines.append(f"    .{field} = 0x{value:02x},")
    for field, reason in spec["guest_table_empty"].items():
        if not isinstance(reason, str) or not reason:
            raise ValueError(f"an empty field needs its reason: {field}")
    check_exhaustive("data table", data_table_fields(), set(spec["data_tables"]) | set(spec["data_tables_empty"]))
    both = set(spec["data_tables"]) & set(spec["data_tables_empty"])
    if both:
        raise ValueError(f"data table fields both bound and empty: {sorted(both)}")
    lines += ['};', '', f'const OraclesTables oracles_tables_{name} = {{']
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
