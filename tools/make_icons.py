#!/usr/bin/env python3
"""Generate every icon file of the application from the SVGs of launcher/icon/source/.

The generated files are committed, so that a build needs neither this tool nor its dependencies:

- launcher/icon/the-oracles-project.ico: 16 to 256 px (Windows);
- launcher/icon/the-oracles-project.icns: 16 to 1024 px (macOS);
- launcher/icon/the-oracles-project-256.png and -512.png (Linux desktops);
- launcher/icon/window_icon.c: the icon at 64 x 64 in RGBA, the window's icon at run time;
- android/app/src/main/res: the adaptive icon (a foreground, a background and a monochrome layer on the 108 dp
  canvas, per density) and the legacy 48 dp icon.

Each size is drawn from the source made for it: the icon's lines keep a width of about a pixel, and its details go
as it shrinks (mark-small.svg below 44 px, mark-medium.svg below 200 px, mark.svg above; the mark-macos* sources
in the Dock's shape).  The SVGs are rendered by librsvg through ctypes, whose clipping ImageMagick's own SVG
renderer lacks; ImageMagick (magick) writes the ICO and reads the window icon's pixels.

usage: tools/make_icons.py      (Linux, with librsvg 2.46 or later, cairo and ImageMagick's magick)
"""
from __future__ import annotations

import ctypes
import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "launcher" / "icon" / "source"
ICON = ROOT / "launcher" / "icon"
RES = ROOT / "android" / "app" / "src" / "main" / "res"
ICO_SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
# The ICNS entries: type, size in pixels (PNG data for every one, macOS 10.7 and later).
ICNS_ENTRIES = [("icp4", 16), ("icp5", 32), ("icp6", 64), ("ic07", 128), ("ic08", 256), ("ic09", 512), ("ic10", 1024),
                ("ic11", 32), ("ic12", 64), ("ic13", 256), ("ic14", 512)]
DENSITIES = [("mdpi", 1.0), ("hdpi", 1.5), ("xhdpi", 2.0), ("xxhdpi", 3.0), ("xxxhdpi", 4.0)]
WINDOW_ICON = 64


class RsvgRectangle(ctypes.Structure):
    _fields_ = [("x", ctypes.c_double), ("y", ctypes.c_double), ("width", ctypes.c_double), ("height", ctypes.c_double)]


def renderer():
    """librsvg and cairo, their functions typed; the tool stops when either is missing."""
    try:
        rsvg, cairo = ctypes.CDLL("librsvg-2.so.2"), ctypes.CDLL("libcairo.so.2")
    except OSError as error:
        sys.exit(f"make_icons: librsvg and cairo are needed: {error}")
    rsvg.rsvg_handle_new_from_data.restype = ctypes.c_void_p
    rsvg.rsvg_handle_new_from_data.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.POINTER(ctypes.c_void_p)]
    rsvg.rsvg_handle_render_document.restype = ctypes.c_int
    rsvg.rsvg_handle_render_document.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(RsvgRectangle), ctypes.POINTER(ctypes.c_void_p)]
    cairo.cairo_image_surface_create.restype = ctypes.c_void_p
    cairo.cairo_image_surface_create.argtypes = [ctypes.c_int, ctypes.c_int, ctypes.c_int]
    cairo.cairo_create.restype = ctypes.c_void_p
    cairo.cairo_create.argtypes = [ctypes.c_void_p]
    cairo.cairo_surface_write_to_png.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
    for name in ("cairo_destroy", "cairo_surface_destroy"):
        getattr(cairo, name).argtypes = [ctypes.c_void_p]
    ctypes.CDLL("libgobject-2.0.so.0").g_object_unref.argtypes = [ctypes.c_void_p]
    return rsvg, cairo


def render(source: str, size: int, out: Path) -> None:
    """The SVG `source` of launcher/icon/source/ drawn at size x size into the PNG `out`."""
    rsvg, cairo = renderer()
    data = (SOURCE / source).read_bytes()
    error = ctypes.c_void_p()
    handle = rsvg.rsvg_handle_new_from_data(data, len(data), ctypes.byref(error))
    if not handle:
        sys.exit(f"make_icons: {source} is not an SVG librsvg reads")
    surface = cairo.cairo_image_surface_create(0, size, size)   # CAIRO_FORMAT_ARGB32
    cr = cairo.cairo_create(surface)
    viewport = RsvgRectangle(0, 0, size, size)
    if not rsvg.rsvg_handle_render_document(handle, cr, ctypes.byref(viewport), ctypes.byref(error)):
        sys.exit(f"make_icons: {source} cannot be drawn")
    out.parent.mkdir(parents=True, exist_ok=True)
    cairo.cairo_surface_write_to_png(surface, str(out).encode())
    cairo.cairo_destroy(cr)
    cairo.cairo_surface_destroy(surface)
    ctypes.CDLL("libgobject-2.0.so.0").g_object_unref(handle)


