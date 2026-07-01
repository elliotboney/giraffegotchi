#include "engine.h"
#include "../ui.h"                 // renderGiraffeToBuffer, TFT_eSprite, draw primitives
#include "../species/registry.h"   // activeSpecies() — specs + anchors from the descriptor
#include <math.h>                  // sinf (butterfly / bubbles)

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

void anim::composeEat(TFT_eSPI& c, int ox, int oy, uint32_t t, Consume kind) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  const bool dropping = t < EAT_DROP_MS;
  int y = a.mouthY;
  if (dropping) {                      // drop into the mouth
    const float p = (float)t / EAT_DROP_MS;
    y = a.foodDropY + (int)((a.mouthY - a.foodDropY) * p);
  }
  const int step = dropping ? -1 : (int)((t - EAT_DROP_MS) / EAT_BITE_MS);  // 0..EAT_BITES-1

  if (kind == Consume::Water) {        // glass drains in gulps
    int fill = dropping ? 100 : 100 - (step + 1) * 100 / EAT_BITES;
    if (fill < 0) fill = 0;
    drawDrink(c, a.mouthX - ox, y - oy, fill);
  } else {                             // apple shrinks in bites
    int r = dropping ? FOOD_R : FOOD_R - step * (FOOD_R / EAT_BITES + 1);
    if (r < 2) r = 2;
    drawFood(c, a.mouthX - ox, y - oy, r);
  }
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

static void drawHeart(TFT_eSPI& c, int x, int y) {
  c.fillCircle(x - 3, y - 2, 3, TFT_RED);
  c.fillCircle(x + 3, y - 2, 3, TFT_RED);
  c.fillTriangle(x - 6, y, x + 6, y, x, y + 8, TFT_RED);
}
static void drawNote(TFT_eSPI& c, int x, int y) {
  c.fillCircle(x - 3, y + 4, 3, TFT_NAVY);
  c.fillRect(x, y - 6, 3, 11, TFT_NAVY);
  c.fillRect(x, y - 6, 7, 3, TFT_NAVY);
}

// Thought-bubble shape (cloud + the current wish icon).
static void drawDreamShape(TFT_eSPI& c, int x, int y, int icon) {
  c.fillCircle(x,      y,      14, TFT_WHITE);        // thought cloud
  c.fillCircle(x - 14, y + 5,   9, TFT_WHITE);
  c.fillCircle(x + 14, y + 5,   9, TFT_WHITE);
  c.fillCircle(x,      y + 9,   9, TFT_WHITE);
  c.fillCircle(x - 21, y + 21,  3, TFT_WHITE);        // connector dots down-left to head
  c.fillCircle(x - 28, y + 28,  2, TFT_WHITE);
  switch (icon) {                                     // the wish
    case 0:  drawFood(c, x, y, 8);          break;    // apple
    case 1:  drawHeart(c, x, y);            break;
    case 2:  drawNote(c, x, y);             break;
    default: drawButterfly(c, x, y, true);  break;
  }
}

void anim::composeDaydreamBand(TFT_eSprite& band, int icon) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  drawDreamShape(band, a.dreamCx - spriteX(), a.dreamCy - spriteY(), icon);
}

void anim::composeDaydreamDirect(TFT_eSPI& c, int icon) {
  const SpeciesAnchors& a = activeSpecies().anchors;
  const int boxR = spriteX() + spriteW();
  c.setViewport(boxR, 0, 320 - boxR, horizonY(), false);
  drawDreamShape(c, a.dreamCx, a.dreamCy, icon);
  c.resetViewport();
}

void anim::eraseDaydreamDirect(TFT_eSPI& c) {          // only the open-sky part persists
  const SpeciesAnchors& a = activeSpecies().anchors;
  const int boxR = spriteX() + spriteW();
  restoreBg(c, boxR, a.dreamCy - 16, 320 - boxR, 52);
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
