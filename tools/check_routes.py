#!/usr/bin/env python3
"""Replay the routes through the harness and hold their figures.

Every checks.tsv under routes/, found recursively, holds the rows of the
routes of its own directory; each row names a route, a mode, what must hold and the core it holds on:

    route	mode	options	expect	run_hash	core

route: a file of the manifest's directory (no `/`, no `..`); the identity of
a route is its path relative to routes/ (ages/intro.route), from which its
labels and working directories derive (ages-intro-faithful), and a route
named by two manifests is an error.

mode: faithful (native renderer against the core, colour correction off,
plus the ghost instance against every scrolling transition, its reads traced
against the cache key), faithful-cc (the renderer with colour correction on),
enhanced (the Enhanced surface of every frame, camera profile 2, the ghost
synchronous: reproducible, hashed), enhanced-threaded (the same with the ghost
in its thread, as played: the figures that do not depend on timing),
enhanced-zoom and enhanced-zoom-threaded (the same two with the drawn-back
view's 480x270 surface, --zoom-out; the synchronous one never runs
the ghost into a wrong room either).
options: extra harness flags, `-` for none (a route recorded with
--continuous-transitions carries the option in its header, and the harness
turns it on by itself); a --mods directory is relative to the repository; a
row with --language and no --eu-rom gets the ROM directory as its --eu-rom,
where the harness finds the European ROM by its SHA-1.  expect: `;`-separated `key=value` or `key<=value`
over the harness summary (--summary), on top of the rules every row must
satisfy (below); a rule prefixed with a core (`mgba:key<=value`) holds on that
core only, a row of both holding each core's measure.  run_hash: the Enhanced run hash the row last produced, `-`
when the mode has none; each core has its own (the game and its timing differ
from one to the other), so that a row of both cores in a hashed mode holds
`sameboy:HASH,mgba:HASH`; a change is a failure until --update rewrites it, so
that a change of behaviour is named in the commit that carries it.
core: the core the row replays on, `sameboy` or `mgba`, or `-` for both.  The
suite runs on one core (--core, SameBoy by default) and takes the rows of that
core and those of both; the harness gets the core.  A route that parts on mGBA
from the game it recorded on SameBoy (joypad bouncing, which mGBA does not
have, or a frame the game's logic ends on the other side of a vblank) keeps
its rows for SameBoy and gets its own for mGBA, with the figures of mGBA's
replay of it, the parting named in a comment above them.  Off SameBoy, a
faithful row is replayed on SameBoy too and the rooms both replays go through
are compared (compare_positions.py, cores.rooms_same and cores.parted): a row
of both cores must not part, a core's own row holds a floor on the rooms in
common.
A ceiling on the black a synchronous row shows (enhanced.black_pixels_mean,
counted from the band's first full frame) is the value last measured: the
replay is deterministic, so it is a ratchet, lowered by the commit whose change
lowers the black and never raised without naming why.

Besides the rows, the live-state fingerprints (--out) of the faithful row of
a route and of each of its enhanced rows, the ghost synchronous and threaded,
at both sizes, are compared: the presentation writes nothing into the live instance
(ARCHITECTURE.md), on every route and in the mode that is played; so are those of
its faithful rows with a check that only reads (READING_CHECKS).

The ROMs are found by the SHA-1 of the route's header in $ORACLES_ROM_DIR,
a fan game's as the image a BPS patch there makes of a ROM there, replayed
from the two as the launcher plays it (--patch); without the directory the
script exits 77 (ctest: skipped).  The routes' starting SRAM
sits beside them; no ROM data is read from the repository.

usage: check_routes.py [--harness PATH] [--core sameboy|mgba] [--routes DIR]
                       [--manifest FILE] [--rom-dir DIR] [--work DIR] [--jobs N]
                       [--only TEXT] [--update]
"""
import argparse
import concurrent.futures
import hashlib
import os
import shlex
import subprocess
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from bps import patch_report  # noqa: E402  (the offline tools' BPS applier, CRCs checked)
from compare_positions import Unreadable, rooms_in_common  # noqa: E402

HERE = Path(__file__).resolve().parent
REPO = HERE.parent
DEFAULT_JOBS = 8   # eight replays keep a desktop responsive; a machine that fails under a heavier parallel load is not a bug of the routes

