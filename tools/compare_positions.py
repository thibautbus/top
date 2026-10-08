#!/usr/bin/env python3
"""Where two replays of a route, on two cores, part: the rooms they go through and the frame each enters them.

The harness writes, with --positions FILE, the group, the room and Link's position after every frame.  Two cores never
give the same memory (the fingerprints differ), but they play the same game while the rooms follow one another in the
same order; a core's timing differs by a frame or a few, so the comparison is of the rooms' sequence, not of the
frames.  The rooms before the game first sets one (the boot, room 0 of group 0) are left out.

Prints the rooms both replays enter, with the frame each enters them and the offset, then the first room where they
part (exit status 1), or that they go through the same rooms to the end (0).

usage: compare_positions.py A.tsv B.tsv [--names NAME_A NAME_B]
"""
from __future__ import annotations

import argparse
import sys


def rooms(path: str) -> list[tuple[int, int, int]]:
    """(frame, group, room) of each room entered, the boot's left out."""
    entered = []
    last = None
    with open(path, encoding="utf-8") as lines:
        for line in lines:
            frame, group, room, _y, _x = (int(v) for v in line.split())
            if (group, room) != last:
                last = (group, room)
                if (group, room) != (0, 0) and frame > 0:
                    entered.append((frame, group, room))
    return entered


def rooms_in_common(a: str, b: str) -> tuple[int, bool]:
    """How many rooms the two replays enter in the same order from the start, and whether they part."""
    ra, rb = rooms(a), rooms(b)
    for i, (x, y) in enumerate(zip(ra, rb)):
        if x[1:] != y[1:]:
            return i, True
    return min(len(ra), len(rb)), len(ra) != len(rb)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("a")
    ap.add_argument("b")
    ap.add_argument("--names", nargs=2, default=["A", "B"])
    args = ap.parse_args()
    ra, rb = rooms(args.a), rooms(args.b)
    name_a, name_b = args.names
    print(f"{'room':>10} {name_a:>10} {name_b:>10} {'offset':>7}")
    for i, (x, y) in enumerate(zip(ra, rb)):
        if x[1:] != y[1:]:
            print(f"part at the {i + 1}th room: {name_a} enters {x[1]:02x}:{x[2]:02x} at frame {x[0]}, "
                  f"{name_b} {y[1]:02x}:{y[2]:02x} at frame {y[0]}")
            return 1
        print(f"{x[1]:02x}:{x[2]:02x}{'':>5} {x[0]:>10} {y[0]:>10} {y[0] - x[0]:>+7}")
    if len(ra) != len(rb):
        longer, shorter = (name_a, name_b) if len(ra) > len(rb) else (name_b, name_a)
        print(f"part after {min(len(ra), len(rb))} rooms: {longer} enters more rooms, {shorter} no other")
        return 1
    print(f"the same {len(ra)} rooms to the end")
    return 0


if __name__ == "__main__":
    sys.exit(main())
