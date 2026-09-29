# JetBrains Mono, vendored

`JetBrainsMono-Regular.ttf` is unmodified from `fonts/ttf/` of the release
archive `JetBrainsMono-2.304.zip` of <https://github.com/JetBrains/JetBrainsMono>,
v2.304 (SHA-256 of the font: `a0bf60ef0f83c5ed4d7a75d45838548b1f6873372dfac88f71804491898d138f`);
`OFL.txt` is the file of tag `v2.304`. Licence: SIL Open Font License 1.1.
It comes from the release rather than from google/fonts, which carries only
the variable font: stb_truetype reads static fonts.

The launcher's monospace (key caps, paths), embedded in the `oracles` binary at build time
(`launcher/ui/ui_embed.cmake`). Its licence goes with it even embedded:
`OFL.txt` is copied beside the executable as `JetBrainsMono-OFL.txt`. To update:
replace both files and change this file.
