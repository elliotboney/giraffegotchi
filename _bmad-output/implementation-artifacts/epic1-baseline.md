# Epic 1 — Behavior-Neutral Verification Baseline (Story 1.1)

_Captured: 2026-06-30 · pre-refactor · baseline commit `4f10320`_

This is the **golden reference** for Epic 1. Every Epic 1 story claims "identical behavior."
That claim is signed off **against this document**, not by inspection. Two kinds of check:

- **Automated** (§1, §2): re-run the command, diff the numbers/names. A change here that
  isn't explained by the story is a regression — stop.
- **Frozen literals** (§3): every giraffe-specific fact the refactor relocates into the
  species descriptor. After a story moves one, its **value must be unchanged** here. This is
  how "no giraffe-ism escaped" (NFR5/AD-11) and "renders identically" are verified by diff.
- **On-device A/B** (§4): the visual walk-through. Flash pre-refactor (A) once, record each
  row (pre-filled below from the code). After each story, flash (B) and confirm A == B.

> Reproduce the baseline build at any time: `git stash && git checkout 4f10320` → run §1/§2 →
> `git checkout -` . The captured numbers below are from a clean checkout of `4f10320`.

---

## 1. Native test baseline (automated)

```
pio test -e native
```

**Result: `37 test cases: 37 succeeded` (0.44 s).** Green state = the pass-list below. Later
stories add tests (`test_sky`, …) but must **never** drop or fail one of these 37.

Frozen pass-list (`test/test_pet/test_pet.cpp`):

```
init_full, feed, feed_clamp, hunger_decay_tick, decay_clamp_zero, thirst_decay_tick,
fun_decay_tick, hygiene_not_timed, all_stats_decay_together, drink, play, drink_play_clamp,
clean_restores_hygiene, poop_spawns_on_timer, poop_count_caps, poop_spawn_drops_hygiene,
clean_removes_all_poop, clean_resets_poop_timer, emotion_happy_default, emotion_hungry_boundary,
emotion_sad_boundary, emotion_excited_then_expires, emotion_excited_on_drink_and_play,
emotion_sick_from_hunger, idle_no_daytime_nap, emotion_reading, emotion_thirsty, emotion_bored,
emotion_dirty, emotion_lowest_need_wins, emotion_tie_breaks_by_statid, emotion_sick_from_hygiene,
night_sleep_pauses_world, night_wake_on_action, night_off_is_normal, load_restores_state,
load_clamps_corrupt
```

## 2. Firmware build baseline (automated)

```
pio run -e esp32dev
```

**Result: `SUCCESS`.**

| Segment | Bytes | % of partition |
| --- | --- | --- |
| RAM   | 92,612  | 28.3% (of 327,680) |
| Flash | 895,373 | 68.3% (of 1,310,720) |

Behavior-neutral Epic 1 stories should not move these materially. Small deltas from moving
code between translation units are fine; a **large** jump (esp. RAM) means something changed —
investigate. Record the new numbers in each story's sign-off.

---

## 3. Frozen giraffe literals (the diff target)

Every value here currently lives in `src/ui.{h,cpp}` or `src/main.cpp`. The refactor relocates
each into the giraffe `Species` descriptor (AD-11). **The value must not change** — only its
home. Grep after each story: none of these literals may remain outside the descriptor once its
story lands (NFR5).

### 3.1 Geometry & footprint (`ui.h`) → descriptor geometry (Story 1.6)

| Name | Value | Note |
| --- | --- | --- |
| `GIRAFFE_W` | 150 | pose-buffer & band width |
| `GIRAFFE_H` | 160 | pose-buffer height |
| `GIRAFFE_X` | 85  | `(320-150)/2` |
| `GIRAFFE_Y` | 34  | y 34..194 |
| `BAND_H`    | 160 | `= GIRAFFE_H` |
| `HORIZON_Y` | 165 | sky/ground split |
| box L/R (`BOX_L`/`BOX_R`) | 85 / 235 | `GIRAFFE_X` .. `GIRAFFE_X+GIRAFFE_W` |
| `IMG_W`/`IMG_H` aliases | 150 / 160 | decode stride = `IMG_W` (must follow active width) |

