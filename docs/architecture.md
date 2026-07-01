# Architecture — Giraffegotchi

_Updated 2026-07-01 — as-built after the swappable-animals refactor (Epics 1–5)._

The giraffe is no longer hardcoded: it's **one data descriptor in a registry**, and animals
are switched on-device. Render and animation read the *active* descriptor, so a new animal is
data, not a code fork. The authoritative invariants are the architecture spine
(`_bmad-output/planning-artifacts/architecture/.../ARCHITECTURE-SPINE.md`, AD-1..15); this doc
is the as-built summary. See [STATUS.md](./STATUS.md) for rendering internals.

## 1. Layers (one-way dependencies)

```
main  →  { render, anim, species, core }  →  hardware libs
```

| Layer | Files | Role |
|---|---|---|
| **core** | `pet.{h,cpp}`, `core/sky.{h,cpp}` | Pure logic, no hardware. `pet` (left at `src/pet.*` — already clean) = meters/emotions/decay/night-sleep. `sky` = solar/phase math. Compiles + unit-tests under `env:native`. |
| **species** | `species/species.h`, `species/registry.{h,cpp}`, `species/<animal>.cpp` | The `Species`/`AnimSpec`/`Biome`/`FoodItem` **data types** + the registry + each animal as a data file. Hardware-free (a `TFT_eSPI` forward-decl only, for the biome prop-hook pointer). |
| **anim** | `anim/engine.{h,cpp}` | Species-agnostic animation engine: the emotion-base pose floor, idle rotation/tics, foreground composers (eat/sleep/daydream/bubbles/butterfly), per-species food. Reads the active descriptor; writes the pose buffer / composes the band; never pushes to the panel. |
| **render** | `ui.{h,cpp}` (the `render/` layer) | Pixels: biome scene (palette/grass/trees/stars/critters from the active biome), sun/moon arc, the compositing band, sprite decode, meters/buttons/picker primitives. Sole writer of `SKY_COLOR`/`GROUND_COLOR` + the sky-phase id. |
| **io** | `io/save.{h,cpp}` | NVS persistence: per-species care blocks + the active-species id. |
| **orchestration** | `main.cpp` | `setup`/`loop`, timing, touch, WiFi/NTP driver, the **active-species pointer + atomic swap**, backlight, the picker UI. |

## 2. The species descriptor (single source of animal truth — AD-11)

`Species` (in `species.h`) carries **everything** animal-specific, so no giraffe-ism leaks into
render/anim/main:

- `name` (internal id / asset key), `displayName` (picker label), `assetFolder`.
- `geom` — `w,h,x,y` + `horizonY` (buffers are sized from this; nothing assumes 150×160).
- `anchors` — mouth / food-drop / sleep-Z / daydream positions.
- `caps` — `Capability` flags (CAP_KITE/CAP_KICK): opt-in signature moves.
- `anims` — `AnimSet` (idle rotation + variable-length tics).
- `biome` — `Biome` (8-phase palette table + grass/stars/trees/fireflies + a tree draw-hook).
- `food` — optional `FoodItem` (sprite + size); absent → the drawn apple.
- `icon` — picker icon pose name (absent → name-only tile).

The registry (`registry.cpp`) holds the `SPECIES[]` list; `main` is the only module that swaps
(`setActiveSpecies`), everyone else reads `activeSpecies()`.

## 3. Runtime shape (single-threaded `loop()`, ~100 Hz)

```
loop():
  apply latched species swap  (top of loop only — AD-13)
  pet.update(dt)              (skipped while dead)
  updateDayNight() [1/min]    (recompute sun pos + phase; setSkyPhase indexes the ACTIVE biome)
  touch: care actions (press edge) · BOOK tap=read / hold=picker · fast-mash die/revive
  picker modal (if open): render grid, route taps, else early-return
  animateScenery(tft)         (biome grass/trees/critters + open-sky clouds/birds/sun/moon)
  anim engine: resolve the one pose writer this frame (dead > kick > tic > emotion — AD-5)
  composeSkyBand + foreground composers + pushSprite   (one atomic in-box push)
  meters / poop redraw on change
  save::tick(now)             (throttled ≤ once / 5 s)
```

