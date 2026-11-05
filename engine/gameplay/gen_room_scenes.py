#!/usr/bin/env python3
"""Generate room_scenes.c from the sources of oracles-disasm: the objects whose first frames may leave the player
without the controls once the scroll after them is decided.

At a corner the scroll into the room beside starts at the frame the first scroll ends: the room entered's objects run
their first frame in the next one, the scroll beside already decided, which none of them can refuse, and that frame's
load of the room beside (func_49c9) clears wDisabledObjects, wDisableScreenTransitions and wDisableWarpTiles.  What
stays is the menu disabled (disableinput, disablemenu, wMenuDisabled), Link's collisions and menu disabled
(wDisableLinkCollisionsAndMenu), Link's state forced (wLinkForceState), and a scene (wCutsceneTrigger, wCutsceneIndex).
The room entered's objects run that one frame, their state 0: at the next load they are frozen and cleared.  The
generator marks an object when the code or the scripts its first frame may run name one of these, by name or, as the
sources of Seasons often write them, by address, in whatever branch, subid or condition: what may happen, not what
does.  The first frame runs a table of states (ld e,X.state / ld a,(de) / rst_jumpTable) at its first entry alone, not
the branch a state not 0 takes after checkInteractionState, and a script up to its first command that waits
(check..., wait, delay, rungenericnpc, scriptend).

It reads the objects' code (object_code/<game>/<kind>, object_code/common/<kind>) and every file of scripts
(the game's own and the common ones), split into blocks at their labels, a block's object the banner above it (its INTERAC_,
ENEMY_ or PART_ constant) if any.  From an object's entry it follows every reference a block makes to another (a jump,
a call, a table's entry, a script's address, mainScripts.X, scriptHelp.X and the like by their last name, the next
block it runs on into); an interaction whose code chooses by subid (ld e,Interaction.subid / ld a,(de) /
rst_jumpTable) is followed by subid, each table chosen so to that subid's entry, and so is a table of addresses the
code reads by the subid.  An object that names another's constant (to create it) is marked when that one is, for any
subid, until nothing more is marked.  A reference out of the files read (a routine of the engine) is not followed.

usage: gen_room_scenes.py --disasm DIR [--out FILE] [--check]
       gen_room_scenes.py --check [--out FILE]   (no disassembly: the generator's own digest against the one the file names)
"""
from __future__ import annotations

import argparse
import hashlib
import re
import subprocess
import sys
from pathlib import Path

GAMES = ("ages", "seasons")
KINDS = (("interactions", "INTERAC_"), ("enemies", "ENEMY_"), ("parts", "PART_"))
NAMES = ("disableinput", "disablemenu", "wMenuDisabled", "wDisableLinkCollisionsAndMenu", "wLinkForceState",
         "setLinkForceState\\w*", "wCutsceneTrigger", "wCutsceneIndex")
ADDRESSED = ("wMenuDisabled", "wDisableLinkCollisionsAndMenu", "wLinkForceState", "wCutsceneTrigger")
LABEL = re.compile(r"^(@*[A-Za-z_][\w.]*):")
WORD = re.compile(r"[@A-Za-z_][\w@.]*")
ENDS = re.compile(r"^(jp|jr|ret|reti|scriptend|scriptjump|jump2byte|rungenericnpc\w*)\b(?!.*,)")
CONSTANT = re.compile(r"\b((?:INTERAC|ENEMY|PART)_[A-Z0-9_]+)\b")


def constants(disasm: Path, game: str, kind: str, prefix: str) -> dict[str, int]:
    values: dict[str, int] = {}
    for line in (disasm / "constants" / game / f"{kind}.s").read_text(errors="replace").splitlines():
        m = re.match(rf"\.define ({prefix}\w+)\s+\$([0-9a-fA-F]+)", line.strip())
        if m:
            values[m.group(1)] = int(m.group(2), 16)
    return values


