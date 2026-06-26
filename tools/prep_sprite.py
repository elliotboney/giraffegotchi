#!/usr/bin/env python3
"""Convert generated giraffe art (img/*.png) into CYD-ready sprites (data/).

For each emotion: detect & key the flat background, autocrop the giraffe,
pixelate + scale to fit the 150x160 display rect, and bake the firmware's
exact background color (0xAEDC) so the sprite tiles seamlessly over the
screen fill. Source images can be any size (e.g. 1254x1254).

Run:  tools/prep_sprite.py            # all emotions
Tune: PIX (chunkiness), FIT_* (margin), BG_TOL (bg keying).
"""
import os
from PIL import Image, ImageChops

W, H = 150, 160            # firmware display rect
FIT_W, FIT_H = 142, 152    # content box inside the rect (leaves a margin)
PIX = 2                    # chunkiness: 1 = finest, higher = blockier
BG = (168, 216, 224)       # == BG_COLOR 0xAEDC after 565 quantization
BG_TOL = 26                # per-pixel diff vs bg color that still counts as bg

EMOTIONS = ["happy", "hungry", "sad", "excited", "sleepy", "sick"]


def corner_key(im):
    px = im.load()
    w, h = im.size
    pts = [(2, 2), (w - 3, 2), (2, h - 3), (w - 3, h - 3)]
    r = sum(px[x, y][0] for x, y in pts) // 4
    g = sum(px[x, y][1] for x, y in pts) // 4
    b = sum(px[x, y][2] for x, y in pts) // 4
    return (r, g, b)


def mask_vs(im, color, tol):
    diff = ImageChops.difference(im, Image.new("RGB", im.size, color)).convert("L")
    return diff.point(lambda p: 255 if p > tol else 0)


def prep(src, dst):
    im = Image.open(src).convert("RGB")
    key = corner_key(im)

    # isolate the giraffe, autocrop, composite onto a flat bg (kills the halo)
    fg = mask_vs(im, key, BG_TOL)
    bbox = fg.getbbox()
    crop = Image.composite(im, Image.new("RGB", im.size, BG), fg).crop(bbox)

    # scale to fit the content box, preserving aspect
    cw, ch = crop.size
    s = min(FIT_W / cw, FIT_H / ch)
    dw, dh = max(1, round(cw * s)), max(1, round(ch * s))

    # pixelate: average down, then nearest up -> crisp chunky pixels
    small = crop.resize((max(1, dw // PIX), max(1, dh // PIX)), Image.BOX)
    chunky = small.resize((dw, dh), Image.NEAREST)

    # center on the final canvas, then snap any near-bg pixels to exact bg
    canvas = Image.new("RGB", (W, H), BG)
    canvas.paste(chunky, ((W - dw) // 2, (H - dh) // 2))
    m = mask_vs(canvas, BG, 18)
    canvas = Image.composite(canvas, Image.new("RGB", (W, H), BG), m)

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
