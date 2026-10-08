#!/usr/bin/env python3
"""Rewrite a route recorded with SameBoy's joypad bouncing as the keys the game read, without the bouncing.

SameBoy's joypad bouncing (cut since; a route recorded after says `core joypad-bouncing-off`) makes the keys the game
reads differ, now and then, from the keys pressed: a press is missed, or read a frame late.  mGBA has no bouncing, and a
route recorded with it parts there from the game it recorded.  The game reads the keys in one place, its input poll
(pollInput, the one writer of wKeysPressed), once a frame at most: this tool replays the route on SameBoy, bouncing
and all, takes the keys the game read in each frame it polled (the harness's --keys-read), and writes them as the
route's keys in those frames, the keys recorded staying in the frames it did not poll (on another core, whose frames
without the game's logic fall elsewhere, a poll there reads what the player held).  The route says
`core joypad-bouncing-off`, and `options continuous-transitions` when it was replayed with that option.  Its other lines
(the item hotkeys' `equip`) are kept, and so is its last frame, the replay's length.  A `use` line, which is not
replayed but tells the harness where a simulated press starts, moves to the start of that press in the new keys, a
frame or two later where the bouncing delayed it, and goes where two presses became one.  The recording stays in it
as `#recorded` comments, its header and its input lines, which readers skip and from which the tool starts again on a
route it rewrote.

It then replays the new route on SameBoy, without the bouncing, and compares the live-state fingerprints with the
recording's, once for every set of options given (--options, one per set; the suite's rows of the route may run with
several), and once more with the Enhanced view for the first: the two must be the same game, frame for frame, or
nothing is written.  A replay that loads a state back (--enhanced-reload-at, which only the Enhanced check honours)
plays another game after the load than the recording, with or without the bouncing: its keys there are not those
this tool took, and its run hash changes.

usage: debounce_route.py ROUTE [--options="OPTS"]... [--harness PATH] [--rom-dir DIR] [--out FILE] [--work DIR]
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import check_routes  # noqa: E402  (the ROMs by the route's SHA-1, a fan game's through its BPS patch)

REPO = Path(__file__).resolve().parent.parent
BOUNCING_OFF = "joypad-bouncing-off"
RECORDED_HEADER = "#recorded-header "
RECORDED = "#recorded "
NOTE = ("# Keys as the game read them on SameBoy, its joypad bouncing included, in the frames it read them, and as "
        "recorded in the others (tools/debounce_route.py): the same game without the bouncing.")
RECORDED_NOTE = "# The route as recorded, with SameBoy's joypad bouncing: its header and its input lines."
USE_BITS = {"a": 0x10, "b": 0x20}   # the buttons in a route's order
USE_REACH = 3                       # frames a press may start after its `use` line


def recorded(route: Path) -> str:
    """The route as recorded: itself, or, for a route this tool rewrote, the recording its comments keep."""
    lines = route.read_text(encoding="ascii").split("\n")
    header = [line[len(RECORDED_HEADER):] for line in lines if line.startswith(RECORDED_HEADER)]
    inputs = [line[len(RECORDED):] for line in lines if line.startswith(RECORDED)]
    if not header:
        core = [line[5:].split(",") for line in lines if line.startswith("core ")]
        if any("mgba" in c for c in core):
            sys.exit(f"debounce_route: {route} was recorded on mGBA, which has no joypad bouncing")
        if any(BOUNCING_OFF in c for c in core):
            sys.exit(f"debounce_route: {route} was recorded without the bouncing")
        return "\n".join(lines)
    return "\n".join(header + ["inputs"] + inputs) + "\n"


def replay(harness: Path, rom: list, route: Path, options: list, out: Path, keys_read: Path | None = None,
           enhanced: Path | None = None) -> None:
    cmd = [str(harness)] + rom + ["--route", str(route), "--core", "sameboy", "--out", str(out)] + options
    if keys_read:
        cmd += ["--keys-read", str(keys_read)]
    if enhanced:
        enhanced.mkdir(parents=True, exist_ok=True)
        cmd += ["--enhanced-check", str(enhanced), "--enhanced-camera", "2"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 or not out.exists():
        sys.exit(f"debounce_route: the harness failed on {route} ({proc.returncode}): {proc.stderr.strip()[-400:]}")


def rewrite(original: str, keys_read: Path, continuous: bool) -> tuple[str, int, int]:
    """The new route, and how many `use` lines moved and went."""
    header, inputs, in_inputs = [], [], False
    for line in original.split("\n"):
        if not in_inputs:
            if line == "inputs":
                in_inputs = True
            elif line:
                header.append(line)
            continue
        if line.strip() and not line.startswith("#"):
            inputs.append(line.split(" ", 2))
    if not inputs:
        sys.exit("debounce_route: the route has no input")
    last = max(int(cols[0]) for cols in inputs)
    others = [(int(cols[0]), " ".join(cols)) for cols in inputs if cols[1] != "keys"]
    recorded_lines = [" ".join(cols) for cols in inputs]
    changes = {int(cols[0]): cols[2] for cols in inputs if cols[1] == "keys"}
    read = {}
    for line in keys_read.read_text(encoding="ascii").split("\n"):
        if line.strip():
            frame, mask, polled = line.split("\t")
            if polled == "-":
                sys.exit("debounce_route: the harness ran without hooks: it cannot tell the frames the game polled")
            if polled == "1":
                read[int(frame)] = mask
    keys, previous, held, masks = [], None, "00", {}
    for frame in range(0, last + 1):
        held = changes.get(frame, held)
        mask = read.get(frame, held)
        masks[frame] = mask
        if mask != previous or frame == last:
            keys.append((frame, f"{frame} keys {mask}"))
            previous = mask
    moved = gone = 0
    placed = []
    for frame, text in others:
        cols = text.split(" ")
        if cols[1] == "use" and cols[2] in USE_BITS:
            bit = USE_BITS[cols[2]]
            starts = [f for f in range(frame, min(frame + USE_REACH, last) + 1)
                      if int(masks[f], 16) & bit and not (f > 0 and int(masks[f - 1], 16) & bit)]
            if not starts:
                gone += 1
                continue
            if starts[0] != frame:
                moved += 1
                text = " ".join([str(starts[0])] + cols[1:])
                frame = starts[0]
        placed.append((frame, text))
    others = placed
    # At a same frame the lines apply in the file's order: the keys first, as the launcher records them.
    merged = sorted(keys + others, key=lambda item: (item[0], 0 if " keys " in item[1] else 1))
    head = [("oracles-route 2" if line.startswith("oracles-route 1") else line) for line in header]
    if continuous and not any(line.startswith("options ") for line in head):
        head.append("options continuous-transitions")
    head += [f"core {BOUNCING_OFF}", NOTE]
    kept = [RECORDED_NOTE] + [f"{RECORDED_HEADER}{line}" for line in header] + [f"{RECORDED}{line}" for line in recorded_lines]
    return "\n".join(head + ["inputs"] + [text for _, text in merged] + kept) + "\n", moved, gone


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("route", type=Path)
    ap.add_argument("--options", action="append", default=None, help="harness options of one replay to compare (repeat for each set; none by default)")
    ap.add_argument("--harness", type=Path, default=REPO / "build" / "oracles-harness")
    ap.add_argument("--rom-dir", type=Path, default=None, help="defaults to $ORACLES_ROM_DIR")
    ap.add_argument("--out", type=Path, default=None, help="defaults to the route itself, rewritten in place")
    ap.add_argument("--work", type=Path, default=None)
    args = ap.parse_args()
    rom_dir = args.rom_dir or (Path(os.environ["ORACLES_ROM_DIR"]) if os.environ.get("ORACLES_ROM_DIR") else None)
    if rom_dir is None:
        sys.exit("debounce_route: no ROM directory (set ORACLES_ROM_DIR or pass --rom-dir)")
    route = args.route.resolve()
    original_text = recorded(route)
    header = check_routes.route_header(route)
    rom = check_routes.find_roms(rom_dir).get(header.get("rom_sha1", ""))
    if rom is None:
        sys.exit(f"debounce_route: no ROM with SHA-1 {header.get('rom_sha1', '?')} in {rom_dir}")
    option_sets = [o.split() for o in (args.options or [""])]
    continuous = any("--continuous-transitions" in o for o in option_sets)
    work = Path(args.work or tempfile.mkdtemp(prefix="debounce-"))
    work.mkdir(parents=True, exist_ok=True)
    # The recording and the new route sit beside the route during the check: they replay from the same starting save.
    original = route.with_name(route.stem + ".recorded.route")
    candidate = route.with_name(route.stem + ".debounced.route")
    sram = route.with_name(route.name + ".sram")
    copies = [original, candidate] + [p.with_name(p.name + ".sram") for p in (original, candidate)]
    try:
        original.write_text(original_text, encoding="ascii", newline="\n")
        for p in (original, candidate):
            if sram.exists():
                p.with_name(p.name + ".sram").write_bytes(sram.read_bytes())
        replay(args.harness, rom, original, check_routes.harness_options(option_sets[0]), work / "original-0.tsv", work / "keys-read.tsv")
        text, moved, gone = rewrite(original_text, work / "keys-read.tsv", continuous)
        candidate.write_text(text, encoding="ascii", newline="\n")
        checks = [(str(n), check_routes.harness_options(o), False) for n, o in enumerate(option_sets)]
        checks.append(("view", check_routes.harness_options(option_sets[0]), True))
        for tag, options, view in checks:
            if tag != "0":
                replay(args.harness, rom, original, options, work / f"original-{tag}.tsv", enhanced=work / f"view-o-{tag}" if view else None)
            replay(args.harness, rom, candidate, options, work / f"debounced-{tag}.tsv", enhanced=work / f"view-d-{tag}" if view else None)
            same = subprocess.run([str(args.harness), "--compare", str(work / f"original-{tag}.tsv"), str(work / f"debounced-{tag}.tsv")],
                                  capture_output=True, text=True)
            verdict = (same.stdout.strip().splitlines() or ["no output"])[-1]
            label = (" ".join(options) or "no option") + (", the Enhanced view" if view else "")
            if same.returncode != 0 or "identical" not in verdict:
                print(f"debounce_route: {route.name} ({label}): not the same game without the bouncing: {verdict}")
                return 1
            print(f"debounce_route: {route.name} ({label}): the same game without the bouncing ({verdict})")
        if moved or gone:
            print(f"debounce_route: {route.name}: {moved} `use` line(s) moved to the start of their press, {gone} gone with their press")
        out = args.out or route
        out.write_text(text, encoding="ascii", newline="\n")
        print(f"debounce_route: written to {out}")
        return 0
    finally:
        for p in copies:
            p.unlink(missing_ok=True)


if __name__ == "__main__":
    sys.exit(main())
