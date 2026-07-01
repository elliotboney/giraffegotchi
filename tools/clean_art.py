#!/usr/bin/env python3
"""Clean + register source art for a species — the step BEFORE `bun prep`.

LLM-generated frames arrive with (a) a background to strip and (b) a few pixels
of drift/scale wobble between frames. This does both, IN PLACE on img/<sp>/, so
the transparent, registered frames are ready for two things at once:
  - `bun prep` (which union-crops but does NOT correct drift on its own), and
  - hand-editing: frames line up on a shared canvas, so you can copy a part from
    one frame straight onto another and it lands in the right place.

  - Originals are backed up to <dir>/.backups/<name>.png (written once; never
    overwritten, so a re-run can't clobber your true original with a clean copy).
  - Body poses (top-level *.png, minus icon): background removed +
    centroid-aligned onto one shared canvas.
  - objects/*.png and icon.png: background removed only (each is its own thing).

Background removal is a border-connected flood fill, NOT ML: the art has a hard
black outline around a flat light background (incl. ChatGPT's fake-transparent
white/gray checkerboard). The fill floods from the image edge THROUGH every
light-to-mid grey pixel and stops only at the dark outline or saturated art — so
it eats the anti-aliased grey halo right up to the black line, while interior
pink can't hole through and interior light areas (book pages) stay.

Alignment registers frames by the integer shift that MAXIMISES silhouette
overlap (FFT cross-correlation to a reference frame), NOT by centroid — a moving
tail or a held prop won't drag the body off, because the big shared body wins.

Run:    tools/clean_art.py flamingo
        tools/clean_art.py flamingo --no-align   # just strip backgrounds
Revert: tools/clean_art.py flamingo --revert     # restore every .bak, delete it
Tune:   OUTLINE_DARK / FLOOD_SAT (flood barrier), ALPHA_THRESH, MARGIN, REF_POSE.
"""
import os
import sys

OUTLINE_DARK = 55  # flood stops at pixels darker than this (the black outline)
FLOOD_SAT = 60     # flood stops at pixels more saturated than this (pink/cream art)
ALPHA_THRESH = 16  # alpha >= this counts as foreground
MARGIN = 24        # transparent padding (source px) around the union content
REF_POSE = "happy" # frame every other frame is aligned to (base idle pose)


def body_and_object_dirs(img_dir, name):
    """(body_dir, objects_dir). A species is a subfolder of img/; the legacy
    giraffe lives at top-level img/ with img/objects/ (mirrors prep_sprite)."""
    sub = os.path.join(img_dir, name)
    if os.path.isdir(sub):
        return sub, os.path.join(sub, "objects")
    if name == "giraffe":
        return img_dir, os.path.join(img_dir, "objects")
    return None, None


def backup_path(path):
    return os.path.join(os.path.dirname(path), ".backups", os.path.basename(path))


def backup_once(path):
    """Copy path -> .backups/<name>, but only the first time (preserve original)."""
    bak = backup_path(path)
    if not os.path.exists(bak):
        os.makedirs(os.path.dirname(bak), exist_ok=True)
        with open(path, "rb") as s, open(bak, "wb") as d:
            d.write(s.read())


def strip_bg(path):
    """Key out the flat light background in place (border-connected flood fill),
    backing the original up first. Interior light regions and the black outline
    are preserved because only edge-reachable bg is removed. Returns RGBA."""
    import numpy as np
    from PIL import Image, ImageDraw
    backup_once(path)
    # Flatten onto white so an already-transparent source keys the same way
    # (transparent -> white -> removed) regardless of its hidden RGB.
    src = Image.open(path).convert("RGBA")
    im = Image.alpha_composite(Image.new("RGBA", src.size, (255, 255, 255, 255)), src).convert("RGB")
    arr = np.asarray(im).astype(np.int16)
    mx, mn = arr.max(2), arr.min(2)
    cand = (mx >= OUTLINE_DARK) & (mx - mn <= FLOOD_SAT)      # floodable: not-dark + desaturated
    # .copy() detaches from the numpy buffer so floodfill can mutate in place
    flood = Image.fromarray(np.where(cand, 255, 0).astype(np.uint8), "L").copy()
    w, h = flood.size
    for xy in ((0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)):
        if flood.getpixel(xy) == 255:                        # seed from the corners
            ImageDraw.floodfill(flood, xy, 128, thresh=0)
    bg = np.asarray(flood) == 128                            # border-connected bg only
    rgba = im.convert("RGBA")
    rgba.putalpha(Image.fromarray(np.where(bg, 0, 255).astype(np.uint8), "L"))
    rgba.save(path)
    return rgba


