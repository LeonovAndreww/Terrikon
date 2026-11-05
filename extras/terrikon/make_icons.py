"""Builds the Terrikon icon set: master SVGs, PNGs for every size (rendered by
headless Edge) and a multi-size .ico with PNG entries.

Usage: python make_icons.py [output folder]
Then copy terrikon-{16,32,48,128,256}.png to share/icons/<size>x<size>/vlc.png,
terrikon-512.png to share/vlc512x512.png, terrikon.ico to share/icons/vlc.ico,
terrikon-large.svg to share/icons/terrikon.svg and terrikon-cone.svg to
modules/gui/qt/pixmaps/misc/cone.svg. The build does not track these images:
delete the generated Qt assets and bin/vlc_win32_rc.o before rebuilding."""
import os
import struct
import subprocess
import sys
import tempfile

OUT = sys.argv[1] if len(sys.argv) > 1 else r"E:\VLC-fork\design\icon"
EDGE = r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"
TILE = "#141414"   # anthracite
HEAP = "#ffffff"

HEAP_PATH = "M22 192 L108 106 L148 106 L234 192 Z"
# A thin step from the heap's flat top (108..148) up to the winch house, with
# gentler sides (about 37 degrees) than the heap's 45 degree slopes; its top
# corners are the house's bottom corners. The house is square at the bottom
# with small 45 degree chamfers on top; the left one lies on the rail's upper
# edge (y = -x + 202), so the rail runs straight into it.
HOUSE = ('<path d="M108 106 L116 100 L140 100 L148 106 Z"/>'
         '<path d="M116 100 L116 86 L118 84 L138 84 L140 86 L140 100 Z"/>')
GULLIES = (f'<path d="M164 192 L172 192 L150 124 Z" fill="{TILE}"/>'
           f'<path d="M196 192 L203 192 L178 146 Z" fill="{TILE}"/>')
# Rail between y = -x + 202 and y = -x + 209, parallel to the left slope
# (y = -x + 214). It is a filled band with vertical ends: the lower end is
# flush with the foot trestle's left side, the upper end with the house's
# left wall, below its chamfer. The trestles share the rail's upper edge and
# run into the ground or the slope, so the shapes meet without seams or stubs.
RAIL = (f'<g fill="{HEAP}">'
        f'<path d="M32 170 L116 86 L116 93 L32 177 Z"/>'
        f'<path d="M32 170 L40 162 L40 192 L32 192 Z"/>'
        f'<path d="M62 140 L70 132 L70 148 L62 156 Z"/></g>')
RAIL_SMALL = f'<path d="M30 174 L114 90" fill="none" stroke="{HEAP}" stroke-width="16"/>'


def silhouette(gullies):
    """The whole white shape as one path, so anti-aliasing never meets itself
    along shared edges (which shows as seams or half-pixel bumps).

    Outer contour, clockwise from the heap's left foot: up the slope to the
    foot trestle, up to the rail, along the rail's upper edge (y = -x + 202)
    straight into the house's left chamfer, over the house, down its right
    wall, the gentle step, the right slope, then back along the ground,
    notching the erosion gullies. The two dark windows between the rail's
    lower edge (y = -x + 209), the slope (y = -x + 214) and the trestles are
    holes (even-odd)."""
    ground = ("L203 192 L178 146 L196 192 L172 192 L150 124 L164 192 " if gullies else "")
    outer = ("M22 192 L32 182 L32 170 L118 84 L138 84 L140 86 L140 100 "
             "L148 106 L234 192 " + ground + "Z")
    holes = ("M40 169 L62 147 L62 152 L40 174 Z "
             "M70 139 L116 93 L116 100 L108 106 L70 144 Z")
    return outer + " " + holes


def svg(tier):
    """tier: 'large' (48 px and up), 'medium' (32 px), 'small' (24 px and below)"""
    if tier == "small":
        shape = f'<path d="{HEAP_PATH}" fill="{HEAP}"/>{RAIL_SMALL}'
    else:
        shape = (f'<path d="{silhouette(tier == "large")}" fill="{HEAP}" '
                 'fill-rule="evenodd"/>')
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 256 256">'
            f'<rect width="256" height="256" rx="56" fill="{TILE}"/>'
            f'<g transform="translate(0 -12)">{shape}</g></svg>')


def cone_svg():
    """The bare silhouette for use inside the interface, in a square view box.
    #FF8800 is the colour the Qt interface replaces with a theme colour."""
    return ('<svg xmlns="http://www.w3.org/2000/svg" viewBox="18 16 220 220">'
            '<g transform="translate(0 -12)">'
            f'<path d="{silhouette(True)}" fill="#FF8800" fill-rule="evenodd"/>'
            '</g></svg>')


def tier_for(size):
    return "large" if size >= 48 else "medium" if size >= 32 else "small"


def render(size, dest):
    sized = svg(tier_for(size)).replace("<svg ", '<svg width="%d" height="%d" ' % (size, size), 1)
    html = ('<!doctype html><html><body style="margin:0;background:transparent">'
            f'<div style="width:{size}px;height:{size}px">{sized}</div>'
            '</body></html>')
    with tempfile.NamedTemporaryFile("w", suffix=".html", delete=False, encoding="utf-8") as f:
        f.write(html)
        page = f.name
    try:
        subprocess.run([EDGE, "--headless=new", "--disable-gpu", "--hide-scrollbars",
                        "--force-device-scale-factor=1", "--default-background-color=00000000",
                        f"--window-size={size},{size}", f"--screenshot={dest}",
                        "file:///" + page.replace("\\", "/")],
                       check=True, capture_output=True, timeout=60)
    finally:
        os.remove(page)


def write_ico(entries, dest):
    """entries: [(size, png bytes)]; PNG-compressed ICO entries (Vista and later)"""
    header = struct.pack("<HHH", 0, 1, len(entries))
    offset = 6 + 16 * len(entries)
    directory, data = b"", b""
    for size, png in entries:
        dim = 0 if size >= 256 else size
        directory += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset + len(data))
        data += png
    with open(dest, "wb") as f:
        f.write(header + directory + data)


def main():
    os.makedirs(OUT, exist_ok=True)
    for tier in ("large", "medium", "small"):
        with open(os.path.join(OUT, f"terrikon-{tier}.svg"), "w", encoding="utf-8") as f:
            f.write(svg(tier))
    with open(os.path.join(OUT, "terrikon-cone.svg"), "w", encoding="utf-8") as f:
        f.write(cone_svg())

    sizes = [16, 24, 32, 48, 64, 128, 256, 512]
    entries = []
    for size in sizes:
        dest = os.path.join(OUT, f"terrikon-{size}.png")
        render(size, dest)
        with open(dest, "rb") as f:
            png = f.read()
        if size <= 256:
            entries.append((size, png))
        print(size, len(png), "bytes")
    write_ico(entries, os.path.join(OUT, "terrikon.ico"))
    print("ico", os.path.getsize(os.path.join(OUT, "terrikon.ico")), "bytes")


if __name__ == "__main__":
    sys.exit(main())
