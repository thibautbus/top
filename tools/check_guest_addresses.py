#!/usr/bin/env python3
"""Refuse guest addresses written by hand in the engine.

Every WRAM, HRAM or ROM address of the game must come from the generated
tables (engine/game/guest/guest_tables_*.c, guest_struct_offsets.h) or from
engine/game/data's generated tables, because Ages and Seasons differ.  This check
scans the hand-written C of the engine for hexadecimal literals in the guest
address space and only accepts the hardware map constants listed below.

usage: check_guest_addresses.py [--root DIR]     exit status 1 on any violation
"""
from __future__ import annotations

import argparse
import re
from pathlib import Path

SCANNED_DIRS = ["engine", "launcher", "harness"]
GENERATED = {"engine/game/guest/guest_tables_ages.c", "engine/game/guest/guest_tables_seasons.c",
             "engine/game/guest/guest_struct_offsets.h", "engine/core/cgb_boot_rom.c",
             "engine/core/cgb_boot_rom_mgba.c",
             "engine/game/data/oracles_tables_ages.c", "engine/game/data/oracles_tables_seasons.c",
             "engine/game/profiles/moonrise-regalia/tables.c", "engine/game/profiles/moonrise-regalia/identity.h",
             "engine/game/profiles/temple-of-seasons/tables.c", "engine/game/profiles/temple-of-seasons/identity.h",
             "engine/game/profiles/gifts-of-kinomi/tables.c", "engine/game/profiles/gifts-of-kinomi/identity.h"}
# The Game Boy memory map and cartridge header, not game data.
HARDWARE = {
    0x0100, 0x0104, 0x0134, 0x0143, 0x0144, 0x0147, 0x0148, 0x0149, 0x014a, 0x0150,
    0x4000, 0x7fff, 0x8000, 0x1000, 0x2000, 0x0fff, 0x00ff,
    0xc000, 0xcfff, 0xd000, 0xdfff, 0xe000, 0xfe00, 0xfea0,
    0xff00, 0xff7f, 0xff80, 0xfffe, 0xffff,
    0xff40, 0xff41, 0xff42, 0xff43, 0xff44, 0xff45, 0xff46, 0xff47, 0xff48, 0xff49, 0xff4a, 0xff4b,
    0xff4f, 0xff51, 0xff52, 0xff53, 0xff54, 0xff55, 0xff68, 0xff69, 0xff6a, 0xff6b, 0xff70,
}
LITERAL = re.compile(r"\b0x([0-9a-fA-F]{4})\b")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    root = args.root.resolve()
    violations: list[str] = []
    for directory in SCANNED_DIRS:
        for path in sorted((root / directory).rglob("*.[ch]")):
            relative = path.relative_to(root).as_posix()
            if relative in GENERATED:
                continue
            for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
                code = line.split("//", 1)[0]
                for match in LITERAL.finditer(code):
                    value = int(match.group(1), 16)
                    if 0xc000 <= value <= 0xffff and value not in HARDWARE:
                        violations.append(f"{relative}:{number}: guest address 0x{value:04x} written by hand")
    for v in violations:
        print(v)
    print(f"guest addresses: {len(violations)} violation(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
