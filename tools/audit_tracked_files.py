#!/usr/bin/env python3
"""Audit of the public boundary: what the repository may carry.

The repository must not carry the user's ROM, anything decompressed from it,
a save, a build output or a binary. The audit walks the files git tracks and
refuses:

- a ROM, save or state extension (.gb, .gbc, .gba, .rom, .sav, .srm, .state);
- a binary or archive extension (.exe, .dll, .so and a versioned .so.N, .dylib, .o, .a, .apk, .zip, .tar, .gz, .7z,
  .xz, .bz2): what a build or a release makes;
- a path with a build or private component (generated, cache, build, build-*, dist, output);
- a file starting with a Game Boy cartridge header (the Nintendo logo at $104);
- a file larger than 4 MiB, except the vendored SameBoy sources;
- a top-level entry outside the repository's roots, among the files not left
  out by --exclude FILE (prefixes, one per line; none without the option);
- a vendored tree (SameBoy's Core, Lua's src) whose hash is not the one pinned in config/.

usage: audit_tracked_files.py [--root DIR] [--exclude FILE] [--json]     exit status 1 on any violation
"""
from __future__ import annotations

import argparse
import functools
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path, PurePosixPath

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_public_text import published_files  # noqa: E402

ALLOWED_TOP = {".github", ".gitattributes", ".gitignore", "README.md", "CONTRIBUTING.md", "LICENSE",
               "CMakeLists.txt",
               "android", "config", "docs", "engine", "harness", "launcher", "mods", "routes",
               "tests", "third_party", "tools"}
ROM_EXTENSIONS = {".gb", ".gbc", ".gba", ".rom", ".sav", ".srm", ".state"}
BINARY_EXTENSIONS = {".exe", ".dll", ".so", ".dylib", ".o", ".a", ".obj", ".lib", ".apk",
                     ".zip", ".tar", ".gz", ".tgz", ".7z", ".xz", ".bz2"}
VERSIONED_LIBRARY = re.compile(r"\.so(\.[0-9]+)+$", re.IGNORECASE)   # libSDL3.so.0, beside a Linux release's executable
PRIVATE_COMPONENTS = {"generated", "cache", "build", "dist", "output", "results"}
NINTENDO_LOGO_START = bytes.fromhex("ceed6666cc0d000b")
MAX_SIZE = 4 * 1024 * 1024
LARGE_FILE_ALLOWED_PREFIX = "third_party/"


def tracked_files(root: Path) -> list[str]:
    out = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], check=True, capture_output=True).stdout
    return [p for p in out.decode("utf-8").split("\0") if p]


@functools.lru_cache(maxsize=None)
def published(root: Path, exclude: Path | None) -> frozenset:
    return frozenset(published_files(root, exclude))


def violations_for(root: Path, relative: str, exclude: Path | None = None) -> list[str]:
    path = PurePosixPath(relative)
    found: list[str] = []
    if path.parts[0] not in ALLOWED_TOP and relative in published(root, exclude):
        found.append("unknown-top-level-entry")
    if any(part in PRIVATE_COMPONENTS or part.startswith("build-") for part in path.parts):
        found.append("private-output-path")
    suffix = path.suffix.lower()
    if suffix in ROM_EXTENSIONS:
        found.append("rom-or-save-extension")
    if suffix in BINARY_EXTENSIONS or VERSIONED_LIBRARY.search(path.name):
        found.append("binary-or-archive-extension")
    file = root / relative
    if file.is_symlink():
        found.append("symlink")
        return found
    if not file.is_file():
        return found
    size = file.stat().st_size
    if size > MAX_SIZE and not relative.startswith(LARGE_FILE_ALLOWED_PREFIX):
        found.append("oversized-file")
    if size >= 0x150:
        with open(file, "rb") as handle:
            handle.seek(0x104)
            if handle.read(len(NINTENDO_LOGO_START)) == NINTENDO_LOGO_START:
                found.append("cartridge-header")
    return found


def core_tree_hash(core: Path) -> str:
    """SHA-256 over the sorted relative paths and contents of the vendored Core tree."""
    h = hashlib.sha256()
    for path in sorted(p for p in core.rglob("*") if p.is_file()):
        relative = path.relative_to(core).as_posix().encode()
        h.update(len(relative).to_bytes(4, "little"))
        h.update(relative)
        data = path.read_bytes()
        h.update(len(data).to_bytes(8, "little"))
        h.update(data)
    return h.hexdigest()


# The vendored trees and the files that pin them: each tree is the unmodified
# upstream one (third_party/sameboy/VERSION.md, third_party/lua/VERSION.md).
PINNED_TREES = [("config/sameboy.json", "third_party/sameboy/Core", "core_tree_sha256"),
                ("config/lua.json", "third_party/lua/src", "src_tree_sha256")]


def pinned_core_violations(root: Path) -> list[dict[str, str]]:
    """Every vendored tree must be the one its config file pins."""
    report = []
    for config_name, tree_name, key in PINNED_TREES:
        config, tree = root / config_name, root / tree_name
        try:
            pinned = json.loads(config.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            report.append({"path": config_name, "reason": "unreadable-core-pin"})
            continue
        if not tree.is_dir():
            report.append({"path": tree_name, "reason": "vendored-core-missing"})
            continue
        actual = core_tree_hash(tree)
        if actual != pinned.get(key):
            report.append({"path": tree_name, "reason": f"vendored-core-differs-from-pin (tree {actual})"})
    return report


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument("--exclude", type=Path, help="prefixes whose top-level entries are not checked, one per line")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args()
    root = args.root.resolve()
    report = []
    for relative in tracked_files(root):
        for reason in violations_for(root, relative, args.exclude):
            report.append({"path": relative, "reason": reason})
    report += pinned_core_violations(root)
    status = "pass" if not report else "fail"
    if args.json:
        print(json.dumps({"status": status, "violations": report}, indent=2))
    else:
        for item in report:
            print(f"{item['path']}: {item['reason']}")
        print(f"audit {status}: {len(report)} violation(s)")
    return 0 if not report else 1


if __name__ == "__main__":
    raise SystemExit(main())
