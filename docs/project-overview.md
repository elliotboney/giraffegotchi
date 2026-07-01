# Project Overview — Giraffegotchi

_Updated 2026-07-01 · post swappable-animals refactor._

## What it is

A Tamagotchi-style digital pet running as **ESP32 Arduino firmware** on the
**ESP32-2432S028R "Cheap Yellow Display" (CYD)**. It began as a giraffe and is now a
**swappable-animal platform**: each animal is one data descriptor (sprites, animations, biome,
food) and you switch between them on-device. Ships with four — a giraffe (savanna), a groundhog
(meadow), a flamingo ("frances", lagoon), and a cheetah ("spot", plains). Five buttons drive four
care meters; mood, biome, and a real-time day/night cycle are
computed on-device.

## Classification

| Attribute | Value |
|---|---|
| Project type | Embedded (ESP32 / Arduino / C++) |
| Architecture | Layered firmware + data-driven species registry |
| Build system | PlatformIO; bun task scripts (`package.json`) |
| Languages | C++ (Arduino), Python (art pipeline) |
| Entry point | `src/main.cpp` (`setup()` / `loop()`) |
| Tests | 43 native Unity tests over `pet` + `sky` (hardware-free) |

## Tech stack

| Category | Technology | Notes |
|---|---|---|
| MCU / board | ESP32-2432S028R (CYD) | ~320 KB RAM |
| Framework | Arduino via PlatformIO `espressif32` | |
| Display | TFT_eSPI (ILI9341) | forced onto **HSPI** (`-DUSE_HSPI_PORT=1`) |
| Touch | XPT2046_Touchscreen | on its **own VSPI bus** |
| Images | PNGdec + LittleFS | per-species sprites decoded from flash at draw time |
| Time | WiFi + NTP (one-shot at boot) | RTC keeps time after; solar math local |
| Persistence | NVS (`Preferences`) | per-species care blocks + active id, versioned |
| Test framework | Unity (`env:native`) | compiles `pet.cpp` + `core/sky.cpp` only |
| Art pipeline | Python + Pillow (`.venv`, run via bun) | `tools/prep_sprite.py` (`img/<sp>/` → `data/<sp>/`) |

## The layers

- **core** (`pet` at `src/pet.*`, `sky` at `src/core/sky.*`) — pure logic (meters/emotions/decay/night, solar/phase math), no hardware, unit-tested.
- **species** (`species/species.h`, `registry`, `<animal>.cpp`) — the `Species`/`AnimSpec`/`Biome` data types + registry + each animal as data.
- **anim** (`anim/engine`) — the data-driven animation engine (pose floor, tics, foreground composers, food).
- **render** (`ui`) — the biome scene, compositing band, sprite decode, meters/buttons/picker.
- **io** (`io/save`) — NVS per-species persistence.
- **orchestration** (`main.cpp`) — loop, touch, WiFi/NTP, the active-species pointer + atomic swap, picker, backlight.

Adding an animal touches no engine code — just art + a descriptor + a registry line (FR10). See
[architecture.md](./architecture.md) and the README's "Adding an animal".

## Related docs

- [index.md](./index.md) · [architecture.md](./architecture.md) · [source-tree-analysis.md](./source-tree-analysis.md) · [development-guide.md](./development-guide.md) · [STATUS.md](./STATUS.md) · [PET_PROMPT.md](./PET_PROMPT.md)