def effects(disasm: Path, game: str) -> re.Pattern:
    """The names of the effects, and their WRAM addresses as `$xxxx` from the game's symbol file."""
    addresses = []
    for line in (disasm / f"{game}.sym").read_text(errors="replace").splitlines():
        m = re.match(r"^00:([0-9a-f]{4}) (\w+)$", line.strip())
        if m and m.group(2) in ADDRESSED:
            addresses.append(m.group(1))
    words = "|".join(NAMES)
    raw = "|".join(rf"\${a}" for a in addresses)
    return re.compile(rf"\b(?:{words})\b|(?:{raw})(?![0-9a-fA-F])")


class Blocks:
    """The blocks of the files read: a label's lines up to the next label, a local label (@x) scoped by the global one
    before it; the object of the banner above it, None outside one."""

    def __init__(self, danger: re.Pattern) -> None:
        self.lines: dict[str, list[str]] = {}
        self.order: list[str] = []
        self.next: dict[str, str] = {}
        self.owner: dict[str, str | None] = {}
        self.danger = danger
        self.short: dict[str, str] = {}
        self.script: set[str] = set()

    def add(self, path: Path) -> None:
        current = scope = owner = None
        previous = None
        lines = path.read_text(errors="replace").splitlines()
        for i, raw in enumerate(lines):
            m = re.match(r";\s*([A-Z][A-Z0-9_]+)\s*$", raw)
            if m and i > 0 and lines[i - 1].startswith("; ===="):
                owner = m.group(1) if CONSTANT.fullmatch(m.group(1)) else None
                previous = current = None
                continue
            line = re.sub(r"/\*.*?\*/", "", raw.split(";")[0]).strip()
            m = LABEL.match(line)
            if m:
                name = m.group(1)
                if name.startswith("@"):
                    name = f"{scope}{name}"
                else:
                    scope = name
                if name not in self.lines:
                    self.lines[name] = []
                    if "scripts" in path.parts:
                        self.script.add(name)
                    self.order.append(name)
                    self.owner[name] = owner
                    self.short.setdefault(name.split(".")[-1], name)
                if previous is not None:
                    self.next[previous] = name
                previous = current = name
                line = line[m.end():].strip()
            if current and line:
                self.lines[current].append(line)

    def resolve(self, word: str, scope: str) -> str | None:
        if word.startswith("@"):
            word = f"{scope}{word}"
        if word in self.lines:
            return word
        last = word.split(".")[-1]   # mainScripts.X, scriptHelp.X, scripts2.X, a bank's namespace
        if last in self.lines:
            return last
        return self.short.get(last)

    def table(self, name: str) -> list[str] | None:
        body = self.lines.get(name, [])
        if not body or not all(line.startswith(".dw") for line in body):
            return None
        scope = name.split("@")[0]
        out = []
        for line in body:
            for w in line[3:].split(","):
                target = self.resolve(w.strip(), scope)
                out.append(target or w.strip())
        return out

    def first_frame(self, name: str) -> tuple[list[str], bool]:
        """The lines of `name` an object's first frame may run, and whether it may run on into the next block: a table
        of states (ld e,X.state / ld a,(de) / rst_jumpTable) taken at its first entry alone, the branch a state not 0
        takes after checkInteractionState, checkEnemyState or checkPartState dropped; a script up to the first command
        that waits (check..., wait, delay, rungenericnpc, scriptend), the rest coming at a later frame."""
        body = self.lines[name]
        if name in self.script:
            for i, line in enumerate(body):
                if re.match(r"(check\w*|wait\w*|delay\w*|rungenericnpc\w*|scriptend)\b", line):
                    return body[:i], False
            return body, True
        out = []
        for i, line in enumerate(body):
            if (re.match(r"ld e,(Interaction|Enemy|Part)\.state$", line) and i + 2 < len(body) and body[i + 1] == "ld a,(de)"
                    and body[i + 2] == "rst_jumpTable"):
                if i + 3 < len(body):
                    out.append(body[i + 3])
                return out, False
            if re.match(r"j[pr] nz,", line) and i > 0 and re.match(r"call check(Interaction|Enemy|Part)State$", body[i - 1]):
                continue
            out.append(line)
        return out, True

    def refs(self, name: str, lines: list[str], chosen: str | None, subid: int | None, onward: bool = True) -> list[str]:
        scope = name.split("@")[0]
        out = [chosen] if chosen else []
        by_subid = subid is not None and any("Interaction.subid" in line for line in lines)
        for line in lines:
            for w in WORD.findall(line):
                target = self.resolve(w, scope)
                if not target:
                    continue
                entries = self.table(target) if by_subid else None
                if entries is not None and subid < len(entries):
                    out.append(entries[subid])
                else:
                    out.append(target)
        last = self.lines[name][-1] if self.lines[name] else ""
        if onward and chosen is None and name in self.next and not ENDS.match(last) and not last.startswith(".dw"):
            out.append(self.next[name])
        return out

    def reaches(self, start: str, tables: dict[str, tuple[int, list[str]]], index: int | None, marked_constants: set[str]) -> bool:
        """Whether a block reached from `start` names an effect or an object marked; through each of `tables` to the
        entry of subid `index` alone."""
        seen, todo = set(), [start]
        while todo:
            name = todo.pop()
            if name in seen or name not in self.lines:
                continue
            seen.add(name)
            table = tables.get(name) if index is not None else None
            if table and index >= len(table[1]):
                continue
            lines, onward = self.first_frame(name)
            if table:
                lines = lines[:table[0]]
            for line in lines:
                if self.danger.search(line) or any(c in marked_constants for c in CONSTANT.findall(line)):
                    return True
            todo.extend(self.refs(name, lines, table[1][index] if table else None, index, onward))
        return False


