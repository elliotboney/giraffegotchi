# Source Tree Analysis — Giraffegotchi

_Generated: 2026-06-30_

```
giraffegotchi/
├── platformio.ini          # Build config: esp32dev (firmware) + native (tests) envs,
│                           #   TFT_eSPI pin/driver flags, lib_deps, pre-build env hook
├── .env                    # (gitignored) WiFi creds + lat/lon/TZ → injected as build flags
├── src/
│   ├── pet.h  / pet.cpp     # ★ PURE game logic — stats, decay, poop, emotions, night sleep.
│   │                        #   No Arduino/TFT includes → compiles & tests on native.
│   ├── ui.h  / ui.cpp       # Rendering layer: scene, sky palettes, sun/moon arc, solar math,
│   │                        #   PNG decode, drawing primitives, meters/buttons/poop.
│   └── main.cpp             # ★ Orchestration entry point (setup/loop). Touch, WiFi/NTP,
│                            #   day/night driver, NVS save, backlight, prank-death,
│                            #   and all animation state machines.
├── test/
│   └── test_pet/
│       └── test_pet.cpp     # 37 Unity tests for Pet (runs under env:native, no hardware)
├── data/                   # LittleFS payload — flashed with `uploadfs`
│   ├── giraffe_*.png        #   21 giraffe sprites: 10 emotions + happy2/3 + blink×3 +
│   │                        #   ears up/down + tail + kick1/2 + dead (150×160, magenta key)
│   └── beach_ball.png       #   80×80 prop sprite (magenta key)
├── img/                    # High-res AI source art (1254×1254) → prep pipeline input
│   └── objects/             #   prop source art (beach_ball, kite)
├── tools/
│   ├── prep_sprite.py       # Art pipeline: img/*.png → data/*.png (key-out bg, autocrop, downscale)
│   ├── gen_giraffe.py       # Legacy PIL fallback art generator (superseded)
│   └── load_env.py          # PlatformIO pre-build hook: .env → -D build flags
├── docs/                   # ← this documentation set + STATUS.md / PET_PROMPT.md
└── README.md               # Public-facing feature overview + setup
```

★ = entry points / highest-signal files.

## Critical directories

| Path | Purpose | Entry / notes |
|---|---|---|
| `src/` | All firmware source | `main.cpp` is the Arduino entry (`setup`/`loop`) |
| `test/test_pet/` | Native unit tests | `int main()` runs Unity; only `pet.cpp` compiled |
| `data/` | LittleFS filesystem image | Sprites read from flash at draw time; re-flash with `uploadfs` |
| `img/` | Source art (not shipped) | Processed into `data/` by `prep_sprite.py` |
| `tools/` | Build + art tooling (Python) | `load_env.py` runs at every build |

## Entry points & bootstrap

- **Firmware:** `main.cpp::setup()` → TFT init, backlight PWM, sprite-buffer alloc, sky-band
  sprite alloc, `syncTime()` (WiFi/NTP), first scene paint, LittleFS mount, touch init, NVS
  `loadState()`. Then `main.cpp::loop()` runs the frame loop (~10 ms delay/frame).
- **Tests:** `test_pet.cpp::main()` under `env:native` — no board required.
- **Build hook:** `tools/load_env.py` runs `pre:` every build to inject `.env` values.

## Build outputs (excluded from analysis)

- `.pio/` — PlatformIO build artifacts, downloaded libs (TFT_eSPI, XPT2046, PNGdec, Unity).
- `docs/other_giraffes/` — local reference art, gitignored.
