#!/usr/bin/env python3
"""Convert generated giraffe art (img/*.png) into CYD-ready sprites (data/).

For each emotion: detect & key the flat background, autocrop the giraffe,
pixelate + scale to fit the 150x160 display rect, and composite it over a
solid magenta (0xF81F) background. The firmware uses that colour as a
transparency key when pushing the sprite, so the scene shows through behind
the giraffe silhouette. Source images can be any size.

Run:  tools/prep_sprite.py            # all emotions
Tune: PIX (chunkiness), FIT_* (margin), BG_TOL (bg keying).
"""
import os
from PIL import Image, ImageChops, ImageDraw

W, H = 150, 160            # firmware display rect
FIT_W, FIT_H = 144, 153    # content box inside the rect (leaves a margin)
PX = 2                     # screen pixels per sprite pixel (chunkiness)
BG_TOL = 44                # diff vs source bg that counts as foreground

# Transparent key colour baked into every sprite background.
# Firmware reads buf[0] as the key, so this only needs to be a colour that
# won't appear inside any giraffe silhouette. RGB888 for RGB565 0xF81F.
MAGENTA_RGB = (248, 0, 248)

EMOTIONS = ["happy", "hungry", "sad", "excited", "sleepy", "sick", "reading",
            "thirsty", "bored", "dirty"]

# Extra animation frames (not pet emotions): alternate happy faces, kick poses,
# the 3-frame idle blink, and the ear/tail idle tics.
FRAMES = ["happy2", "happy3", "kick1", "kick2", "blink", "blink2", "blink3",
          "ears_up", "ears_down", "tail_left"]

# Square props from img/objects/ -> data/<name>.png. (name, sprite size px).
OBJECTS = [("beach_ball", 80)]


def corner_key(im):
    px = im.load()
    w, h = im.size
    pts = [(2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3)]
    r = sum(px[x, y][0] for x, y in pts) // 4
    g = sum(px[x, y][1] for x, y in pts) // 4
    b = sum(px[x, y][2] for x, y in pts) // 4
    return (r, g, b)


def foreground_mask(im, key, tol):
    """255 = giraffe, 0 = background. Only background CONNECTED to the image
    border is removed, so interior pixels that happen to match the source bg
    colour (e.g. the sick giraffe's pale tint) stay part of the giraffe."""
    diff = ImageChops.difference(im, Image.new("RGB", im.size, key)).convert("L")
    bgcol = diff.point(lambda p: 255 if p <= tol else 0)   # 255 = bg-coloured
    flood = bgcol.copy()
    for xy in [(0, 0), (im.width - 1, 0), (0, im.height - 1), (im.width - 1, im.height - 1)]:
        if flood.getpixel(xy) == 255:
            ImageDraw.floodfill(flood, xy, 128, thresh=0)  # mark border-connected bg
    # giraffe = everything the flood did NOT reach (giraffe + enclosed holes)
    return flood.point(lambda p: 0 if p == 128 else 255)


def prep(src, dst):
    im = Image.open(src).convert("RGB")
    key = corner_key(im)

    # isolate the giraffe with a hard alpha mask, autocrop
    fg = foreground_mask(im, key, BG_TOL)
    bbox = fg.getbbox()
    rgba = im.convert("RGBA")
    rgba.putalpha(fg)
    crop = rgba.crop(bbox)

    # choose a low logical (sprite-pixel) resolution so PX-sized blocks fill
    # the content box, preserving aspect
    cw, ch = crop.size
    lh = max(1, FIT_H // PX)
    lw = max(1, round(lh * cw / ch))
    if lw > FIT_W // PX:
        lw = max(1, FIT_W // PX)
        lh = max(1, round(lw * ch / cw))

    # nearest-neighbour BOTH ways: hard edges, no halo, alpha stays 0/255
    small = crop.resize((lw, lh), Image.NEAREST)
    chunky = small.resize((lw * PX, lh * PX), Image.NEAREST)
    dw, dh = chunky.size

    # composite the giraffe over a solid magenta key colour
    canvas = Image.new("RGB", (W, H), MAGENTA_RGB)
    canvas.paste(chunky, ((W - dw) // 2, (H - dh) // 2), chunky)
    canvas.save(dst)


def prep_object(src, dst, size, margin=2):
    """Same bg-keying as prep(), but output a square `size` sprite (used for
    props like the beach ball) over the magenta key."""
    im = Image.open(src).convert("RGB")
    key = corner_key(im)
    fg = foreground_mask(im, key, BG_TOL)
    rgba = im.convert("RGBA")
    rgba.putalpha(fg)
    crop = rgba.crop(fg.getbbox())

    fit = size - 2 * margin
    cw, ch = crop.size
    lw = max(1, round(cw * fit / max(cw, ch) / PX))
    lh = max(1, round(ch * fit / max(cw, ch) / PX))
    chunky = crop.resize((lw, lh), Image.NEAREST).resize((lw * PX, lh * PX), Image.NEAREST)
    dw, dh = chunky.size

    canvas = Image.new("RGB", (size, size), MAGENTA_RGB)
    canvas.paste(chunky, ((size - dw) // 2, (size - dh) // 2), chunky)
    canvas.save(dst)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_dir = os.path.join(root, "img")
    out_dir = os.path.join(root, "data")
    os.makedirs(out_dir, exist_ok=True)
    for e in EMOTIONS + FRAMES:
        src = os.path.join(src_dir, f"{e}.png")
        if not os.path.exists(src):
            print("skip (missing):", src)
            continue
        dst = os.path.join(out_dir, f"giraffe_{e}.png")
        prep(src, dst)
        print("wrote", dst)
    for name, size in OBJECTS:
        src = os.path.join(src_dir, "objects", f"{name}.png")
        if not os.path.exists(src):
            print("skip (missing):", src)
            continue
        dst = os.path.join(out_dir, f"{name}.png")
        prep_object(src, dst, size)
        print("wrote", dst)


if __name__ == "__main__":
    main()