def mark(size: int, macos: bool = False) -> str:
    """The source that draws the icon at `size` pixels: its details and line widths are made for that range."""
    prefix = "mark-macos" if macos else "mark"
    return f"{prefix}-small.svg" if size < 44 else f"{prefix}-medium.svg" if size < 200 else f"{prefix}.svg"


def magick(*args: str) -> bytes:
    return subprocess.run(["magick", *args], check=True, capture_output=True).stdout


def write_icns(entries: list[tuple[str, Path]], out: Path) -> None:
    chunks = b"".join(kind.encode() + struct.pack(">I", 8 + png.stat().st_size) + png.read_bytes() for kind, png in entries)
    out.write_bytes(b"icns" + struct.pack(">I", 8 + len(chunks)) + chunks)


def write_window_icon(png: Path) -> None:
    pixels = magick(str(png), "-depth", "8", "rgba:-")
    if len(pixels) != WINDOW_ICON * WINDOW_ICON * 4:
        sys.exit(f"make_icons: the window icon is {len(pixels)} bytes, not {WINDOW_ICON} x {WINDOW_ICON} RGBA")
    rows = [", ".join(f"0x{b:02x}" for b in pixels[i:i + 16]) for i in range(0, len(pixels), 16)]
    (ICON / "window_icon.h").write_text(
        "/* Generated by tools/make_icons.py from launcher/icon/source/mark-medium.svg; do not edit. */\n"
        "#ifndef ORACLES_WINDOW_ICON_H\n#define ORACLES_WINDOW_ICON_H\n\n"
        f"#define ORACLES_WINDOW_ICON_SIZE {WINDOW_ICON}\n\n"
        "/* The application's icon for the window, ORACLES_WINDOW_ICON_SIZE pixels square, RGBA, rows from the top. */\n"
        "extern const unsigned char oracles_window_icon[ORACLES_WINDOW_ICON_SIZE * ORACLES_WINDOW_ICON_SIZE * 4];\n\n"
        "#endif\n")
    (ICON / "window_icon.c").write_text(
        "/* Generated by tools/make_icons.py from launcher/icon/source/mark-medium.svg; do not edit. */\n"
        '#include "window_icon.h"\n\n'
        "const unsigned char oracles_window_icon[ORACLES_WINDOW_ICON_SIZE * ORACLES_WINDOW_ICON_SIZE * 4] = {\n"
        + ",\n".join("    " + row for row in rows) + "\n};\n")


ADAPTIVE = """<?xml version="1.0" encoding="utf-8"?>
<!-- Generated by tools/make_icons.py; do not edit.  The adaptive icon: its layers on the 108 dp canvas, per density. -->
<adaptive-icon xmlns:android="http://schemas.android.com/apk/res/android">
    <background android:drawable="@mipmap/ic_launcher_background"/>
    <foreground android:drawable="@mipmap/ic_launcher_foreground"/>
    <monochrome android:drawable="@mipmap/ic_launcher_monochrome"/>
</adaptive-icon>
"""


def main() -> int:
    if not shutil.which("magick"):
        sys.exit("make_icons: ImageMagick's magick is needed")
    written = []
    with tempfile.TemporaryDirectory() as tmp:
        work = Path(tmp)
        icons = []
        for size in ICO_SIZES:
            icons.append(work / f"ico-{size}.png")
            render(mark(size), size, icons[-1])
        magick(*map(str, icons), str(ICON / "the-oracles-project.ico"))
        written.append(ICON / "the-oracles-project.ico")
        entries = []
        for kind, size in ICNS_ENTRIES:
            png = work / f"icns-{size}.png"
            if not png.exists():
                render(mark(size, macos=True), size, png)
            entries.append((kind, png))
        write_icns(entries, ICON / "the-oracles-project.icns")
        written.append(ICON / "the-oracles-project.icns")
        for size in (256, 512):
            render(mark(size), size, ICON / f"the-oracles-project-{size}.png")
            written.append(ICON / f"the-oracles-project-{size}.png")
        render(mark(WINDOW_ICON), WINDOW_ICON, work / "window.png")
        write_window_icon(work / "window.png")
        written += [ICON / "window_icon.h", ICON / "window_icon.c"]
    for density, scale in DENSITIES:
        folder = RES / f"mipmap-{density}"
        for layer in ("foreground", "background", "monochrome"):
            render(f"android-{layer}.svg", round(108 * scale), folder / f"ic_launcher_{layer}.png")
            written.append(folder / f"ic_launcher_{layer}.png")
        render("android-legacy.svg", round(48 * scale), folder / "ic_launcher.png")
        written.append(folder / "ic_launcher.png")
    (RES / "mipmap-anydpi-v26").mkdir(parents=True, exist_ok=True)
    (RES / "mipmap-anydpi-v26" / "ic_launcher.xml").write_text(ADAPTIVE)
    written.append(RES / "mipmap-anydpi-v26" / "ic_launcher.xml")
    for path in written:
        print(f"{path.relative_to(ROOT)}: {path.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
