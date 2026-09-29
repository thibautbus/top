#!/usr/bin/env python3
"""Check the text of the tracked files, line by line.

The checked files are the files git tracks, less third_party/ (vendored as
upstream ships it), the binary files and this tool with its list of
exceptions.  A line is refused when it holds:

- french: an accented letter, or a French stop word between spaces;
- local: a local path (a home directory, a Windows user profile).

Options add to that:

- --exclude FILE: prefixes, one per line, whose files are not checked (a
  prefix ending in / is a directory, any other a single file);
- --rules FILE: more rules, one per line, `name<TAB>regex` (the regex may
  start with (?i));
- --allow FILE, repeatable: the exceptions, one per line, `path<TAB>regex`,
  a line of that file matching the regex is not reported; without the
  option, tools/check_public_text.allow beside this tool.

In the three files, blank lines and lines starting with # are ignored.

usage: check_public_text.py [--root DIR] [--exclude FILE] [--rules FILE] [--allow FILE]...
       exit status 1 on any violation
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
SELF = {"tools/check_public_text.py", "tools/check_public_text.allow"}
BINARY_EXTENSIONS = {".ttf", ".jar", ".keystore", ".sram", ".store", ".png"}
RULES = [
    ("french", re.compile(r"[À-ÿ]", re.IGNORECASE)),
    ("french", re.compile(r" (le|les|des|est|une) ")),
    ("local", re.compile(r"/home/|C:\\+Users", re.IGNORECASE)),
]


def read_lines(path: Path | None) -> list[tuple[int, str]]:
    """The numbered lines of a list file that are neither blank nor comments; none without a file."""
    if path is None:
        return []
    return [(n, line) for n, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1)
            if line.strip() and not line.startswith("#")]


def pairs(path: Path | None, what: str) -> list[tuple[str, re.Pattern]]:
    entries = []
    for n, line in read_lines(path):
        if "\t" not in line:
            sys.exit(f"{path}:{n}: {what}<TAB>regex expected")
        name, pattern = line.split("\t", 1)
        entries.append((name, re.compile(pattern)))
    return entries


def published_files(root: Path, exclude: Path | None = None) -> list[str]:
    """The tracked files, less the prefixes that `exclude` lists."""
    out = subprocess.run(["git", "-C", str(root), "ls-files", "-z"], check=True, capture_output=True).stdout
    files = [p for p in out.decode("utf-8").split("\0") if p]
    prefixes = [line.strip() for _, line in read_lines(exclude)]
    return [f for f in files if not any(f == p or (p.endswith("/") and f.startswith(p)) for p in prefixes)]


def rules(extra: Path | None = None) -> list[tuple[str, re.Pattern]]:
    """The two rules above, and those of `extra`."""
    return RULES + pairs(extra, "name")


def is_binary(path: Path) -> bool:
    if path.suffix.lower() in BINARY_EXTENSIONS:
        return True
    with open(path, "rb") as f:
        return b"\0" in f.read(8192)


def violations(root: Path, exclude: Path | None = None, extra: Path | None = None, allows: list[Path] | None = None) -> list[str]:
    allow = [entry for path in (allows if allows else [HERE / "check_public_text.allow"]) for entry in pairs(path, "path")]
    checked_rules = rules(extra)
    found = []
    for relative in published_files(root, exclude):
        path = root / relative
        if relative.startswith("third_party/") or relative in SELF:
            continue
        if not path.is_file() or path.is_symlink() or is_binary(path):
            continue
        allowed = [pattern for p, pattern in allow if p == relative]
        for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
            if any(pattern.search(line) for pattern in allowed):
                continue
            rule = next((name for name, pattern in checked_rules if pattern.search(line)), None)
            if rule:
                found.append(f"{relative}:{number}: {rule}: {line.strip()}")
    return found


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=HERE.parent)
    ap.add_argument("--exclude", type=Path, help="prefixes not checked, one per line")
    ap.add_argument("--rules", type=Path, help="more rules, name<TAB>regex")
    ap.add_argument("--allow", type=Path, action="append", help="exceptions, path<TAB>regex (repeatable)")
    args = ap.parse_args()
    found = violations(args.root.resolve(), args.exclude, args.rules, args.allow)
    for v in found:
        print(v)
    print(f"public text: {len(found)} violation(s)")
    return 1 if found else 0


if __name__ == "__main__":
    raise SystemExit(main())
