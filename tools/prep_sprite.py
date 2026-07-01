#!/usr/bin/env python3
"""Convert per-species source art into CYD-ready sprites.

Layout (AD-14): each animal owns a source folder and a flat output folder.

  img/<animal>/*.png            body poses  ->  data/<animal>/<pose>.png
  img/<animal>/objects/*.png    props/food  ->  data/<animal>/<name>.png
  img/<animal>/icon.png         picker icon ->  data/<animal>/icon.png (small)

The legacy giraffe art at the top level (img/*.png + img/objects/) is still
picked up as the "giraffe" species, so it need not be moved.

Keying: source art is TRANSPARENT-background. Transparent (alpha) pixels become
the magenta key (0xF81F); the firmware treats that colour as transparent when
pushing the sprite. Edges stay hard (1-bit alpha, nearest downscale). Opaque
legacy art with no alpha falls back to border-connected background keying.

A despeckle pass drops stray specks so a few loose edge pixels don't show as
dots around the silhouette.

Run:  tools/prep_sprite.py            # all species
      tools/prep_sprite.py groundhog  # one species (won't touch others)
Tune: PX (chunkiness), FIT_* (margin), ALPHA_THRESH / BG_TOL (keying), MIN_BLOB.
"""
import os
import sys
from PIL import Image, ImageChops, ImageDraw

W, H = 150, 160            # default body display rect
BODY_SIZES = {             # per-species body rect (W, H) — must match descriptor geom
    "groundhog": (150, 150),   # squat/square, distinct from the giraffe (NFR3)
}
BODY_MARGIN = 3            # content margin per side inside the body rect (~ old 144x153 fit)
PX = 2                     # screen pixels per sprite pixel (chunkiness)
ALPHA_THRESH = 128         # source alpha >= this counts as foreground (1-bit key)
BG_TOL = 44                # opaque-art fallback: diff vs bg that counts as fg
MIN_BLOB = 4               # despeckle: drop fg components smaller than this (low-res px)

# Transparent key colour baked into every sprite background. Firmware reads
# buf[0] as the key. RGB888 for RGB565 0xF81F. Keep in lockstep with the
# firmware key (AD-6).
MAGENTA_RGB = (248, 0, 248)

ICON_PX = 64               # square size for a species' picker icon
OBJECT_DEFAULT = 48        # square size for an unlisted prop
OBJECT_SIZES = {           # per-prop square sizes
    "beach_ball": 80,
    "food": 40,
    "kite": 56,
}

# LittleFS partition budget (default esp32 4MB table -> 0x170000). Refined/
# enforced in Story 5.3; here we just report against it.
FS_BUDGET = 0x170000       # 1,507,328 bytes


