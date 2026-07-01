# Adversarial Review — Runtime Species Swap (AD-11 … AD-15)

**Scope:** Only the newly added/amended decisions AD-11 through AD-15. AD-1..AD-10 are treated as passed except where a new AD collides with one. Grounded against the real `src/main.cpp` and `src/ui.cpp` (the current single-species build).

**Verdict:** **PASS-WITH-FIXES**

The five new ADs correctly identify the surface (single source of truth, data-driven anim, one-owner active species, asset namespacing, biome-from-data). But three of them are written as *properties of the end state* ("atomic swap", "one owner", "before the next composite") without naming **who** executes the sequence and **where in the loop** it is allowed to fire. On a single-threaded cooperative loop with a once-malloc'd fixed buffer and a compile-time `IMG_W` stride, those omissions let two AD-compliant modules corrupt the pose buffer or straddle two species. Findings below are ordered most-severe first.

---

## Method

For each finding I construct a **failure pair**: two modules (or two points in the same loop) that each obey every letter of the relevant AD, yet together build incompatibly or corrupt state. I anchor every claim to a line in the shipped code so the hole is concrete, not hypothetical.

---

## F1 — CRITICAL — Fixed-size buffers overflow on swap to a larger animal (AD-11 vs AD-13, no re-alloc rule)

**The pair:**
- `species/giraffe.cpp` declares geometry `W/H` per AD-11 ("geometry `W/H/X/Y` … lives **only** in the active descriptor"). A second animal legally declares a **larger** `W×H`.
- `main.cpp` swap logic obeys AD-13: on swap it "reload[s] the footprint's sprite buffers for the new species." Reloading == decoding the new PNGs into `giraffeBuf`.

**Why it corrupts:** `giraffeBuf` is `malloc(GIRAFFE_W * GIRAFFE_H * sizeof(uint16_t))` **once** in `setup()` (main.cpp:617) and never freed or resized. `skyBand` is `createSprite(GIRAFFE_W, BAND_H)` **once** (main.cpp:623). Worse, the decode path hard-codes the stride: `pngDraw` uses `uint16_t line[IMG_W]` with `IMG_W == GIRAFFE_W` and the comment literally asserts *"IMG_W is the widest sprite (the giraffe)"* (ui.cpp:389-390); `renderGiraffeToBuffer` sets `g_bufW = IMG_W` (ui.cpp:451). Decode a taller/wider animal into that buffer and you write past the allocation — heap corruption, not a clipped sprite. A **smaller** animal is *also* wrong: the band still fills `GIRAFFE_W × BAND_H` and composites `GIRAFFE_W * BAND_H` pixels from a buffer sized for a different geometry (ui.cpp:323, 336-340), reading stale/foreign bytes.

**Which AD is missing/weak:** AD-13 says "reload the footprint's sprite buffers" but **never says the buffers are re-allocated to the new geometry**, and AD-11 explicitly *forbids* a compile-time `GIRAFFE_W`/`GIRAFFE_H` — so the current fixed `malloc` and the `line[IMG_W]` stack array both become AD-11 violations *and* AD-13 leaves their replacement unspecified. Two modules each "comply" and the device faults.

**Fix (one line to the AD):** AD-13 must state that a swap **frees and re-allocates `giraffeBuf` and re-creates `skyBand` to the new descriptor's `W×H` (and re-derives the decode stride from the active `W`) before any decode**, and that swap fails safe (keep old species, set an `*Ok=false` flag per the AD-4/degrade convention) if either alloc fails. Also drop the `line[IMG_W]` fixed array in favor of a heap line buffer sized to active `W`, or cap `W` to a spine-declared `MAX_SPECIES_W` and document that cap as the invariant.

---

## F2 — CRITICAL — Swap has no named owner and no defined firing point in the loop (AD-13 "before the next composite" is unanchored)

**The pair:**
- Selector code (the deferred swap-UX, or the existing touch handler in `loop()`) sets the active species when a button is pressed — at touch-handling time, roughly main.cpp:722-755.
- The compositor at the bottom of `loop()` runs `composeSkyBand(skyBand, giraffeBuf)` then `skyBand.pushSprite(...)` unconditionally every frame (main.cpp:842-855).

**Why it corrupts:** AD-13 says the reload happens "*before* the next frame composites; no frame straddles two species/biomes" — but it names **no module** as the owner of that sequencing and **no point in the loop** where the swap is applied. In the real loop the touch handler runs *near the top* and the composite runs *at the very bottom* of the same iteration. Between them sit the entire giraffe-ownership block (main.cpp:765-836) — eat expiry, kick pose writes, tic/happy-frame rotation — all of which read `pet.emotion()`, write `giraffeBuf`, and reference `GIRAFFE_X/Y/W/H`. If the swap merely flips a pointer at touch time, the *same iteration* then runs old-geometry animation logic and composites with a half-swapped state: new descriptor, old buffer contents, old anchors. That is exactly the "new geometry with old sprites" AD-13 claims to prevent. A rule that says "reload before next composite" with no owner of the sequencing **is the hole**, verbatim from the brief.

