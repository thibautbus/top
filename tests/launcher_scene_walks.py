#!/usr/bin/env python3
"""The launcher's replay of a route written before scene walks were left to the game (route format 16): Ages' intro
replayed headless as written (format 16) and as an earlier format names it (2).  The first leaves the 44 frames of the
scroll the scene walks Link into to the game; the second takes them as the transitions did, leaving fewer alone, and
says so.  The earlier format cut before the scene (its last keys at 1850, the scene walking Link from 1860) gives the
player's game its walk back: the 44 frames left alone again.  Skipped (77) without the ROM in $ORACLES_ROM_DIR."""
import hashlib
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

AGES_US = "880374fb978b18af4aa529e2e32f7ffb4d7dd2f4"


def find_rom(directory):
    if not directory or not Path(directory).is_dir():
        return None
    for p in Path(directory).iterdir():
        if p.suffix.lower() in (".gb", ".gbc") and p.is_file() and hashlib.sha1(p.read_bytes()).hexdigest() == AGES_US:
            return p
    return None


def replay(launcher, rom, route, home):
    env = dict(os.environ, ORACLES_SETTINGS_DIR=str(home), SDL_VIDEO_DRIVER="dummy")
    out = subprocess.run([launcher, "--rom", str(rom), "--play", str(route), "--no-window", "--frames", "2000"],
                         env=env, capture_output=True, text=True, timeout=600)
    log = out.stdout + out.stderr
    alone = re.search(r"continuous transitions: \d+ taken over.*?left alone on (\d+)", log)
    return out.returncode, log, int(alone.group(1)) if alone else -1


def main():
    launcher, intro = sys.argv[1], Path(sys.argv[2])
    rom = find_rom(os.environ.get("ORACLES_ROM_DIR"))
    if rom is None:
        print("launcher_scene_walks: no Ages ROM in $ORACLES_ROM_DIR, skipped")
        return 77
    failures = 0
    with tempfile.TemporaryDirectory() as tmp:
        tmp = Path(tmp)
        (tmp / "home").mkdir()
        lines = intro.read_text(encoding="ascii").splitlines(keepends=True)
        results = {}
        inputs = lines.index("inputs\n") + 1
        cut = lines[:inputs] + [l for l in lines[inputs:] if int(l.split()[0]) < 1850] + ["1850 keys 00\n"]
        for name, fmt, body in ((16, 16, lines), (2, 2, lines), ("cut", 2, cut)):
            route = tmp / f"intro-{name}.route"
            route.write_text(f"oracles-route {fmt}\n" + "".join(body[1:]), encoding="ascii")
            shutil.copyfile(str(intro) + ".sram", str(route) + ".sram")
            results[name] = replay(launcher, rom, route, tmp / "home")
        said = "before scene walks were left to the game"
        for fmt, (code, log, alone) in results.items():
            if code != 0:
                print(f"FAIL format {fmt}: exit {code}\n{log[-2000:]}")
                failures += 1
        if results[16][2] != 44 or said in results[16][1]:
            print(f"FAIL format 16: {results[16][2]} frames left alone (44 expected), the earlier format's message {'given' if said in results[16][1] else 'absent'}")
            failures += 1
        if not 0 <= results[2][2] < results[16][2] or said not in results[2][1]:
            print(f"FAIL format 2: {results[2][2]} frames left alone (fewer than {results[16][2]} expected), the earlier format's message {'given' if said in results[2][1] else 'absent'}")
            failures += 1
        if results["cut"][2] != 44 or said not in results["cut"][1] or "route finished at frame" not in results["cut"][1]:
            print(f"FAIL format 2 cut before the scene: {results['cut'][2]} frames left alone (44 expected once the replay is over)")
            failures += 1
    print("launcher_scene_walks: ok" if not failures else f"launcher_scene_walks: {failures} failure(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
