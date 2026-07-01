# Architecture — Giraffegotchi

_Generated: 2026-06-30 · exhaustive scan_

This documents the **as-built** architecture and, because the next planned work is a
refactor, calls out the **coupling and natural seams** so you can decide scope. See
[STATUS.md](./STATUS.md) for the rendering internals and hard-won display gotchas — this
doc does not repeat them.

## 1. Runtime shape

A single-threaded Arduino `loop()` at ~100 Hz (`delay(10)`). Each frame:

```
loop():
  pet.update(dt)              # advance decay/poop/sick timers (skipped while "dead")
  uiSetPhase(now)             # breeze phase for grass sway
  updateDayNight() [1/min]    # recompute sun pos + sky phase; repaint on phase change
  read touch (edge-triggered) # map raw XPT2046 → screen coords → hit-test buttons
    → care actions / prank death / revive
  backlight dim check
  animateScenery(tft)         # grass, clouds, birds, sun/moon (direct draw, open sky)
  giraffe ownership block     # pick pose: eat / kick / emotion / happy-rotation / tics
  composeSkyBand + pushSprite # atomic composite of everything in the giraffe footprint
  meters / poop redraw on change
  saveState() [throttled 5s]  # persist care stats to NVS if dirty
```

The **compositing band** (`skyBand`, an off-screen `TFT_eSprite` the size of the giraffe
footprint) is the heart of the renderer: the panel has no double-buffering, so anything that
must sit behind/in front of the giraffe without flicker is composited once and pushed
atomically. Full detail in STATUS.md §3.

## 2. Module dependency map

```
          ┌────────────┐
          │  main.cpp  │  orchestration, I/O, state machines (891 LOC)
          └─────┬──────┘
       ┌────────┴─────────┐
       ▼                  ▼
 ┌───────────┐      ┌───────────┐
 │   pet.*   │      │   ui.*    │
 │  PURE     │◄─────│ uses      │  ui reads Pet:: constants (LOW_THRESHOLD, MAX_POOP)
 │  logic    │      │ rendering │  and Emotion enum
 └───────────┘      └─────┬─────┘
   no deps                ▼
                   TFT_eSPI, PNGdec, LittleFS  (hardware libs)
```

- **`pet`** depends on nothing (only `<stdint.h>`). Clean, isolated, testable. ✅
- **`ui`** depends on `pet.h` (for `Emotion`, `StatId`, thresholds) and the TFT/PNG/FS libs.
- **`main`** depends on both, plus WiFi, NVS (`Preferences`), and `time.h`.

### Cross-module coupling (the part that matters for refactoring)

`main.cpp` reaches into `ui` through a **wide surface of `extern` globals and free
functions**, not a narrow interface:

- Geometry constants: `GIRAFFE_X/Y/W/H`, `HORIZON_Y`, `BAND_H`, `BOX_L/BOX_R`.
- Live palette globals: `SKY_COLOR`, `GROUND_COLOR` (mutated by `setSkyPhase`).
- Button hit-rects: `FEED_BTN`, `DRINK_BTN`, `PLAY_BTN`, `CLEAN_BTN`, `BOOK_BTN`.
- ~20 free draw functions: `drawScene`, `restoreBg`, `composeSkyBand`, `drawMeters`,
  `drawFood`, `drawKite`, `celestialPos`, `solarTimes`, etc.

`ui.cpp` itself carries **file-static mutable state**: the PNG decode target globals
(`g_tft`, `g_buf`, `g_bufW`), cloud/bird/firefly positions, celestial position, sky phase id.
This is fine for a single-instance firmware but is the reason the rendering layer is not
independently testable.

## 3. Invariants (the spine — keep these true through any refactor)

1. **`pet` stays hardware-free.** No Arduino/TFT includes in `pet.h`/`pet.cpp`, so
   `env:native` keeps compiling it and the 37 tests keep running.
2. **Display on HSPI, touch on VSPI, separate buses.** Collapsing them breaks the display.
3. **Magenta `0xF81F` is the transparency key.** Must never appear inside a giraffe
   silhouette; `giraffeBuf[0]` (top-left) is read as the key at runtime.
4. **All erases go through `restoreBg`**, never a flat fill — it repaints the correct sky/
   ground band + any overlapping prop. Panel readback (`readRect`) does NOT work on this CYD.