### 3.2 Sprite path map → descriptor asset folder + pose names (Story 1.5)

Current form: `/giraffe_<pose>.png` (flat `data/`). Target: `<folder>/<pose>.png`.

**Emotion base** (`emotionPath`, `ui.cpp`):

| Emotion | Path |
| --- | --- |
| Happy (default) | `/giraffe_happy.png` |
| Hungry  | `/giraffe_hungry.png` |
| Sad     | `/giraffe_sad.png` |
| Excited | `/giraffe_excited.png` |
| Sleepy  | `/giraffe_sleepy.png` |
| Sick    | `/giraffe_sick.png` |
| Reading | `/giraffe_reading.png` |
| Thirsty | `/giraffe_thirsty.png` |
| Bored   | `/giraffe_bored.png` |
| Dirty   | `/giraffe_dirty.png` |

**Frame lists** (`main.cpp`):

| Set | Frames | Const |
| --- | --- | --- |
| Happy rotation | `giraffe_happy.png`, `giraffe_happy2.png`, `giraffe_happy3.png` | `HAPPY_FRAMES` (n=3) |
| Blink tic | `giraffe_blink.png`, `giraffe_blink2.png`, `giraffe_blink3.png` | `BLINK_FR` (n=3) |
| Ears tic  | `giraffe_ears_up.png`, `giraffe_ears_down.png` | `EARS_FR` (n=2) |
| Tail tic  | `giraffe_tail_left.png` | `TAIL_FR` (n=1) |
| Kick pose 1 | `giraffe_kick1.png` | `setKickPose(1)` |
| Kick pose 2 | `giraffe_kick2.png` | `setKickPose(2)` |
| Dead | `giraffe_dead.png` | `die()` / boot |
| Ball (prop) | `beach_ball.png` @ 80px | `BALL_PX` |

> Note (spine additional-req, do NOT do in Epic 1): later `kick1`→`kick` rename and single-frame
> `blink` are for **new** animals; the giraffe keeps its existing 3-frame blink + `kick1`/`kick2`
> art. Freeze them as-is here.

### 3.3 Anchor points (`main.cpp`) → descriptor anchors (Epic 2)

| Anchor | Value | Used by |
| --- | --- | --- |
| Mouth X | 160 | `MOUTH_X` — eat item, bubbles |
| Food drop start Y | 52 | `DROP_Y0` |
| Mouth rest Y | 101 | `MOUTH_Y` |
| Sleep-Z start | (202, 86) | `SLEEP_X0/Y0` |
| Sleep-Z end   | (216, 42) | `SLEEP_X1/Y1` |
| Daydream cloud centre | (230, 42) | `DREAM_CX/CY` |

### 3.4 Day/night palette table (`ui.cpp` `PALETTES[8]`) → biome data (Epic 3)

Order matches `enum SkyPhase`. Runtime default = DAY (`SKY_COLOR 0x6DBC`, `GROUND_COLOR 0xCD4B`).

| Phase | sky | ground |
| --- | --- | --- |
| NIGHT     | 0x0865 | 0x2104 |
| DAWN      | 0x39ED | 0x2966 |
| SUNRISE   | 0xFC6D | 0x92E9 |
| MORNING   | 0xE671 | 0xBD2B |
| DAY       | 0x6DBC | 0xCD4B |
| AFTERNOON | 0xF64F | 0xCD0A |
| SUNSET    | 0xE36B | 0x6A25 |
| DUSK      | 0x5A2F | 0x2947 |

Transparency key (AD-6): magenta `0xF81F`, read at runtime from `giraffeBuf[0]` / `BALL_KEY`.

### 3.5 Timings & cadences (freeze; must match A/B)

