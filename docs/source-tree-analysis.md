# Source Tree Analysis — Giraffegotchi

_Updated 2026-07-01 · post swappable-animals refactor._

```
giraffegotchi/
├── platformio.ini          # esp32dev (firmware) + native (tests) envs; TFT flags; pre-build env hook
├── package.json            # bun task scripts (setup/compile/upload/uploadfs/flash/native/monitor/prep)
├── .env                    # (gitignored) WiFi + lat/lon/TZ → build flags
├── src/
│   ├── main.cpp            # ★ orchestration: setup/loop, touch, WiFi/NTP, day/night driver,
│   │                       #   active-species pointer + atomic swap, picker UI, backlight, prank
│   ├── pet.{h,cpp}         # ★ CORE (pure): stats, decay, poop, emotions, night sleep, load.
│   │                       #   No hardware includes → native-tested. (Left in place by the refactor.)
│   ├── ui.{h,cpp}          #   RENDER: biome scene (palette/grass/trees/stars/critters), sun/moon
│   │                       #   arc, compositing band, sprite decode, meters/buttons/picker prims
│   ├── core/
│   │   └── sky.{h,cpp}     #   CORE (pure): solar/phase math — native-tested
│   ├── species/
│   │   ├── species.h       #   Species / AnimSpec / AnimSet / Biome / FoodItem types + Capability
│   │   ├── registry.{h,cpp}#   built-in species list + active-species accessor/swap
│   │   ├── giraffe.cpp     #   giraffe as data (descriptor + anims + savanna biome)
│   │   └── groundhog.cpp   #   groundhog as data (descriptor + anims + meadow biome)
│   ├── anim/
│   │   └── engine.{h,cpp}  #   data-driven animation engine + foreground composers (eat/sleep/…)
│   └── io/
│       └── save.{h,cpp}    #   NVS: per-species care blocks + active-species id (versioned)
├── test/
│   ├── test_pet/test_pet.cpp   # 37 Unity tests (pet)
│   └── test_sky/test_sky.cpp   # 6 Unity tests (sky) — 43 total, env:native
├── data/                   # LittleFS payload (flashed with uploadfs) — one folder per species
│   ├── giraffe/  groundhog/     #   <pose>.png (150x160), icon.png (64), objects → beach_ball/food
│   └── flamingo/               #   (WIP — partial art, no descriptor yet)
├── img/                    # transparent source art → prep input, one folder per species
│   └── <species>/{*.png, objects/, icon.png}
├── tools/
│   ├── prep_sprite.py      # art pipeline: img/<sp>/ → data/<sp>/ (alpha-key, align, despeckle, budget)
│   ├── load_env.py         # PlatformIO pre-build hook: .env → -D flags
│   └── gen_giraffe.py      # legacy PIL art generator (superseded)
├── docs/                   # this doc set + STATUS.md / PET_PROMPT.md
└── _bmad-output/           # planning artifacts (architecture spine, epics/stories) + epic1 baseline
```

★ = highest-signal files.

## Layers (one-way deps)

`main → { render (ui), anim (engine), species (registry+data), core (pet, sky) } → hardware libs`.
`core` and `species` are hardware-free. Everything animal-specific lives in the `Species`
descriptor (`species/<animal>.cpp`); render/anim/main read the *active* one via `activeSpecies()`.

## Entry points

- **Firmware:** `main.cpp::setup()` → TFT/backlight init → LittleFS + `save::restore` (sets the
  active species) → size pose buffer (max species) + band → WiFi/NTP → first paint → touch. Then
  `loop()` runs the frame loop (~10 ms/frame), applying any latched swap at the top.
- **Tests:** `test_pet` / `test_sky` under `env:native` — no board (compiles `pet.cpp` + `core/sky.cpp`).
- **Build hook:** `tools/load_env.py` (`pre:`) injects `.env` every build.

## Excluded from analysis

- `.pio/` (build artifacts + libs), `.venv/` (art-pipeline Python), `docs/other_giraffes/` (gitignored).