5. **Anything crossing the giraffe box is split** into an out-of-box direct draw (viewport
   pixel-clip) + an in-box band composite. Pixel-level clip, never whole-object skip.
6. **NVS save is throttled** (≤ once / 5 s) to spare flash; the save layout is versioned by
   `SAVE_MAGIC` — bump it if `PetSave` changes.

## 4. Data & persistence

- **In-RAM state:** `Pet` holds the 4 stats + timers + poop count. All animation state lives
  in `main.cpp` structs (`EatAnim`, `SleepAnim`, `DaydreamAnim`, `PlayAnim`, `CleanAnim`).
- **Persisted (NVS namespace `"giraffe"`):** `PetSave{ magic, hunger, thirst, fun, hygiene,
  poop, dead }` — care stats + the prank-death flag. Restore is as-is (a power cut does not
  age the pet). No decay/time is persisted.
- **Filesystem (LittleFS):** 22 PNG sprites, read on demand at draw time.

## 5. Hardware interface (pinout)

From `platformio.ini` build flags + `main.cpp`:

| Function | Pin(s) | Bus |
|---|---|---|
| Display (ILI9341) | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST -1, BL 21 | HSPI |
| Touch (XPT2046) | IRQ 36, MOSI 32, MISO 39, CLK 25, CS 33 | VSPI |
| Backlight PWM | GPIO 21 (TFT_BL) via `ledc` ch 0, 5 kHz, 8-bit | — |
| Speaker (unused) | GPIO 26 (noted in backlog) | — |

Touch is calibrated with raw bounds `TS_MINX/MAXX/MINY/MAXY` and inverted to match the 180°
display flip (`setRotation(3)`).

---

## 6. Refactor seams (why this is a *structural* refactor)

`pet` needs nothing. The opportunity is entirely in **`main.cpp` (891 LOC) and `ui.cpp`
(684 LOC)**, both of which mix several concerns in one translation unit. Natural extractions,
roughly in order of value-to-risk:

| # | Seam | Extract from → to | Why | Risk |
|---|---|---|---|---|
| 1 | **Solar/sky math** | `ui.cpp` → new `sky.{h,cpp}` | `solarTimes`, `celestialPos`, `skyPhaseFor`, `dayOfYear` are **pure functions** — make them unit-testable like `pet` (add to `env:native`). | Low |
| 2 | **Animation state machines** | `main.cpp` → `anim.{h,cpp}` | Eat/sleep/daydream/play(×4)/clean/tics/happy-rotation are ~500 of main's 891 lines. Each is a self-contained `struct + start + tick`. | Medium (they touch `giraffeBuf` + band) |
| 3 | **Day/night driver** | `main.cpp` → `daynight.{h,cpp}` | `syncTime`, `tzOffsetMinutes`, `updateDayNight`, phase-change repaint. Owns WiFi + clock. | Medium (calls back into scene repaint) |
| 4 | **Persistence** | `main.cpp` → `save.{h,cpp}` | `PetSave`, `saveState`, `loadState`, dirty/throttle. Small, isolated, clear boundary. | Low |
| 5 | **Rendering primitives vs scene** | split `ui.cpp` | `drawFood/Drink/Ball/Sparkle/Butterfly/Bubble/Kite` (stateless primitives) vs the stateful scene compositor. | Low–Medium |

**The core tension:** the animation state machines (#2) and the band compositor share a lot of
implicit contract — pose ownership of `giraffeBuf`, who draws into the band vs direct, the
box-split rule. Extracting them cleanly means **first defining that contract as an interface**
(e.g. an `Animation` that returns "what to composite into the band this frame"), which is the
real design work. Everything else (1, 3, 4, 5) is mechanical file-splitting.

**Recommended sequencing if you proceed:** do the low-risk pure extractions first (#1 solar
math, #4 persistence) to build confidence and test coverage, then tackle the animation-system
contract (#2) as its own epic, then #3/#5. This is the kind of multi-step restructure the
BMad epics-and-stories flow is built for.

### Safety net that already exists
- 37 native tests cover `pet` — refactors there are safe. **There are no tests for `ui`/
  `main`** (hardware-bound). Extracting the pure math (#1) is the cheapest way to grow the
  tested surface before touching rendering.
- `git` is clean on `main`; every change is flash-verifiable on the device.
