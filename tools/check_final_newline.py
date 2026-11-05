#!/usr/bin/env python3
"""Refuse a source file that does not end with a newline.

GCC and MinGW accept a C file whose last line has no newline, but Apple's clang
reports -Wnewline-eof, an error under warnings as errors: the macOS builds would
stop on what the Linux and Windows builds let through.  This check reads the
C, Objective-C, Lua, Python and CMake files of the repository's own directories
(not third_party) and reports each one whose last byte is not a newline.

usage: check_final_newline.py [--root DIR]     exit status 1 on any violation
"""
from __future__ import annotations

import argparse
from pathlib import Path

SCANNED_DIRS = ["engine", "launcher", "harness", "tests", "tools", "mods"]
SUFFIXES = {".c", ".h", ".m", ".lua", ".py"}
NAMES = {"CMakeLists.txt"}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = ap.parse_args()
    root = args.root.resolve()
    violations: list[str] = []
    paths = [root / "CMakeLists.txt"]
    for directory in SCANNED_DIRS:
        if (root / directory).is_dir():
            paths += sorted(p for p in (root / directory).rglob("*") if p.is_file() and (p.suffix in SUFFIXES or p.name in NAMES))
    for path in paths:
        if not path.is_file():
            continue
        data = path.read_bytes()
        if data and not data.endswith(b"\n"):
            violations.append(f"{path.relative_to(root).as_posix()}: no newline at the end of the file")
    for v in violations:
        print(v)
    print(f"final newlines: {len(violations)} violation(s)")
    return 1 if violations else 0


if __name__ == "__main__":
    raise SystemExit(main())
