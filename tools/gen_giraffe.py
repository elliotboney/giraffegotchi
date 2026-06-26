#!/usr/bin/env python3
"""Generate the giraffe pet art as PNGs for the CYD (LittleFS).

Renders at 4x and downsamples with LANCZOS for anti-aliased edges.
Background is baked to match the firmware's BG_COLOR (0xAEDC -> (168,216,224))
so the image tiles seamlessly over the screen fill.

Run:  tools/gen_giraffe.py   (outputs to data/)
Swap in nicer art any time — the firmware just decodes whatever PNG is here.
"""
import os
from PIL import Image, ImageDraw

S = 4                    # supersample factor
W, H = 150, 160          # final image size (fits between hunger bar and button)
BG = (168, 216, 224)     # == BG_COLOR 0xAEDC after 565 quantization

BODY_H  = (242, 196, 92)   # happy: golden
BODY_U  = (210, 198, 150)  # hungry: pale/dull
SPOT    = (170, 110, 60)
OUTLINE = (120, 78, 40)
HOOF    = (90, 60, 35)
SNOUT_H = (236, 210, 150)
SNOUT_U = (210, 200, 168)
WHITE   = (255, 255, 255)
BLACK   = (45, 33, 28)
BLUSH   = (240, 150, 140)
SWEAT   = (130, 205, 240)


def make(mood):
    img = Image.new("RGB", (W * S, H * S), BG)
    d = ImageDraw.Draw(img)
    def P(*xs):
        return tuple(int(v * S) for v in xs)
    ow = 3 * S
    body = BODY_H if mood == "happy" else BODY_U
    snout = SNOUT_H if mood == "happy" else SNOUT_U

    # legs + hooves
    for lx in (40, 60, 86, 108):
        d.rounded_rectangle(P(lx, 138, lx + 16, 158), 6 * S, body, OUTLINE, ow)
        d.rounded_rectangle(P(lx, 150, lx + 16, 158), 5 * S, HOOF, OUTLINE, 2 * S)

    # tail
    d.line([P(32, 116), P(16, 148)], fill=OUTLINE, width=ow)
    d.ellipse(P(11, 144, 24, 158), fill=SPOT, outline=OUTLINE, width=2 * S)

    # body
    d.rounded_rectangle(P(30, 100, 128, 150), 24 * S, body, OUTLINE, ow)

    # neck
    d.polygon([P(88, 110), P(112, 110), P(122, 56), P(100, 50)], fill=body)
    d.line([P(88, 110), P(100, 50)], fill=OUTLINE, width=ow)
    d.line([P(112, 110), P(122, 56)], fill=OUTLINE, width=ow)

    # head + snout
    d.rounded_rectangle(P(96, 34, 140, 70), 14 * S, body, OUTLINE, ow)
    d.rounded_rectangle(P(122, 50, 148, 68), 8 * S, snout, OUTLINE, 2 * S)
    d.ellipse(P(141, 55, 146, 60), fill=OUTLINE)
    d.ellipse(P(141, 62, 146, 67), fill=OUTLINE)

    # ear
    d.ellipse(P(88, 40, 104, 54), body, OUTLINE, 2 * S)

    # ossicones
    d.line([P(108, 34), P(106, 18)], fill=OUTLINE, width=ow)
    d.ellipse(P(100, 11, 114, 25), SPOT, OUTLINE, 2 * S)
    d.line([P(126, 34), P(130, 18)], fill=OUTLINE, width=ow)
    d.ellipse(P(124, 11, 138, 25), SPOT, OUTLINE, 2 * S)

    # spots
    for sx, sy, sr in [(50, 118, 8), (78, 134, 10), (102, 122, 7), (104, 88, 6)]:
        d.ellipse(P(sx - sr, sy - sr, sx + sr, sy + sr), fill=SPOT)

    # face
    if mood == "happy":
        d.ellipse(P(106, 44, 117, 55), WHITE, OUTLINE, 2 * S)
        d.ellipse(P(110, 47, 115, 52), BLACK)
        d.ellipse(P(102, 56, 113, 62), BLUSH)
        d.arc(P(110, 52, 132, 68), start=10, end=80, fill=OUTLINE, width=ow)
    else:
        d.line([P(106, 50), P(118, 50)], fill=OUTLINE, width=ow)
        d.arc(P(106, 46, 118, 56), start=200, end=340, fill=OUTLINE, width=2 * S)
        d.arc(P(110, 58, 132, 74), start=285, end=355, fill=OUTLINE, width=ow)
        d.ellipse(P(120, 36, 130, 49), SWEAT, (80, 160, 200), 2 * S)

    return img.resize((W, H), Image.LANCZOS)


def main():
    here = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    out = os.path.join(here, "data")
    os.makedirs(out, exist_ok=True)
    for mood in ("happy", "hungry"):
        p = os.path.join(out, f"giraffe_{mood}.png")
        make(mood).save(p)
        print("wrote", p)


if __name__ == "__main__":
    main()
