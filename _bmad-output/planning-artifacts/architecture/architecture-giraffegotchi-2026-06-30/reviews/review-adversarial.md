# Adversarial Review — Architecture Spine (giraffegotchi)

**Reviewer lens:** Adversary. For each piece of shared mutable state that today lives
as file-static/global in `main.cpp`/`ui.cpp`, construct a pair of post-split modules
that each obey EVERY listed AD to the letter yet still build/behave incompatibly.
Every surviving pair is an ownership hole to close.

**Verdict: PASS-WITH-FIXES.** The spine's layering (AD-1/AD-2), panel-push serialization
(AD-3/AD-4), and pose contract (AD-5) are sound and cover the *draw-order* races well.
But the spine is silent on **state ownership** for almost every mutable global that the
refactor splits across modules. AD-5 is the *only* AD that names an owner for a specific
piece of state (`giraffeBuf`, "at most one animation owns the pose buffer"). Everything
else — the day/night phase, the sky/ground palette, the PNG decoder target globals, the
cloud/bird/celestial arrays, the `s_celForce` repaint flag — has no declared owner. Two
modules can each satisfy AD-1..AD-9 and still fight over the same word of RAM.

The fixes are cheap: add an **AD-10 "single writer per mutable datum"** rule plus an
ownership table, and tighten AD-5 to cover the PNG decoder re-entrancy it silently
depends on. Details below, each as a concrete incompatible pair.

---

## Method

For each datum I name (a) where it lives today, (b) the natural post-split home per the
Structural Seed, (c) a concrete pair of modules that both obey the ADs, and (d) the exact
way they diverge. I only count a pair if *both* modules are AD-legal — divergence that an
existing AD already forbids is not a hole.

---

## Hole 1 — Day/night phase is stored TWICE, no AD names a single source

**State:** the current phase. It exists as **two** independent copies today:
- `s_dayPhase` (file-static in `main.cpp:148`), the change-detector for repaint.
- `s_phaseId` (file-static in `ui.cpp:35`), read by `drawStars`, `animateCritters`,
  and gated on by `PHASE_NIGHT`/day checks.

They're kept in sync only by the discipline that `main` calls `setSkyPhase(p)` *and*
assigns `s_dayPhase = p` on the same lines (`main.cpp:226-227`, `712-713`).

**Post-split:** `s_dayPhase` → `io/daynight`. `s_phaseId` → `render/scene`
(it drives star/critter/night rendering, which is pure render).

**AD-legal incompatible pair:**
- `daynight` obeys AD-2 (depends down on `render` via `setSkyPhase`, `setCelestial` — fine)
  and AD-3 (never pushes). It owns `s_dayPhase` and calls `setSkyPhase(p)` only on change.
- `scene` obeys everything: `s_phaseId` is file-static single-instance render state
  exactly as the Consistency Conventions bless ("single-instance mutable render state =
  file-static").

**Divergence:** Nothing in the spine says these are the *same* fact. A future `scene`
refactor that (legally) recomputes night from `currentSkyPhase()` internally, or a
`daynight` that (legally) stops mirroring into `s_dayPhase` and reads `currentSkyPhase()`
back, will silently desync from the other copy. The `DAYNIGHT_DEMO` path
(`main.cpp:708-713`) already proves the duplication is load-bearing and hand-maintained:
it re-implements the "assign `s_dayPhase` + call `setSkyPhase`" pairing a *second* time.
Two writers, two storage locations, one logical fact — the classic divergence hole.

**No AD covers this.** AD-3 covers *push* ordering, not *state* duplication.

**Fix:** Declare `render/scene` the single owner of phase (it already exposes
`currentSkyPhase()`). `daynight` must not keep its own `s_dayPhase` mirror — it derives
"did the phase change?" from `currentSkyPhase()` before calling `setSkyPhase()`. Encode as
AD-10 (single-writer rule) + an ownership-table row: *phase → scene*.

---

## Hole 2 — The sky/ground palette (`SKY_COLOR`/`GROUND_COLOR`) has a reader in every render path but no declared owner

**State:** `SKY_COLOR` / `GROUND_COLOR` — **non-static extern globals** (`ui.cpp:18-19`,
`ui.h:9-10`). Written by `setSkyPhase` (`ui.cpp:601-602`). Read by `restoreBg`,
`drawScene`, `composeSkyBand`, `drawCelestialAt` (crescent bite), and aliased as
`BG_COLOR` for meter/text backgrounds. These are the *most* widely shared mutable words
in the codebase and they are `extern`, i.e. deliberately reachable from `main.cpp`.

**Post-split:** the write (`setSkyPhase`) belongs to `scene`; the *trigger* to change it
belongs to `daynight`; the reads are spread across `scene` (restore/compose/celestial)
**and** any module that draws a background — today that's also `anim` (e.g. `restoreBg`
calls from `eraseKite`, `eraseKickBall`, `tickClean` all read `GROUND_COLOR`/`SKY_COLOR`
transitively).