def subid_tables(blocks: Blocks, start: str) -> dict[str, tuple[int, list[str]]]:
    """The tables an interaction's code reached from `start` chooses by subid (ld e,Interaction.subid / ld a,(de) /
    rst_jumpTable): by block, the line it starts at and its entries in subid order."""
    found: dict[str, tuple[int, list[str]]] = {}
    seen, todo = set(), [start]
    while todo:
        name = todo.pop()
        if name in seen or name not in blocks.lines:
            continue
        seen.add(name)
        body = blocks.lines[name]
        scope = name.split("@")[0]
        cut = len(body)
        for i in range(len(body) - 2):
            if re.match(r"ld e,Interaction\.subid$", body[i]) and body[i + 1] == "ld a,(de)" and body[i + 2] == "rst_jumpTable":
                out = []
                for line in body[i + 3:]:
                    m = re.match(r"\.dw\s+(\S+)$", line)
                    if not m:
                        break
                    out.append(blocks.resolve(m.group(1), scope) or m.group(1))
                found[name] = (i, out)
                cut = i
                break
        todo.extend(blocks.refs(name, body[:cut], None, None))
    return found


def marked(disasm: Path, game: str) -> tuple[list[list[int]], list[tuple[int, list[int]]]]:
    """Per kind the ids marked whatever their subid, and the interactions marked for some subids alone."""
    blocks = Blocks(effects(disasm, game))
    for kind, _ in KINDS:
        for root in (disasm / "object_code" / game / kind, disasm / "object_code" / "common" / kind):
            for path in sorted(root.glob("*.s")):
                blocks.add(path)
    for root in (disasm / "scripts" / game, disasm / "scripts" / "common"):
        for path in sorted(root.glob("*.s")):
            blocks.add(path)
    objects = []
    for k, (kind, prefix) in enumerate(KINDS):
        for name, value in constants(disasm, game, kind, prefix).items():
            entry = next((n for n in blocks.order if blocks.owner.get(n) == name
                          and re.match(r"(interaction|enemy|part)Code[0-9a-f]{2}$", n, re.I)), None)
            roots = [entry] if entry else [n for n in blocks.order if blocks.owner.get(n) == name]
            if roots:
                objects.append((k, name, value, roots))
    whole: dict[str, bool] = {}
    partial: dict[str, list[int]] = {}
    while True:
        marked_constants = {n for n, w in whole.items() if w} | {n for n, p in partial.items() if p}
        changed = False
        for k, name, value, roots in objects:
            tables = subid_tables(blocks, roots[0]) if k == 0 and len(roots) == 1 else {}
            count = max((len(t[1]) for t in tables.values()), default=0)
            if count:
                bad = [i for i in range(count) if blocks.reaches(roots[0], tables, i, marked_constants - {name})]
                is_whole, some = len(bad) == count, ([] if len(bad) == count else bad)
            else:
                is_whole, some = any(blocks.reaches(r, {}, None, marked_constants - {name}) for r in roots), []
            if whole.get(name) != is_whole or partial.get(name, []) != some:
                whole[name], partial[name] = is_whole, some
                changed = True
        if not changed:
            break
    tables_out, partial_out = [], []
    for k, (kind, prefix) in enumerate(KINDS):
        ids = sorted(value for kk, name, value, _ in objects if kk == k and whole.get(name))
        tables_out.append(ids)
    for k, name, value, _ in objects:
        if k == 0 and not whole.get(name) and partial.get(name):
            partial_out.append((value, partial[name]))
    return tables_out, sorted(partial_out)


