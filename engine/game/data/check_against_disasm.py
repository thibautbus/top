#!/usr/bin/env python3
"""Compare a datacheck dump with the decompressed assets of oracles-disasm.

- rooms: rooms/<game>/small/roomGGRR.bin (80 bytes) and large/roomGGRR.bin (176 bytes);
- tilesets: tileset_layouts/<game>/tilesetMappingsXX.bin (2048) and tilesetCollisionsXX.bin (256);
- graphics: every header entry re-decompressed with the disasm's own Python
  decompressor (tools/common.py, decompressGfxData) from the same ROM bytes.

usage: check_against_disasm.py --dump DIR --disasm PATH --game ages|seasons --rom ROM
Exit status is non-zero on any mismatch.
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path


def compare_dir(dump: Path, reference: Path, pattern: str, label: str) -> tuple[int, int, list[str]]:
    ok = bad = 0
    details: list[str] = []
    for ref in sorted(reference.glob(pattern)):
        candidate = dump / ref.name
        if not candidate.exists():
            bad += 1
            details.append(f"{label} {ref.name}: missing in dump")
            continue
        if candidate.read_bytes() == ref.read_bytes():
            ok += 1
        else:
            bad += 1
            a, b = candidate.read_bytes(), ref.read_bytes()
            first = next((i for i in range(min(len(a), len(b))) if a[i] != b[i]), min(len(a), len(b)))
            details.append(f"{label} {ref.name}: differs at byte {first} (dump {len(a)} bytes, reference {len(b)})")
    return ok, bad, details


def compare_gfx(dump: Path, disasm: Path, rom: bytes) -> tuple[int, int, list[str]]:
    sys.path.insert(0, str(disasm / "tools"))
    import common  # noqa: E402  (the disasm's own helper module)

    ok = bad = 0
    details: list[str] = []
    for line in (dump / "gfx" / "manifest.tsv").read_text().splitlines():
        header, entry, bank, addr, mode, size_byte, written = line.split("\t")
        bank_i, addr_i, mode_i, size_i = int(bank, 16), int(addr, 16), int(mode), int(size_byte, 16)
        physical = bank_i * 0x4000 + (addr_i - 0x4000) if bank_i else addr_i
        _, expected = common.decompressGfxData(rom, physical, size_i, mode_i)
        actual = (dump / "gfx" / f"h{header}-e{int(entry):02d}.bin").read_bytes()
        if bytes(expected) == actual:
            ok += 1
        else:
            bad += 1
            first = next((i for i in range(min(len(actual), len(expected))) if actual[i] != expected[i]),
                         min(len(actual), len(expected)))
            details.append(f"gfx h{header} e{entry} mode {mode_i}: differs at byte {first} "
                           f"(dump {len(actual)}, reference {len(expected)})")
    return ok, bad, details


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", type=Path, required=True)
    ap.add_argument("--disasm", type=Path, required=True)
    ap.add_argument("--game", choices=("ages", "seasons"), required=True)
    ap.add_argument("--rom", type=Path, required=True)
    args = ap.parse_args()

    results = []
    results.append(("small rooms", *compare_dir(args.dump / "rooms", args.disasm / "rooms" / args.game / "small", "room*.bin", "room")))
    results.append(("large rooms", *compare_dir(args.dump / "rooms", args.disasm / "rooms" / args.game / "large", "room*.bin", "room")))
    results.append(("tileset mappings", *compare_dir(args.dump / "tilesets", args.disasm / "tileset_layouts" / args.game, "tilesetMappings*.bin", "tileset")))
    results.append(("tileset collisions", *compare_dir(args.dump / "tilesets", args.disasm / "tileset_layouts" / args.game, "tilesetCollisions*.bin", "tileset")))
    results.append(("gfx entries", *compare_gfx(args.dump, args.disasm, args.rom.read_bytes())))

    failed = 0
    for name, ok, bad, details in results:
        print(f"{name}: {ok} ok, {bad} mismatching")
        for d in details[:10]:
            print(f"  {d}")
        if bad > 10:
            print(f"  ... {bad - 10} more")
        failed += bad
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
