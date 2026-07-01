# Development Guide — Giraffegotchi

_Updated 2026-07-01 (post swappable-animals refactor)._

## Prerequisites

- **[PlatformIO](https://platformio.org/)** (`pio`).
- **[bun](https://bun.sh)** + **Python 3.12** — for the art pipeline and the command shortcuts.
- **A CYD board** (ESP32-2432S028R) + USB. Find the port with `ls /dev/cu.*usbserial*`.

One-time: `bun setup` creates `.venv/` and installs Pillow (Python 3.14 has no Pillow wheels yet, so the venv pins 3.12).

## Commands (`package.json` bun scripts)

| Command | Raw equivalent |
|---|---|
| `bun run help` | (lists these commands) |
| `bun compile` | `pio run -e esp32dev` |
| `bun upload` | `pio run -e esp32dev -t upload` |
| `bun uploadfs` | `pio run -e esp32dev -t uploadfs` |
| `bun flash` | `pio run -e esp32dev -t upload -t uploadfs` |
| `bun native` | `pio test -e native` |
| `bun monitor` | `pio device monitor -e esp32dev` |
| `bun prep [name]` | `.venv/bin/python tools/prep_sprite.py [name]` |

- **`bun upload`** for code-only changes; **`bun flash`** (or `bun uploadfs`) whenever `data/` sprites changed. After `uploadfs`, the firmware reads each PNG from flash on draw.
- Port "busy or doesn't exist" ≈ board unplugged or a serial monitor holding it.

## Environments

`platformio.ini`:

| Env | Target | Compiles | Use |
|---|---|---|---|
| `esp32dev` | ESP32 | all of `src/` | build + flash |
| `native` | host | `pet.cpp` + `core/sky.cpp` (`build_src_filter`) | unit tests, no board |

## Configuration (`.env`)

WiFi + location in a gitignored `.env`, injected as build flags by `tools/load_env.py`:

```ini
WIFI_SSID="Your Network"
WIFI_PW="your-password"
LAT=30.0858
LON=-97.8403
TZ=CST6CDT,M3.2.0,M11.1.0
```

No `.env` → still builds, skips WiFi/NTP, stays daytime.

## Art pipeline

Source art is **transparent-background**, in per-species folders:

```
img/<species>/*.png          body poses   -> data/<species>/<pose>.png   (150x160, or per BODY_SIZES)
img/<species>/objects/*.png  props/food   -> data/<species>/<name>.png   (small, per OBJECT_SIZES)
img/<species>/icon.png       picker icon  -> data/<species>/icon.png      (64x64)
```

`bun prep` (all) or `bun prep <species>` (one). `prep_sprite.py`:
- keys **alpha → magenta** (`0xF81F`); opaque legacy art falls back to border-bg keying,
- auto-discovers species + poses (no hardcoded pose list; single-frame blink is fine),
- **aligns** a species' idle/emotion frames to a common bbox so the body doesn't jump between poses (kick/dead framed individually),
- despeckles stray edge pixels, sizes by asset type,
- reports the LittleFS budget and **exits non-zero if over** (so `uploadfs` can't silently truncate).

Then `bun uploadfs`. Magenta must never appear inside a silhouette (it's the transparency key, read at runtime from `giraffeBuf[0]`).

## Adding an animal

Data + art only — no engine changes. Full human steps: the **Adding an animal** section in the top-level [README](../README.md#adding-an-animal). Using an AI agent: [`adding-an-animal-agent.md`](adding-an-animal-agent.md) (the playbook — encodes invariants, the food+biome rule, and the ask-the-human-for-a-name step). Flow: drop `img/<name>/` art → `bun run cleanart <name>` (bg-remove + align; re-runnable after adding one pose) → `bun prep <name>` → add `src/species/<name>.cpp` (copy `groundhog.cpp`; give it its own food + biome) → register in `registry.cpp` → `bun native && bun compile` → `bun flash`.

## Testing

- **`core` (pet + sky) is the tested surface** — 43 Unity tests (`test/test_pet`, `test/test_sky`) cover decay/clamp/poop/emotion/night/load and the solar/phase math. They run on `native` (no hardware includes) — keep it that way.
- `render`/`anim`/`species`/`io`/`main` are hardware-bound; verify by flashing + watching the device and the serial traces (`[daynight]`, `[save]`, `[swap]`, `[prank]`).
- The Epic 1 behavior-neutral A/B baseline lives in `_bmad-output/implementation-artifacts/epic1-baseline.md` — a good on-device checklist after render changes.

## Conventions

- One-way layer deps: `main → {render, anim, species, core} → hardware`. `core`/`species` are hardware-free.
- Everything species-specific (paths, geometry, anchors, palette, anims, food) lives **only** in the `Species` descriptor — no giraffe-isms in render/anim/main.
- Erases go through `restoreBg`, never a flat fill. In-box content composites into the one `skyBand` sprite, pushed once per frame (flicker-free).
- **`TFT_eSprite::pushImage` has no working transparent overload** — composite into the band by hand (byte-swap + key-skip + bounds-clip), like `composeSkyBand` / `blitFoodToBand`. Using `pushImage(..., key)` on a sprite can run off-bounds and freeze the device.
- Constants are `static constexpr`/`static const` with an inline rationale; comments explain **why**.
