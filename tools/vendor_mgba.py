#!/usr/bin/env python3
"""Vendor the part of mGBA the engine builds: its Game Boy core (third_party/mgba/VERSION.md).

Takes the archive config/mgba.json pins (downloaded, or --archive FILE), checks its SHA-256, copies the files listed
below into a new tree, applies the project's one patch (third_party/mgba/boot-rom.patch) and prints the hash of the
tree, which config/mgba.json pins (core_tree_sha256) and tools/audit_tracked_files.py checks.  Only then does the
tree replace third_party/mgba/core/, and mGBA's licence third_party/mgba/LICENSE.  With --check, nothing is written:
the exit status says whether the tree built equals the committed one and its hash the pinned one.

usage: vendor_mgba.py [--archive FILE] [--check]
"""
from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_tracked_files import core_tree_hash  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
CONFIG = ROOT / "config" / "mgba.json"
DEST = ROOT / "third_party" / "mgba"
PATCH = DEST / "boot-rom.patch"

# The sources third_party/CMakeLists.txt compiles, all of them (it compiles every .c of the tree, and one of the two
# platform files): the Game Boy core, its SM83 processor, mGBA's core layer and the parts of its utilities they call,
# and inih, which reads the core's configuration.  Neither the GBA core, nor the debugger, scripting, threads, files on
# disk or any external library.
SOURCES = """
src/core/bitmap-cache.c src/core/cache-set.c src/core/cheats.c src/core/config.c src/core/core.c src/core/input.c
src/core/interface.c src/core/log.c src/core/map-cache.c src/core/serialize.c src/core/sync.c src/core/tile-cache.c
src/core/timing.c
src/gb/audio.c src/gb/cheats.c src/gb/core.c src/gb/gb.c src/gb/input.c src/gb/io.c src/gb/mbc.c src/gb/mbc/huc-3.c
src/gb/mbc/licensed.c src/gb/mbc/mbc.c src/gb/mbc/pocket-cam.c src/gb/mbc/tama5.c src/gb/mbc/unlicensed.c
src/gb/memory.c src/gb/overrides.c src/gb/renderers/cache-set.c src/gb/renderers/software.c src/gb/serialize.c
src/gb/sio.c src/gb/timer.c src/gb/video.c
src/sm83/decoder.c src/sm83/isa-sm83.c src/sm83/sm83.c
src/util/audio-buffer.c src/util/audio-resampler.c src/util/circle-buffer.c src/util/configuration.c src/util/crc32.c
src/util/formatting.c src/util/gbk-table.c src/util/geometry.c src/util/hash.c src/util/image.c src/util/interpolator.c
src/util/md5.c src/util/patch.c src/util/patch-ips.c src/util/patch-ups.c src/util/sha1.c src/util/string.c
src/util/table.c src/util/vector.c src/util/vfs.c src/util/vfs/vfs-mem.c
src/third-party/inih/ini.c
src/platform/posix/memory.c src/platform/windows/memory.c
""".split()

# What those sources include or read beside them: a private header, inih's header and licence, and the template of
# the version file the build generates.
OTHER_FILES = """
src/gb/mbc/mbc-private.h src/third-party/inih/ini.h src/third-party/inih/LICENSE.txt src/core/version.c.in
include/mgba/internal/defines.h
""".split()

# The public headers, by directory, with their subdirectories: those of the utilities, less the other platforms' and
# the GUI's, and those of the core layer, the Game Boy, the SM83 and the debugger and feature declarations the core's
# headers name.
HEADER_DIRS = """
include/mgba-util include/mgba/core include/mgba/gb include/mgba/debugger include/mgba/feature
include/mgba/internal/debugger include/mgba/internal/gb include/mgba/internal/sm83
""".split()
HEADER_EXCLUDED = """
include/mgba-util/gui include/mgba-util/platform/3ds include/mgba-util/platform/psp2
include/mgba-util/platform/switch
""".split()


def wanted(relative: str) -> bool:
    path = PurePosixPath(relative)
    if relative in SOURCES or relative in OTHER_FILES:
        return True
    under = lambda prefixes: any(path.is_relative_to(p) for p in prefixes)  # noqa: E731
    return path.suffix == ".h" and under(HEADER_DIRS) and not under(HEADER_EXCLUDED)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--archive", type=Path, help="the pinned archive, already downloaded")
    ap.add_argument("--check", action="store_true", help="build the tree aside and compare it, writing nothing")
    args = ap.parse_args()
    pin = json.loads(CONFIG.read_text(encoding="utf-8"))

    with tempfile.TemporaryDirectory() as scratch:
        archive = args.archive
        if archive is None:
            archive = Path(scratch) / "mgba.tar.gz"
            print(f"downloading {pin['archive_url']}", file=sys.stderr)
            with urllib.request.urlopen(pin["archive_url"]) as response, open(archive, "wb") as out:
                shutil.copyfileobj(response, out)
        digest = hashlib.sha256(archive.read_bytes()).hexdigest()
        if digest != pin["archive_sha256"]:
            print(f"{archive}: SHA-256 {digest}, {pin['archive_sha256']} pinned", file=sys.stderr)
            return 1

        built = Path(scratch) / "core"
        licence = None
        found = set()
        top = f"mgba-{pin['commit']}/"
        with tarfile.open(archive) as tar:
            for member in tar.getmembers():
                if not member.isfile() or not member.name.startswith(top):
                    continue
                relative = member.name[len(top):]
                if relative == "LICENSE":
                    licence = tar.extractfile(member).read()
                    found.add(relative)
                elif wanted(relative):
                    target = built / relative
                    target.parent.mkdir(parents=True, exist_ok=True)
                    target.write_bytes(tar.extractfile(member).read())
                    found.add(relative)
        missing = sorted((set(SOURCES) | set(OTHER_FILES) | {"LICENSE"}) - found)
        if missing:
            print("not in the archive: " + " ".join(missing), file=sys.stderr)
            return 1
        # Outside any repository, git apply patches the files of its working directory.
        if subprocess.run(["git", "apply", "-p1", str(PATCH)], cwd=built).returncode != 0:
            print(f"{PATCH.name} does not apply to this commit", file=sys.stderr)
            return 1
        digest = core_tree_hash(built)
        print(f"core_tree_sha256: {digest}")

        core = DEST / "core"
        if args.check:
            same_tree = core.is_dir() and core_tree_hash(core) == digest
            same_licence = (DEST / "LICENSE").is_file() and (DEST / "LICENSE").read_bytes() == licence
            same_pin = digest == pin.get("core_tree_sha256")
            print(f"committed tree {'equal' if same_tree else 'DIFFERENT'}, licence {'equal' if same_licence else 'DIFFERENT'}, "
                  f"pin {'equal' if same_pin else 'DIFFERENT'}")
            return 0 if same_tree and same_licence and same_pin else 1
        if core.exists():
            shutil.rmtree(core)
        shutil.copytree(built, core)
        (DEST / "LICENSE").write_bytes(licence)
    return 0


if __name__ == "__main__":
    sys.exit(main())