# A neighbour's objects animated in place: each render of a pose draws the pose its step holds, its lines drawn alone
# are those a whole render draws, every object of several poses has its animation's data, and a pose shown waits at
# most the frames the render budget lets a render wait (view_render.c), the frame it is drawn counted.  An object its
# capture drew otherwise than its record says (enhanced.poses_frozen_unplaced) stays frozen: counted, not refused.
POSES = ["enhanced.poses_drawn_wrong=0", "enhanced.pose_lines_wrong=0", "enhanced.poses_frozen_unreadable=0", "enhanced.pose_lag_max<=5"]
# The game marks visited the room it plays by the first frame of play after a load or a scroll (setVisitedRoomFlag):
# the rooms never visited the view keeps dark (--unvisited-rooms hidden) are the game's own map's, none of them visited
# and none drawn in clear unvisited, the room in play aside; a room kept dark lit only along a side where something in
# clear stands beyond it.
UNVISITED = ["enhanced.unvisited_reference=0", "enhanced.dark_visited=0", "enhanced.shown_unvisited=0", "enhanced.dark_edge_stray=0"]
ALWAYS = {
    # The floors say the check ran at all: a check that stopped measuring counts
    # zero difference and would satisfy every rule above it.  What a
    # route exercises is its own (the `expect` column): a route that never
    # scrolls into another room has no transition for the ghost to check.
    "faithful": ["result=ok", "render.verdict=pass", "ghost.different=0", "ghost.wrong_room=0", "ghost.load_failed=0", "ghost.no_snapshot=0",
                 "ghost.reads_outside_key=0", "ghost.untraced_runs=0", "ghost.reads_dropped=0", "ghost.returns_purged=0", "ghost.returns_overflow=0"],
    "faithful-cc": ["result=ok", "render.verdict=pass"],
    "enhanced": ["result=ok", "enhanced.camera_jumps=0", "enhanced.window_misplaced=0", "enhanced.terrains_different=0",
                 "enhanced.live_diff_max=0", "enhanced.plain_shown_frames=0", "object_sprites.objects_wrong=0", "object_sprites.frames_wrong=0", "object_sprites.read_failures=0",
                 "sprites.shadows_unowned=0", "sprites.shadows_left=0", "sprites.shadows_grounded=0", "enhanced.text_box_frames_in_place=0",
                 "enhanced.curtain_window_frames_wrong=0", "enhanced.terrains_old=0", "enhanced.text_box_frames_at_origin=0",
                 "enhanced.blurb_frames_in_window=0",   # the area blurb never left in the game's window, its last column included
                 "object_sprites.frames_checked>=1", "animation.frames_checked>=1", "enhanced.transitions_other_room=0",
                 # a scroll's animation, run by the view alone, lands on the game's step, image for image, at the frame its plan
                 # spread the loops over (a jump at the landing is a landing off that frame)
                 "enhanced.scroll_animation_landed_off=0", "enhanced.scroll_animation_gaps=0", "enhanced.scroll_animation_gap_max=0", "enhanced.scroll_animation_tiles_off=0"] + POSES + UNVISITED,
    "enhanced-threaded": ["result=ok", "enhanced.camera_jumps=0", "enhanced.window_misplaced=0", "enhanced.terrains_different=0",
                          "enhanced.live_diff_max=0", "enhanced.plain_shown_frames=0", "enhanced.text_box_frames_at_origin=0",
                          "enhanced.blurb_frames_in_window=0",
                          "enhanced.transitions_other_room=0",   # never a room drawn that way other than the one entered
                          "enhanced.scroll_animation_landed_off=0", "enhanced.scroll_animation_gaps=0", "enhanced.scroll_animation_gap_max=0", "enhanced.scroll_animation_tiles_off=0"] + POSES + UNVISITED,
}
# The band must fill at least once: the black is counted from its first full frame, and a route whose band never fills
# would count none and pass any ceiling.
ALWAYS["enhanced-zoom"] = ALWAYS["enhanced"] + ["enhanced.ghost_wrong_room=0", "enhanced.cold_fill_frames>=0"]
ALWAYS["enhanced-zoom-threaded"] = ALWAYS["enhanced-threaded"]   # a run the thread's timing sends astray is dropped, as in enhanced-threaded: nothing wrong is shown
# A row at another of the view's sizes (`--view`) holds what a zoom row holds: the synchronous ghost in the right room, and
# the band filled at least once; a row that gives one of these keys its own ceiling (`key<=N`, or `core:key<=N` for one
# core) holds that figure instead on the cores it names, the reason in a comment above it (a route played whose ghost runs
# astray where nothing is shown wrong).
SIZED = {"enhanced": ["enhanced.ghost_wrong_room=0", "enhanced.cold_fill_frames>=0"]}
# On every row whose transitions are continuous (their counters in the summary), in every mode: the collisions of the
# room a scroll enters, predicted while the game loads them, are byte for byte what it then loads, every prediction
# compared (transitions.predicted, predicted_matched) but one whose load the replay ends in (predicted_pending).
PREDICTION = ["transitions.predicted_mismatched=0"]


