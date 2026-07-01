---
stepsCompleted: ["step-01", "step-02", "step-03"]
inputDocuments:
  - _bmad-output/planning-artifacts/architecture/architecture-giraffegotchi-2026-06-30/ARCHITECTURE-SPINE.md
  - _bmad-output/planning-artifacts/ux-designs/ux-giraffegotchi-2026-06-30/DESIGN.md
  - _bmad-output/planning-artifacts/ux-designs/ux-giraffegotchi-2026-06-30/EXPERIENCE.md
---

# giraffegotchi - Epic Breakdown

## Overview

This document decomposes the **swappable-animals refactor** into implementable stories. There is
no PRD — this is an internal refactor, so requirements are derived from the Architecture Spine
(ADs + goal) and the animal-selector UX contract. Goal: turn the hardcoded giraffe into one
**data descriptor in a species registry**, with animals switchable **on-device**, each carrying
its own **animations** (data-driven engine) and its own **biome** (environment).

## Requirements Inventory

### Functional Requirements

FR1: The device holds multiple animal species, each a distinct pet with its own sprite set and environment.
FR2: The user selects the active animal on-device: long-press BOOK (~800ms) → full-screen grid picker → tap a tile.
FR3: Switching animals is atomic — no partial or half-swapped visuals (new geometry over old sprites, half-painted biome) are ever shown.
FR4: Each animal has its own biome — sky palettes, ground, trees/grass/props, ambient critters — that changes when the animal changes.
FR5: Each animal declares its animation set as **data** (emotions, idle tics, play, etc.); the giraffe is the reference entry.
FR6: An animal may have a signature behavior that isn't expressible as data (e.g. the giraffe's ball-kick) via a named capability hook.
FR7: The active-animal selection persists across power loss (NVS) and restores on boot.
FR8: The existing care model — stats, emotions, decay, night sleep, poop, prank death — works identically for every animal.
FR9: The picker is unavailable while the pet is in the prank-"dead" state (revive first).
FR10: Adding a data-only animal requires only dropping art + writing a descriptor — no engine code changes.
FR11: An animal's food item is optional and per-species — the default is the drawn apple; a descriptor may supply its own food sprite (and eat params) that the eat animation uses when present. (Drink/water stays universal.)
FR12: Each animal keeps its **own independent care state** (per-species stats + poop). A swap never touches stats — each animal's life runs on its own clock; no shared stats, no reset on swap.

### NonFunctional Requirements

NFR1: The pure domain core (`pet`, `sky`) stays hardware-free and unit-tested under `env:native` (AD-1).
NFR2: Rendering stays flicker-free — atomic band composite/push, no direct in-box draws (AD-4).
NFR3: A swap re-allocates the pose/band buffers to the new animal's geometry and fails safe on alloc failure; a larger animal must never overflow a fixed buffer (AD-13).
NFR4: Total shipped sprite bytes fit the LittleFS partition with headroom; the build/prep step reports the budget (AD-14).
NFR5: No giraffe-specific literal (path, geometry, anchor, palette) remains outside the active species descriptor (AD-11/AD-15).
NFR6: The per-frame budget stays within headroom (~10ms band push today) as per-species reload and biome props land.
NFR7: Persistence is versioned (`SAVE_MAGIC` bump) and validates the saved species id against the registry with a default fallback — no boot-loop on a stale save (AD-8/AD-13).
NFR8: Layer dependencies stay one-way and every shared datum has a single writer (AD-2/AD-10).

### Additional Requirements

