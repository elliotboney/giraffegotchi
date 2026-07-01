# Adding an animal — AI-agent playbook

A step-by-step contract for an **AI coding agent** (Claude Code, Cursor, etc.) asked to
"add an animal" to giraffegotchi. Follow it top to bottom. It encodes the architecture's hard
rules so you don't relearn them the painful way. The README has the human-facing version; this
file is the one to load into an agent's context.

**Golden rule: an animal is _data + art_. You add ONE `.cpp` descriptor and register it. You do
NOT touch the render / anim / core / hardware layers.** If a task seems to need an engine change,
stop and surface that — it means the descriptor model is missing something, and that's a design
decision for Elliot, not a silent edit.

Every animal MUST define **its own food** and **its own environment (biome)** — never ship an
animal that borrows another's world or falls back to the generic drawn apple. (This is a project
rule, not an engine limit: the engine _allows_ `food = nullptr`, but we don't.)

---

## 0. Preconditions — what to gather before writing code

Read these first (they are the source of truth; this playbook can drift):

- `src/species/species.h` — the `Species` / `AnimSpec` / `Biome` / `FoodItem` structs. Field order
  and types here win over any example below.
- `src/species/giraffe.cpp` — reference descriptor with a tree hook + 2 capabilities.
- `src/species/groundhog.cpp` — reference descriptor with its own food + no capabilities.
- `CLAUDE.md` → "Invariants / gotchas" — the rules that freeze the device if broken.

Confirm the art state (the two pipelines may already be done):

```bash
ls img/<name>/          # cleaned source art (transparent PNGs)
ls data/<name>/         # prepped sprites (RGB565 PNGs the firmware loads) — these are what ship
```

- Both populated → art is done; go straight to the descriptor (Step 3).
- `img/<name>/` only → run the prep pipeline (Step 2).
- Neither → art must be generated first (Step 1). Art generation is usually **Elliot's lane**
  (ChatGPT + hand-tuning); don't fabricate PNGs. Flag it and continue with everything you _can_ do.

**Then get the name.** Two identifiers, both required (see the descriptor table): the internal
`name` (lowercase, = the `img/`/`data/` folder, e.g. `cheetah`) and the `displayName` — the pet's
**given name** shown in the picker (e.g. giraffe→`george`, flamingo→`frances`, cheetah→`spot`,
lowercase to match). **Ask the human for the given name — do NOT invent one.** It's a personal
choice, not a derivable value; guessing it is the one step that reliably gets redone.

---

## 1. Art (usually human-run) — generate the pose set

Prompt lives in `docs/PET_PROMPT.md`: fill its two `{{...}}` slots (animal + look), paste into
ChatGPT, save the 19 poses into `img/<name>/` (props/food under `img/<name>/objects/`).

Required pose names (the descriptor references these strings):
`happy happy2 happy3 hungry sad excited sleepy sick reading thirsty bored dirty dead blink
ears_up ears_down tail_left` + signature-move poses (`kick1`, `kick2`) + `icon` + optional `food`
+ optional daydream objects (`objects/dream1`, `dream2`, … — 64×64, one per `dreamN`).

A single-frame pose (e.g. only `blink.png`, no `blink2/3`) is fine — the engine is
variable-length. Never hardcode "3 frames".