**Which AD is missing/weak:** AD-13. "Orchestration owns the active-species pointer" names the owner of the *datum*, not the owner of the *swap sequence*. There is no statement of the form "swaps are latched and applied at a single point — top of `loop()`, before `pet.update` and before any animation ticks — and never mid-iteration."

**Fix:** AD-13 must add: *"A swap request is **latched** (a pending-species flag owned by `main`); `main` applies it at exactly one point — the top of `loop()`, before `pet.update()` and before any anim tick — by (1) aborting/finalizing any in-flight pose animation, (2) re-allocating buffers (F1), (3) decoding the new sprites, (4) full-screen biome repaint. No other module applies a swap; no swap takes effect mid-iteration."*

---

## F3 — HIGH — Mid-animation swap orphans a pose-owning animation and its direct-drawn debris (AD-13 vs AD-5)

**The pair:**
- The kick capability hook (AD-12 named hook) is mid-flight: it "owns `giraffeBuf`" per AD-5 priority (`s_kickPose` set, ball position latched in `play_.bx/by`), and it draws ball pixels **directly to the panel outside the box** each frame (main.cpp:520-534, 555) plus into the band inside the box.
- A swap fires (F2) and, per AD-13, reloads sprites and does a full-screen repaint.

**Why it corrupts / builds incompatibly:** AD-5 guarantees exactly one writer of `giraffeBuf` *per frame by priority*, and pose animations own the buffer across many frames. AD-13's atomic swap decodes into that same buffer. Nothing in either AD says a swap must first **cancel** an in-flight pose animation. Today the code only cleans up the kick's out-of-box ball via `eraseKickBall()` at expiry (main.cpp:538-543); a swap that repaints full-screen would erase the *visual* debris but leave `play_.active`/`s_kickPose` state believing it still owns a buffer that now holds a *different animal's* pose — the next `tickKick` writes `giraffe_kick2.png` (a giraffe path!) over the new animal (main.cpp:462-468). Also note the kick path decodes **hardcoded `/giraffe_kick*.png` literals** (main.cpp:464-465), an AD-11/AD-14 violation the swap epic must relocate into the descriptor's hook — otherwise a swapped-in animal's kick loads giraffe art.

**Which AD is weak:** AD-13 ("atomic") and AD-5 (pose ownership) don't compose: neither states that applying a swap must **terminate all in-flight animations and reset their ownership/latched-position state** as step 0 of the atomic sequence.

**Fix:** AD-13 step (1) from F2's fix must explicitly *"cancel every active animation (pose and foreground-band), reset their owned state (`s_kickPose`, `play_`, `eat`, `slp`, `dream`), and only then reload."* AD-12's capability-hook rule should add that hook sprite paths come from the descriptor folder (AD-14), never a literal.

---

## F4 — HIGH — Palette ownership fights on swap: biome (AD-15) vs day/night (AD-8/AD-10) both claim the sky colours

**The pair:**
- `render/scene` owns `SKY_COLOR`/`GROUND_COLOR` via `setSkyPhase()` (AD-10 table; ui.cpp:599-604), and the palette *table* `PALETTES[8]` is a scene-static (ui.cpp:25-34).
- AD-15 says the **biome descriptor** owns "sky/ground palette **set**"; AD-13 says on swap "day/night phase re-resolves against the **new palette set**."

**Why they fight:** Today the 8-entry `PALETTES` table is the single source of sky colour, indexed by `SkyPhase`, and `updateDayNight` in `main` picks the phase and calls `setSkyPhase(p)` (main.cpp:221-228). AD-15 now moves the palette *set* into the biome. So after a swap there are two candidate owners of `SKY_COLOR`: the daynight driver (which still computes a `SkyPhase` from the clock) and the biome (which now supplies the 8 colours for that phase). AD-13's "re-resolve phase against the new palette set" is a **data-flow that crosses two owners** but never says which module holds the biome palette table or how daynight reaches it. Concretely: `daynight.cpp` computes `PHASE_SUNSET`; does it index the biome's table or the old scene `PALETTES[]`? If both exist post-refactor you get the exact "two-tone sky / stale phase" desync AD-10 was written to kill. Second, smaller collision: the moon's crescent bite is drawn with `SKY_COLOR` (ui.cpp:84) and `drawStars` gates on `s_phaseId == PHASE_NIGHT` (ui.cpp:96) — both assume the *scene* owns phase, so a biome that redefines night palettes must still feed `s_phaseId` through the scene owner, not a second copy.

**Which AD is weak:** AD-15 grants the biome the palette *set* but doesn't reconcile with AD-10's existing rule that `render/scene` owns `SKY_COLOR` and that phase-id is a **single copy** (`currentSkyPhase()`). AD-13's re-resolve clause names no holder of the biome table.