| Behavior | Value |
| --- | --- |
| Happy face rotation | `HAPPY_FRAME_MS` 3500 |
| Tic cadence | start `s_ticNext` ~5000 init, then 3.5–8.5 s; blink hold 90, ears 150, tail 220 ms |
| Eat | drop 450 + 3 bites × 200 = 1050 ms total (`EAT_*`) |
| Sleep Z cycle | 2400 ms, 3 Z's |
| Daydream | show 3800 ms, gap 20000 ms, 4 icons |
| Play durations | butterfly 2200, bubbles 2400, kite 2600, kick 2400 ms (`PLAY_MS`) |
| Kick | roll-end 600, launch 1150; ball 80px, contact x195, rest y150 |
| Clean | 600 ms sparkle |
| Prank | fast-tap < 350 ms; 6 care taps → die; 6 BOOK taps → revive |
| Backlight | full 255, dim 28 after 5 min idle |
| Save throttle | ≤ once / 5 s, only when dirty (AD-8) |

### 3.6 Persistence (`main.cpp`) → `io/save` (Story 1.3)

- `struct PetSave { uint8_t magic, hu, th, fn, hy, poop, dead; }`
- `SAVE_MAGIC = 0x68` — **do not bump in Epic 1** (layout unchanged). NVS namespace `"giraffe"`, key `"pet"`.
- Restore is as-is (no decay advance). Only `Pet` stats + prank-death flag persist.

---

## 4. On-device A/B visual checklist

Flash the **pre-refactor** build (A) and confirm each row matches the "Expected (A)" column
(pre-filled from the code). After each Epic 1 story, flash (B) and confirm **B == A** for every
row the story could touch. Record pass/fail + date per story.

| # | Check | Expected (A) — pre-refactor | 1.2 | 1.3 | 1.4 | 1.5 | 1.6 |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | Sprite per emotion | Each of the 10 emotions loads its `/giraffe_<pose>.png` (§3.2); missing asset → "giraffe png missing" placeholder, no crash | | | | | |
| 2 | Meters | H/T/F/C bars at x 2/80/158/236; fill turns RED below `LOW_THRESHOLD`; redraw only on stat change | | | | | |
| 3 | Idle tics | While happy: face rotates every 3.5 s; blink(3f)/ears(2f)/tail(1f) tics cycle every 3.5–8.5 s | | | | | |
| 4 | Eat (apple) | FEED → apple drops from y52 to mouth y101, 3 shrinking bites, ~1.05 s, flicker-free | | | | | |
| 5 | Drink | DRINK → glass drains in 3 gulps at the mouth (universal, unchanged) | | | | | |
| 6 | Play cycle | PLAY rotates butterfly → bubbles → kite → kick; kick = ball rolls in, giraffe volleys it up-right off-screen | | | | | |
| 7 | Clean | CLEAN → sparkles twinkle over each poop slot, ~0.6 s, poop cleared | | | | | |
| 8 | Day/night arc | Sky/ground swap through the 8 phases; sun/moon arcs L→R, ducks behind the giraffe at midday; stars at night; fireflies dusk/night; butterfly by day | | | | | |
| 9 | Save/restore | Change stats, reboot → stats + poop restore identically, no decay jump | | | | | |
| 10 | Prank death | Mash 6 care taps fast → dead sprite, animations freeze; survives reboot | | | | | |
| 11 | Revive | While dead, mash 6 BOOK taps fast → back to full stats, normal render | | | | | |
| 12 | Sleep | At night / Sleepy → Z's drift up-right beside the head; world paused | | | | | |
| 13 | Daydream | Idle + happy → thought bubble (apple/heart/note/butterfly) above head now and then | | | | | |

**Sign-off rule:** a story is "behavior-neutral, done" only when (a) §1 still 37/37, (b) §2
still builds (record sizes), (c) every §3 literal it moved is byte-identical here, and (d) every
§4 row it could affect reads B == A. Otherwise it is not done.