def prediction_rules(values: dict) -> list:
    if "transitions.predicted" not in values:
        return []
    pending = int(values.get("transitions.predicted_pending", "0"))
    return PREDICTION + ["transitions.predicted_pending<=1", f"transitions.predicted_matched={int(values['transitions.predicted']) - pending}"]


# On every row whose transitions are continuous, but for the cap of the transitions alone (the harness measures it
# otherwise): no scroll, the policy's or one it leaves to the game, leaves Link on foot on land in a wall of the room entered, where the game's
# walk never stands him, his eight wall points as calculateAdjacentWallsBitset tests them, for the frames of normal play
# after it (transitions.wall_arrivals); a row that gives the key its own rule (`transitions.wall_arrivals<=1`, or
# `mgba:transitions.wall_arrivals<=1` for one core) holds that instead on the cores it names, the reason in a comment above it.
def wall_rules(values: dict, expect: list, core: str) -> list:
    own = [rule.split(":", 1)[1] for rule in expect if rule.split(":", 1)[0] == core] + [rule for rule in expect if rule.split(":", 1)[0] not in CORES]
    if "transitions.wall_arrivals" not in values or any(rule.startswith("transitions.wall_arrivals") for rule in own):
        return []
    return ["transitions.wall_arrivals=0"]


# On every row with a text size (its counters in the summary): the box drawn again at the other sizes and back, as the
# pause draws it, gives back the frame as composed.
def redraw_rules(values: dict) -> list:
    return ["text_box.redraw_wrong=0"] if "text_box.redraw_frames" in values else []


MODE_FLAGS = {
    "faithful": lambda w: ["--render-check", str(w / "render"), "--colour-correction", "off", "--ghost-check", str(w / "ghost"), "--ghost-trace",
                           "--out", str(w / "fingerprints.tsv")],
    "faithful-cc": lambda w: ["--render-check", str(w / "render"), "--colour-correction", "on"],
    "enhanced": lambda w: ["--enhanced-check", str(w / "enhanced"), "--enhanced-camera", "2", "--out", str(w / "fingerprints.tsv")],
    "enhanced-threaded": lambda w: ["--enhanced-check", str(w / "enhanced"), "--enhanced-camera", "2", "--enhanced-threaded", "--out", str(w / "fingerprints.tsv")],
}
MODE_FLAGS["enhanced-zoom"] = lambda w: MODE_FLAGS["enhanced"](w) + ["--zoom-out"]
MODE_FLAGS["enhanced-zoom-threaded"] = lambda w: MODE_FLAGS["enhanced-threaded"](w) + ["--zoom-out"]
SESSION_CORE = "joypad-bouncing-off"   # the header's `core` line of a route recorded since the core's joypad bouncing was cut
HASHED_MODES = ("enhanced", "enhanced-zoom")   # the threaded ghost is not reproducible frame for frame
CORES = ("sameboy", "mgba")


def core_hashes(run_hash: str, row_core: str) -> dict:
    """The run hash of each core a row holds: `sameboy:HASH,mgba:HASH`, or one hash, that of the row's core (SameBoy's
    for a row of both written before the cores had their own)."""
    if run_hash == "-":
        return {}
    if ":" not in run_hash:
        return {row_core if row_core != "-" else "sameboy": run_hash}
    return dict(part.split(":", 1) for part in run_hash.split(","))


def with_core_hash(run_hash: str, row_core: str, core: str, new: str) -> str:
    """The run_hash column with the hash of `core` replaced."""
    if row_core != "-":
        return new
    hashes = core_hashes(run_hash, row_core)
    hashes[core] = new
    return ",".join(f"{c}:{hashes[c]}" for c in CORES if hashes.get(c, "-") != "-") or "-"


def route_header(path: Path) -> dict:
    header = {}
    with open(path, "r", encoding="ascii", errors="replace") as f:
        for line in f:
            line = line.strip()
            if line == "inputs":
                break
            parts = line.split(" ", 1)
            if len(parts) == 2:
                header[parts[0]] = parts[1]
    return header


def european_rom(game: str) -> str:
    """The SHA-1 of the European ROM a game's languages come from: the one its table of exceptions pins
    (config/language_exceptions_<game>.tsv, its `rom` row), which the extraction requires of the ROM it is given."""
    with open(REPO / "config" / f"language_exceptions_{game}.tsv", encoding="utf-8") as f:
        for line in f:
            if line.startswith("rom\t"):
                return line.split("\t")[1].strip()
    sys.exit(f"config/language_exceptions_{game}.tsv pins no European ROM")