**Fix:** State the ownership seam explicitly: *"The biome descriptor supplies the palette **table** (data); `render/scene` remains the sole **writer** of `SKY_COLOR`/`GROUND_COLOR` and the sole holder of the phase id (AD-10 unchanged). `daynight` computes a `SkyPhase` and calls `setSkyPhase(phase)`; `setSkyPhase` indexes the **active biome's** table, not a scene-static `PALETTES[]`. On swap, `main` re-points the scene's active-palette-table to the new biome, then re-calls `setSkyPhase(currentSkyPhase())` to re-resolve."* That keeps one writer, one phase copy, biome as pure data.

---

## F5 — MEDIUM — Biome geometry (horizon / footprint) is assumed constant, but band composite is screen-absolute (AD-15 vs AD-4)

**The pair:**
- AD-15 puts "horizon" and "grass/tree/prop positions" in the biome descriptor (variable per animal).
- AD-4 fixes the compositing band to the **giraffe footprint** and `composeSkyBand` computes the in-band horizon as `HORIZON_Y - GIRAFFE_Y` and fills `GIRAFFE_W × BAND_H` (ui.cpp:322-324), while the direct scene uses absolute `HORIZON_Y` (ui.cpp:359-360, 365-371 in `restoreBg`).

**Why it can diverge:** If a biome descriptor is allowed to move `HORIZON_Y` (AD-15) *and* a species is allowed to move `GIRAFFE_Y`/`H` (AD-11), then `composeSkyBand`'s band-local horizon row and `restoreBg`'s absolute-horizon split must both re-derive from the *same* active values every frame. AD-4 says the band mechanics are "biome-independent and unchanged" (AD-15 closing line) — but that's only true if horizon and footprint are read from the active descriptor, not the file-static constants they are today. Two compliant modules (a biome that lowers the horizon, a band compositor that keeps the old `HORIZON_Y`) render a horizon seam: sky/ground split differs inside vs outside the box.

**Which AD is weak:** AD-15's "band mechanics unchanged" is true in spirit but glosses that `HORIZON_Y` and the footprint are now *variables*; nothing states they resolve from the active descriptor at composite time.

**Fix:** Add to AD-15: *"`HORIZON_Y`, footprint origin, and footprint size are read from the active species/biome at composite time; `composeSkyBand` and `restoreBg` derive their splits from those live values, never from a compile-time constant."* (This is really the F1/AD-11 relocation applied to the horizon.)

---

## F6 — LOW / watch — AD-14 same-filenames rule + AD-11 "descriptor names the folder" is good, but flash-budget gate is advisory

AD-14 requires each species folder to carry the **same** filenames and the descriptor to name only the folder — this cleanly kills the hardcoded `/giraffe_*.png` literals scattered through main.cpp (HAPPY_FRAMES, BLINK_FR, kick poses) and ui.cpp (`emotionPath`). Good. The one soft spot: "must fit the LittleFS partition with headroom — the prep/build step **reports** the budget." *Reports* is not *enforces*. Two compliant animals can each fit individually and jointly overflow the partition, and a report nobody reads passes CI. **Fix:** make the budget a **build-failing** check (non-zero exit when total shipped sprite bytes > partition × headroom), not a printout. Cheap, and it's the only thing standing between "add an animal" and a silent `uploadfs` truncation.

---

## Cross-cutting note (not a finding, a sequencing risk)

AD-13 binds `save`. The active-species id now rides in the versioned NVS blob (AD-8). The current `PetSave` struct and `SAVE_MAGIC` (main.cpp:59-60) don't include it — so the AD-8 magic **must** bump when species-id is added, and restore must validate the stored id against the registry (an id for an animal no longer in the build must fall back to the reference species, not index out of the registry). AD-13 says "restores on boot" but not "validates against the registry." Add the validate-or-fallback clause; it's a one-liner and prevents a boot-loop on a stale save.

---

## Summary of required AD amendments

1. **AD-13:** re-alloc `giraffeBuf` + re-create `skyBand` + re-derive decode stride to new `W×H` before decode; fail-safe on alloc failure. *(F1)*
2. **AD-13:** name `main` as owner of a **latched** swap applied at **one point — top of `loop()`**, before `pet.update`/anim ticks. *(F2)*
3. **AD-13 / AD-12:** swap step 0 = cancel all in-flight animations and reset their owned state; hook sprite paths come from the descriptor folder, never literals. *(F3)*
4. **AD-15 / AD-10:** biome supplies the palette **table** (data); scene stays sole **writer** of `SKY_COLOR` and sole holder of phase-id; `setSkyPhase` indexes the active biome table; swap re-points table then re-resolves. *(F4)*
5. **AD-15:** horizon + footprint resolve from the active descriptor at composite time. *(F5)*
6. **AD-14:** flash budget is a **build-failing** gate, not a report. *(F6)*
7. **AD-8/AD-13:** bump `SAVE_MAGIC`; validate restored species-id against the registry with fallback. *(cross-cutting)*

None of these change the paradigm; they close the "who and when" holes that turn an atomic-swap *goal* into an atomic-swap *guarantee*.