The **compositing band** (`skyBand`, an off-screen `TFT_eSprite` sized to the active footprint)
is the heart of the renderer: the panel isn't double-buffered, so in-box content is composited
once and pushed atomically. Out-of-box content draws direct with a viewport pixel-clip.

## 4. Live species swap (AD-13)

A swap request is **latched** and applied at exactly one point — the top of `loop()`, before
`pet.update()` — as an atomic sequence, so no frame straddles two species:

1. cancel all in-flight animations + reset pose state,
2. `save::captureActive` (animal A's stats), switch species, re-create the band to the new
   footprint (the pose buffer is allocated **once at the max species size** in `setup`, never
   reallocated — this avoids the 2×-buffer heap peak that would otherwise fail/fragment),
3. `save::loadActive` (animal B's own stats), decode B's sprites + reset anim indices,
4. refresh the palette from B's biome + full-screen repaint (clears any out-of-box element),
   then persist.

Triggered by the on-device picker (long-press BOOK → grid → tap). A dev serial trigger exists
behind `-DDEBUG_SWAP` only.

## 5. Persistence (AD-8)

NVS namespace `"giraffe"`, key `"pet"`. Versioned blob (`SAVE_MAGIC = 0x69`):
`{ magic, activeIdx, CareBlock[MAX_SP] }` where `CareBlock = { hu, th, fn, hy, poop, dead }`.
On boot the active id is validated against the registry (unknown → default species, no
boot-loop); each animal's block is independent (FR12). Restore is as-is (no decay advance).
Throttled ≤ once / 5 s, plus immediate writes on swap/die/revive.

## 6. Key invariants (keep true)

1. **core stays hardware-free** — `pet`/`sky` compile under `env:native`; 43 tests are the safety net.
2. **No species-specific literal outside the descriptor** (paths, geometry, anchors, palette, props).
3. **Magenta `0xF81F` is the transparency key**, read at runtime from `giraffeBuf[0]`; never inside a silhouette. Keep `prep_sprite.py MAGENTA_RGB` in lockstep.
4. **All erases go through `restoreBg`** (never a flat fill; `readRect` is broken on this CYD).
5. **One pose-buffer writer per frame** by priority `dead > kick > tic > emotion` (AD-5); foreground anims compose into the band and never touch the pose buffer.
6. **Anything crossing the box is split** into out-of-box direct (viewport clip) + in-box band composite — pixel-level, never whole-object skip.
7. **`TFT_eSprite::pushImage` has no working transparent overload** — composite into the band by hand (byte-swap + key-skip + bounds-clip). `pushImage(..., key)` on a sprite can run off-bounds and freeze the device.
8. **Display on HSPI, touch on VSPI** — separate buses.

## 7. Hardware (pinout)

| Function | Pin(s) | Bus |
|---|---|---|
| Display (ILI9341) | MISO 12, MOSI 13, SCLK 14, CS 15, DC 2, RST -1, BL 21 | HSPI |
| Touch (XPT2046) | IRQ 36, MOSI 32, MISO 39, CLK 25, CS 33 | VSPI |
| Backlight PWM | GPIO 21 via `ledc` ch 0, 5 kHz, 8-bit | — |
| Speaker (unused) | GPIO 26 | — |

Touch is calibrated with raw bounds `TS_MINX/MAXX/MINY/MAXY` and inverted to match the 180°
display flip (`setRotation(3)`).

## 8. Art pipeline (AD-14)

Per-species folders on LittleFS (`data/<species>/`, identical filenames). `tools/prep_sprite.py`
keys transparent source → magenta, aligns frames, despeckles, sizes by asset type, and reports
the flash budget (fails if over). Two full animals ≈ 320 KB (~21% of the partition). See the
[development guide](./development-guide.md) and the README's "Adding an animal".