# The European ROMs the rows in a language read their translations from, by the routes' folder: a fan game's routes
# read the translations of its Oracle's European ROM.
EU_ROMS = {"ages": european_rom("ages"), "seasons": european_rom("seasons")}
EU_ROMS.update({"moonrise-regalia": EU_ROMS["ages"], "gifts-of-kinomi": EU_ROMS["ages"], "temple-of-seasons": EU_ROMS["seasons"]})


def find_roms(rom_dir: Path) -> dict:
    """SHA-1 -> the harness's ROM arguments, for every ROM in the directory (not recursive) and every image
    a BPS patch there makes of one of them."""
    roms, crcs = {}, {}
    for p in sorted(rom_dir.iterdir()):
        if p.suffix.lower() not in (".gbc", ".gb") or not p.is_file():
            continue
        data = p.read_bytes()
        roms[hashlib.sha1(data).hexdigest()] = ["--rom", str(p)]
        crcs.setdefault(zlib.crc32(data), p)
    for p in sorted(rom_dir.iterdir()):
        if p.suffix.lower() != ".bps" or not p.is_file():
            continue
        patch = p.read_bytes()
        source_crc = int.from_bytes(patch[-12:-8], "little") if len(patch) > 16 else None
        base = crcs.get(source_crc) if source_crc is not None else None
        if base is None:
            # Said, not silent: the routes of that fan game would otherwise fail as "no ROM with SHA-1 ...".
            print(f"check_routes: {p.name}: no ROM in the directory is its base (source CRC32 "
                  f"{'unreadable' if source_crc is None else f'{source_crc:08x}'}): its image is not offered")
            continue
        try:
            image, report = patch_report(base.read_bytes(), None, patch)
        except (ValueError, IndexError) as error:   # a patch cut short or damaged reads past its end
            print(f"check_routes: {p.name}: does not apply to {base.name} ({error or 'cut short'}): its image is not offered")
            continue
        roms.setdefault(report["patched_sha1"], ["--rom", str(base), "--patch", str(p)])
    return roms


def read_manifest(path: Path, routes: Path) -> list:
    rows = []
    try:
        directory = path.parent.resolve().relative_to(routes.resolve())
    except ValueError:
        sys.exit(f"{path}: a manifest must lie under the routes directory {routes}")
    with open(path, encoding="utf-8") as f:
        for n, line in enumerate(f, 1):
            if not line.strip() or line.startswith("#"):
                continue
            cols = line.rstrip("\n").split("\t")
            if len(cols) != 6:
                sys.exit(f"{path}:{n}: six tab-separated columns expected, {len(cols)} found")
            route, mode, options, expect, run_hash, core = cols
            if mode not in MODE_FLAGS:
                sys.exit(f"{path}:{n}: unknown mode {mode}")
            if core != "-" and core not in CORES:
                sys.exit(f"{path}:{n}: unknown core {core}")
            if "/" in route or "\\" in route or route in (".", "..") or not route:
                sys.exit(f"{path}:{n}: {route} is not a route of the manifest's own directory")
            route_id = (directory / route).as_posix()
            rows.append({"manifest": path, "line": n, "route": route_id, "label": route_id.removesuffix(".route").replace("/", "-"),
                         "mode": mode, "options": [] if options == "-" else options.split(),
                         "expect": [] if expect == "-" else expect.split(";"), "run_hash": run_hash, "core": core})
    return rows


def read_manifests(manifests: list, routes: Path) -> list:
    """The rows of every manifest; a route named by two manifests is an error."""
    rows, owner = [], {}
    for manifest in manifests:
        for row in read_manifest(manifest, routes):
            first = owner.setdefault(row["route"], manifest)
            if first != manifest:
                sys.exit(f"{manifest}:{row['line']}: {row['route']} is already named by {first}")
            rows.append(row)
    return rows


def harness_options(options: list, rom_dir: Path) -> list:
    """The row's options as the harness takes them: a --mods directory resolved against the repository, the harness
    running in the build directory, and a language's European ROM looked for in the ROM directory."""
    out = list(options)
    for i in range(len(out) - 1):
        if out[i] == "--mods" and not Path(out[i + 1]).is_absolute():
            out[i + 1] = str(REPO / out[i + 1])
    if "--language" in out and "--eu-rom" not in out:
        out += ["--eu-rom", str(rom_dir)]
    return out


def parse_summary(path: Path) -> dict:
    values = {}
    if not path.exists():
        return values
    with open(path, encoding="ascii", errors="replace") as f:
        for line in f:
            if "=" in line:
                k, v = line.rstrip("\n").split("=", 1)
                values[k] = v
    return values


