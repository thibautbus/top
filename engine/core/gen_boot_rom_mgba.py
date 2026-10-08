#!/usr/bin/env python3
"""Derive the boot ROM the mGBA core runs (cgb_boot_rom_mgba.c, cgb_boot_rom_mgba.h) from SameBoy's free CGB boot ROM
(cgb_boot_rom.c): the same code and the same animation, its logo reading MGBA instead of SAMEBOY.

usage: gen_boot_rom_mgba.py PATH/TO/pb12

PATH/TO/pb12 is SameBoy's logo compressor, built from BootROMs/pb12.c of the release pinned in config/sameboy.json
(cc -std=c99 pb12.c -o pb12); the script checks that it gives back the logo SameBoy's boot ROM carries.

The logo is 128x24 pixels in 48 tiles, compressed (PB12) and laid out by the boot ROM's tilemap loop, which shares
the three tiles common to SameBoy's E and B.  Here the letters M, B and A are SameBoy's own glyphs, a G is drawn from
its O, and MGBA is set in their spacing, centred; the tile sharing is turned off by two immediates of the tilemap loop
(the shared range emptied, the row step from 44 to 47), every tile then its own.  The compressed logo is shorter and
takes the original's place, the rest of that place zeroed; no other byte moves, and the boot hands over to the
cartridge at the same frame.
"""
from __future__ import annotations

import hashlib
import pathlib
import re
import subprocess
import sys
import zlib

HERE = pathlib.Path(__file__).resolve().parent
LOGO_W, LOGO_H = 128, 24
# The tilemap loop's two immediates (cgb_boot.asm, Load Tilemap): `sub $3` of the shared tiles' test, `sub 44` at the
# end of a row.
SHARED_TEST, ROW_STEP = 0x60, 0x6F


def sameboy_boot_rom() -> bytearray:
    text = (HERE / "cgb_boot_rom.c").read_text(encoding="utf-8")
    body = text[text.index("{", text.index("oracles_cgb_boot_rom[]")):]
    return bytearray(int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]{2})\b", body))


def find_logo(rom: bytes) -> int:
    """The logo's address: LoadTileset's `ld hl, SameBoyLogo`, followed by `ld de, $807F` and `ld c, $30`."""
    for i in range(len(rom) - 8):
        if rom[i] == 0x21 and rom[i + 3:i + 8] == bytes([0x11, 0x7F, 0x80, 0x0E, 0x30]):
            return rom[i + 1] | rom[i + 2] << 8
    raise SystemExit("no logo found: not SameBoy's CGB boot ROM")


def pb12_decode(rom: bytes, src: int) -> tuple[dict[int, int], int]:
    """The boot ROM's PB12 decoder, step by step: the VRAM bytes it writes from $8080, and its terminator's address."""
    hl, de, out, a = src, 0x807F, {}, 0

    def shift(b: int) -> tuple[int, int]:
        return (b << 1) & 0xFF, (b >> 7) & 1

    while True:
        b = rom[hl]
        if b == 1:
            return out, hl
        hl += 1
        b = (b << 1) | 1
        carry, b = (b >> 8) & 1, b & 0xFF
        while True:
            if carry:                       # .simple_repeat
                b, carry = shift(b)
                if not carry:               # far repeat: the byte before the last written
                    a = out.get(de - 1, 0)
            else:
                b, carry = shift(b)
                if carry:                   # .shifty_repeat
                    b, carry = shift(b)
                    if b == 0:
                        b = (rom[hl] << 1) | 1
                        hl += 1
                        carry, b = (b >> 8) & 1, b & 0xFF
                    c = a
                    a = a >> 1 if carry else (a << 1) & 0xFF
                    b, carry = shift(b)
                    a = a & c if carry else a | c
                else:
                    a = rom[hl]
                    hl += 1
            de += 1
            out[de] = a
            b, carry = shift(b)
            if b == 0:
                break


