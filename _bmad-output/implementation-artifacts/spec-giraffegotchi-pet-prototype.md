---
title: 'Giraffegotchi Pet Prototype (CYD)'
type: 'feature'
created: '2026-06-25'
status: 'done'
review_loop_iteration: 0
context: []
baseline_commit: '7b876a9'
---

<frozen-after-approval reason="human-owned intent — do not modify unless human renegotiates">

## Intent

**Problem:** There is no firmware for the giraffegotchi. We need a first playable prototype: a giraffe pet rendered on the CYD screen whose hunger decays over time and that the user can feed by touching the screen.

**Approach:** A PlatformIO + Arduino firmware for the ESP32-2432S028R. Pure pet-state logic (hunger stat, mood) is kept hardware-free so it can be unit-tested natively; `main.cpp` drives timing, reads the XPT2046 touch panel for a "feed" hit-zone, and renders the giraffe and a hunger bar using TFT_eSPI drawing primitives (no bitmap assets).

## Boundaries & Constraints

**Always:**
- Target board ESP32-2432S028R, ILI9341 240×320, landscape rotation (rotation 1).
- TFT_eSPI configured via `build_flags` in `platformio.ini` (no edited library `User_Setup.h`).
- Touch on its own SPI bus via the XPT2046_Touchscreen library (CYD touch is a separate bus from the display).
- Pet logic in `pet.*` stays free of any Arduino/TFT include so it builds in the `native` test environment.
- Redraw only what changed (mood face on transition, hunger bar each tick) to avoid full-screen flicker.

**Ask First:**
- Adding Wi-Fi, persistence (NVS/flash save), sound, or additional stats beyond the single hunger/mood stat.
- Swapping the touch or display library, or changing the board target.

**Never:**
- Bitmap/sprite assets or an asset-conversion pipeline (primitives only this prototype).
- More than one decaying stat; no multi-screen UI; no settings menu.

## I/O & Edge-Case Matrix

| Scenario | Input / State | Expected Output / Behavior | Error Handling |
|----------|--------------|---------------------------|----------------|
| Decay tick | hunger=80, elapsed=1 decay interval | hunger decreases by decay step, clamped at 0 | N/A |
| Cross hungry threshold | hunger drops from 31 to 29 (threshold 30) | mood flips Happy→Hungry | N/A |
| Feed | feed() called at hunger=40 | hunger += feed amount, clamped at 100; mood recomputed | N/A |
| Feed at full | feed() called at hunger=100 | hunger stays 100 (no overflow/wrap) | clamp |
| Touch in feed zone | raw touch maps inside feed-button rect | feed() invoked once per press (no repeat while held) | N/A |
| Touch outside zone | touch maps outside button | no state change | ignore |

</frozen-after-approval>

## Code Map

- `platformio.ini` -- project def, deps (TFT_eSPI, XPT2046_Touchscreen), CYD TFT build flags, `native` test env
- `src/main.cpp` -- setup/loop: init display+touch, decay timing via millis, touch read + feed-zone hit test (edge-triggered), render dispatch
- `src/pet.h` / `src/pet.cpp` -- hardware-free `Pet`: hunger stat, `Mood` enum, `update(elapsedMs)`, `feed()`, `mood()`
- `src/ui.h` / `src/ui.cpp` -- TFT_eSPI drawing: `drawGiraffe(tft, mood)`, `drawHungerBar(tft, hunger)`, `drawFeedButton(tft)`, feed-zone rect constant
- `test/test_pet/test_pet.cpp` -- native unit tests for the I/O matrix (decay, threshold, feed clamp)

## Tasks & Acceptance

