# SameBoy, vendored

`Core/` is the unmodified `Core/` directory of the SameBoy release pinned in
`config/sameboy.json` (version, tag, commit, SHA-256 of the archive, SHA-256
of the vendored `Core/` tree). `LICENSE` is the Expat (MIT) licence of that
release.

Nothing in `Core/` is edited: `tools/audit_tracked_files.py`
recomputes the tree hash and fails when it differs from `config/sameboy.json`.
The version string reaches the build (`GB_VERSION`) and every composite
savestate (`oracles_core_version()`) from that file only.

To update: replace `Core/` and `LICENSE` from the new archive, regenerate
`engine/core/cgb_boot_rom.c` from the new build's `build/bin/BootROMs/cgb_boot.bin`
with `engine/core/gen_boot_rom.py`, then update `config/sameboy.json` (the
audit prints the new tree hash), and say so in the commit that brings it.