def sized_rules(mode: str, options: list, expect: list, core: str) -> list:
    """The rules a row at another of the view's sizes holds on `core` on top of its mode's: SIZED's, less those whose key
    the row gives its own ceiling on that core (`key<=N`, or `core:key<=N` naming it)."""
    if "--view" not in options:
        return []
    own = set()
    for rule in expect:
        on, colon, rest = rule.partition(":")
        if colon and on in CORES:
            if on != core:
                continue
            rule = rest
        if "<=" in rule:
            own.add(rule.split("<=", 1)[0])
    return [rule for rule in SIZED.get(mode, []) if rule.split(">=", 1)[0].split("=", 1)[0] not in own]


def check(rule: str, values: dict) -> str:
    """The violated rule as text, or an empty string."""
    if ">=" in rule:   # a floor: what the route must exercise
        key, floor = rule.split(">=", 1)
        actual = values.get(key)
        if actual is None:
            return f"{key} missing"
        try:
            if int(actual) < int(floor):
                return f"{key}={actual} < {floor}"
        except ValueError:
            return f"{key}={actual} not a number"
        return ""
    if "<=" in rule:
        key, limit = rule.split("<=", 1)
        actual = values.get(key)
        if actual is None:
            return f"{key} missing"
        try:
            if int(actual) > int(limit):
                return f"{key}={actual} > {limit}"
        except ValueError:
            return f"{key}={actual} not a number"
        return ""
    key, wanted = rule.split("=", 1)
    actual = values.get(key)
    if actual is None:
        return f"{key} missing"
    if actual != wanted:
        return f"{key}={actual}, {wanted} expected"
    return ""


# Killed by a signal: a crash seen only under the load of a parallel suite, on rows of every game, the same row passing
# alone, under a debugger and under AddressSanitizer: its core dumps show the instruction pointer one or a few bytes
# inside an instruction no code branches to (SameBoy's GB_is_sgb, GB_display_run), the processor faulting under the
# load, not the replay.  A run so killed is run again, up to CRASH_RETRIES times, and the row says so; a row whose every
# run dies fails.
CRASH_RETRIES = 2


def run_harness(cmd: list, log_path: Path) -> tuple:
    """Runs the harness into log_path: its exit status, and a note of the runs killed by a signal and run again (their
    logs kept beside it, NAME.crashed.log then NAME.crashed-2.log)."""
    signals = []
    while True:
        with open(log_path, "w") as log:
            proc = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT)
        if proc.returncode >= 0 or len(signals) == CRASH_RETRIES:
            break
        signals.append(-proc.returncode)
        log_path.rename(log_path.with_name(f"{log_path.stem}.crashed{'' if len(signals) == 1 else f'-{len(signals)}'}.log"))
    if proc.returncode < 0:
        signals.append(-proc.returncode)
    if not signals:
        return proc.returncode, ""
    again = len(signals) - (proc.returncode < 0)
    return proc.returncode, (f" ({log_path.name} killed by signal {', '.join(str(n) for n in signals)}, run again {again} time{'s' if again > 1 else ''};"
                             f" the logs of the runs killed are {log_path.stem}.crashed*.log)")


def compare_cores_rooms(reference: Path, positions: Path, reference_exit: int, row_core: str, values: dict) -> list:
    """The rooms the row's replay goes through against SameBoy's (reference, its harness's exit status): what does not
    hold, the figures put in values.  A replay killed by a signal leaves its positions cut short, a line maybe half
    written: the row fails, the suite goes on."""
    if reference_exit < 0:
        return [f"SameBoy's replay to compare with died (signal {-reference_exit}; see {reference.parent / 'harness-sameboy.log'})"]
    if not positions.exists() or not reference.exists():
        return ["no positions to compare with SameBoy's replay"]
    try:
        same, parted = rooms_in_common(str(reference), str(positions))
    except Unreadable as error:
        return [f"positions unreadable: {error}"]
    values["cores.rooms_same"], values["cores.parted"] = str(same), str(int(parted))
    # A row of both cores goes through the game as on SameBoy; a core's own row names where it parts (its expect).
    if row_core == "-" and parted:
        return [f"parts from SameBoy's replay after {same} rooms (tools/compare_positions.py)"]
    return []