def _best_shift(fref, mask, h, w):
    """Integer (dx,dy) to move `mask` onto the reference for max silhouette
    overlap, via FFT cross-correlation. dx>0 pushes content right/down."""
    import numpy as np
    corr = np.fft.irfft2(fref * np.conj(np.fft.rfft2(mask)), s=(h, w))
    dy, dx = np.unravel_index(int(np.argmax(corr)), corr.shape)
    if dy > h // 2:
        dy -= h                                     # wrap negative lags
    if dx > w // 2:
        dx -= w
    return dx, dy


def align_frames(files, frames):
    """Register every frame to the reference pose by the shift that maximises
    silhouette overlap (FFT cross-correlation), then re-canvas them all to one
    shared size. Overlap-based, not centroid: a moving tail or a held prop can't
    drag the body off, because the large shared body dominates the match.
    Overwrites in place. Prints each frame's shift."""
    import numpy as np
    from PIL import Image
    items = []   # (path, rgba, mask)
    for path, rgba in zip(files, frames):
        m = (np.asarray(rgba.getchannel("A")) >= ALPHA_THRESH).astype(np.float32)
        if m.any():
            items.append((path, rgba, m))
    if not items:
        return
    h, w = items[0][2].shape
    ref_i = next((i for i, (p, _, _) in enumerate(items)
                  if os.path.basename(p) == REF_POSE + ".png"), 0)
    fref = np.fft.rfft2(items[ref_i][2])
    shifts = [_best_shift(fref, m, h, w) for _, _, m in items]
    # Union of the shifted content bboxes -> shared canvas.
    ux0 = uy0 = 1 << 30
    ux1 = uy1 = -(1 << 30)
    for (_, _, m), (dx, dy) in zip(items, shifts):
        ys, xs = np.nonzero(m >= 0.5)
        ux0, uy0 = min(ux0, int(xs.min()) + dx), min(uy0, int(ys.min()) + dy)
        ux1, uy1 = max(ux1, int(xs.max()) + dx), max(uy1, int(ys.max()) + dy)
    cw, ch = ux1 - ux0 + 1 + 2 * MARGIN, uy1 - uy0 + 1 + 2 * MARGIN
    for (path, rgba, _), (dx, dy) in zip(items, shifts):
        canvas = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        canvas.alpha_composite(rgba, (dx - ux0 + MARGIN, dy - uy0 + MARGIN))
        canvas.save(path)
        print(f"    {os.path.basename(path):<16} shift ({dx:+d},{dy:+d}) -> {cw}x{ch}")


def revert(body_dir, objects_dir):
    n = 0
    for d in (body_dir, objects_dir):
        bdir = os.path.join(d, ".backups") if d else None
        if not bdir or not os.path.isdir(bdir):
            continue
        for f in os.listdir(bdir):
            if not f.endswith(".png"):
                continue
            os.replace(os.path.join(bdir, f), os.path.join(d, f))   # move back, consuming the backup
            print(f"    restored {f}")
            n += 1
        if not os.listdir(bdir):                                    # drop the empty .backups dir
            os.rmdir(bdir)
    print(f"  reverted {n} file(s)")


def clean_species(name, img_dir, do_align):
    body_dir, objects_dir = body_and_object_dirs(img_dir, name)
    if not body_dir:
        print(f"unknown species '{name}' (no img/{name}/ and not legacy giraffe)")
        sys.exit(1)

    print(f"Cleaning '{name}':")
    body_files, body_rgba = [], []
    for f in sorted(os.listdir(body_dir)):
        if not f.endswith(".png") or f.endswith(".bak"):
            continue
        path = os.path.join(body_dir, f)
        rgba = strip_bg(path)
        print(f"    bg-removed {f}")
        pose = f[:-4]
        if pose not in ("icon", "dead") and not pose.startswith("kick"):  # standalone poses (matches prep's is_aligned_pose) — bg-removed but not nudged
            body_files.append(path)
            body_rgba.append(rgba)

    if do_align and body_files:
        print("  aligning body frames to a shared canvas:")
        align_frames(body_files, body_rgba)

    if os.path.isdir(objects_dir):
        for f in sorted(os.listdir(objects_dir)):
            if not f.endswith(".png") or f.endswith(".bak"):
                continue
            strip_bg(os.path.join(objects_dir, f))
            print(f"    bg-removed objects/{f}")
    print("  done. Eyeball img/, then `bun prep " + name + "`. Revert: --revert")


def main():
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    img_dir = os.path.join(root, "img")
    args = sys.argv[1:]
    names = [a for a in args if not a.startswith("--")]
    if not names:
        print("usage: clean_art.py <species> [--no-align] [--revert]")
        sys.exit(1)
    name = names[0]
    if "--revert" in args:
        body_dir, objects_dir = body_and_object_dirs(img_dir, name)
        if not body_dir:
            print(f"unknown species '{name}'")
            sys.exit(1)
        print(f"Reverting '{name}':")
        revert(body_dir, objects_dir)
        return
    clean_species(name, img_dir, do_align="--no-align" not in args)


if __name__ == "__main__":
    main()
