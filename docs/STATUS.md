# Giraffegotchi — Project Status & Handoff

_Last updated: 2026-06-29_

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
| `tools/prep_sprite.py` | Converts AI art (`img/*.png`) → CYD sprites (`data/`). `EMOTIONS` + `FRAMES` (happy2/3, kick1/2) → 150×160 giraffe sprites; `OBJECTS` (beach_ball) → square prop sprites via `prep_object()`. |
| `tools/gen_giraffe.py` | Old PIL fallback art generator (superseded by AI sprites). |
| `img/*.png` | Source AI pixel-art (1254×1254): emotions + extra frames (happy2/3, kick1/2). |
| `img/objects/*.png` | Source art for props (beach_ball, kite). |
| `data/giraffe_*.png` | Final 150×160 giraffe sprites (magenta key) flashed to LittleFS. |
| `data/beach_ball.png` | 80×80 prop sprite (magenta key). |
| `docs/PET_PROMPT.md` | The AI image-gen prompt used to make new giraffe sprites (keep style consistent). |

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

### The compositing band (`skyBand`) — READ THIS, it's the heart of the rendering
Everything that must sit **behind or in front of the giraffe without flicker** goes through one off-screen sprite that is composited and pushed in **one atomic transfer**. The panel has **no double-buffering**, so any "erase → draw over giraffe → restamp" sequence shows a half-drawn frame = a visible flash. The band avoids that entirely.

