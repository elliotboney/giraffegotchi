# Giraffegotchi — Documentation Index

_Updated 2026-07-01 · post swappable-animals refactor · entry point for AI-assisted development._

## Project overview

- **Type:** Embedded (ESP32 / Arduino / C++) · layered firmware + data-driven species registry
- **Primary language:** C++ (Arduino), Python tooling, bun task scripts
- **Build system:** PlatformIO (`esp32dev` firmware env + `native` test env)
- **Architecture:** Single-threaded `loop()` with an off-screen compositing band; the pet is
  **one data descriptor in a registry**, animals swappable on-device. Layers:
  `main → {render, anim, species, core} → hardware`.

## Quick reference

- **Entry point:** `src/main.cpp` (`setup()` / `loop()` + active-species pointer + swap)
- **Pure core:** `src/pet.{h,cpp}` + `src/core/sky.{h,cpp}` — 43 native unit tests, no hardware
- **Add an animal:** drop art → `bun prep <name>` → `src/species/<name>.cpp` → register. See the [README](../README.md#adding-an-animal).
- **Board:** ESP32-2432S028R "Cheap Yellow Display" (ILI9341 + XPT2046)

## Documentation

- [Architecture](./architecture.md) — as-built layers, the species descriptor, live swap, invariants
- [Project Overview](./project-overview.md) — what it is, tech stack, module roles
- [Source Tree Analysis](./source-tree-analysis.md) — annotated directory tree
- [Development Guide](./development-guide.md) — bun commands, build/flash/test, art pipeline, adding an animal
- [STATUS.md](./STATUS.md) — rendering internals, the compositing band, display/transparency gotchas
- [PET_PROMPT.md](./PET_PROMPT.md) — AI image-gen prompt for pet sprites
- `_bmad-output/planning-artifacts/` — the architecture spine (AD-1..15) + epic/story backlog the refactor was built from
- `_bmad-output/implementation-artifacts/epic1-baseline.md` — the behavior-neutral A/B baseline

## Getting started

1. `bun setup` — one-time: `.venv` + Pillow for the art pipeline.
2. `bun native` — confirm the 43 core unit tests pass.
3. `bun compile` — build firmware; `bun flash` — firmware + sprites. See [development-guide.md](./development-guide.md) for `.env` + ports.
