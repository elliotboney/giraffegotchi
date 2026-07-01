#include "engine.h"
#include "../ui.h"                 // renderGiraffeToBuffer, TFT_eSprite, draw primitives
#include "../species/registry.h"   // activeSpecies() — specs + anchors from the descriptor
#include <math.h>                  // sinf (butterfly / bubbles)
#include <stdlib.h>                // malloc/free (food-sprite cache)
#include <stdio.h>                 // snprintf (dream-object pose path)

namespace anim {

void Engine::start(uint32_t now) {
  started_ = now;
  loaded_  = false;   // force the pose to be (re)decoded on the next setPose/forcePose
}

bool Engine::setPose(Emotion e, uint16_t* buf) {
  if (e == emotion_ && loaded_) return false;   // unchanged floor — leave the buffer
  emotion_ = e;
  renderGiraffeToBuffer(buf, e);                // emotion -> pose -> active descriptor folder
  loaded_ = true;
  return true;
}

void Engine::forcePose(Emotion e, uint16_t* buf) {
  emotion_ = e;
  renderGiraffeToBuffer(buf, e);
  loaded_ = true;
}

// Reset the idle rotation + tic timers — called when the pet (re)enters Happy.
// Mirrors the pre-refactor emotion-change reset exactly.
void Engine::enterIdle(uint32_t now) {
  const AnimSet* set = activeSpecies().anims;
  rotIdx_    = 0;
  rotNext_   = now + (set && set->idle.n ? set->idle.cadenceMs : 3500);
  ticActive_ = false;
  ticNext_   = now + 4000;
}

// Advance the idle rotation + tics while Happy. Exactly one pose write per frame,
// by the same priority the pre-refactor loop used: step an active tic > start the
// next tic > advance the face rotation. Plays any frame-count (variable length).
void Engine::tickIdle(uint32_t now, uint16_t* buf) {
  if (emotion_ != Emotion::Happy) return;
  const AnimSet* set = activeSpecies().anims;
  if (!set) return;

  if (ticActive_) {                                   // step through the active tic
    const AnimSpec& t = set->tics[ticKind_];
    if (now >= ticStepNext_) {
      ticIdx_++;
      if (ticIdx_ < t.n) {
        renderPoseToBuffer(buf, t.frames[ticIdx_]);
        ticStepNext_ = now + t.cadenceMs;
      } else {                                        // tic done -> resume the current rotation frame
        ticActive_ = false;
        if (set->idle.n) renderPoseToBuffer(buf, set->idle.frames[rotIdx_]);
        ticNext_ = now + 3500 + (now % 5000);         // 3.5–8.5 s until the next tic
      }
    }
  } else if (set->ticN && now >= ticNext_) {          // start the next tic (cycles kinds)
    ticKind_   = (ticKind_ + 1) % set->ticN;
    ticActive_ = true; ticIdx_ = 0;
    renderPoseToBuffer(buf, set->tics[ticKind_].frames[0]);
    ticStepNext_ = now + set->tics[ticKind_].cadenceMs;
  } else if (set->idle.n && now >= rotNext_) {        // idle face rotation
    rotIdx_ = (rotIdx_ + 1) % set->idle.n;
    renderPoseToBuffer(buf, set->idle.frames[rotIdx_]);
    rotNext_ = now + set->idle.cadenceMs;
  }
}

}  // namespace anim

// ---- Foreground band/direct composers (Story 2.3) ----
// Moved verbatim from main.cpp; anchors now come from the active descriptor
// (AD-11). Each draws into the given surface only — never the pose buffer (AD-5).

static const int      FOOD_R = 13;
static const uint32_t SLEEP_CYCLE_MS = 2400;
static const int      SLEEP_ZS = 3;

// Cached decode of the active species' food sprite (decoded once per sprite; the
// active-species swap in Epic 4 changes food.sprite, re-triggering the decode).
static uint16_t*      s_foodBuf  = nullptr;
static const FoodItem* s_foodItem = nullptr;   // key on the FoodItem*, not the sprite NAME: two
static int            s_foodW = 0, s_foodH = 0; // species can share the name "food" (pointers pool)

