# Giraffegotchi — Documentation Index

_Generated: 2026-06-30 · initial exhaustive scan · primary entry point for AI-assisted development_

## Project overview

- **Type:** Monolith · Embedded (ESP32 / Arduino / C++)
- **Primary language:** C++ (Arduino), Python tooling
- **Build system:** PlatformIO (`esp32dev` firmware env + `native` test env)
- **Architecture:** Single-threaded Arduino `loop()` with an off-screen compositing band;
  one pure logic module (`pet`) + a rendering layer (`ui`) + orchestration (`main`)

## Quick reference

- **Entry point:** `src/main.cpp` (`setup()` / `loop()`)
- **Pure core:** `src/pet.{h,cpp}` — 37 native unit tests, no hardware
- **Board:** ESP32-2432S028R "Cheap Yellow Display" (ILI9341 + XPT2046)
- **Total source:** ~1,854 LOC across 5 files (`main.cpp` 891 · `ui.cpp` 684 · `pet` 186)

## Generated documentation

- [Project Overview](./project-overview.md) — what it is, tech stack, module roles
- [Architecture](./architecture.md) — runtime shape, coupling map, invariants, **refactor seams**
- [Source Tree Analysis](./source-tree-analysis.md) — annotated directory tree
- [Development Guide](./development-guide.md) — build / flash / test / art pipeline

## Existing documentation (hand-written)

- [STATUS.md](./STATUS.md) — full handoff: rendering internals, the compositing band, and the
  hard-won display/transparency gotchas. **Read before touching `ui.cpp`.**
- [PET_PROMPT.md](./PET_PROMPT.md) — the AI image-gen prompt for new giraffe sprites
- [../README.md](../README.md) — public-facing feature overview + setup

## Getting started

1. `pio test -e native` — confirm the 37 pet-logic tests pass.
2. `pio run -e esp32dev` — compile firmware.
3. See [development-guide.md](./development-guide.md) for flashing and the `.env` setup.

## For the planned refactor

Start with [architecture.md §6 "Refactor seams"](./architecture.md#6-refactor-seams-why-this-is-a-structural-refactor).
It maps the mixed concerns in `main.cpp`/`ui.cpp` to five candidate extractions ranked by
value-to-risk, and recommends a sequencing. When ready to plan, feed this index to the BMad
Architecture workflow (`bmad-architecture`) to ratify the spine, then break the work into
epics and stories.
