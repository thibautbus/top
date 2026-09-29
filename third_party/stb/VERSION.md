# stb_truetype, vendored

`stb_truetype.h` is the unmodified header of stb_truetype v1.26 from
<https://github.com/nothings/stb>, commit `2c980bb59875b0d32144a71867fbdebb2f77cd20`
(SHA-256 of the header: `ecd30b05e0dd4fea3a13c26810dd9e1992dc379049482c393d5a19e6b5090aab`).
`LICENSE` is the repository's licence file of that commit: public domain or
MIT, at the user's choice.

The launcher rasterises the vendored fonts (`../fonts/`) with it into glyph
atlases at the window's size. The implementation is compiled once,
in `launcher/ui/ui_impl.c`, without the engine's warnings.

To update: replace both files from the new commit and change this file.
