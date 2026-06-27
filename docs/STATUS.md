# Giraffegotchi — Project Status & Handoff

_Last updated: 2026-06-27_

A Tamagotchi-style digital pet (a giraffe) running on an **ESP32 "Cheap Yellow Display" (CYD)**.
This doc is the single source of truth to resume work in a fresh session.

---

## 1. Hardware & toolchain

- **Board:** ESP32-2432S028R (CYD) — ILI9341 240×320, landscape (rotation 1 → 320×240). Resistive touch XPT2046.
- **Framework:** Arduino via **PlatformIO** (`platformio.ini`).
- **Display:** TFT_eSPI on **HSPI** (`-DUSE_HSPI_PORT=1`). Touch XPT2046 on its **own VSPI bus** (separate from display — critical).
- **Sprites:** PNGs on **LittleFS**, decoded with **PNGdec**.
- **Serial port:** currently `/dev/cu.usbserial-110` (was `-310`; the CH340 number changes per USB slot — always re-check with `ls /dev/cu.*usbserial*`).

### Build / flash / test commands
```bash
pio test -e native                                   # run pet-logic unit tests (32 tests)
pio run -e esp32dev                                  # compile firmware
pio run -e esp32dev -t uploadfs --upload-port <PORT> # flash sprites (data/) to LittleFS
pio run -e esp32dev -t upload   --upload-port <PORT> # flash firmware (auto-resets board)
```
- **`uploadfs` is only needed when sprites in `data/` change.** Firmware-only changes just need `upload`.
- After `uploadfs`, tap **RST** (or trigger a redraw) — the firmware reads each PNG from flash on draw.
- Port "busy or doesn't exist" almost always = board unplugged or a serial monitor holding it.

### Sprite conversion (needs Python + Pillow)
```bash
python3 -m venv /tmp/gg_venv && /tmp/gg_venv/bin/pip install pillow
/tmp/gg_venv/bin/python tools/prep_sprite.py    # img/*.png -> data/giraffe_*.png
```

---

## 2. Repo map

| File | Role |
|---|---|
| `src/pet.h` / `pet.cpp` | **Pure** game logic (no hardware). 4 stats, poop, emotion resolution. Unit-tested. |
| `src/ui.h` / `ui.cpp` | All drawing: sprites, scene, meters, buttons, animation primitives. |
| `src/main.cpp` | Orchestration: setup, loop, touch, animation state machines. |
| `test/test_pet/test_pet.cpp` | 32 native Unity tests for `Pet`. |
| `tools/prep_sprite.py` | Converts AI art (`img/*.png`) → CYD sprites (`data/giraffe_*.png`). |
| `tools/gen_giraffe.py` | Old PIL fallback art generator (superseded by AI sprites). |
| `img/*.png` | Source AI pixel-art (1254×1254), one per emotion. |
| `data/giraffe_*.png` | Final 150×160 sprites flashed to LittleFS. |

---

## 3. Architecture & hard-won gotchas

### Pet model (`pet.*`) — pure, testable
- **4 stats** (`StatId`: Hunger, Thirst, Fun, Hygiene), array-backed, all 0–100, shared decay loop.
- **Actions:** `feed/drink/play/clean/read`. Each care action clamps to 100 and resets the Excited window.
- **Poop:** spawns on a timer (`POOP_INTERVAL_MS=30000`), each spawn drops Hygiene by `POOP_SPAWN_PENALTY=15`; `clean()` removes all poop + restores Hygiene. Cap `MAX_POOP=4`.
- **Emotions** (priority order in `emotion()`): Reading > Excited > Sick(hunger or hygiene empty too long) > Sad(hunger<15) > **lowest stat below 30** → Hungry/Thirsty/Bored/Dirty (ties break by StatId order) > Sleepy(idle 60s) > Happy.
- Backward-compatible constructor `Pet(hunger, thirst=100, fun=100, hygiene=100)` keeps old tests valid.