// Composite the decoded food sprite into the band BY HAND at band-local centre
// (cx,cy): byte-swapped + key-skipped + bounds-clipped, exactly like
// composeSkyBand composites the pet. TFT_eSprite::pushImage has no working
// transparent overload and runs off the sprite bounds — using it here froze the
// device when the food dropped to a negative band-local y.
static void blitFoodToBand(TFT_eSprite& band, int cx, int cy) {
  if (!s_foodBuf) return;
  uint16_t* dst = (uint16_t*)band.getPointer();
  const int bw = band.width(), bh = band.height();
  const uint16_t key = s_foodBuf[0];
  const int x0 = cx - s_foodW / 2, y0 = cy - s_foodH / 2;
  for (int j = 0; j < s_foodH; j++) {
    const int by = y0 + j;
    if (by < 0 || by >= bh) continue;
    for (int i = 0; i < s_foodW; i++) {
      const int bx = x0 + i;
      if (bx < 0 || bx >= bw) continue;
      const uint16_t p = s_foodBuf[j * s_foodW + i];
      if (p != key) dst[by * bw + bx] = (uint16_t)((p << 8) | (p >> 8));
    }
  }
}

void anim::composeEat(TFT_eSPI& c, int ox, int oy, uint32_t t, Consume kind) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  const bool dropping = t < EAT_DROP_MS;
  int y = a.mouthY;
  if (dropping) {                      // drop into the mouth
    const float p = (float)t / EAT_DROP_MS;
    y = a.foodDropY + (int)((a.mouthY - a.foodDropY) * p);
  }
  const int step = dropping ? -1 : (int)((t - EAT_DROP_MS) / EAT_BITE_MS);  // 0..EAT_BITES-1

  if (kind == Consume::Water) {        // glass drains in gulps (universal, never per-species)
    int fill = dropping ? 100 : 100 - (step + 1) * 100 / EAT_BITES;
    if (fill < 0) fill = 0;
    drawDrink(c, a.mouthX - ox, y - oy, fill);
    return;
  }

  if (activeSpecies().food) return;    // species food sprite is composited into the band (composeFoodBand)

  int r = dropping ? FOOD_R : FOOD_R - step * (FOOD_R / EAT_BITES + 1);   // default: drawn apple, shrinks
  if (r < 2) r = 2;
  drawFood(c, a.mouthX - ox, y - oy, r);
}

// Composite the active species' food sprite into the band at the mouth (FR11).
// No-op for a species that has no food (it uses the drawn apple in composeEat).
void anim::composeFoodBand(TFT_eSprite& band, uint32_t t) {
  const FoodItem* food = activeSpecies().food;
  if (!food) return;
  if (s_foodItem != food) {                    // decode once (cached; re-decodes after a swap)
    free(s_foodBuf);
    s_foodBuf = (uint16_t*)malloc((size_t)food->w * food->h * sizeof(uint16_t));
    if (s_foodBuf && !renderPoseToBuffer(s_foodBuf, food->sprite, food->w)) { free(s_foodBuf); s_foodBuf = nullptr; }
    s_foodW = food->w; s_foodH = food->h; s_foodItem = food;
  }
  if (!s_foodBuf) return;
  const SpeciesAnchors& a = activeSpecies().anchors;
  int y = a.mouthY;
  if (t < EAT_DROP_MS) { const float p = (float)t / EAT_DROP_MS; y = a.foodDropY + (int)((a.mouthY - a.foodDropY) * p); }
  blitFoodToBand(band, a.mouthX - spriteX(), y - spriteY());
}

void anim::composeSleepZ(TFT_eSprite& c, uint32_t sinceStart) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  c.setTextColor(TFT_NAVY);   // transparent background
  for (int i = 0; i < SLEEP_ZS; i++) {
    const uint32_t ph = (sinceStart + (uint32_t)i * (SLEEP_CYCLE_MS / SLEEP_ZS)) % SLEEP_CYCLE_MS;
    const float p = (float)ph / SLEEP_CYCLE_MS;
    const int x = a.sleepX0 + (int)((a.sleepX1 - a.sleepX0) * p) - spriteX();
    const int y = a.sleepY0 + (int)((a.sleepY1 - a.sleepY0) * p) - spriteY();
    const int s = 1 + (int)(p * 2.0f);          // grows 1 -> 3 as it rises
    c.setTextSize(s);
    c.setCursor(x, y);
    c.print("Z");
  }
  c.setTextSize(1);   // restore so other text (meters, buttons) is normal
}