def run_row(row: dict, harness: Path, core: str, routes: Path, roms: dict, rom_dir: Path, work: Path, threaded_args: tuple = ()) -> dict:
    # A route's rows for one core and its rows for both may share a mode: the line tells them apart.
    name = f"{row['label']}-{row['mode']}" + (f"-{row['line']}" if row["options"] or row["core"] != "-" else "") + ("" if core == "sameboy" else f"-{core}")
    w = work / name
    w.mkdir(parents=True, exist_ok=True)
    for sub in ("render", "ghost", "enhanced"):
        (w / sub).mkdir(exist_ok=True)
    route_path = routes / row["route"]
    header = route_header(route_path)
    rom = roms.get(header.get("rom_sha1", ""))
    if rom is None:
        game = Path(row["route"]).parent.name
        return {"row": row, "name": name, "error": f"no ROM of {game} with SHA-1 {header.get('rom_sha1', '?')} in the ROM directory"
                " (a fan game replays from its base ROM and its BPS patch placed there, or from its patched image)"}
    options = harness_options(row["options"], rom_dir) + ["--core", core]
    summary = w / "summary.txt"
    if summary.exists():
        summary.unlink()
    # Off SameBoy, a faithful row is also compared with SameBoy's replay of the route: the rooms both go through.
    compare_cores = core != "sameboy" and row["mode"] == "faithful"
    positions = [] if not compare_cores else ["--positions", str(w / "positions.tsv")]
    extra = list(threaded_args) if row["mode"].endswith("-threaded") else []
    cmd = [str(harness)] + rom + ["--route", str(route_path)] + MODE_FLAGS[row["mode"]](w) + options + extra + positions + ["--summary", str(summary)]
    returncode, retried = run_harness(cmd, w / "harness.log")
    values = parse_summary(summary)
    violations = []
    if compare_cores and values:
        reference = w / "positions-sameboy.tsv"
        reference_exit, reference_retried = run_harness([str(harness)] + rom + ["--route", str(route_path), "--positions", str(reference)]
                                                        + harness_options(row["options"], rom_dir) + ["--core", "sameboy"], w / "harness-sameboy.log")
        retried += reference_retried
        violations += compare_cores_rooms(reference, w / "positions.tsv", reference_exit, row["core"], values)
    if not values:
        violations.append(f"no summary written (harness exit {returncode}{retried}; see {w / 'harness.log'})")
    sized = sized_rules(row["mode"], row["options"], row["expect"], core)
    for rule in ALWAYS[row["mode"]] + sized + prediction_rules(values) + wall_rules(values, row["expect"], core) + redraw_rules(values) + row["expect"]:
        on, colon, rest = rule.partition(":")
        if colon and on in CORES:
            if on != core:
                continue
            rule = rest
        v = check(rule, values)
        if v:
            violations.append(v)
    got_hash = values.get("enhanced.run_hash", "-") if row["mode"] in HASHED_MODES else "-"
    # A run killed by a signal leaves its fingerprints cut short: the row fails by itself, compared with nothing.
    fingerprints = w / "fingerprints.tsv" if returncode >= 0 else None
    # A route recorded since the core's joypad bouncing was cut says so: the emulated state must then be the same
    # whatever the audio sample rate, a windowed session's as the harness's.  Its faithful row is replayed again at 48 kHz.
    if row["mode"] == "faithful" and SESSION_CORE in header.get("core", "") and fingerprints and fingerprints.exists():
        at_rate = w / "fingerprints-48000.tsv"
        at_rate_exit, at_rate_retried = run_harness([str(harness)] + rom + ["--route", str(route_path), "--sample-rate", "48000", "--out", str(at_rate)] + options,
                                                    w / "harness-48000.log")
        retried += at_rate_retried
        if at_rate_exit < 0:
            violations.append(f"the replay at 48 kHz died (signal {-at_rate_exit}; see {w / 'harness-48000.log'})")
        same = subprocess.run([str(harness), "--compare", str(fingerprints), str(at_rate)], capture_output=True, text=True)
        if same.returncode != 0 and at_rate_exit >= 0:
            violations.append("the replay at 48 kHz is not the replay without audio: " + (same.stdout.strip().splitlines() or ["no output"])[-1])
    return {"row": row, "name": name, "values": values, "violations": violations, "hash": got_hash, "log": w / "harness.log", "retried": retried,
            "fingerprints": fingerprints if fingerprints and fingerprints.exists() else None}


# Checks that only read the live instance: a faithful row with one of them is compared with its route's faithful row.
READING_CHECKS = ("--object-animation-check", "--beside-witness")