### Sprite pipeline (`prep_sprite.py`) — read this before touching art
- Each sprite is **150×160 with a solid magenta background** (`0xF81F` → RGB `(248,0,248)`). The firmware treats that colour as a **transparency key** and skips it, so the scene (and clouds/birds) shows through behind the giraffe's actual silhouette. The bands are NO LONGER baked in — the scene provides the background.
- **Background is removed by flood-fill from the image border**, NOT by color-keying every cyan pixel. This keeps interior giraffe pixels that happen to match the source bg colour (e.g. the sick giraffe's pale tint) — otherwise they become transparent holes.
- Crisp pixel-art look = **NEAREST downscale both ways** (no averaging → no soft halo). BOX/averaging caused blur earlier.
- Magenta must not appear inside any giraffe silhouette (it would punch a hole). If a future sprite legitimately needs magenta, change the key in both `prep_sprite.py` (`MAGENTA_RGB`) and rely on `buf[0]` in firmware (top-left pixel = the key, read byte-order-safe).

### Scene system (`ui.cpp`)
- `drawScene(tft)` paints sky band + ground band + `drawProps` (sun, 2 acacia trees, layered grass).
- `restoreBg(tft, x,y,w,h)` is the **universal erase** — fills the correct band(s) for a rect and redraws any prop it overlaps. **Every animation erases via `restoreBg`, never a flat fill.**
- `animateScenery(tft)` runs each frame: grass sway, tree sway, cloud drift, bird flap. `uiSetPhase(now)` feeds a `millis()` breeze phase used by all sway/drift.

### Transparency & flicker-free silhouette occlusion (the sky-band sprite) — READ THIS
Clouds/birds pass behind the giraffe's **actual silhouette**, with no flash. The panel has **no double-buffering**, so the naive "erase old cloud → draw new cloud over giraffe → restamp giraffe" shows the half-drawn frame (cloud sitting on the giraffe) for a few ms = a visible flash. Fix: composite the affected region **off-screen** and push it in **one atomic transfer**.

- **Giraffe sprites are transparent** via the magenta key (`giraffeBuf[0]` = top-left pixel = the key). The persistent `giraffeBuf` (48 KB, `GIRAFFE_W*GIRAFFE_H`) is decoded once per emotion.
- **`skyBand`** = a `TFT_eSprite` (`GIRAFFE_W × BAND_H`, ~25 KB) covering the **top `BAND_H=84` rows** of the giraffe footprint (screen y34..118) — the only band where clouds/birds/eat-food appear. The lower body never changes per-frame so it's excluded.
- **Per engaged frame** (`composeSkyBand` + push, only when `cloudOrBirdInBox()` or eating): fill sky → draw in-box clouds/birds → **manually composite the giraffe** over them (skip magenta) → (if eating) draw the food item → `pushSprite`. One atomic write, zero flicker. One extra "cleanup" push after the last object leaves (`wasBand`).
- **Open-sky clouds/birds draw directly** to the panel, **pixel-clipped to OUTSIDE the box** via `setViewport(..., vpDatum=false)` (see `drawCloudDirect`). The band fills the in-box gap. Result is one continuous cloud across the x85/x235 seam.
- **Emotion changes / play / clean** use `updateGiraffe` (full `restoreBg` + transparent `pushGiraffe`) — owns the lower body and prevents ghosts when a silhouette shrinks (e.g. excited ears drop).

### Animation patterns (two kinds)
1. **Over the giraffe head** (eating/drinking, clouds, birds): composited into the `skyBand` sprite and pushed atomically (see above). Flicker-free, no per-frame PNG decode.
2. **Ambient / background-safe, OUTSIDE the box** (sleep Z's at x≥238, play ball at x42, clean sparkles at the poop slots, grass, trees): live where erasing to the scene is safe; erase old position via `restoreBg`, redraw at new position. Keep these clear of x85..235 so the band push never clobbers them.

### GOTCHAS (cost real debugging time — don't relearn the hard way)
- **RGB565 byte order (direct path):** decode with `getLineAsRGB565(..., PNG_RGB565_LITTLE_ENDIAN, ...)` + `tft.setSwapBytes(true)`. Other combos give scrambled/negative colors.
- **`TFT_eSprite::pushImage` has NO transparent-colour overload.** Passing a key as the last arg silently binds to the `uint8_t sbpp` (bits-per-pixel) param — the key is ignored and you get a solid (e.g. magenta) box. Composite transparency **by hand** into the sprite buffer (`getPointer()`), or use a giraffe-sprite + `pushToSprite(dst, x, y, key)`.
- **`pushSprite` outputs the buffer RAW** (it sets the parent's `swapBytes=false` internally). So pixels written by hand into the sprite buffer must be **byte-swapped** into the sprite's native order (`(p<<8)|(p>>8)`) to match `fillSprite`/`drawCloud` colours. This is why `composeSkyBand` swaps each kept giraffe pixel.
- **Splitting a moving sprite across two surfaces needs PIXEL-level clipping, not whole-object skipping.** Skipping a whole cloud puff the moment it touches the box drops the sliver still outside the box → the cloud "shrinks/restarts" at the edge. Use a viewport.
- **`readRect` (panel readback) does NOT work on this CYD** — returns garbage. Restore pixels by decoding the giraffe into RAM (`renderGiraffeToBuffer`) or recompositing the band, never by reading the panel.
- **Flash-erase always covers the FULL footprint** — under-sized erase rects leave trailing lines (clouds) or stranded fragments. Clip moving sprites by their **edge**, not their center.
- **Redrawing a big solid fill every frame flashes** (trees did this) — only redraw it when its value actually changes; thin lines (grass) are fine every frame.

### Key screen coordinates
- Giraffe sprite: `x 85–235, y 34–194` (content starts at **y38** = horn tips).
- Meters: top row, `y 8–~24`. Buttons: `y 198–236` (FEED/DRINK/PLAY/CLEAN/BOOK, 60px each).
- Horizon: `y 165`. Poop slots: left ~`x48`, right ~`x264` (lower ground). Sun: `~(288,52) r16`.

---

## 4. Features built (all working on hardware)

- **Care model:** Hunger / Thirst / Fun / Hygiene meters (top row, turn red when low) + on-screen poop.
- **5 action buttons:** FEED, DRINK, PLAY, CLEAN, BOOK(read).
- **10 emotion sprites:** happy, hungry, sad, excited, sleepy, sick, reading, thirsty, bored, dirty.
- **7 animations:** eat (apple→mouth), drink (glass→gulps), play (bouncing ball), clean (sparkle poof), sleep (rising Zzz), reading state, excited reactions.
- **Savanna scene:** sky + golden ground + sun + 2 acacia trees + **layered swaying grass** (3 depth rows, scattered, rippling breeze) + **drifting clouds & flapping birds**.
- **True silhouette occlusion:** clouds/birds pass behind the giraffe's actual shape, flicker-free, via the off-screen sky-band sprite (see §3).
- **32 passing unit tests** for all pet logic.

---

## 5. Current state

- **Branch:** `main`, pushed to `git@github.com:elliotboney/giraffegotchi.git`.
- **Transparency rework DONE** (flashed & verified): magenta-key sprites + sky-band sprite compositing. Clouds/birds occlude behind the silhouette with no flicker; eat animation composited cleanly; no ghost ears. See §3 "Transparency & flicker-free silhouette occlusion".
- Everything above is committed and on the device.

---

## 6. Backlog ideas (not started)
- **Day/night cycle** — shift sky colour over time, arc the sun, stars at night. (Pairs well with clouds; the sky-band sprite already owns the sky region.)
- **Sound** — CYD has a speaker pin (GPIO 26); beeps for actions/moods.
- **Persistence** — save stats to NVS/flash so the pet survives reboot.
- More food types; tree-canopy bird landings; etc.

---

## 7. Loose ends
- `docs/other_giraffes/` holds local reference art and is **gitignored** (kept out of history on purpose).
- **Minor:** the band re-composites every engaged frame (~4 ms `pushSprite`); fine at current frame rates. If more sky elements are added, consider only pushing changed sub-rows.
</content>