// --- Daydream thought bubble: a white vector cloud + a per-species "wish" object
// loaded from <assetFolder>/objects/dream<N>.png. Objects are a fixed square and
// decoded once per (species, icon) into a cache, exactly like the food sprite; a
// missing PNG leaves the buffer null and the whole bubble no-ops (no empty cloud).
static const int DREAM_OBJ = 64;   // dream-object size (px); prep art to this square (== prep_sprite DREAM_PX)

static uint16_t*      s_dreamBuf  = nullptr;
static const Species* s_dreamSp   = nullptr;   // decoded for this (species, icon) pair
static int            s_dreamIcon = -1;

// (Re)decode the wish object for `icon` when the active species or icon changed.
// Prefilled with the magenta key so an object PNG smaller than DREAM_OBJ (or a
// missing/short one) stays transparent rather than showing garbage rows.
static void ensureDreamBuf(int icon) {
  const Species& sp = activeSpecies();
  if (s_dreamSp == &sp && s_dreamIcon == icon && s_dreamBuf) return;   // cached
  free(s_dreamBuf); s_dreamBuf = nullptr;
  s_dreamSp = &sp; s_dreamIcon = icon;
  if (sp.dreamN == 0) return;
  const int n = DREAM_OBJ * DREAM_OBJ;
  s_dreamBuf = (uint16_t*)malloc((size_t)n * sizeof(uint16_t));
  if (!s_dreamBuf) return;
  for (int i = 0; i < n; i++) s_dreamBuf[i] = 0xF81F;   // magenta transparency key
  char pose[24];
  snprintf(pose, sizeof(pose), "dream%d", icon + 1);   // prep flattens objects/ -> <folder>/dreamN.png
  if (!renderPoseToBuffer(s_dreamBuf, pose, DREAM_OBJ)) { free(s_dreamBuf); s_dreamBuf = nullptr; }
}

// The white thought cloud (bigger than the old icon bubble). The wish object is
// composited on top separately — band vs panel need different transparent blits.
static void drawDreamCloud(TFT_eSPI& c, int x, int y) {
  c.fillCircle(x,      y,      34, TFT_WHITE);        // thought cloud (holds a 64px object)
  c.fillCircle(x - 32, y + 13, 20, TFT_WHITE);
  c.fillCircle(x + 32, y + 13, 20, TFT_WHITE);
  c.fillCircle(x,      y + 22, 20, TFT_WHITE);
  c.fillCircle(x - 42, y + 42,  6, TFT_WHITE);        // connector dots down-left to head
  c.fillCircle(x - 52, y + 52,  3, TFT_WHITE);
}

// Wish object into the BAND sprite: hand-composite (byte-swap + key-skip +
// bounds-clip), like blitFoodToBand — TFT_eSprite::pushImage transparency is broken.
static void blitDreamToBand(TFT_eSprite& band, int cx, int cy) {
  if (!s_dreamBuf) return;
  uint16_t* dst = (uint16_t*)band.getPointer();
  const int bw = band.width(), bh = band.height();
  const uint16_t key = s_dreamBuf[0];
  const int x0 = cx - DREAM_OBJ / 2, y0 = cy - DREAM_OBJ / 2;
  for (int j = 0; j < DREAM_OBJ; j++) {
    const int by = y0 + j;
    if (by < 0 || by >= bh) continue;
    for (int i = 0; i < DREAM_OBJ; i++) {
      const int bx = x0 + i;
      if (bx < 0 || bx >= bw) continue;
      const uint16_t p = s_dreamBuf[j * DREAM_OBJ + i];
      if (p != key) dst[by * bw + bx] = (uint16_t)((p << 8) | (p >> 8));
    }
  }
}

