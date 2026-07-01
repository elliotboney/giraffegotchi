# CLAUDE.md — giraffegotchi

Tamagotchi-style digital pet on an ESP32 **"Cheap Yellow Display" (CYD)**, ILI9341 240×320 +
XPT2046 touch. PlatformIO / Arduino / C++. It's a **swappable-animal platform**: each animal is
one data descriptor (sprites, animations, biome, food), switchable on-device. Ships four:
giraffe (savanna), groundhog (meadow), flamingo/"frances" (lagoon), cheetah/"spot" (plains).

## Commands (bun scripts — `bun run help` lists them)

- `bun compile` — build · `bun upload` — flash firmware · `bun uploadfs` — flash sprites (`data/`)
- `bun flash` — firmware + sprites · `bun native` — 43 unit tests · `bun monitor` — serial
- `bun run cleanart <sp>` — prep raw art in `img/<sp>/` (bg-remove + align frames; `.backups/` + `cleanart:revert`)
- `bun prep [species]` — sprite conversion (`img/<sp>/` → `data/<sp>/`) · `bun setup` — one-time `.venv`+Pillow+numpy
- Rule: `bun upload` for code changes; `bun flash`/`uploadfs` when `data/` sprites changed.
- Art flow: raw art → `cleanart` (clean transparent source) → `prep` (sprites). `docs/PET_PROMPT.md` = the generation prompt.

## Architecture (layered, one-way: `main → {render, anim, species, core} → hw`)

- `src/pet.{h,cpp}` + `src/core/sky.{h,cpp}` — **core**: pure logic, no hardware, native-tested. (`pet` stayed at `src/`; only `sky` moved to `core/`.)
- `src/species/` — `species.h` (Species/AnimSpec/Biome/FoodItem types + Capability), `registry.{h,cpp}`, and each animal as data (`giraffe.cpp`, `groundhog.cpp`, `flamingo.cpp`, `cheetah.cpp`).
- `src/anim/engine.{h,cpp}` — data-driven animation engine (pose floor, tics, foreground composers, food).
- `src/ui.{h,cpp}` — **render**: biome scene, compositing band, sprite decode, meters/buttons/picker.
- `src/io/save.{h,cpp}` — NVS per-species care blocks + active-species id (versioned, `SAVE_MAGIC` 0x69).
- `src/main.cpp` — orchestration: loop, touch, WiFi/NTP, active-species pointer + atomic swap, picker, backlight.

Full detail: `docs/architecture.md`. Build/flash/art: `docs/development-guide.md`. Rendering
gotchas: `docs/STATUS.md`. Plan of record: `_bmad-output/planning-artifacts/` (spine AD-1..15).

## Adding an animal

Data + art, **no engine changes**. Generate art (`docs/PET_PROMPT.md`) into `img/<name>/` →
`bun run cleanart <name>` (clean+align) → `bun prep <name>` → add `src/species/<name>.cpp`
(copy `groundhog.cpp`) → register in `registry.cpp` → `bun flash`. Full steps: README "Adding an animal".
**AI-agent playbook: `docs/adding-an-animal-agent.md`** (every animal defines its own food + biome).

## Invariants / gotchas (these bite — don't relearn them)

- **Nothing species-specific outside the `Species` descriptor** (paths, geometry, anchors, palette, props, food).
- **Every animal defines its OWN food + biome** (project rule — no drawn-apple fallback, no borrowed world). A missing food sprite safely no-ops (renderPose fails → buffer freed → nothing drawn), so the descriptor can ship before the art.
- **The per-species food cache keys on the `FoodItem*`, not the sprite name** (`engine.cpp` `s_foodItem`). Two species can both name their sprite `"food"`; identical string literals pool to one pointer across files, so keying on the name shows the *previous* species' food after a swap. Don't "simplify" it back to the name.
- **Picker icons are fixed 64×64** — `decodePng` copies each line at the PNG's native width (no scaling), so tiles can't shrink below 64px wide. The grid is 4-across and adapts rows + back-bar position + vertical centering to species count (`pickerTop`/`pickerTile`).
- **`TFT_eSprite::pushImage` has no working transparent overload** — composite into the band by hand (byte-swap + key-skip + bounds-clip), like `composeSkyBand` / `blitFoodToBand`. `pushImage(...,key)` on a sprite can run off-bounds and **freeze the device**.
- **Magenta `0xF81F` is the transparency key**, read at runtime from `giraffeBuf[0]`; never inside a silhouette. Keep `prep_sprite.py MAGENTA_RGB` in lockstep.
- **All erases go through `restoreBg`** (never flat fill; panel `readRect` is broken on this CYD).
- **One pose-buffer writer per frame**, priority `dead > kick > tic > emotion`. Foreground anims compose into the band, never touch the pose buffer.
- **Anything crossing the pet box** splits: out-of-box direct (viewport clip) + in-box band composite.
- **Display on HSPI, touch on VSPI** (separate buses). Touch inverted for the 180° mount.
- Species swap is **latched, applied only at the top of `loop()`** (atomic; no frame straddles two species). Pose buffer is allocated once at the max species size — never realloc on swap.

## Testing

- `bun native` — 43 Unity tests over `pet` + `sky` (hardware-free; the automated safety net). Keep core hardware-free.
- render/anim/main are hardware-bound → verify on-device (Elliot flashes + watches; serial traces `[daynight]`/`[save]`/`[swap]`/`[prank]`). Behavior-neutral A/B baseline: `_bmad-output/implementation-artifacts/epic1-baseline.md`.

## State (2026-07-01)

Swappable-animals refactor **complete**, all 5 epics on `main`. Four species shipping:
giraffe, groundhog, **flamingo** ("frances", lagoon), **cheetah** ("spot", plains) — both added
this session (descriptor + own biome + own food), committed + pushed (`ff006ee`). Also this
session: food-cache swap bug fixed (key on `FoodItem*`), `cleanart` re-run crash fixed (pads
frames to a common size), picker relaid-out (4-across, small title, adaptive back-bar + vertical
centering), and **`docs/adding-an-animal-agent.md`** added — the AI-agent playbook for adding
animals (ask the human for the name; every animal defines its own food + biome). `sleepy` is the
night-sleep pose (draw eyes-closed; prompt now specs a sleep mask + hat). All in the working tree
is committed; next step is an on-device `bun flash` to verify the two new animals + picker + food.

## Working style

Elliot: work story-by-story, commit per story, verify on the physical device, often edits art
in parallel. Direct + momentum. Commit/push only when asked. End commit messages with the
`Co-Authored-By` trailer.
