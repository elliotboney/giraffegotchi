#!/usr/bin/env python3
"""Convert generated giraffe art (img/*.png) into CYD-ready sprites (data/).

For each emotion: detect & key the flat background, autocrop the giraffe,
pixelate + scale to fit the 150x160 display rect, and bake the firmware's
savanna scene (sky band over golden ground) behind the giraffe so the sprite
tiles seamlessly over the on-screen scene. Source images can be any size.

The two band colors and horizon MUST match the firmware (ui.h: SKY_COLOR
0x6DBC, GROUND_COLOR 0xCD4B, HORIZON_Y 165, GIRAFFE_Y 34).

Run:  tools/prep_sprite.py            # all emotions
Tune: PIX (chunkiness), FIT_* (margin), BG_TOL (bg keying).
"""
import os
from PIL import Image, ImageChops, ImageDraw

W, H = 150, 160            # firmware display rect
FIT_W, FIT_H = 144, 153    # content box inside the rect (leaves a margin)
PX = 2                     # screen pixels per sprite pixel (chunkiness)
BG_TOL = 44                # diff vs source bg that counts as foreground

# Savanna bands — must match ui.h after 565 quantization.
SKY_RGB    = (104, 180, 224)   # 0x6DBC
GROUND_RGB = (200, 168, 88)    # 0xCD4B
HORIZON_Y  = 165               # screen y of the horizon
GIRAFFE_Y  = 34                # screen y of the sprite top
BAND_ROW   = HORIZON_Y - GIRAFFE_Y   # sprite row where ground starts (131)

EMOTIONS = ["happy", "hungry", "sad", "excited", "sleepy", "sick", "reading",
            "thirsty", "bored", "dirty"]


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


def band_canvas():
    """150x160 sky-over-ground canvas matching the firmware scene."""
    c = Image.new("RGB", (W, H), SKY_RGB)
    if BAND_ROW < H:
        c.paste(Image.new("RGB", (W, H - BAND_ROW), GROUND_RGB), (0, BAND_ROW))
    return c


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

    # composite the giraffe over the savanna bands via its alpha
    canvas = band_canvas()
    canvas.paste(chunky, ((W - dw) // 2, (H - dh) // 2), chunky)
    canvas.save(dst)


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    src_dir = os.path.join(root, "img")
    out_dir = os.path.join(root, "data")
    os.makedirs(out_dir, exist_ok=True)
    for e in EMOTIONS:
        src = os.path.join(src_dir, f"{e}.png")
        if not os.path.exists(src):
            print("skip (missing):", src)
            continue
        dst = os.path.join(out_dir, f"giraffe_{e}.png")
        prep(src, dst)
        print("wrote", dst)


if __name__ == "__main__":
    main()
