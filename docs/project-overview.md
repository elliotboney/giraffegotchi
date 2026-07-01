# Project Overview — Giraffegotchi

_Generated: 2026-06-30 · initial exhaustive scan_

## What it is

A Tamagotchi-style digital pet (a giraffe) that runs as **ESP32 Arduino firmware** on the
**ESP32-2432S028R "Cheap Yellow Display" (CYD)** — an ILI9341 240×320 TFT with resistive
XPT2046 touch. Five buttons drive four care meters; the pet's mood, a savanna scene, and a
real-time day/night cycle are all computed on-device.

## Classification

| Attribute | Value |
|---|---|
| Repository type | Monolith, single part |
| Project type | Embedded (ESP32 / Arduino / C++) |
| Build system | PlatformIO |
| Primary language | C++ (Arduino), plus Python tooling |
| Source size | ~1,854 LOC across 5 source files |
| Entry point | `src/main.cpp` (`setup()` / `loop()`) |
| Tests | 37 native Unity tests over `Pet` (hardware-free) |

## Tech stack

| Category | Technology | Notes |
|---|---|---|
| MCU / board | ESP32-2432S028R (CYD) | 320 KB usable RAM budget |
| Framework | Arduino | via PlatformIO `espressif32` |
| Display | TFT_eSPI (ILI9341) | forced onto **HSPI** (`-DUSE_HSPI_PORT=1`) |
| Touch | XPT2046_Touchscreen | on its **own VSPI bus** (must stay separate from display) |
| Images | PNGdec + LittleFS | sprites decoded from flash at draw time |
| Time | WiFi + NTP (one-shot at boot) | RTC keeps time after; solar math is local |
| Persistence | NVS (`Preferences`) | care stats survive power loss |
| Test framework | Unity (`env:native`) | compiles only `pet.cpp`, no hardware |
| Art pipeline | Python + Pillow | `tools/prep_sprite.py` (`img/` → `data/`) |

## The three source modules

1. **`pet` (`pet.h` / `pet.cpp`, ~186 LOC)** — Pure game logic. Four stats (Hunger, Thirst,
   Fun, Hygiene), poop timer, decay, night-sleep, and priority-based emotion resolution.
   **No hardware includes**, fully unit-tested. This is the clean core.
2. **`ui` (`ui.h` / `ui.cpp`, ~777 LOC)** — All rendering: savanna scene, sky palettes, the
   sun/moon arc, solar-time math, drawing primitives, PNG decode, meters, buttons, poop.
3. **`main` (`main.cpp`, 891 LOC)** — Orchestration: `setup`/`loop`, touch handling,
   WiFi/NTP, the day/night driver, NVS persistence, backlight dimming, the prank-death
   Easter egg, and **~7 animation state machines** (eat, sleep, daydream, play with four
   sub-kinds, clean, idle tics, happy-face rotation).

## Where the weight is (refactor-relevant)

`pet` is small and clean. The mass — and the mixed concerns — live in `main.cpp` and
`ui.cpp`. See [architecture.md](./architecture.md) for the coupling map and the proposed
refactor seams.

## Related docs

- [index.md](./index.md) — documentation index
- [architecture.md](./architecture.md) — architecture + refactor seams
- [source-tree-analysis.md](./source-tree-analysis.md) — annotated tree
- [development-guide.md](./development-guide.md) — build / flash / test
- [STATUS.md](./STATUS.md) — hand-written handoff (rendering gotchas, screen coords)
- [PET_PROMPT.md](./PET_PROMPT.md) — AI sprite-generation prompt
