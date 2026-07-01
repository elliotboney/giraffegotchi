---
name: giraffegotchi-selector
status: final
created: '2026-06-30'
updated: '2026-06-30'
scope: How a user switches the active animal on-device. Behavior, states, and flow for the selector; visual identity lives in DESIGN.md.
sources: [_bmad-output/planning-artifacts/architecture/architecture-giraffegotchi-2026-06-30/ARCHITECTURE-SPINE.md]
design_ref: ./DESIGN.md
---

# Experience — Giraffegotchi Animal Selector

## Foundation

- **Form factor:** one embedded touchscreen — ESP32 CYD, 320×240 landscape, **resistive single-touch**. No swipe, no multitouch, no hover, no keyboard. Every interaction is a press inside a rect.
- **UI system:** none. Custom TFT_eSPI primitives; the selector inherits the firmware's button/meter idiom (see DESIGN.md). No web/native toolkit.
- **Modality:** the picker is a **full-screen modal** over the pet screen. Only one screen is visible at a time.

## Information Architecture

Two surfaces, one round-trip:

```
PET SCREEN  ──(long-press BOOK ~800ms)──▶  PICKER (modal grid)
    ▲                                          │
    └──────(tap a tile → swap)  /  (tap back)──┘
```

- **Pet screen** — the existing gameplay screen (giraffe/animal + meters + 5 buttons). Unchanged except that BOOK gains a long-press.
- **Picker** — a grid of animal tiles + a `back` affordance. The only new surface. Closes back to the pet screen either way (swap or cancel).

Surface closure: the single stated need — *switch the active animal* — is delivered by the picker; the picker is reached from the pet screen and always returns there. Closed.

## Voice and Tone

Playful, tiny, lowercase-friendly (space is scarce). Title: **`PICK YOUR PAL`**. Tile labels: the animal's lowercase name (`giraffe`, `zebra`). Cancel: **`back`**. Optional transient: **`swapping…`** during the reload beat. No sentences; there's no room.

## Component Patterns

- **`picker_tile`** (visual in DESIGN.md) — shows the animal's dedicated **`icon` sprite** (falls back to a scaled `happy` sprite if an animal ships no icon) + lowercase name. Tap-to-select. On tap: commit the swap immediately (no separate confirm step — selection *is* the action). The current animal's tile shows a highlight outline but is still tappable (tapping it = a no-op close, harmless).
- **`back_button`** — tap closes the picker with **no change**; returns to the pet exactly as it was.
- **`long_press_zone`** (the BOOK rect) — a **continuous** press held ≥ `HOLD_MS` (~800 ms) inside the BOOK rect opens the picker, firing **once** per hold. A quick tap still means *read*; a fast-mash while dead still means *revive* (unchanged). The three BOOK gestures are disambiguated by duration/state, never overlapping.

## State Patterns

| State | Trigger | Behavior |
| --- | --- | --- |
| **Pet (normal)** | default | gameplay; BOOK tap=read, hold=open picker |
| **Pet (dead / prank)** | fast-mash killed it | **picker disabled** — long-press BOOK does nothing; revive first (keeps the death gag intact) `[ASSUMPTION]` |
| **Picker open** | long-press BOOK | modal grid; world keeps ticking underneath (no decay pause), but the pet is not drawn |
| **Swapping** | tap a non-current tile | brief `swapping…`; atomic reload per AD-13 (cancel anims → realloc buffers to new W×H → decode new sprites → full biome repaint → persist to NVS) |
| **Returned** | swap done, or `back` | pet screen repainted; on swap it shows the new animal in its biome |

- **No partial state is ever visible:** because the swap is atomic (AD-13), the user never sees new geometry over old sprites or a half-painted biome. They see the picker, then the new world.
- **Persistence:** the chosen animal is written to NVS during the swap (AD-13); after a power cut the device boots straight into that animal, no re-pick.

## Interaction Primitives

- **Long-press** — continuous touch inside a rect for ≥ `HOLD_MS` (~800 ms) fires one event on threshold cross; releasing earlier falls through to the rect's tap meaning. (First long-press primitive in the firmware; must not break the existing edge-triggered tap in `loop()`.)
- **Tap** — single press inside a rect (existing primitive), used for tiles and `back`.
- **No** swipe, drag, long-drag, or double-tap — resistive touch makes them unreliable; the design avoids them entirely.

## Accessibility Floor

- **Touch targets** ≥ 44 px — tiles are 64×72, `back` reuses the 60×38 button size. Comfortable for a fingertip on resistive glass.
- **Contrast** — white text on dark tiles / panel (inherits the existing high-contrast scheme).
- **No timed-out selection** — the picker waits indefinitely; nothing auto-dismisses out from under the user. (Backlight auto-dim still applies and wakes on any touch, unchanged.)
- **Single-input safe** — every action is one discrete press; nothing requires holding-plus-tapping or two points.
- **No audio dependency** — there is no sound subsystem; all feedback is visual.

## Key Flow — Elliot swaps to the penguin

1. Elliot has just flashed a new penguin build. His giraffe is grazing the savanna.
2. He presses and **holds BOOK**. At ~800 ms the savanna drops away to a dark grid: **`PICK YOUR PAL`**, tiles for giraffe (outlined — current), zebra, penguin.
3. He taps the **penguin** tile. A blink of `swapping…`.
4. **Climax:** the whole world turns over — the golden savanna becomes snow, and a penguin blinks up where the giraffe stood. Not a reskin: a different animal in a different place.
5. He power-cycles the board later; it boots straight back to the penguin. The choice stuck.

## Open Items

- `HOLD_MS` exact value (~800 ms) to be tuned on-device against accidental opens vs. sluggish feel — a dev-story detail, not a design change.
- Roster > 10 animals would need paging/scroll — deferred (bounded by the AD-14 flash budget). `[ASSUMPTION]`
