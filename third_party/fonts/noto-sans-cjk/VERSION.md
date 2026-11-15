# Noto Sans CJK JP, subset, vendored

`NotoSansCJKjp-Subset.otf` is a subset of `Sans/OTF/Japanese/NotoSansCJKjp-Regular.otf` of
<https://github.com/notofonts/noto-cjk>, tag `Sans2.004`, commit `523d033d6cb47f4a80c58a35753646f5c3608a78`
(SHA-256 of the full font: `68a3fc98800b2a27b371f2fb79991daf3633bd89309d4ffaa6946fd587f375b5`; of the subset:
`5a1418c7dcf517fffeb04180eef758df44d33567a52daabd7ddffc0c27acc2ab`). It keeps the glyphs of 日本語 and nothing else,
made with fontTools 4.55.0:

    pyftsubset NotoSansCJKjp-Regular.otf --text="日本語" --output-file=NotoSansCJKjp-Subset.otf \
        --layout-features='' --no-hinting --desubroutinize --name-IDs='*' --name-languages='*' --notdef-outline

`OFL.txt` is the repository's `LICENSE` with the font's copyright line put on top, as google/fonts ships the Noto CJK
fonts' licence ("Copyright 2014-2021 Adobe (http://www.adobe.com/), with Reserved Font Name 'Source'."; the font's own name
table says "Copyright 2014-2021 Adobe (http://www.adobe.com/). Noto is a trademark of Google Inc.", without the reserved
name). The subset keeps the font's name, Noto Sans CJK JP, which does not hold the reserved name 'Source'.
Licence: SIL Open Font License 1.1.

The launcher's fallback for the characters its three faces do not have: a glyph missing from the face a text is drawn
in is taken from this font, at the same size, on the same baseline. Embedded in the `oracles` binary at build time
(`launcher/ui/ui_embed.cmake`). Its licence goes with it even embedded: `OFL.txt` is copied beside the executable as
`NotoSansCJK-OFL.txt`. To add characters (the Chinese and Korean fan translators' names), subset the full font again
with them added to `--text` and change this file.