def view_free(options: list) -> list:
    """The options less the view's level and shape (--view, --aspect), the screen far is fitted to (--screen), the
    status bar's width (--hud), the text box's size (--text-size), the neighbours' objects frozen (--neighbour-animation),
    the rooms never visited kept dark (--unvisited-rooms) and the large menus (--menus-large), which do not change the
    game, and the checks that only read: a row of a size, of the bar drawn full width, of the box twice as large, of the
    objects frozen, of the dark rooms, or of such a check, is compared with the faithful row of its route, or without a
    faithful row (the compact text, the wide box) with its row without them."""
    out, skip = [], False
    for option in options:
        if skip:
            skip = False
        elif option in ("--view", "--aspect", "--screen", "--hud", "--text-size", "--neighbour-animation", "--unvisited-rooms"):
            skip = True
        elif option == "--menus-large" or option in READING_CHECKS:
            pass
        else:
            out.append(option)
    return out


def compare_fingerprints(results: list, harness: Path) -> list:
    """The faithful row of a route and each of its enhanced rows, synchronous and threaded (same options), and its
    faithful rows with a check that only reads, must give the same live-state fingerprints."""
    by_key = {}
    for res in results:
        if "error" in res or not res.get("fingerprints"):
            continue
        row = res["row"]
        by_key.setdefault((row["route"], tuple(view_free(row["options"]))), {}).setdefault(row["mode"], []).append(res)
    verdicts = []
    for (route, options), modes in sorted(by_key.items()):
        presentation_only = False
        if "faithful" in modes:
            faithful = modes["faithful"][0]
        else:
            # No faithful row (an option Faithful refuses): the rows that differ by the presentation alone are compared
            # with the synchronous row without it.
            plain = [res for res in modes.get("enhanced", []) + modes.get("enhanced-zoom", []) if list(res["row"]["options"]) == list(options)]
            if not plain:
                continue
            faithful = plain[0]
            presentation_only = True
        for mode in ("faithful", "enhanced", "enhanced-threaded", "enhanced-zoom", "enhanced-zoom-threaded"):
            for res in modes.get(mode, [])[1 if mode == "faithful" else 0:]:
                if presentation_only and list(res["row"]["options"]) == list(options):
                    continue
                proc = subprocess.run([str(harness), "--compare", str(faithful["fingerprints"]), str(res["fingerprints"])],
                                      capture_output=True, text=True)
                text = (proc.stdout + proc.stderr).strip().splitlines()
                last = text[-1] if text else "no output"
                ok = proc.returncode == 0 and "identical" in last
                verdicts.append({"route": route, "label": faithful["row"]["label"], "options": tuple(res["row"]["options"]) or options,
                                 "mode": mode, "ok": ok, "text": last})
    return verdicts


