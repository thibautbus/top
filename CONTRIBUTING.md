# Contributing

The port runs the game unmodified and observes it; a change keeps it that way. Read [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) first, then [`docs/GAME_HOOKS.md`](docs/GAME_HOOKS.md) if your change reads or writes the game.

## The rules of the code

- **Delete before adding.** A capability that is replaced goes in the same commit as its replacement. No `v1` and `v2` side by side; no new format without the old one removed.
- **Size.** A source file stays under 800 lines, a function within a screen. Past that, split before going on.
- **One fact in one place.** An address, a format or a constant of the game lives in the reference documents ([`docs/ROM_DATA_FORMATS.md`](docs/ROM_DATA_FORMATS.md), [`docs/GAME_HOOKS.md`](docs/GAME_HOOKS.md)) or in code generated from the pinned disassembly; elsewhere, refer to it. An address of the game is never written by hand: `tools/check_guest_addresses.py` refuses it.
- **No hardening out of scope.** The tools run on the user's machine, on the user's files. No defence against hostile processes, no locks, no opening of directories component by component. The ROM's validation by hash and the refusal of invalid mods are the only security checks expected.
- **Portability.** No SDL, POSIX or system call outside the host facade (`engine/host`) and the launcher. No Python at run time: Python is for the offline tools and the checks.
- **State comparisons.** Every WRAM fingerprint excludes the dead stack bytes below each thread's SP, and only those; the threads' saved contexts are compared.
- **Hooks.** A hook reads freely, by physical address (bank and offset) and through direct access to the core, never through the guest CPU's bus. It writes only at the safe points of [`docs/GAME_HOOKS.md`](docs/GAME_HOOKS.md), section 6, through a closed transaction of the guest bus; it never touches SP, the stacks or the bank registers. The one exception is the mods' call transaction, which pushes the address of a routine from a closed list under the return address of the one `call checkReloadStatusBarGraphics` of `mainThreadStart`, SP and return address checked; nowhere else.
- **Tests.** Proportionate to the change, fast, without a ROM in the CI. A test that needs a ROM is skipped without one, never simulated.

## A change of the Enhanced view

A change of what the Enhanced view shows comes with a route that exercises it: play the case, record it, add its rows to its directory's `checks.tsv`, and replay the whole suite; every other row keeps its figures and its run hash, or the commit says which moved and why. [`docs/ROUTES.md`](docs/ROUTES.md) says how, step by step. The live state must stay identical with and without the view on every route: the presentation never writes into the game.

A change of the launcher's layout updates the values of the probes it moves in `tests/launcher_layout_reference.json` ([`docs/BUILDING.md`](docs/BUILDING.md), tests).

Adding a fan game (its profile, its manifest of addresses, its routes) is done with the maintainer: the tools that bind a fan game's table from its source are not in this repository yet.

## Commits

- One commit per coherent change, validated; no mixing of a feature, an unrelated refactoring and independent documentation.
- A title in the imperative, with a capital, without a type prefix (`Keep the ghost's key on its slates`, not `feat: ...`); a body that says why, what proves it, and the limits worth knowing.
- No trailer.
- Before committing: the tests proportionate to the change, `git diff --check`, only the files of the change staged.

## The checks the CI runs

On every pull request the CI builds and tests on Linux, macOS (Intel and Apple Silicon), Windows and Android, and runs:

- `tools/audit_tracked_files.py`: no ROM, save, build output or binary tracked, no file over 4 MiB outside the vendored trees, the vendored trees equal to their pinned hashes;
- `tools/check_public_text.py`: the text of the tracked files, in English, without local paths; `tools/check_public_text.allow` names its exceptions;
- `tools/check_docs.py`: every relative link of the documents resolves, and `README.md` stays under 150 lines;
- `tools/check_guest_addresses.py`: no guest address written by hand.

Run them before you push:

```bash
python3 tools/audit_tracked_files.py --root .
python3 tools/check_public_text.py --root .
python3 tools/check_docs.py
python3 tools/check_guest_addresses.py --root .
```