**Execution:**
- [x] `platformio.ini` -- define `esp32dev`/`arduino` env with `board_build` + CYD TFT_eSPI `-D` build flags (ILI9341, pins MISO12 MOSI13 SCLK14 CS15 DC2 RST-1 BL21, SPI 40MHz), libs `bodmer/TFT_eSPI` + `paulstoffregen/XPT2046_Touchscreen`, plus a `native` env for tests
- [x] `src/pet.h` / `src/pet.cpp` -- `Pet` with `uint8_t hunger` (0–100), `Mood{Happy,Hungry}`, `feed()` (+FEED_AMOUNT, clamp 100), `update(uint32_t elapsedMs)` accumulating to DECAY_INTERVAL_MS and decrementing DECAY_STEP (clamp 0), `mood()` = Hungry if hunger < HUNGRY_THRESHOLD else Happy
- [x] `src/ui.h` / `src/ui.cpp` -- giraffe from rects/circles/lines (body, long neck, head, ossicones, spots), color/expression varies by mood; horizontal hunger bar; labelled feed button; export `FEED_BTN` rect
- [x] `src/main.cpp` -- init TFT (rotation 1, fill bg) + XPT2046 on second SPI bus; loop computes elapsed via `millis()`, calls `pet.update()`, reads touch, maps raw→screen, edge-triggers `pet.feed()` on press inside `FEED_BTN`; redraw face on mood change + hunger bar each ~500ms
- [x] `test/test_pet/test_pet.cpp` -- assert each I/O matrix row against `Pet`

**Acceptance Criteria:**
- Given a flashed CYD, when it boots, then the giraffe and a full hunger bar render in landscape without flicker.
- Given the pet is idle, when hunger decays below the threshold, then the giraffe's face visibly changes to the hungry expression.
- Given the hungry pet, when the user taps the feed button, then hunger rises and the face returns to happy on a single tap (not on hold-repeat).
- Given the native env, when `pio test -e native` runs, then all pet-logic tests pass.

## Design Notes

CYD pin facts (ESP32-2432S028R): display ILI9341 on VSPI (CS15 DC2 SCLK14 MOSI13 MISO12, RST-1, backlight GPIO21 active-high); touch XPT2046 on its own bus (CLK25 MOSI32 MISO39 CS33 IRQ36). Resistive touch raw values run roughly X≈200–3700, Y≈240–3800; map raw→screen and tune the calibration constants (`TS_MINX/MAXX/MINY/MAXY` in `main.cpp`) if the feed hit-zone feels off — note this is per-unit and expected. If the button is unhittable in landscape, the axes are swapped: map `p.y`→sx and `p.x`→sy.

```cpp
// edge-triggered feed (in loop):
bool down = ts.touched();
if (down && !wasDown && FEED_BTN.contains(sx, sy)) pet.feed();
wasDown = down;
```

## Verification

**Commands:**
- `pio run -e esp32dev` -- expected: firmware compiles and links
- `pio test -e native` -- expected: all pet-logic unit tests pass

**Manual checks:**
- Flash with `pio run -e esp32dev -t upload`; on hardware confirm giraffe renders, hunger bar drains, face flips to hungry, and a tap on the feed button refills + returns to happy.

## Suggested Review Order

**Pet logic (the testable core)**

- Hunger clamp on feed — `uint16_t` widening prevents 8-bit wrap.
  [`pet.cpp:3`](../../src/pet.cpp#L3)
- Decay accumulator — carries remainder, catches up over long elapsed gaps.
  [`pet.cpp:8`](../../src/pet.cpp#L8)

**Main loop & input**

- Render/decay loop; redraw-on-change for mood and hunger bar (no lag/overdraw).
  [`main.cpp:52`](../../src/main.cpp#L52)
- Edge-triggered feed with pressure gate + clamp (no phantom feeds).
  [`main.cpp:62`](../../src/main.cpp#L62)
- Touch on its own VSPI bus; pairs with the HSPI display flag.
  [`main.cpp:41`](../../src/main.cpp#L41)

**Hardware config**

- USE_HSPI_PORT keeps display off the touch's VSPI bus (collision fix).
  [`platformio.ini:21`](../../platformio.ini#L21)

**Rendering**

- Giraffe band y34..194 — clears horns clear of the hunger bar.
  [`ui.cpp:15`](../../src/ui.cpp#L15)
- Mood-driven face + giraffe primitives.
  [`ui.cpp:17`](../../src/ui.cpp#L17)

**Tests (peripherals)**

- Boundary + partial-clamp cases added after review.
  [`test_pet.cpp:39`](../../test/test_pet/test_pet.cpp#L39)