// Wish object onto the PANEL: pushImage with the magenta key, clipped by whatever
// viewport the caller set (the picker-icon pattern — pushImage on the panel is safe
// and honours the viewport, unlike the frozen sprite overload).
static void blitDreamToPanel(TFT_eSPI& c, int cx, int cy) {
  if (!s_dreamBuf) return;
  const bool sw = c.getSwapBytes();
  c.setSwapBytes(true);
  c.pushImage(cx - DREAM_OBJ / 2, cy - DREAM_OBJ / 2, DREAM_OBJ, DREAM_OBJ, s_dreamBuf, s_dreamBuf[0]);
  c.setSwapBytes(sw);
}

void anim::composeDaydreamBand(TFT_eSprite& band, int icon) {
  ensureDreamBuf(icon);              // decode-once cache; drives the whole bubble this frame
  if (!s_dreamBuf) return;           // no art (yet) -> draw nothing, not an empty cloud
  const SpeciesAnchors& a = activeSpecies().anchors;
  drawDreamCloud(band, a.dreamCx - spriteX(), a.dreamCy - spriteY());
  blitDreamToBand(band, a.dreamCx - spriteX(), a.dreamCy - spriteY());
}

// The band only covers the pet footprint (y >= spriteY), so the bubble's top
// rows would be clipped. Draw the two parts the band can't: the open sky to the
// RIGHT of the pet (full height), and the strip ABOVE the band over the pet's
// x-range (below the meter row).
static const int DREAM_TOP_Y = 24;   // just below the meter labels

void anim::composeDaydreamDirect(TFT_eSPI& c, int icon) {
  ensureDreamBuf(icon);              // cached (band ran it first); safe if the band was skipped
  if (!s_dreamBuf) return;
  const SpeciesAnchors& a = activeSpecies().anchors;
  const int boxR = spriteX() + spriteW();
  c.setViewport(boxR, 0, 320 - boxR, horizonY(), false);            // right of the pet
  drawDreamCloud(c, a.dreamCx, a.dreamCy);
  blitDreamToPanel(c, a.dreamCx, a.dreamCy);
  c.resetViewport();
  c.setViewport(spriteX(), DREAM_TOP_Y, boxR - spriteX(), spriteY() - DREAM_TOP_Y, false);  // above the band
  drawDreamCloud(c, a.dreamCx, a.dreamCy);
  blitDreamToPanel(c, a.dreamCx, a.dreamCy);
  c.resetViewport();
}

void anim::eraseDaydreamDirect(TFT_eSPI& c) {          // erase both direct parts
  const SpeciesAnchors& a = activeSpecies().anchors;
  const int boxR = spriteX() + spriteW();
  restoreBg(c, boxR, a.dreamCy - 36, 320 - boxR, 96);                          // right open-sky
  restoreBg(c, spriteX(), DREAM_TOP_Y, boxR - spriteX(), spriteY() - DREAM_TOP_Y);  // above-band strip
}

void anim::composeButterfly(TFT_eSprite& band, uint32_t t, uint32_t period) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  const float ph = (float)t / period * 6.2832f;                     // one loop
  const int x = a.mouthX + (int)(42.0f * sinf(ph))        - spriteX();
  const int y = 80       + (int)(26.0f * sinf(ph * 2.0f)) - spriteY(); // figure-8
  drawButterfly(band, x, y, ((t / 110) & 1));
}

void anim::composeBubbles(TFT_eSprite& band, uint32_t t) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  for (int i = 0; i < 4; i++) {
    const int bt = (int)t - i * 480;                  // staggered spawn
    if (bt < 0) continue;
    const float life = (float)bt / 1500.0f;
    if (life >= 1.0f) continue;                       // popped / gone
    const int x = a.mouthX + (int)(9.0f * sinf(bt / 180.0f + i)) - spriteX();
    const int y = a.mouthY - (int)(54.0f * life)                 - spriteY();
    if (life > 0.82f) drawSparkle(band, x, y, 3, TFT_WHITE); // pop
    else              drawBubble(band, x, y, 3 + (i & 1));
  }
}
