#!/usr/bin/env python3
"""Check the documents of the repository.

- every relative Markdown link in a document resolves to a file;
- README.md has fewer than 150 lines.

The documents: the Markdown files git tracks, except the vendored ones
(third_party/) and those under the prefixes --exclude FILE lists, one per line.

usage: check_docs.py [--root DIR] [--exclude FILE]      exit status 1 on any failure
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_public_text import published_files  # noqa: E402

LINK = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)\)")
README_MAX_LINES = 150


def documents(root: Path, exclude: Path | None = None) -> list[Path]:
    return [root / p for p in sorted(published_files(root, exclude))
            if p.endswith(".md") and not p.startswith("third_party/") and (root / p).exists()]


def broken_links(root: Path, doc: Path) -> list[str]:
    """The relative links of `doc` whose file does not exist."""
    rel = doc.relative_to(root).as_posix()
    broken = []
    for target in LINK.findall(doc.read_text(encoding="utf-8")):
        if target.startswith(("http://", "https://", "mailto:", "#")):
            continue
        path = target.split("#", 1)[0]
        if path and not (doc.parent / path).exists():
            broken.append(f"{rel}: broken link to {target}")
    return broken


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    ap.add_argument("--exclude", type=Path, help="prefixes whose documents are not checked, one per line")
    args = ap.parse_args()
    root = args.root.resolve()
    docs = documents(root, args.exclude)
    failures = [f for doc in docs for f in broken_links(root, doc)]
    readme_lines = len((root / "README.md").read_text(encoding="utf-8").splitlines())
    if readme_lines >= README_MAX_LINES:
        failures.append(f"README.md: {readme_lines} lines, at most {README_MAX_LINES - 1} expected")
    for f in failures:
        print(f)
    print(f"{len(docs)} documents, {len(failures)} problem(s)")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