def tile_pixels(vram: dict[int, int], tile: int) -> list[list[int]]:
    base = 0x8000 + tile * 16
    return [[((vram.get(base + 2 * y, 0) >> (7 - x)) & 1) | (((vram.get(base + 2 * y + 1, 0) >> (7 - x)) & 1) << 1)
             for x in range(8)] for y in range(8)]


def shared_tilemap() -> dict[tuple[int, int], int]:
    """The original tilemap loop: {(row, column): tile} of the logo, the E's last column written again for the B."""
    grid, cell, a = {}, 0, 8
    for row in range(3):
        c, col = 16, 0
        while True:
            grid[(row, col)] = a
            col += 1
            if (a - 0x20) & 0xFF < 3:
                grid[(row, col)] = (a - 3) & 0xFF
                col += 1
                c -= 1
            a = (a + 3) & 0xFF
            c -= 1
            if c == 0:
                break
        a = (a - 44) & 0xFF
    return grid


def logo_image(rom: bytes) -> list[list[int]]:
    vram, _ = pb12_decode(rom, find_logo(rom))
    img = [[0] * LOGO_W for _ in range(LOGO_H)]
    for (row, col), tile in shared_tilemap().items():
        for y, line in enumerate(tile_pixels(vram, tile)):
            img[row * 8 + y][col * 8:col * 8 + 8] = line
    return img


def glyphs(img: list[list[int]]) -> dict[str, dict[tuple[int, int], int]]:
    """SAMEBOY's letters as {(y, x): value}: the full pixels in 8-connected parts, each soft pixel to the nearest."""
    seen, parts = set(), []
    for y in range(LOGO_H):
        for x in range(LOGO_W):
            if img[y][x] == 3 and (y, x) not in seen:
                stack, part = [(y, x)], []
                seen.add((y, x))
                while stack:
                    cy, cx = stack.pop()
                    part.append((cy, cx))
                    for ny in (cy - 1, cy, cy + 1):
                        for nx in (cx - 1, cx, cx + 1):
                            if 0 <= ny < LOGO_H and 0 <= nx < LOGO_W and img[ny][nx] == 3 and (ny, nx) not in seen:
                                seen.add((ny, nx))
                                stack.append((ny, nx))
                parts.append(part)
    parts.sort(key=lambda part: min(x for _, x in part))
    if len(parts) != 7:
        raise SystemExit(f"{len(parts)} letters in the logo, SAMEBOY's 7 expected")
    letters = [{p: 3 for p in part} for part in parts]
    for y in range(LOGO_H):
        for x in range(LOGO_W):
            if img[y][x] == 1:
                nearest = min(range(7), key=lambda i: min(abs(y - py) + abs(x - px) for py, px in parts[i]))
                letters[nearest][(y, x)] = 1
    return dict(zip("SAMEBOY", letters))


def letter_g(o: dict[tuple[int, int], int]) -> dict[tuple[int, int], int]:
    """A G from the O: its right stroke opened under the top arc (rows 7 to 10), a bar into the middle (rows 11, 12)."""
    x0 = min(x for _, x in o)
    g = {p: v for p, v in o.items() if not (7 <= p[0] <= 10 and p[1] - x0 >= 14)}
    for y in (11, 12):
        for x in range(x0 + 10, x0 + 18):
            g[(y, x)] = 3
        g[(y, x0 + 9)] = max(g.get((y, x0 + 9), 0), 1)
    return g


def compose(text: str, letters: dict[str, dict[tuple[int, int], int]]) -> list[list[int]]:
    """The letters one after the other, one column between their full pixels as in SameBoy's logo, centred."""
    placed, x = [], 0
    for ch in text:
        full = [px for (_, px), v in letters[ch].items() if v == 3]
        placed.append((letters[ch], x - min(full)))
        x += max(full) - min(full) + 2
    offset = (LOGO_W - (x - 2)) // 2
    img = [[0] * LOGO_W for _ in range(LOGO_H)]
    for letter, dx in placed:
        for (y, px), v in letter.items():
            img[y][px + dx + offset] = max(img[y][px + dx + offset], v)
    return img


