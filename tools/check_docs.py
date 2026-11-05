#!/usr/bin/env python3
"""Check the documents of the repository.

- every relative Markdown link in a document resolves to a file;
- every link to a section (`#section`, `OTHER.md#section`) names a heading of that document, as GitHub makes its
  anchors: the heading's text in lower case, its punctuation dropped, its spaces as hyphens, `-1`, `-2`... after a
  heading met again;
- README.md has fewer than 160 lines.

The documents: the Markdown files git tracks, except the vendored ones
(third_party/) and those under the prefixes --exclude FILE lists, one per line.

usage: check_docs.py [--root DIR] [--exclude FILE]      exit status 1 on any failure
"""
from __future__ import annotations

import argparse
import re
import sys
from functools import lru_cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from check_public_text import published_files  # noqa: E402

LINK = re.compile(r"(?<!\!)\[[^\]]*\]\(([^)\s]+)\)")
HEADING = re.compile(r"^ {0,3}#{1,6}[ \t]+(.*?)[ \t#]*$")
FENCE = re.compile(r"^ {0,3}(```|~~~)")
HTML_ANCHOR = re.compile(r"<a\s[^>]*\b(?:id|name)=\"([^\"]+)\"", re.IGNORECASE)
README_MAX_LINES = 160


def documents(root: Path, exclude: Path | None = None) -> list[Path]:
    return [root / p for p in sorted(published_files(root, exclude))
            if p.endswith(".md") and not p.startswith("third_party/") and (root / p).exists()]


def slug(heading: str) -> str:
    """The anchor GitHub gives a heading: its text without the links' targets, the tags and the punctuation, in lower
    case, a hyphen for each space."""
    text = re.sub(r"!?\[([^\]]*)\]\([^)]*\)", r"\1", heading)
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"[^\w\- ]", "", text.lower())
    return text.replace(" ", "-")


@lru_cache(maxsize=None)
def anchors(doc: Path) -> frozenset[str]:
    """The sections a link may name in `doc`: its headings' anchors, outside the code blocks, and its HTML anchors."""
    found: set[str] = set()
    seen: dict[str, int] = {}
    fenced = False
    for line in doc.read_text(encoding="utf-8").splitlines():
        if FENCE.match(line):
            fenced = not fenced
            continue
        if fenced:
            continue
        found.update(HTML_ANCHOR.findall(line))
        m = HEADING.match(line)
        if not m:
            continue
        base = slug(m.group(1))
        n = seen.get(base, 0)
        seen[base] = n + 1
        found.add(base if n == 0 else f"{base}-{n}")
    return frozenset(found)


def broken_links(root: Path, doc: Path) -> list[str]:
    """The relative links of `doc` whose file does not exist, or whose section is not a heading of that file."""
    rel = doc.relative_to(root).as_posix()
    broken = []
    for target in LINK.findall(doc.read_text(encoding="utf-8")):
        if target.startswith(("http://", "https://", "mailto:")):
            continue
        path, _, section = target.partition("#")
        linked = (doc.parent / path) if path else doc
        if path and not linked.exists():
            broken.append(f"{rel}: broken link to {target}")
        elif section and linked.suffix == ".md" and linked.is_file() and section not in anchors(linked.resolve()):
            broken.append(f"{rel}: no section #{section} in {linked.resolve().relative_to(root).as_posix()} ({target})")
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