`sleepy` is the **night-sleep** pose (the only time it's shown — `Emotion::Sleepy` is night-only,
drawn with rising Z's). Draw it **fully asleep, eyes closed** — not merely drowsy — or the animal
looks awake while it "sleeps".

## 2. Prep pipeline — clean + convert

```bash
bun run cleanart <name>   # bg-remove + align frames; originals -> img/<name>/.backups/
bun prep <name>           # keys transparency + aligns + reports flash budget -> data/<name>/
```

Revert a bad clean with `bun run cleanart:revert <name>`. `bun prep` prints a per-sprite byte
budget — watch the flash total (the firmware image already sits ~69% of 1.31 MB).

**Transparency invariant:** magenta `0xF81F` is the key, read at runtime from pixel `[0]`. It must
never appear inside a silhouette. `prep_sprite.py`'s `MAGENTA_RGB` and the runtime key stay in
lockstep — don't change one without the other.

## 3. Descriptor — `src/species/<name>.cpp`

Copy `groundhog.cpp` (has food, no caps) or `giraffe.cpp` (has caps + tree hook) and edit values.
`extern const Species` for external linkage. Fill every field:

| Field | What it is | How to pick it |
|---|---|---|
| `name` | lowercase internal id + registry key + asset folder + save id | must equal the `img/`/`data/` folder name |
| `displayName` | the pet's given name in the picker | **ASK the human — never invent it** (see Step 0). e.g. `"frances"`, lowercase; separate from `name` |
| `assetFolder` | LittleFS root, `"/<name>"` | leading slash |
| `geom` | `{ w, h, x, y, horizonY }` — pose-buffer/band size + screen placement | check a prepped sprite's real size (`data/<name>/happy.png`); reuse giraffe's `{150,160,85,34,165}` if same footprint |
| `anchors` | mouth / food-drop / sleep-Z / daydream screen points | copy the reference, then **refine on device** — these are visual |
| `caps` | OR of `Capability` flags for signature moves | only flags whose **art + code** exist: `CAP_KICK` needs `kick` poses; `CAP_KITE` needs the kite. Omit → that code never runs |
| `anims` | `&<X>_ANIMS` (idle rotation + tics) | frame lists are pose-name arrays; sizes are per-species |
| `biome` | `&<X>_BIOME` — REQUIRED, see below | build a real world, don't borrow one |
| `food` | `&<X>_FOOD` — REQUIRED, see below | define one even if the sprite art lands later |
| `icon` | picker tile sprite name | `"icon"` (null → scaled `happy`) |
| `dreamN` | count of daydream wish-objects (optional) | `0` = no daydreams, or N → loads `data/<name>/dream1..N.png` (source in `img/<name>/objects/`, flattened by prep). Safe to ship `0` or set N before the art exists |

### Food (required)

```cpp
static const FoodItem <X>_FOOD = { "food", 40, 40 };   // sprite name in data/<name>/, w, h
```

Pick a thematic food (flamingo → shrimp; that's why they're pink). It decodes from
`data/<name>/food.png`. **If that sprite isn't on the FS yet, the eat animation safely draws
nothing** — `renderPoseToBuffer` fails, the buffer is freed, `blitFoodToBand` no-ops. No crash.
So you can commit the descriptor before the art exists; just flag the gap.

### Daydream objects (optional)

```cpp
/* dreams */ 3,   // -> objects/dream1.png, dream2.png, dream3.png
```

While idle + content, the pet floats a thought bubble that cycles through `dreamN` wish-objects —
one per thing the animal daydreams about (a treat, a toy, a heart). Source PNGs go in
`img/<name>/objects/dream1.png` … `dreamN.png`; `prep` sizes them to a **64×64 square** and
flattens them to `data/<name>/dream1.png` … `dreamN.png` (no `objects/` subfolder on the FS —
same as `food`). Set `dreamN = 0` to opt out entirely (no bubble). **Missing PNGs safely no-op** —
decode fails → buffer freed → nothing drawn (not an empty cloud), so you can set `dreamN` before the
art lands, same as food. `prep`'s `DREAM_PX` and the engine's `DREAM_OBJ` must stay in lockstep.

### Biome (required)

```cpp
static const Blade   <X>_GRASS[] = { {x,y,h,amp,color565}, ... };  // props stay clear of pet box x 85..235
static const Star    <X>_STARS[] = { {x,y}, ... };
static const Firefly <X>_FF[]    = { {x,y}, ... };
static const Biome <X>_BIOME = {
  { /*NIGHT*/{sky,ground}, DAWN, SUNRISE, MORNING, DAY, AFTERNOON, SUNSET, DUSK },  // 8 phases, RGB565
  <X>_GRASS, N, <X>_STARS, N,
  nullptr, 0, nullptr,     // trees: {array, N, drawHook} — null hook => no trees
  <X>_FF, N,
};
```

- Palette is 8 `{sky, ground}` pairs indexed `NIGHT..DUSK`. Give the world a distinct identity
  (savanna = tan ground; meadow = green; lagoon = turquoise water + pink skies). RGB565 values are
  hard to eyeball — expect to **tune on device**.
- Trees are a **draw hook** (`TreeDrawFn`), not data — acacia lives in `ui.cpp` as `drawAcaciaTree`.
  A new tree shape is the ONE case that needs a render-layer function. Prefer `nullptr` (no trees)
  unless a tree is core to the animal's world; if you add a hook, that's a flagged render-layer
  change, not a silent one.
- Keep every prop out of the pet box (x `85..235`) so it never fights the sprite.

## 4. Register — `src/species/registry.cpp`

```cpp
extern const Species FLAMINGO;                                   // add the extern
static const Species* const SPECIES[] = { &GIRAFFE, &GROUNDHOG, &FLAMINGO };  // add the pointer
```

That's it — the picker, save system, and swap logic read the array. The pose buffer auto-sizes to
the largest species, so a bigger animal needs no other change.

## 5. Verify — gates in order

```bash
bun native     # 43 hardware-free tests over pet+sky — must stay green (regression gate)
bun compile    # REQUIRED: native does NOT compile the descriptors; this catches C++ errors + link
```

`bun compile` also prints the flash budget — confirm it still fits. Then hand off to Elliot for the
on-device gate (only he has the hardware):

```bash
bun flash      # firmware + sprites (use when data/ changed; bun upload for code-only)
```

On device: hold **BOOK** → the new tile appears in the picker → select it → watch idle, blink,
ears, tail, eat, sleep, day/night. Serial traces `[daynight]` `[save]` `[swap]` `[prank]`.
Anchors and palette are the two things you'll almost always tweak here.

---

## Invariants that bite (do not relearn these)

- **Nothing species-specific outside the `Species` descriptor.** No paths/geometry/palette/food in
  render or engine code.
- **Never `pushImage(...,key)` onto a sprite** — the transparent overload runs off-bounds and
  **freezes the device**. Composite by hand (byte-swap + key-skip + bounds-clip) like
  `composeSkyBand` / `blitFoodToBand`.
- **All erases go through `restoreBg`** — never a flat fill (`readRect` is broken on this CYD).
- **One pose-buffer writer per frame**, priority `dead > kick > tic > emotion`. Foreground anims
  (eat/sleep/food) compose into the band and never touch the pose buffer.
- **Species swap is latched, applied only at the top of `loop()`** — atomic, no frame straddles two
  species. The pose buffer is allocated once at the max size; never realloc on swap.

## Worked example: the flamingo ("frances")

`src/species/flamingo.cpp` is the reference for this playbook. It has: 150×160 giraffe-sized
footprint; 3-frame idle + single-frame blink; `CAP_KICK` (one-leg kick, no kite); its own
`FL_FOOD` (shrimp, art pending) and its own `LAGOON` biome (turquoise water, flamingo-pink
sunrise/sunset, teal reeds, no trees). Only open gap: generate `img/flamingo/objects/food.png`
(shrimp) → `bun prep flamingo` and the eat animation lights up.