**AD-legal incompatible pair:**
- `daynight` legally calls `setSkyPhase(p)` (AD-2 down-dependency). It assumes the palette
  is now phase-`p` for the *rest of the frame*.
- `anim` legally calls `restoreBg(...)` mid-frame (AD-7 says restore is the only erase).
  `restoreBg` reads the *current* `SKY_COLOR`.

**Divergence:** `setSkyPhase` is only ever called from `daynight`, but nothing in the
spine forbids a second caller. If a future `anim` or `render` primitive legally calls
`setSkyPhase` (nothing prohibits it — it's a public render entry point), the palette flips
under `daynight`'s feet mid-frame and `composeSkyBand`'s `fillRect(..., SKY_COLOR)` uses a
different colour than the direct-path `drawScene` painted, producing a two-tone sky. Both
modules obeyed AD-2, AD-4, AD-7. The invariant "exactly one module mutates the palette per
frame, and it does so before any reader runs" is **assumed, never written.**

Compare AD-6, which *does* nail down the single transparency-key source. There is no
equivalent AD-6-for-palette. That asymmetry is the giveaway: the spine's author saw the
"single source" pattern for the magenta key but didn't generalize it to the palette.

**Fix:** AD-10 ownership row: *palette (`SKY_COLOR`/`GROUND_COLOR`) → scene, written only
by `setSkyPhase`, called only from `daynight`, and only at a phase boundary before any
draw in that frame.* Optionally make the globals `static` inside `scene` with a read
accessor so `extern` mutation is structurally impossible.

---

## Hole 3 — PNG decoder target globals (`g_tft`/`g_buf`/`g_bufW`/`png`) are shared between render and anim with no re-entrancy owner

**State:** `static PNG png` + `g_tft` / `g_buf` / `g_bufW` (`ui.cpp:383-386`). These are
the capture-less callback targets for `pngDraw`. They are mutated by **three** public
entry points that the refactor splits across modules:
- `drawGiraffe` → sets `g_tft`, clears `g_buf` (a `render/scene` concern).
- `renderGiraffeToBuffer` → sets `g_buf`, `g_bufW=IMG_W` (render — emotion sprites).
- `renderSpriteToBuffer` → sets `g_buf`, `g_bufW=w` (driven by **`anim`**: kick poses
  `setKickPose`, idle tics, happy-frame rotation — `main.cpp:464-465,798,805,809`).

**Post-split:** `render/scene` owns `png`/`g_*` and exposes `renderGiraffeToBuffer`.
`anim` calls `renderSpriteToBuffer` for pose/frame swaps. So `anim` reaches *through*
`render`'s decoder into `render`'s file-static globals every time it swaps a frame.

**AD-legal incompatible pair:**
- `render` obeys AD-1 (it's not core; HW includes fine), AD-2, AD-3, AD-4. Its decoder
  globals are "single-instance mutable render state = file-static" — explicitly blessed.
- `anim` obeys AD-5 to the letter: a foreground-only anim "never touches `giraffeBuf`."
  But AD-5 says nothing about the *decoder*. A foreground anim that legally decodes a
  small sprite of its own (say a future bubble PNG) via `renderSpriteToBuffer(myBuf, ...)`
  sets `g_buf`/`g_bufW` — and if that ever interleaves with a `render` decode, the shared
  `png`/`g_*` state is clobbered.

**Divergence:** the decoder is **not re-entrant** and has **no declared owner**. Today it's
safe only because the single-threaded loop never nests decodes (AD-3's single-loop rule
gives *temporal* exclusivity by luck, not by contract). But AD-3 is about the *panel push*,
not about the decoder. Two modules can both legally call into the decoder in the same
frame; the second silently repoints `g_buf` and the callback writes to the wrong buffer /
wrong stride. `g_bufW` is especially sharp: `renderSpriteToBuffer` sets it to `w` and
resets to `IMG_W` in a `finally`-less sequence (`ui.cpp:459-463`) — any early return or
future exception path leaves the stride wrong for the *next* `render` decode.

**No AD covers this.** AD-1 is about `env:native` purity. AD-5 is about `giraffeBuf`, a
*different* buffer. The decoder's shared target state falls between them.

**Fix:** Two options, pick one in AD-10 / tightened AD-5:
1. Declare the PNG decoder a **single-owner render service**: only `render` mutates
   `g_*`/`png`; `anim` never calls `renderSpriteToBuffer` directly — it asks `render` to
   decode into an anim-owned buffer, so the target globals only ever move under `render`.
2. Or state the invariant explicitly: *decodes never nest; `g_bufW` is restored to
   `IMG_W` on every exit path; there is exactly one decode in flight per frame.* This is
   the weaker fix (relies on discipline) but at least writes the assumption down.

---

## Hole 4 — `giraffeBuf` writer set is larger than AD-5 admits

**State:** `giraffeBuf` (`main.cpp:78`), the persistent giraffe sprite buffer. AD-5 is the
one AD that assigns it an owner: "at most one animation owns the pose buffer per frame …
full-body (kick) owns it via `setKickPose`; foreground-only anims never touch `giraffeBuf`."

**But today the writers are:**
- `setKickPose` (anim, kick) — `main.cpp:464-466` — AD-5's named owner. ✅
- `updateGiraffe` / `renderGiraffeToBuffer` (render/emotion) — `main.cpp:119`. This is a
  *render/scene* write, not an animation. AD-5 doesn't mention it.
- **idle tics + happy-frame rotation** — `main.cpp:794,798,805,809` — these call
  `renderSpriteToBuffer(giraffeBuf, ...)`. The Structural Seed puts **tics under `anim`**
  ("eat/sleep/daydream/play(×4)/clean/**tics**"). So per the seed, `anim` (tics) writes
  `giraffeBuf` — directly contradicting AD-5's "foreground-only anims never touch
  `giraffeBuf`." Tics are foreground-only (they don't own a full-body pose), yet they
  swap the whole buffer.
- `die`/`revive` (orchestration prank) — `main.cpp:675,683` — a *fourth* writer.

**AD-legal incompatible pair:**
- `anim` (kick) obeys AD-5: owns `giraffeBuf` exclusively via `setKickPose` this frame.
- `anim` (tics) *also* legally lives in `anim` per the seed, and legally swaps
  `giraffeBuf` frames (that's literally what a tic is).

**Divergence:** AD-5 says "at most one animation owns the pose buffer per frame," but the
seed's own module assignment puts **two** `giraffeBuf`-writing animations (tics and kick)
in the same `anim` module, and classifies tics as "foreground-only" — the exact category
AD-5 says must NOT touch `giraffeBuf`. The current code dodges this only via the
`else`-chain ordering in `loop()` (kick is handled before tics via
`else if (play_.kind == PLAY_KICK)`). That ordering is an orchestration accident, not an
AD. The `⚠️ Ratify` note on AD-5 flags that the contract needs confirming — this is the
concrete thing to confirm: **tics are full-buffer swaps, so AD-5's "foreground-only anims
never touch giraffeBuf" is factually wrong about tics.**

**Fix:** Re-cut AD-5 into two categories that match reality:
1. **Pose-buffer owners** (write `giraffeBuf`): emotion (render), tics, kick poses. Rule:
   at most one writes per frame, resolved by an explicit priority (dead > kick > tic >
   emotion), stated in the AD — not left to loop ordering.
2. **Band-only composers** (never write `giraffeBuf`): eat item, sleep Z's, daydream,
   butterfly, bubbles, ball. These `compose(band)` only.
Move tics from "never touch giraffeBuf" to "pose-buffer owner," and name the priority.

---

## Hole 5 — `s_celForce` has two writers, one clearer, and the clearer is a third module

**State:** `s_celForce` (`ui.cpp:40`), "force a celestial redraw next frame." Written
`true` by **`setSkyPhase`** (`ui.cpp:603`) and read/cleared by **`animateScenery`**
(`ui.cpp:299,306`). `setCelestial` does *not* set it. So the flag couples
`setSkyPhase` (triggered by `daynight`) to `animateScenery` (scene, run by `main`).

**Post-split:** both `setSkyPhase` and `animateScenery` land in `render/scene`, so *within*
scene this is one owner — OK so far. But the *trigger* (`setSkyPhase` being called) comes
from `daynight`, and the *clear* happens whenever `main` next calls `animateScenery`.

**AD-legal incompatible pair:**
- `daynight` legally calls `setSkyPhase` (sets `s_celForce=true`), then — obeying AD-3
  (never pushes) — returns without drawing.
- `main` orchestration legally calls `repaintScene()` (`main.cpp:228`) on a phase change,
  which calls `drawScene` **but not `animateScenery`**. So `s_celForce` stays `true` until
  the *next* `loop()` iteration's `animateScenery` call.

**Divergence:** this one is subtle and largely internal to `scene`, so it's the weakest of
the five — but it shows the same shape: a cross-module *flag* (`daynight` sets the
condition, `scene` consumes it, `main` sequences the two) with no AD naming who may set it
or guaranteeing it's cleared exactly once. If a future `daynight` calls `setCelestial`
without a phase change (the moon-arc-only path, `main.cpp:217-218`), it legally does NOT
set `s_celForce`, relying on the `s_celX != celDrawnX` position-change check in
`animateScenery` to trigger the redraw. That coupling ("celestial redraw is triggered
either by force-flag OR by position delta, owned across two setters") is undocumented.

**Fix:** Fold into AD-10's ownership table: *`s_celForce` → scene, set only by
`setSkyPhase`, cleared only by `animateScenery`, exactly once per set.* Low priority
relative to Holes 1–4 but belongs in the table for completeness.

---

## Cross-cutting: cloud/bird/celestial position arrays (checked, NOT a hole)

I checked `s_cloudX`/`s_birdX`/`s_birdUp`/`s_celX`/`s_celY`/`s_celSun` (`ui.cpp:41,117-119`).
Writers: `animateScenery` (positions) and `setCelestial` (celestial). Readers: `composeSkyBand`,
`drawCelestialDirect`, `cloudOrBirdInBox`. All land in `render/scene` post-split **except**
the `setCelestial` *call*, which comes from `daynight`. Because `setCelestial` is a pure
store (no draw, no push) and the only cross-module edge is `daynight → scene` (AD-2-legal,
down-direction), and there's exactly one writer per array, this is **not** a divergence
hole — it's a clean single-writer store already. It only becomes a hole if a second module
starts writing celestial position; AD-10's single-writer rule would prevent that
pre-emptively. Noting it so the ownership table is exhaustive, not to flag a defect.

---

## Recommended AD to add

### AD-10 — Single writer per mutable datum; cross-module state has one declared owner `[PROPOSE]`
- **Binds:** all.
- **Prevents:** two post-split modules mutating the same global (palette, phase, decoder
  target, force-flags) and diverging — the class of bug AD-3/AD-4 solve for *pixels* but
  not for *state*.
- **Rule:** Every mutable datum shared across modules has exactly **one** owning module
  that may write it; other modules read via an accessor or ask the owner to mutate. A frame
  never contains two writers of the same datum. Ownership table (authoritative):

  | Datum | Owner (writes) | Readers | Cross-module trigger |
  | --- | --- | --- | --- |
  | phase (`s_phaseId`) | `render/scene` | scene | `daynight` calls `setSkyPhase` on change; derives "changed" from `currentSkyPhase()` — no private mirror |
  | palette (`SKY_COLOR`/`GROUND_COLOR`) | `render/scene` (`setSkyPhase`) | scene, anim (via `restoreBg`), main (`BG_COLOR`) | `daynight` only, at a phase boundary, before any draw that frame |
  | PNG decoder (`png`,`g_tft`,`g_buf`,`g_bufW`) | `render/scene` | — | `anim` never repoints `g_*`; it requests a decode into an anim-owned buffer. Decodes never nest; `g_bufW` restored to `IMG_W` on every exit |
  | `giraffeBuf` | see tightened AD-5 | `composeSkyBand` | one pose-buffer writer per frame by priority |
  | `s_celForce` | `render/scene` | scene | set only by `setSkyPhase`, cleared only by `animateScenery`, once per set |
  | celestial pos (`s_celX/Y/Sun`) | `render/scene` (`setCelestial`) | scene | `daynight` calls `setCelestial`; pure store, no second writer |

### AD-5 tightening (resolve the `⚠️ Ratify`)
Split pose-buffer writers from band-only composers explicitly, and give the writers a
**priority** rather than relying on `loop()` ordering:
- **Pose-buffer writers** (may write `giraffeBuf`): dead-sprite, kick pose, tic/happy
  frame, emotion. Exactly one applies per frame, resolved by priority
  **dead > kick > tic > emotion**.
- **Band-only composers** (must NOT write `giraffeBuf`): eat, sleep Z's, daydream,
  butterfly, bubbles, ball. They `compose(band)` only.
- Correct the current AD-5 text: **tics ARE pose-buffer writers** (full-frame swaps), so
  the phrase "foreground-only animations (… ) never touch `giraffeBuf`" must not list or
  imply tics.

---

## Summary of holes

| # | Datum | Incompatible pair | Missing/weak AD | Severity |
| --- | --- | --- | --- | --- |
| 1 | day/night phase (stored twice: `s_dayPhase` + `s_phaseId`) | `daynight` vs `scene` each hold a phase copy | none → AD-10 | High |
| 2 | palette `SKY_COLOR`/`GROUND_COLOR` | `daynight` flips palette mid-frame vs `anim`/`scene` reading it | none (AD-6 covers key, not palette) → AD-10 | High |
| 3 | PNG decoder `g_*`/`png` | `render` decode vs `anim` `renderSpriteToBuffer` decode | none (AD-1≠this, AD-5≠this) → AD-10 / AD-5 | High |
| 4 | `giraffeBuf` | `anim`(kick) vs `anim`(tics), both write the buffer | AD-5 is factually wrong re: tics | High |
| 5 | `s_celForce` | `setSkyPhase` sets vs `animateScenery` clears, sequenced by `main` | none → AD-10 | Low |

**Bottom line:** the spine is strong on *draw-order/push* invariants and weak on *state
ownership*. Add AD-10 (single-writer + ownership table) and correct AD-5's tic claim, and
every pair above is closed.