- **Brownfield — no starter template.** Extends the existing PlatformIO firmware; do not scaffold anew.
- **Target module structure:** `core/` (pet, sky) · `species/` (descriptor, registry, per-animal data) · `anim/` (engine, capability hooks) · `render/` (scene, primitives) · `io/` (save, daynight) · thin `main.cpp`.
- **Recommended sequence (spine Deferred):** (1) pure extractions `sky` + `save`; (2) `Species` descriptor + registry, giraffe as sole entry (behavior-neutral); (3) data-driven `anim` engine; (4) biome renderer; (5) runtime swap + NVS persist; (6) second animal + biome.
- **Asset layout:** per-species folders on LittleFS with identical filenames (AD-14).
- **`prep_sprite.py` change:** source art is now **transparent-background**; key on the alpha (transparent) pixels → magenta, instead of flood-filling a solid background. Edges stay hard (1-bit key, nearest downscale).
- **New long-press interaction primitive** must not break the existing edge-triggered tap in `loop()`.
- **Variable-length animation frames.** Blink is now a **single frame** for newly generated animals (the giraffe may keep its existing 3-frame blink art). Implications: (a) the data-driven engine (Epic 2) must play per-tic frame sequences of **any length** read from the descriptor — never hardcode the current 3-frame `BLINK_FR`; (b) `prep_sprite.py`'s sprite/`FRAMES` list must be **per-species** and must not assume `blink2`/`blink3` exist. Retire the giraffe's `blink2`/`blink3` only if you also drop them from its descriptor.
- **Sprite frame naming convention.** The base frame is the bare pose name; additional frames append a number (`happy`/`happy2`/`happy3`, `kick`/`kick2`, single-frame `blink`). Rename the giraffe's existing `kick1` → `kick` to match, and update the firmware sprite paths (`setKickPose`) + `prep_sprite.py` accordingly.
- **Source art layout.** Body poses live in `img/<animal>/` (e.g. `img/groundhog/happy.png`); props — food, ball — live in `img/<animal>/objects/` (e.g. `img/groundhog/objects/food.png`), mirroring the giraffe's existing `img/objects/` convention. `prep_sprite.py` reads both and flattens them into `data/<animal>/` (per AD-14, one flat folder per species).

### UX Design Requirements

UX-DR1: A long-press primitive on the BOOK rect (continuous press ≥ ~800ms) opens the picker, firing once, disambiguated from BOOK's tap=read and fast-mash=revive.
UX-DR2: A full-screen **modal** picker: a grid of animal tiles, each showing the animal's dedicated `icon` sprite (fallback: scaled happy sprite) + lowercase name, current animal highlighted with an outline.
UX-DR3: Tapping a tile commits the swap immediately (no confirm step); a `back` tile cancels with no change.
UX-DR4: A transient `swapping…` indicator during the atomic reload beat.
UX-DR5: The picker is disabled in the dead state.
UX-DR6: Neutral dark picker panel (no biome behind the grid); reuse existing r8 rounded-rect + FONT2/FONT4; ≥44px touch targets; white-on-dark contrast.
UX-DR7: The chosen animal persists to NVS; the device boots straight into it.

### FR Coverage Map

