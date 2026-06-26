# Giraffegotchi — Project Status & Handoff

_Last updated: 2026-06-26_

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
- Each sprite is **opaque 150×160** with the **savanna bands baked into its background** (sky above the horizon row, golden ground below) so it tiles seamlessly over the on-screen scene.
- **Scene constants must match** between `ui.h` (`SKY_COLOR 0x6DBC`, `GROUND_COLOR 0xCD4B`, `HORIZON_Y 165`, `GIRAFFE_Y 34`) and `prep_sprite.py` (`SKY_RGB`, `GROUND_RGB`, `HORIZON_Y`, `GIRAFFE_Y`). Change one → change both → re-run prep.
- **Background is removed by flood-fill from the image border**, NOT by color-keying every cyan pixel. This keeps interior giraffe pixels that happen to match the source bg colour (e.g. the sick giraffe's pale tint) — otherwise they become transparent holes.
- Crisp pixel-art look = **NEAREST downscale both ways** (no averaging → no soft halo). BOX/averaging caused blur earlier.

### Scene system (`ui.cpp`)
- `drawScene(tft)` paints sky band + ground band + `drawProps` (sun, 2 acacia trees, layered grass).
- `restoreBg(tft, x,y,w,h)` is the **universal erase** — fills the correct band(s) for a rect and redraws any prop it overlaps. **Every animation erases via `restoreBg`, never a flat fill.**
- `animateScenery(tft)` runs each frame: grass sway, tree sway, cloud drift, bird flap. `uiSetPhase(now)` feeds a `millis()` breeze phase used by all sway/drift.

### Animation patterns (two kinds)
1. **One-shot, over the giraffe** (eating, drinking): decode the giraffe into a RAM buffer, capture a small face box, restore that box each frame + draw the item on top. Flicker-free, no per-frame decode.
2. **Ambient / background-safe** (sleep Z's, ball, clean sparkles, grass, clouds, birds): live in regions where erasing to the scene is safe; erase old position via `restoreBg`, redraw at new position.

### GOTCHAS (cost real debugging time — don't relearn the hard way)
- **RGB565 byte order:** decode with `getLineAsRGB565(..., PNG_RGB565_LITTLE_ENDIAN, ...)` + `tft.setSwapBytes(true)`. Other combos give scrambled/negative colors.
- **`readRect` (panel readback) does NOT work on this CYD** — returns garbage. To restore pixels under an animation, decode the giraffe into a RAM buffer instead (see `renderGiraffeToBuffer`).
- **Flash-erase always covers the FULL footprint** — under-sized erase rects leave trailing lines (clouds) or stranded fragments. Clip moving sprites by their **edge**, not their center.
- **The giraffe is an opaque rectangle** — clouds/birds currently pass behind its bounding *box*, not its silhouette (see §6).
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
- **32 passing unit tests** for all pet logic.

---

## 5. Current state

- **Branch:** `main`, pushed to `git@github.com:elliotboney/giraffegotchi.git`.
- **HEAD:** `d7b0c4a feat: drifting clouds and flapping birds in the sky` (flashed & verified).
- Everything above is committed and on the device.

---

## 6. IN PROGRESS — next task (decision already made)

**Goal:** make clouds/birds pass behind the giraffe's **actual silhouette**, not its rectangular box. User chose the **full transparency** route.

This needs more than transparent art — it's a rendering rework. Plan:

1. **Art:** regenerate sprites with a single **magic transparent key colour** as the background (e.g. magenta `0xF81F`) instead of the baked sky/ground bands. Update `prep_sprite.py` to composite the giraffe over the key colour; the scene now provides the background behind the giraffe.
2. **Keep the decoded giraffe in a persistent ~48KB RAM buffer** (re-render on emotion change via the existing `renderGiraffeToBuffer`). `bgKey = buf[0]` (top-left = key colour, read from buffer → byte-order-safe).
3. **Per-frame render order:** erase old clouds/birds (→ scene) → draw clouds/birds **full-width** (no clipping) → `pushImage(giraffeBuf, ..., transparent=bgKey)` to re-stamp the giraffe on top. Magenta pixels are skipped → scene + clouds show through behind the giraffe's shape.
4. **`drawScene` must paint the bands/props behind the giraffe region** (since the sprite no longer carries baked bands).
5. **Eat/drink animation simplifies:** with the giraffe re-pushed each frame, the old food item is erased by the giraffe re-stamp — draw the item *after* the giraffe push (so it's in front of the mouth).

**Risks to budget for:** RGB565 byte-order for the transparent colour (verify on hardware — expect 1–2 reflash iterations); per-frame full-sprite `pushImage` (~10ms) cost; reordering the eat animation. Remove the cloud/bird `OCC_L/OCC_R` clipping once silhouette occlusion works.

**Cheaper fallbacks if the rework is too much:** float clouds above the giraffe's head (y25–36, no occlusion) OR tighten the occlusion box to head/neck width.

---

## 7. Backlog ideas (not started)
- **Day/night cycle** — shift sky colour over time, arc the sun, stars at night. (Pairs well with clouds.)
- **Sound** — CYD has a speaker pin (GPIO 26); beeps for actions/moods.
- **Persistence** — save stats to NVS/flash so the pet survives reboot.
- More food types; tree-canopy bird landings; etc.

---

## 8. Loose ends
- `docs/other_giraffes/` holds local reference art and is **gitignored** (kept out of history on purpose).
</content>