- **`skyBand`** = a `TFT_eSprite` the size of the **full giraffe footprint** (`GIRAFFE_W × BAND_H`, `BAND_H = GIRAFFE_H` = 150×160, ~48 KB, heap-allocated). `bandOk` guards a failed alloc (falls back to a direct `pushGiraffe`).
- **`composeSkyBand(band, giraffeBuf)` each frame**, in this layer order: sky band + ground band (split at the horizon row) → in-box clouds/birds (`drawCloud`/`drawBird` at local coords, sprite auto-clips) → **in-box grass** (`GRASS_BOX`, 3 depth rows behind the legs) → **manually composite the giraffe** over it all (copy `giraffeBuf`, skip the magenta key `giraffeBuf[0]`, byte-swap each kept pixel — see byte-order gotcha).
- **Then the loop draws foreground items INTO the band** (after the giraffe, so they're in front): eat food, sleep Z's, play butterfly/bubbles, the in-box part of the kick ball. Finally `skyBand.pushSprite(GIRAFFE_X, GIRAFFE_Y)`.
- **Pushed EVERY frame** — the in-box grass sways continuously, so there's no "only when needed" gate anymore. ~one 48 KB `pushSprite` (~10 ms) per frame; fine at this frame rate.
- **`giraffeBuf`** (48 KB static): the current giraffe pose, decoded once per emotion. Swapped to alternate happy faces, or kick poses, by re-decoding (see play/happy below).
- **`updateGiraffe(e)`** = full `restoreBg` + transparent `pushGiraffe` direct; used on emotion change / setup. The band then refreshes the footprint each frame on top.

### Open-sky vs in-box split (clouds, birds, the kick ball)
Things that travel from open sky/ground INTO the giraffe box are drawn in **two pieces** so they're continuous and flicker-free:
- **Out-of-box part:** drawn directly to the panel, **pixel-clipped to outside x[85,235]** with `setViewport(x,y,w,h, vpDatum=false)` (absolute coords + clip). See `drawCloudDirect`, `drawBallDirect`.
- **In-box part:** composited into the band (atomic). The two meet at the x85/x235 seam pixel-aligned → one continuous object.
- Pixel-level clipping (viewport), NOT whole-object skipping — skipping a whole cloud puff at the edge drops the sliver still outside the box and the cloud "shrinks/restarts."

### Play animations (rotate per PLAY press) & happy-face variety
- **`PLAY` cycles** `PLAY_BUTTERFLY → PLAY_BUBBLES → PLAY_KITE → PLAY_KICK` (`s_playKind`), each with its own duration in `PLAY_MS[]`.
- **Butterfly / bubbles** → composited into the band (in front of the giraffe head).
- **Kite** → direct draw in the open upper-left sky at a **fixed y32** (the only gap between the meters y≤24 and the clouds y≥43); swoops horizontally.
- **Kick** → "owns" the giraffe: the loop special-cases it and `tickKick` swaps `giraffeBuf` between the normal pose, `kick1` (windup/recover) and `kick2` (extend) via `setKickPose`. The **beach ball** is a separate rotatable `TFT_eSprite` (`ballSpr`, 80×80, heap): it rolls in from the right, rests ~0.5 s at the foot, then is volleyed up-right. `pushRotated` spins it with travel (in-box → into the band; out-of-box → direct, with `swapBytes(false)` around it). `tickKick` redraws poop + meters each frame so the big ball's path doesn't wipe them.
- **Happy variety:** while emotion is Happy, `giraffeBuf` is re-decoded to `happy` / `happy2` / `happy3` every `HAPPY_FRAME_MS` (3.5 s). The band shows the swapped buffer next push — no extra redraw needed.
- **`renderSpriteToBuffer(dst, path, w=GIRAFFE_W)`** decodes any sprite PNG by path into a `w`-wide buffer (used for happy frames, kick poses, the beach ball). `g_bufW` lets the PNG decoder write narrower sprites than the 150-wide giraffe.

### Ambient animations (OUTSIDE the box — drawn direct, erased via `restoreBg`)
Sleep Z's now composite INTO the band (beside the head, x≈202–216). Play ball (kick) handled above. **Clean sparkles** (poop slots x48/x264) and **grass/trees** live outside x85..235 where erasing to the scene is safe; keep new ambient effects clear of the box or the every-frame band push will clobber them.

### GOTCHAS (cost real debugging time — don't relearn the hard way)
- **RGB565 byte order (direct path):** decode with `getLineAsRGB565(..., PNG_RGB565_LITTLE_ENDIAN, ...)` + `tft.setSwapBytes(true)`. Other combos give scrambled/negative colors.
- **`TFT_eSprite::pushImage` has NO transparent-colour overload.** Passing a key as the last arg silently binds to the `uint8_t sbpp` (bits-per-pixel) param — the key is ignored and you get a solid (e.g. magenta) box. Composite transparency **by hand** into the sprite buffer (`getPointer()`), or use a giraffe-sprite + `pushToSprite(dst, x, y, key)`.
- **`pushSprite` outputs the buffer RAW** (it sets the parent's `swapBytes=false` internally). So pixels written by hand into the sprite buffer must be **byte-swapped** into the sprite's native order (`(p<<8)|(p>>8)`) to match `fillSprite`/`drawCloud` colours. This is why `composeSkyBand` swaps each kept giraffe pixel.
- **`pushRotated` to the TFT pushes the sprite's native bytes via `pushPixels`, which DOES honor the TFT's `swapBytes`.** Our sprites are loaded byte-swapped (native), so wrap the direct `pushRotated` in `setSwapBytes(false)` / restore (see `drawBallDirect`), else the rotated ball's colours come out swapped. `pushRotated(&dstSprite, …)` handles its own swap and needs no wrap. Load a rotatable sprite via `spr.setSwapBytes(true); spr.pushImage(0,0,w,h, rawLittleEndianBuf)`, then `setPivot`.
- **Splitting a moving sprite across two surfaces needs PIXEL-level clipping, not whole-object skipping.** Skipping a whole cloud puff the moment it touches the box drops the sliver still outside the box → the cloud "shrinks/restarts" at the edge. Use a viewport.
- **`readRect` (panel readback) does NOT work on this CYD** — returns garbage. Restore pixels by decoding the giraffe into RAM (`renderGiraffeToBuffer`) or recompositing the band, never by reading the panel.
- **Flash-erase always covers the FULL footprint** — under-sized erase rects leave trailing lines (clouds) or stranded fragments. Clip moving sprites by their **edge**, not their center.
- **Redrawing a big solid fill every frame flashes** (trees did this) — only redraw it when its value actually changes; thin lines (grass) are fine every frame.

### Key screen coordinates
- Giraffe sprite / band footprint: `x 85–235, y 34–194` (content starts at **y38** = horn tips). Box edges `BOX_L=85`, `BOX_R=235`.
- Meters: top row, `y 8–~24`. Buttons: `y 198–236` (FEED/DRINK/PLAY/CLEAN/BOOK, 60px each).
- Horizon: `y 165`. Poop slots: left ~`x48`, right ~`x264` (lower ground). Sun: `~(288,52) r16`.
- Play lanes: kite fixed at `y32` (meter↔cloud gap); sleep Z's `x≈202–216` (composited into band); kick ball rests at `(195,150)`, r40.

---

## 4. Features built (all working on hardware)

- **Care model:** Hunger / Thirst / Fun / Hygiene meters (top row, turn red when low) + on-screen poop.
- **5 action buttons:** FEED, DRINK, PLAY, CLEAN, BOOK(read).
- **10 emotion sprites** + **3 happy-face variants** (happy/2/3 cycled on a timer) + **2 kick poses**.
- **Action animations:** eat (apple→mouth), drink (glass→gulps), clean (sparkle poof), sleep (rising Zzz beside the head), reading state, excited reactions.
- **PLAY rotates 4 mini-games:** butterfly flutter, rising bubbles, swooping kite, and a **kick** (rolling, spinning beach-ball that he volleys away).
- **Savanna scene:** sky + golden ground + sun + 2 acacia trees + **layered swaying grass** (3 depth rows, incl. grass behind the giraffe's legs) + **drifting clouds & flapping birds**.
- **True silhouette occlusion, flicker-free:** clouds/birds/grass pass behind the giraffe's actual shape; foreground items (food, Z's, ball) sit in front — all via the off-screen compositing band (see §3).
- **32 passing unit tests** for all pet logic.

---

## 5. Current state

- **Branch:** `main`, pushed to `git@github.com:elliotboney/giraffegotchi.git`.
- **Compositing band + play animations DONE** (flashed & verified): full-footprint `skyBand` pushed every frame; grass behind the legs; rotating PLAY (butterfly/bubbles/kite/kick) with a spinning beach ball; happy-face variety. All flicker-free. See §3.
- **Next sprite slot:** a kick/nudge variant is already wired as PLAY kind #4 (`kick1`/`kick2`). To add more props (e.g. the kite as art instead of primitives), drop the source in `img/objects/`, add to `OBJECTS` in `prep_sprite.py`, re-run prep + `uploadfs`.
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
- **Perf headroom:** the full-footprint band composites + pushes every frame (~10 ms `pushSprite`, 48 KB). Fine now, but if it ever gets tight, options: shrink `BAND_H` back toward the sky rows and gate the push, or only push changed sub-rows.
- **Heap:** `giraffeBuf` (48 KB) is static; `skyBand` (48 KB) and `ballSpr` (~13 KB) are heap (`createSprite`/`malloc`). ~110 KB of buffers total — comfortable on the 320 KB part.
- `img/objects/kite.png` exists as art but the in-game kite is still drawn with primitives.
</content>
