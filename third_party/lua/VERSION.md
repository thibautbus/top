# Lua, vendored

`src/` is the unmodified `src/` directory of the Lua release pinned in
`config/lua.json` (version, URL and SHA-256 of the archive, SHA-256 of the
vendored `src/` tree). `LICENSE` is the MIT licence of that release, from its
`doc/readme.html`.

Nothing in `src/` is edited: `tools/audit_tracked_files.py` recomputes the
tree hash and fails when it differs from `config/lua.json`.  The engine builds
the library files only (not `lua.c` nor `luac.c`) and fixes the string hash
seed with `-Dluai_makeseed(L)=...` on the command line, which replaces
`lstate.c`'s seed made from the clock and from addresses: a mod's tables are
laid out the same in every run.  What a mod may call is decided by
`engine/mods/mod_lua.c`, not here.

To update: replace `src/` and `LICENSE` from the new archive, then update
`config/lua.json` (the audit prints the new tree hash).