def corner_key(im):
    px = im.load()
    w, h = im.size
    pts = [(2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3)]
    return tuple(sum(px[x, y][c] for x, y in pts) // 4 for c in range(3))


def bg_mask(rgb, key, tol):
    """Opaque-art fallback: 255 = foreground. Only background CONNECTED to the
    border is removed, so interior pixels matching the bg colour stay."""
    diff = ImageChops.difference(rgb, Image.new("RGB", rgb.size, key)).convert("L")
    bgcol = diff.point(lambda p: 255 if p <= tol else 0)
    flood = bgcol.copy()
    for xy in [(0, 0), (rgb.width - 1, 0), (0, rgb.height - 1), (rgb.width - 1, rgb.height - 1)]:
        if flood.getpixel(xy) == 255:
            ImageDraw.floodfill(flood, xy, 128, thresh=0)
    return flood.point(lambda p: 0 if p == 128 else 255)


def load_mask(rgba):
    """Hard 0/255 foreground mask. Prefer the alpha channel (transparent art);
    fall back to border-bg keying for fully opaque legacy art."""
    alpha = rgba.getchannel("A")
    lo, _ = alpha.getextrema()
    if lo < 250:                                   # has real transparency
        return alpha.point(lambda p: 255 if p >= ALPHA_THRESH else 0)
    return bg_mask(rgba.convert("RGB"), corner_key(rgba.convert("RGB")), BG_TOL)


def despeckle(rgba, min_blob):
    """Zero the alpha of foreground components smaller than min_blob pixels.
    Runs on the low-res sprite, so it's cheap and removes stray edge specks."""
    a = rgba.getchannel("A")
    px = a.load()
    w, h = a.size
    seen = bytearray(w * h)
    for sy in range(h):
        for sx in range(w):
            if px[sx, sy] < ALPHA_THRESH or seen[sy * w + sx]:
                continue
            comp, stack = [], [(sx, sy)]           # flood this component
            seen[sy * w + sx] = 1
            while stack:
                x, y = stack.pop()
                comp.append((x, y))
                for nx, ny in ((x-1, y), (x+1, y), (x, y-1), (x, y+1),
                               (x-1, y-1), (x+1, y-1), (x-1, y+1), (x+1, y+1)):
                    if 0 <= nx < w and 0 <= ny < h and not seen[ny * w + nx] \
                       and px[nx, ny] >= ALPHA_THRESH:
                        seen[ny * w + nx] = 1
                        stack.append((nx, ny))
            if len(comp) < min_blob:               # speck -> erase
                for x, y in comp:
                    px[x, y] = 0
    rgba.putalpha(a)
    return rgba


def _keyed_canvas(crop, fit_w, fit_h, out_w, out_h):
    """crop (RGBA, hard alpha) -> pixelated + despeckled + magenta-keyed canvas."""
    cw, ch = crop.size
    lh = max(1, fit_h // PX)
    lw = max(1, round(lh * cw / ch))
    if lw > fit_w // PX:
        lw = max(1, fit_w // PX)
        lh = max(1, round(lw * ch / cw))
    small = despeckle(crop.resize((lw, lh), Image.NEAREST), MIN_BLOB)
    chunky = small.resize((lw * PX, lh * PX), Image.NEAREST)
    canvas = Image.new("RGB", (out_w, out_h), MAGENTA_RGB)
    canvas.paste(chunky, ((out_w - chunky.width) // 2, (out_h - chunky.height) // 2), chunky)
    return canvas


def prep(src, dst, out_w, out_h, margin=0):
    rgba = Image.open(src).convert("RGBA")
    rgba.putalpha(load_mask(rgba))
    crop = rgba.crop(rgba.getchannel("A").getbbox())
    _keyed_canvas(crop, out_w - 2 * margin, out_h - 2 * margin, out_w, out_h).save(dst)


def species_sources(img_dir):
    """Map species name -> (body_dir, objects_dir). Subfolders of img/ are
    species; top-level *.png (legacy giraffe) is the 'giraffe' species."""
    out = {}
    for name in sorted(os.listdir(img_dir)):
        p = os.path.join(img_dir, name)
        if os.path.isdir(p) and name != "objects":
            out[name] = (p, os.path.join(p, "objects"))
    if any(f.endswith(".png") for f in os.listdir(img_dir)):
        out.setdefault("giraffe", (img_dir, os.path.join(img_dir, "objects")))
    return out


def prep_species(name, body_dir, objects_dir, out_root):
    out_dir = os.path.join(out_root, name)
    os.makedirs(out_dir, exist_ok=True)
    bw, bh = BODY_SIZES.get(name, (W, H))
    n = 0
    for f in sorted(os.listdir(body_dir)):
        if not f.endswith(".png"):
            continue
        pose = f[:-4]
        dst = os.path.join(out_dir, f)
        if pose == "icon":
            prep(os.path.join(body_dir, f), dst, ICON_PX, ICON_PX, margin=2)
        else:
            prep(os.path.join(body_dir, f), dst, bw, bh, margin=BODY_MARGIN)
        n += 1
    if os.path.isdir(objects_dir):
        for f in sorted(os.listdir(objects_dir)):
            if not f.endswith(".png"):
                continue
            size = OBJECT_SIZES.get(f[:-4], OBJECT_DEFAULT)
            prep(os.path.join(objects_dir, f), os.path.join(out_dir, f), size, size, margin=2)
            n += 1
    print(f"  {name}: {n} sprites -> {out_dir}")


def report_budget(out_root):
    print("\nFlash budget (LittleFS):")
    total = 0
    for name in sorted(os.listdir(out_root)):
        d = os.path.join(out_root, name)
        if not os.path.isdir(d):
            continue
        sz = sum(os.path.getsize(os.path.join(d, f)) for f in os.listdir(d))
        total += sz
        print(f"  {name:<12} {sz:>8,} B")
    pct = 100.0 * total / FS_BUDGET
    print(f"  {'TOTAL':<12} {total:>8,} B  ({pct:.1f}% of {FS_BUDGET:,} B partition)")
    if total > FS_BUDGET:
        print("  !! OVER BUDGET — sprites won't fit the LittleFS partition")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    img_dir = os.path.join(root, "img")
    out_root = os.path.join(root, "data")
    os.makedirs(out_root, exist_ok=True)

    sources = species_sources(img_dir)
    only = sys.argv[1] if len(sys.argv) > 1 else None
    if only and only not in sources:
        print(f"unknown species '{only}'. known: {', '.join(sorted(sources))}")
        return
    targets = {only: sources[only]} if only else sources

    print("Preparing sprites:")
    for name, (body, objects) in targets.items():
        prep_species(name, body, objects, out_root)
    report_budget(out_root)


if __name__ == "__main__":
    main()