FR1: Epic 4 — device holds multiple species, selectable live
FR2: Epic 4 — on-device selector (long-press BOOK → grid → tap)
FR3: Epic 4 — atomic swap, no partial visuals
FR4: Epic 3 — per-animal biome rendered from data
FR5: Epic 1 (structure) + Epic 2 — animation set declared as descriptor data
FR6: Epic 2 — signature behaviors via capability hooks
FR7: Epic 4 — active-animal selection persists to NVS, restores on boot
FR8: Epic 1 — care model works identically for every animal (giraffe unchanged through the registry)
FR9: Epic 4 — picker disabled in the dead state
FR10: Epic 5 — a data-only animal needs only art + descriptor, no engine code
FR11: Epic 2 (eat animation reads descriptor food, defaults to apple) + Epic 5 (groundhog's own food)
FR12: Epic 4 (per-species save blocks, swap preserves stats) + Epic 5 (groundhog boots at its own state)

NFR1: Epic 1 — pure core (pet, sky) hardware-free + native-tested
NFR2: Epics 2–4 — flicker-free atomic band render preserved
NFR3: Epic 4 (built) / Epic 5 (validated) — buffer re-alloc on swap, no overflow
NFR4: Epic 5 — flash budget fits with headroom; build reports it
NFR5: Epic 1 — no giraffe-isms outside the descriptor
NFR6: Epics 4–5 — per-frame budget stays within headroom
NFR7: Epic 1 (save module) + Epic 4 — versioned save, registry-validated id, no boot-loop
NFR8: Epic 1 — one-way layer deps, single-writer-per-datum

## Epic List

### Epic 1: Species Foundation
Extract the pure logic (`sky`, `save`) and route the giraffe entirely through a `Species` descriptor + registry (giraffe as the sole entry). The device runs the identical giraffe, but no giraffe-specific literals remain in code and the tested surface grows. Behavior-neutral groundwork that de-risks every later epic.
**Roundtable refinements:** (a) the pose/band buffers are **sized from the active descriptor's geometry** from day one — no fixed 150×160 assumption baked in, so Epic 4 has nothing to unwind (Amelia). (b) Include a **behavior-neutral verification story** (on-device A/B against the pre-refactor build) since `ui`/`main` have no automated tests (John).
**FRs covered:** FR5 (structure), FR8 · NFR1, NFR3 (variable geometry built here), NFR5, NFR7 (save), NFR8

### Epic 2: Data-Driven Animations
Replace the hardcoded animation state machines with a generic data-driven engine plus named capability hooks for signature moves. The giraffe's emotions, idle tics, eat/sleep/play and the ball-kick become descriptor data — animating identically, but now as data.
**Roundtable refinement:** introduce the **groundhog as a minimal second species (test fixture)** here — different geometry/anchors (squat body, no long neck) — so the data model is pressure-tested *continuously* from this epic on, not only when real art lands in Epic 5 (Grumbal's 3am worry; Winston conceded). The groundhog graduates to a complete pet in Epic 5.
**Food:** the eat animation reads the food from the active descriptor, falling back to the drawn **apple** primitive when a species defines none (FR11). Optional food sprite + eat params live in the descriptor.
**FRs covered:** FR5, FR6, FR11 (mechanism) · NFR2, NFR3 (validated via groundhog fixture)

### Epic 3: Swappable Worlds
Make the scene render the active biome from data (palettes, ground/horizon, grass/trees/props, ambient critters). Today's savanna becomes the giraffe's biome data, so the environment travels with the species.
**FRs covered:** FR4 · NFR2

### Epic 4: Live Animal Swap
Runtime atomic species swap (cancel anims → re-alloc buffers to the new geometry → decode → full biome repaint), NVS persistence of the active animal, and the on-device selector UX (long-press BOOK → modal grid → tap to swap). The payoff: switch animals on the device and it sticks through a reboot.
**Roundtable refinements:** (a) two-phase swap — prove the **atomic swap via a dev-only serial trigger** first, then the **selector UI** on top (Sally + John); the serial trigger is removed before the epic closes. (b) **Per-animal care state** — the save holds a per-species block; a swap preserves each animal's own stats (Sally + Grumbal). (c) Story 4.1 ACs walk the **swap-during-kick** edge case explicitly (Boundary).
**FRs covered:** FR1, FR2, FR3, FR7, FR9, FR12 · NFR2, NFR3 (built), NFR6, NFR7 · all UX-DRs

### Epic 5: Second Animal (Groundhog graduates)
The **groundhog** fixture from Epic 2 graduates to a complete pet: full sprite set (art via the prompt) + its own biome. Change `prep_sprite.py` to key transparent art (alpha→magenta) and **size assets by type** (body vs icon vs food). Validate the flash budget with two full animal sets on device. Proves a data-only animal needs no engine code (FR10).
**FRs covered:** FR10, FR11 (groundhog's own food), FR12 (groundhog boots at its own state) · NFR3 (validated), NFR4, NFR6 · prep-pipeline additional requirement

---

## Epic 1: Species Foundation

Extract the pure logic (`sky`, `save`) and route the giraffe entirely through a `Species` descriptor + registry, with buffers sized from descriptor geometry. The device runs the identical giraffe, but no giraffe-specific literals remain in code and the tested surface grows. Behavior-neutral groundwork that de-risks every later epic.

### Story 1.1: Establish the behavior-neutral verification baseline

As the owner,
I want a documented A/B checklist plus a captured golden reference from the current pre-refactor build,
So that every later Epic 1 story is signed off as a true no-op against a fixed baseline instead of by inspection.

**Acceptance Criteria:**

**Given** the current pre-refactor firmware
**When** a golden reference is captured
**Then** a documented A/B checklist exists (sprite per emotion, meters, idle tics, eat/drink/play/clean, day/night arc, save/restore across a reboot, prank death + revive), each item recording the pre-refactor result.

**Given** the native test suite (pet tests)
**When** it runs
**Then** it passes, and its green state is recorded as the baseline.

**Given** this baseline
**When** any later Epic 1 story claims "identical behavior"
**Then** it is verified against this checklist, not by inspection alone.

### Story 1.2: Extract the `sky` module

As a developer,
I want the solar and sky-phase math extracted into a hardware-free `sky` module with unit tests,
So that the day/night calculations are isolated and testable before the refactor touches rendering.

**Acceptance Criteria:**

**Given** the solar/phase functions currently in `ui.cpp` (`solarTimes`, `celestialPos`, `skyPhaseFor`, `dayOfYear`, `d2r`/`r2d`)
**When** they are moved to `src/core/sky.{h,cpp}`
**Then** the module includes no Arduino/TFT/FS headers and compiles under `env:native`
**And** new Unity tests cover it, pinning sunrise/sunset for a known date/lat/lon to the pre-extraction values.

**Given** the firmware build for `esp32dev`
**When** it runs on device
**Then** the day/night behavior (sky phases, sun/moon arc) is unchanged from before the extraction.

### Story 1.3: Extract the `save` module

As a developer,
I want NVS persistence extracted into an `io/save` module,
So that save/load sits behind a clear interface before swap logic is added later.

**Acceptance Criteria:**

**Given** `PetSave`, `saveState`, `loadState`, and the dirty/throttle logic currently inline in `main.cpp`
**When** they are moved to `src/io/save.{h,cpp}`
**Then** `main.cpp` calls the module's interface and no `Preferences`/NVS code remains inline.

**Given** a saved care state
**When** the device reboots
**Then** care stats and the prank-death flag restore identically to the pre-extraction behavior.

**Given** rapidly changing stats
**When** saves are triggered
**Then** a write still occurs at most once per 5 seconds and only when dirty.

### Story 1.4: Define the `Species` descriptor and registry

As a developer,
I want a `Species` descriptor type and a registry holding the giraffe as the sole entry,
So that later epics have one place for all per-animal data.

**Acceptance Criteria:**

**Given** the giraffe's facts (asset folder, geometry `W/H/X/Y` + horizon + footprint, anchor points)
**When** I define `src/species/species.h`, `src/species/giraffe.cpp`, and a registry with an active-species accessor
**Then** they compile and the accessor returns the giraffe.

**Given** the descriptor type
**Then** it declares fields (structure only, no behavior yet) for the animation set, biome, optional food, and icon to be populated by later epics.

**Given** the build
**When** it runs on device
**Then** behavior is unchanged — the descriptor exists but nothing reads it yet.

### Story 1.5: Route giraffe sprite paths through the descriptor

As a developer,
I want every giraffe sprite path resolved from the active descriptor's asset folder,
So that no `/giraffe_*` path literal remains in code (NFR5).

**Acceptance Criteria:**

**Given** `emotionPath()` and the frame lists (`HAPPY_FRAMES`, tic frames, kick poses, dead)
**When** they are refactored
**Then** each resolves its path from the active descriptor's asset folder plus the pose name, and no `/giraffe_*` string literal remains anywhere in render/main.

**Given** the giraffe is the active species
**When** each emotion and frame renders
**Then** the same sprite loads as before (behavior unchanged).

**Given** a pose the descriptor does not define
**When** it is requested
**Then** the existing missing-asset fallback is used (no crash, visible placeholder).

### Story 1.6: Size buffers and geometry from the descriptor

As a developer,
I want the pose/band buffers and all giraffe geometry read from the descriptor instead of fixed constants,
So that a different-sized animal can never overflow a fixed buffer later (NFR3).

**Acceptance Criteria:**

**Given** the `GIRAFFE_W/H/X/Y`, horizon, and footprint usages in render/main
**When** they are refactored
**Then** each reads from the active descriptor and no compile-time giraffe geometry constant remains.

**Given** `giraffeBuf` and the `skyBand` sprite
**When** the active species is set
**Then** both are allocated from the descriptor's geometry and the PNG-decode stride follows the active width (not a fixed `IMG_W`).

**Given** the giraffe is active
**When** it renders
**Then** the output is pixel-identical to before (same size and position).

**Given** a larger descriptor geometry (hypothetical)
**When** buffers are allocated
**Then** no write exceeds the allocated size, and an allocation failure keeps the current species (fail-safe) rather than crashing.

---

## Epic 2: Data-Driven Animations

Replace the hardcoded animation state machines with a generic data-driven engine plus named capability hooks for signature moves. The giraffe's emotions, idle tics, eat/sleep/play and the ball-kick become descriptor data — animating identically, but now as data. The groundhog fixture enters here to pressure-test the model against a genuinely different animal.

### Story 2.1: Animation data model, engine, and pose-writer contract

As a developer,
I want an animation data model and a generic engine that resolves a single pose-buffer writer by priority,
So that the giraffe's pose-layer animations are driven by data instead of hardcoded state machines.

**Acceptance Criteria:**

**Given** an `AnimSpec` type (frame list, anchor reference, cadence, layer = `pose` | `foreground`)
**When** the engine is defined with `start(now)` / `tick(now)` / `compose(band)`
**Then** it compiles, is species-agnostic, and reads specs from the active descriptor.

**Given** the emotion base sprite (the simplest pose-writer)
**When** it is migrated to an `AnimSpec`
**Then** exactly one writer updates `giraffeBuf` per frame, resolved by the priority `dead > kick > tic > emotion` (AD-5) — never two in one frame — and the emotion sprite renders identically to the Story 1.1 baseline.

### Story 2.2: Migrate happy-rotation and idle tics

As a developer,
I want the happy-face rotation and idle tics migrated onto the engine,
So that the remaining pose-layer animations are data-driven and variable-length.

**Acceptance Criteria:**

**Given** the happy-face rotation and idle tics (blink/ears/tail)
**When** they are migrated to `AnimSpec`s under the pose-writer priority
**Then** they play through the engine and never fight the emotion base for `giraffeBuf` (one writer per frame, AD-5).

**Given** a tic frame list of length 1 or 3
**When** it plays
**Then** the engine plays any length (blink works as 1 frame or 3) with no hardcoded 3-frame assumption.

**Given** the giraffe is active
**When** it idles
**Then** happy-rotation cadence and tic timing match the Story 1.1 baseline.

### Story 2.3: Migrate band-composited animations

As a developer,
I want the foreground animations migrated to data-driven composers,
So that eat, sleep, daydream, bubbles, and butterfly draw into the band without touching the pose buffer.

**Acceptance Criteria:**

**Given** the eat item, sleep Z's, daydream, bubbles, and butterfly
**When** they are migrated to `foreground`-layer `AnimSpec`s
**Then** each composes into the `skyBand` (in front of the giraffe) and none writes `giraffeBuf` (AD-5).

**Given** their anchors (mouth, sleep-Z, daydream)
**When** they play
**Then** positions are read from the active descriptor's anchor points, not hardcoded constants.

**Given** the giraffe is active
**When** each plays
**Then** the visuals and timing are identical to before and remain flicker-free via the single atomic push (AD-4).

### Story 2.4: Capability hooks for signature moves

As a developer,
I want the ball-kick and kite implemented as named capability hooks the descriptor opts into,
So that behaviors that can't be pure data don't force engine changes or leak into every animal (FR6).

**Acceptance Criteria:**

**Given** the ball-kick (pose swaps + ball roll/volley physics + in-box band draw + out-of-box direct draw) and the kite (direct swoop)
**When** they are implemented as named hooks
**Then** the descriptor declares which hooks a species supports and the engine invokes them by name.

**Given** a species that does not declare the kick hook
**When** PLAY cycles
**Then** the kick is simply absent — no crash, and no giraffe-specific code runs.

**Given** the kick hook runs
**Then** it still obeys AD-5 (owns the pose buffer exclusively while active) and AD-4 (in-box via band, out-of-box via viewport clip).

**Given** the giraffe is active
**When** PLAY cycles butterfly → bubbles → kite → kick
**Then** behavior is identical to before.

### Story 2.5: Food from the descriptor

As a player,
I want each animal to be able to eat its own food,
So that feeding fits the animal (default apple otherwise) (FR11).

**Acceptance Criteria:**

**Given** the eat animation
**When** it plays
**Then** it uses the food sprite + eat params from the active descriptor if present, else falls back to the drawn apple primitive.

**Given** the giraffe (no custom food defined)
**When** it is fed
**Then** the apple animation is identical to before.

**Given** a descriptor with a food sprite
**When** the animal is fed
**Then** that sprite is used in the eat animation at the mouth anchor.

**Given** drink/water
**When** used
**Then** it is unchanged (the universal glass) regardless of species.

### Story 2.6: Groundhog fixture enters

As a developer,
I want a minimal groundhog descriptor plus a dev-only compile-time active-species override,
So that the engine and descriptor are validated against a genuinely different animal before the runtime swap exists (NFR3).

**Acceptance Criteria:**

**Given** a groundhog descriptor (distinct geometry — squat, no long neck — distinct anchors, its own animation set, food from `img/groundhog/objects/`)
**When** it is added to the registry
**Then** it compiles alongside the giraffe.

**Given** a dev-only build flag (e.g. `-D ACTIVE_SPECIES=groundhog`)
**When** the firmware is flashed
**Then** the groundhog boots as the active animal and renders and animates correctly with its own geometry, anchors, and sprites — no giraffe assumptions leak in.

**Given** the groundhog is active
**When** buffers allocate
**Then** they size to the groundhog geometry with no overflow (NFR3 validated on a real second animal).

**Given** the groundhog declares a different set of signature moves than the giraffe
**When** PLAY cycles
**Then** only its declared capability hooks run (FR6/FR10 proven).

---

## Epic 3: Swappable Worlds

Make the scene render the active biome from data (palettes, ground/horizon, grass/trees/props, ambient critters). Today's savanna becomes the giraffe's biome data, so the environment travels with the species.

### Story 3.1: Biome data model and palette ownership

As a developer,
I want a `Biome` data type that owns each world's palette table and layout,
So that the scene renders whatever biome the active species declares instead of a hardcoded savanna.

**Acceptance Criteria:**

**Given** a `Biome` type (8-phase sky/ground palette table, horizon, grass-blade array, tree/prop positions, ambient-critter set)
**When** it is defined and attached to the descriptor
**Then** the giraffe's savanna is expressed as the reference biome's data.

**Given** `setSkyPhase`
**When** a phase changes
**Then** it indexes the **active** biome's palette table (AD-15), and `render/scene` remains the sole writer of `SKY_COLOR`/`GROUND_COLOR` and the phase-id holder (AD-10).

**Given** the giraffe is active
**When** day/night runs
**Then** sky/ground colors and phase transitions are identical to before.

### Story 3.2: Migrate scene props to biome data

As a developer,
I want the scene's props read from the active biome's data,
So that no hardcoded savanna geometry remains in scene code (AD-15).

**Acceptance Criteria:**

**Given** `GRASS[]`, tree positions, stars, and the ambient critters (fireflies, daytime butterfly)
**When** they are migrated
**Then** each is read from the active biome data and no hardcoded prop array or literal remains in scene code.

**Given** the giraffe savanna
**When** the scene animates
**Then** grass sway, trees, stars, and critters are visually identical to before.

**Given** the `restoreBg` / band mechanics
**Then** they resolve horizon/footprint from the active descriptor (AD-11) and are unchanged in behavior.

### Story 3.3: Biome-specific prop draw hooks

As a developer,
I want biome-specific props implemented as named draw hooks the biome opts into,
So that different worlds draw different props without scene code knowing each one.

**Acceptance Criteria:**

**Given** props that differ by world (the acacia tree today; pine/coral later)
**When** they are implemented as named draw hooks
**Then** the biome declares which prop hooks it uses and the scene invokes them by name.

**Given** the savanna biome
**When** rendered
**Then** the acacia tree draws via its hook, visually identical to before.

**Given** a biome that declares no tree hook
**When** rendered
**Then** no tree draws — no crash, and no savanna-specific code runs.

### Story 3.4: Groundhog biome

As a player,
I want the groundhog to live in its own world,
So that swapping animals changes the environment, not just the sprite (FR4).

**Acceptance Criteria:**

**Given** the groundhog fixture
**When** given a distinct biome (different palette table, ground, props/critters)
**Then** it compiles and attaches to the groundhog descriptor.

**Given** the dev-only groundhog build
**When** flashed
**Then** the world visibly differs from the savanna and the day/night cycle runs against the groundhog biome's palette table.

**Given** a swap between giraffe and groundhog (dev override)
**When** the active species changes
**Then** the biome changes with it.

---

## Epic 4: Live Animal Swap

Runtime atomic species swap, NVS persistence, and the on-device selector UX. Sequenced in two phases: prove the swap mechanism via a dev-only serial trigger, then build the selector UI on top once the systems risk has cleared.

### Story 4.1: Atomic swap mechanism (dev serial trigger)

As a developer,
I want the atomic species-swap sequence triggered by a dev-only serial command,
So that the systems risk is proven before any UI depends on it (FR3).

**Acceptance Criteria:**

**Given** a serial command to swap (giraffe ↔ groundhog)
**When** received
**Then** the swap is **latched** and applied at exactly one point — the top of `loop()`, before `pet.update()` and any animation tick (AD-13).

**Given** the swap applies
**When** it runs
**Then** the sequence is: cancel all in-flight animations + reset pose state → free and re-alloc `giraffeBuf`/`skyBand` to the new geometry (decode stride follows the new width) → decode the new species' sprites → full-screen biome repaint + phase re-resolve; no frame straddles two species.

**Given** a larger target animal
**When** buffers re-alloc
**Then** no write exceeds the allocation, and an allocation failure keeps the current species (fail-safe).

**Given** a swap completes
**Then** no partial or half-swapped visual is ever shown.

**Given** a swap triggered mid-animation (e.g. during a kick with the beach ball in flight)
**When** the swap applies at the top of `loop()`
**Then** all in-flight animations cancel and the full repaint clears any out-of-box element (the ball leaves no frozen or orphaned frame) — this path is walked explicitly, not assumed.

**Given** a swap
**When** it applies
**Then** it never touches care stats — the swap only changes which species is active; each animal keeps its own state (FR12).

### Story 4.2: Persist per-animal state to NVS

As a player,
I want each animal remembered with its own care state across power loss,
So that the device boots straight into my chosen animal and every pet keeps its own life (FR7, FR12).

**Acceptance Criteria:**

**Given** the save layout
**When** it is extended
**Then** it stores a **per-species care block** (stats + poop + prank-death) for each known animal plus the active-species id; `SAVE_MAGIC` is bumped and the layout stays versioned (AD-8).

**Given** a saved active-species id on boot
**When** it is valid in the registry
**Then** that species becomes active and restores its own stats;
**And** when it is unknown (stale/corrupt save), the default species loads with no boot-loop (NFR7).

**Given** a swap from animal A to animal B
**When** it completes
**Then** A's stats are preserved and B resumes its own last-saved stats — neither is reset or shared (FR12);
**And** the new active id and any changed block are persisted (≤ once/5s throttle).

### Story 4.3: Long-press BOOK primitive

As a player,
I want to hold BOOK to open the animal picker,
So that I can switch animals without a new on-screen button.

**Acceptance Criteria:**

**Given** a continuous press inside the BOOK rect for ≥ ~800ms
**When** the threshold is crossed
**Then** a single long-press event fires (once per hold) and opens a **placeholder screen** ("picker coming") — the full picker is built in Story 4.4 (UX-DR1).

**Given** a quick BOOK tap
**Then** it still means read;
**And** a fast BOOK mash while dead still means revive — the three gestures never overlap.

**Given** the existing edge-triggered tap in `loop()`
**When** the long-press primitive is added
**Then** tap behavior for all buttons is unchanged.

**Given** the pet is in the dead state
**When** BOOK is long-pressed
**Then** the picker does not open (FR9/UX-DR5).

### Story 4.4: Animal-select picker screen

As a player,
I want a full-screen grid to pick my animal,
So that switching is one clear tap.

**Acceptance Criteria:**

**Given** the picker opens
**When** rendered
**Then** it is a full-screen modal grid of tiles — each the species' `icon` sprite (fallback: scaled `happy`) + lowercase name — with the current animal outlined, plus a `back` tile, on the neutral dark panel (DESIGN.md, UX-DR2/6).

**Given** a tile tap on a non-current animal
**When** committed
**Then** a transient `swapping…` shows and the atomic swap (4.1) + NVS persist (4.2) run, returning to the pet screen showing the new animal in its biome (FR1/2/3, UX-DR3/4).

**Given** the `back` tile, or a tap on the current animal
**When** tapped
**Then** the picker closes with no change.

**Given** the tiles
**When** laid out
**Then** touch targets are ≥ 44px and text is white-on-dark (UX-DR6).

**Given** the picker UI now drives swaps
**When** Story 4.4 is complete
**Then** the dev-only serial swap trigger from Story 4.1 is removed (or gated behind a debug build flag) — no swap backdoor ships to production.

---

## Epic 5: Second Animal (Groundhog graduates)

The groundhog fixture graduates to a complete pet: full sprite set + its own biome. The art pipeline is reworked for transparent per-species source, and the flash budget is validated with two full animal sets on device.

### Story 5.1: `prep_sprite.py` rework for transparent per-species art

As a developer,
I want the art pipeline to key transparent source art per species,
So that adding a new animal is drop-art-and-run.

**Acceptance Criteria:**

**Given** transparent-background source PNGs
**When** prep runs
**Then** it fills the alpha (transparent) pixels with magenta (the 1-bit key) instead of flood-filling a solid background, keeping hard edges (nearest downscale).

**Given** `img/<animal>/` (body poses) and `img/<animal>/objects/` (food, ball)
**When** prep runs for an animal
**Then** all are processed and flattened into `data/<animal>/` with matching filenames (AD-14).

**Given** a per-species pose set
**When** prep runs
**Then** it does not assume `blink2`/`blink3` (or any fixed pose) exists — it processes whatever poses the animal has, including `icon` and `food`.

**Given** assets of different kinds (body poses ~150×160, `icon` ~64px badge, `food`/ball small items)
**When** prep processes them
**Then** each is sized/downsampled by its **asset type** — an icon is not run through the 150×160 body-sprite pipeline — so a small badge doesn't become a 150×160 image.

### Story 5.2: Groundhog complete art and descriptor

As a player,
I want the groundhog as a real, selectable pet,
So that I have a second full animal (FR10).

**Acceptance Criteria:**

**Given** the full groundhog art (19 body poses + `icon` + `food`) processed into `data/groundhog/`
**When** the descriptor is completed (geometry, anchors, animation set, biome, food, icon)
**Then** the groundhog is a complete species entry.

**Given** the runtime swap (Epic 4)
**When** the groundhog is selected
**Then** it is fully playable — care model, animations, biome, and food all correct.

**Given** the groundhog is selected for the first time (no saved block yet)
**When** it loads
**Then** it starts at full stats (a fresh pet), and thereafter keeps its own independent state across swaps and reboots (FR12).

**Given** the dev-only compile-time active-species override
**When** this story completes
**Then** it is removed — the groundhog is reached only through the real selector.

### Story 5.3: Flash-budget validation

As a developer,
I want the two-animal build validated against the flash partition,
So that shipping animals can't silently overflow LittleFS (NFR4).

**Acceptance Criteria:**

**Given** two full animal sets (giraffe + groundhog) on LittleFS
**When** the build/prep step runs
**Then** it reports the total sprite bytes and the partition headroom.

**Given** the two-animal build on device
**When** animals are swapped repeatedly
**Then** there is no frame-budget regression (band push stays within headroom, NFR6) and no memory growth across swaps.

**Given** the partition is near capacity
**When** the budget would be exceeded
**Then** the build/prep step surfaces it (fails or warns) rather than silently truncating.