def what(verdict: dict) -> str:
    """What a fingerprint comparison holds apart: the view, or a check that only reads."""
    return "check" if verdict["mode"] == "faithful" else "Enhanced view"


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--harness", type=Path, default=REPO / "build" / "oracles-harness")
    ap.add_argument("--core", choices=CORES, default="sameboy", help="the core the suite replays on: its rows and those of both")
    ap.add_argument("--routes", type=Path, default=REPO / "routes")
    ap.add_argument("--manifest", type=Path, default=None, help="one checks.tsv under the routes directory instead of all of them")
    ap.add_argument("--rom-dir", type=Path, default=None, help="defaults to $ORACLES_ROM_DIR; without it the check is skipped (exit 77)")
    ap.add_argument("--work", type=Path, default=None, help="defaults to build/routes-check, build/routes-check-mgba on mGBA")
    ap.add_argument("--jobs", type=int, default=DEFAULT_JOBS)
    ap.add_argument("--threaded-args", default="", help="harness flags added to the rows whose ghost runs in its thread (\"--enhanced-ghosts 2\")")
    ap.add_argument("--only", default=None, help="run the rows whose route, label or mode contains this text, or, for a text starting with --, whose options do")
    ap.add_argument("--update", action="store_true", help="rewrite the run hashes that changed in their manifests")
    args = ap.parse_args()
    manifests = [args.manifest] if args.manifest else sorted(args.routes.rglob("checks.tsv"))
    rom_dir = args.rom_dir or (Path(os.environ["ORACLES_ROM_DIR"]) if os.environ.get("ORACLES_ROM_DIR") else None)
    if rom_dir is None or not rom_dir.is_dir():
        print("check_routes: no ROM directory (set ORACLES_ROM_DIR or pass --rom-dir): skipped")
        return 77
    if not args.harness.exists():
        print(f"check_routes: no harness at {args.harness}: skipped")
        return 77
    roms = find_roms(rom_dir)
    if not roms:
        print(f"check_routes: no ROM in {rom_dir}: skipped")
        return 77
    rows = [r for r in read_manifests(manifests, args.routes) if r["core"] in ("-", args.core)]
    if args.work is None:
        args.work = REPO / "build" / ("routes-check" if args.core == "sameboy" else f"routes-check-{args.core}")
    if args.only:
        rows = [r for r in rows if args.only in r["route"] or args.only in f"{r['label']}-{r['mode']}" or args.only in r["mode"]
                or (args.only.startswith("--") and args.only in " ".join(r["options"]))]
    # A row in a language replays with the game's European ROM, which a ROM directory may not hold (the CI's never does).
    for r in rows:
        if "--language" in r["options"] and Path(r["route"]).parent.name not in EU_ROMS:
            sys.exit(f"{r['manifest']}:{r['line']}: a row in a language, but no European ROM is known for the routes of "
                     f"{Path(r['route']).parent.name} (EU_ROMS)")
    missing = [r for r in rows if "--language" in r["options"] and EU_ROMS[Path(r["route"]).parent.name] not in roms]
    if missing:
        print(f"check_routes: {len(missing)} row(s) in a language skipped: no European ROM of their game in {rom_dir}")
        rows = [r for r in rows if r not in missing]
        if not rows:
            return 77
    if not rows:
        print("check_routes: no row selected")
        return 2
    args.work.mkdir(parents=True, exist_ok=True)
    with concurrent.futures.ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        results = list(pool.map(lambda r: run_row(r, args.harness, args.core, args.routes, roms, rom_dir, args.work, tuple(shlex.split(args.threaded_args))), rows))
    failed = broken = 0
    changed = {}
    for res in results:
        row = res["row"]
        if "error" in res:
            print(f"ERROR  {res['name']}: {res['error']}")
            failed += 1
            broken += 1
            continue
        hash_note = ""
        held = core_hashes(row["run_hash"], row["core"]).get(args.core, "-")
        if held != "-" or res["hash"] != "-":
            if held == res["hash"]:
                hash_note = f" hash {res['hash']}"
            elif args.update:
                hash_note = f" hash {held} -> {res['hash']} (updated)"
                changed[(row["manifest"], row["line"])] = with_core_hash(row["run_hash"], row["core"], args.core, res["hash"])
            else:
                res["violations"].append(f"run hash {res['hash']}, {held} in the manifest for {args.core} (--update if the change is meant)")
        v = res["values"]
        figures = " ".join(f"{k.split('.', 1)[1]}={v[k]}" for k in (
            "render.verdict", "ghost.equal", "ghost.different", "ghost.reads_outside_key", "enhanced.camera_jumps", "enhanced.camera_jumps_y", "enhanced.link_jumps",
            "enhanced.transitions", "enhanced.transitions_black", "enhanced.ghost_failed", "enhanced.uncovered_frames", "transitions.taken",
            "cores.rooms_same", "cores.parted") if k in v)
        if res["violations"]:
            failed += 1
            # A run hash that moved is told apart from a rule that broke: one hides the other otherwise.
            rules = [x for x in res["violations"] if not x.startswith("run hash")]
            if rules:
                broken += 1
            print(("FAIL   " if rules else "HASH   ") + f"{res['name']}: " + "; ".join(rules + [x for x in res["violations"] if x.startswith("run hash")])
                  + f" [{figures}]{hash_note} (log: {res['log']})")
        else:
            print(f"ok     {res['name']}: {figures}{hash_note}{res.get('retried', '')}")
    verdicts = compare_fingerprints(results, args.harness)
    for verdict in verdicts:
        label = f"{verdict['label']} {verdict['mode']}" + (f" {' '.join(verdict['options'])}" if verdict["options"] else "")
        if verdict["ok"]:
            print(f"ok     {label}: live state identical with and without the {what(verdict)} ({verdict['text']})")
        else:
            failed += 1
            broken += 1
            print(f"FAIL   {label}: live state differs with the {what(verdict)}: {verdict['text']}")
    for manifest in sorted({m for m, _ in changed}):
        lines = manifest.read_text(encoding="utf-8").split("\n")
        for (m, n), h in changed.items():
            if m != manifest:
                continue
            cols = lines[n - 1].split("\t")
            cols[4] = h
            lines[n - 1] = "\t".join(cols)
        manifest.write_text("\n".join(lines), encoding="utf-8")
        print(f"check_routes: {sum(1 for m, _ in changed if m == manifest)} run hash(es) rewritten in {manifest}")
    total = len(results) + len(verdicts)
    print(f"check_routes: {total - failed} of {total} checks hold on {args.core}")
    if failed:
        print(f"check_routes: {broken} with a rule broken, {failed - broken} with only their run hash moved (HASH)")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