def unshared_tiles(img: list[list[int]]) -> bytes:
    """The 48 tiles for the patched loop, tile 8 + 3 * column + row, two bit planes per line."""
    out = bytearray()
    for col in range(16):
        for row in range(3):
            for y in range(8):
                line = img[row * 8 + y][col * 8:col * 8 + 8]
                out += bytes([sum((v & 1) << (7 - x) for x, v in enumerate(line)),
                              sum(((v >> 1) & 1) << (7 - x) for x, v in enumerate(line))])
    return bytes(out)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    pb12 = sys.argv[1]
    rom = sameboy_boot_rom()
    src = find_logo(rom)
    vram, end = pb12_decode(rom, src)
    original = bytes(vram[a] for a in sorted(vram))
    if subprocess.run([pb12], input=original, capture_output=True, check=True).stdout != bytes(rom[src:end + 1]):
        raise SystemExit(f"{pb12} does not give back SameBoy's logo: not the compressor of the pinned release")
    letters = glyphs(logo_image(rom))
    letters["G"] = letter_g(letters["O"])
    img = compose("MGBA", letters)
    packed = subprocess.run([pb12], input=unshared_tiles(img), capture_output=True, check=True).stdout
    room = end - src + 1
    if len(packed) > room:
        raise SystemExit(f"the new logo takes {len(packed)} bytes, {room} available")
    if rom[SHARED_TEST - 1:SHARED_TEST + 1] != bytes([0xD6, 0x03]) or rom[ROW_STEP - 1:ROW_STEP + 1] != bytes([0xD6, 0x2C]):
        raise SystemExit("the tilemap loop is not where it was")
    new = bytearray(rom)
    new[src:src + room] = packed + bytes(room - len(packed))
    new[SHARED_TEST] = 0x00
    new[ROW_STEP] = 47
    vram, _ = pb12_decode(new, src)
    back = [[0] * LOGO_W for _ in range(LOGO_H)]
    for col in range(16):
        for row in range(3):
            for y, line in enumerate(tile_pixels(vram, 8 + 3 * col + row)):
                back[row * 8 + y][col * 8:col * 8 + 8] = line
    if back != img:
        raise SystemExit("the new logo does not decode to the image")

    sha, crc = hashlib.sha256(new).hexdigest(), zlib.crc32(new)
    lines = [
        "/* SameBoy 1.0.3 free CGB boot ROM, its logo reading MGBA: the boot ROM the mGBA core runs.",
        " * Derived from cgb_boot_rom.c by gen_boot_rom_mgba.py, which says how; do not edit.",
        " * Licence: Expat (MIT), see ../third_party/sameboy/LICENSE.",
        f" * SHA-256 of the {len(new)} bytes: {sha}",
        f" * CRC32: 0x{crc:08x}, which third_party/mgba/boot-rom.patch adds to the boot ROMs mGBA accepts. */",
        '#include "cgb_boot_rom_mgba.h"',
        "",
        f"const size_t oracles_cgb_boot_rom_mgba_size = {len(new)};",
        "const unsigned char oracles_cgb_boot_rom_mgba[] = {",
    ]
    lines += ["    " + ", ".join(f"0x{x:02x}" for x in new[i:i + 16]) + "," for i in range(0, len(new), 16)]
    lines += ["};", ""]
    (HERE / "cgb_boot_rom_mgba.c").write_text("\n".join(lines), encoding="utf-8")
    (HERE / "cgb_boot_rom_mgba.h").write_text(
        "#ifndef ORACLES_CGB_BOOT_ROM_MGBA_H\n#define ORACLES_CGB_BOOT_ROM_MGBA_H\n\n#include <stddef.h>\n\n"
        "/* SameBoy's free CGB boot ROM, its logo reading MGBA; cgb_boot_rom_mgba.c carries its SHA-256 and CRC32. */\n"
        "extern const unsigned char oracles_cgb_boot_rom_mgba[];\n"
        "extern const size_t oracles_cgb_boot_rom_mgba_size;\n\n#endif\n", encoding="utf-8")
    print(f"cgb_boot_rom_mgba.c: SHA-256 {sha}, CRC32 0x{crc:08x}, logo {len(packed)} of {room} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