def digest() -> str:
    """The generator's own text, by which the file it made names it."""
    return hashlib.sha256(Path(__file__).read_bytes()).hexdigest()[:16]


def render(disasm: Path) -> str:
    revision = subprocess.run(["git", "-C", str(disasm), "rev-parse", "--short", "HEAD"], capture_output=True, text=True).stdout.strip() or "unknown"
    lines = [f"/* Generated by gen_room_scenes.py ({digest()}) from oracles-disasm's sources ({revision}); do not edit. */",
             '#include "room_predict.h"', ""]
    for game in GAMES:
        tables, partial = marked(disasm, game)
        lines.append(f"const uint8_t oracles_room_scenes_{game}[3][32] = {{")
        for (kind, _), ids in zip(KINDS, tables):
            bits = [0] * 32
            for i in ids:
                bits[i >> 3] |= 1 << (i & 7)
            lines.append(f"    {{ {', '.join(f'0x{b:02x}' for b in bits)} }},   /* {kind}, every subid: {len(ids)} */")
        lines += ["};", "/* The interactions marked for some of their subids alone: the id, then a bit a subid (0 to 63). */",
                  f"const OraclesRoomSceneSubids oracles_room_scene_subids_{game}[] = {{"]
        for value, bad in partial:
            mask = 0
            for b in bad:
                if b < 64:
                    mask |= 1 << b
            lines.append(f"    {{ 0x{value:02x}, 0x{mask:016x}u }},")
        lines += ["    { 0, 0 }", "};", ""]
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--disasm", type=Path)
    ap.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "room_scenes.c")
    ap.add_argument("--check", action="store_true", help="compare with --out instead of writing it; 1 when they differ")
    args = ap.parse_args()
    if args.disasm is None or not (args.disasm / "ages.sym").exists():
        if not args.check:
            ap.error("--disasm DIR, a built oracles-disasm, is needed to generate")
        first = args.out.read_text(encoding="utf-8").split("\n", 1)[0]
        if f"gen_room_scenes.py ({digest()})" not in first:
            print(f"{args.out} was not made by this gen_room_scenes.py: regenerate it with --disasm", file=sys.stderr)
            return 1
        print(f"{args.out} names this generator (no disassembly to compare its tables with)")
        return 0
    text = render(args.disasm)
    if args.check:
        if args.out.read_text(encoding="utf-8") != text:
            print(f"{args.out} is not what gen_room_scenes.py makes from {args.disasm}: regenerate it", file=sys.stderr)
            return 1
        print(f"{args.out} is up to date")
        return 0
    args.out.write_text(text, encoding="utf-8")
    print(f"wrote {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
