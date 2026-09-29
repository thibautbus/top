# NanoSVG, vendored

`nanosvg.h` and `nanosvgrast.h` are the unmodified headers of `src/` in
<https://github.com/memononen/nanosvg>, commit `239e102ec2c691f2902e20ace2ed36ee4a35cfe6`
(SHA-256: `e34fd5d084be106cea972d19ce5d27fd96d17ba89f8d06bdceee058420c8b2b0` and
`79a9c5f4db19debf9f3a648a1589e96d92854f245a5cb4f3d823f263785234d8`).
`LICENSE.txt` is the repository's zlib licence of that commit.

The launcher parses and rasterises the three background motifs
(`launcher/ui/motifs/`) with it at the window's size. The
implementation is compiled once, in `launcher/ui/ui_impl.c`, without the
engine's warnings.

To update: replace the three files from the new commit and change this file.
