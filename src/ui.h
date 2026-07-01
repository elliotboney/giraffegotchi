#pragma once
#include <TFT_eSPI.h>
#include "pet.h"
#include "core/sky.h"        // SkyPhase enum + pure solar/phase math (native-tested)
#include "species/registry.h" // active-species descriptor (geometry accessors below)

// Savanna scene (RGB565). Sky above the horizon, golden ground below. The
// giraffe sprites use a magenta key for the WHOLE background, so the runtime
// sky/ground show through behind the giraffe — both are swapped live by the
// day/night cycle (setSkyPhase). BG_COLOR aliases the sky for meter labels.
extern uint16_t SKY_COLOR;
extern uint16_t GROUND_COLOR;
#define BG_COLOR     SKY_COLOR

// Render-side day/night state (SkyPhase + the pure math live in core/sky.h).
void setSkyPhase(SkyPhase p);                      // apply the sky/ground colours
SkyPhase currentSkyPhase();

// setCelestial stores the sun/moon screen position (from celestialPos) for the
// scenery layer to draw (occluded behind the giraffe via the sky band at centre).
void setCelestial(int cx, int cy, bool isSun);

// Active-species geometry (AD-11) — read from the descriptor at runtime, so a
// swap re-sizes placement, buffers and seams together. These replace the old
// compile-time GIRAFFE_* / HORIZON_Y / BAND_H constants; no giraffe geometry
// literal remains in code. The band covers the FULL sprite footprint (bandH ==
// spriteH): built off-screen each frame and pushed atomically so everything
// behind the silhouette (clouds up top, grass at the feet) is occluded, no
// flicker. boxL/boxR are the sprite's x-edges (the direct cloud/bird clip).
inline int spriteW()  { return activeSpecies().geom.w; }
inline int spriteH()  { return activeSpecies().geom.h; }
inline int spriteX()  { return activeSpecies().geom.x; }
inline int spriteY()  { return activeSpecies().geom.y; }
inline int horizonY() { return activeSpecies().geom.horizonY; }
inline int bandH()    { return activeSpecies().geom.h; }
inline int boxL()     { return activeSpecies().geom.x; }
inline int boxR()     { return activeSpecies().geom.x + activeSpecies().geom.w; }

// Simple axis-aligned rect with a hit-test, used for the touch feed zone.
struct Rect {
  int16_t x, y, w, h;
  bool contains(int px, int py) const {
    return px >= x && px < x + w && py >= y && py < y + h;
  }
};

// Touch hit-zones (screen coords, landscape 320x240) — a row across the bottom.
extern const Rect FEED_BTN;
extern const Rect DRINK_BTN;
extern const Rect PLAY_BTN;
extern const Rect CLEAN_BTN;
extern const Rect BOOK_BTN;

// Savanna scene: full draw, and a clipped restore used by animations to erase.
void drawScene(TFT_eSPI& tft);
void restoreBg(TFT_eSPI& tft, int x, int y, int w, int h);
void uiSetPhase(uint32_t now);      // set the breeze phase before drawing
void animateScenery(TFT_eSPI& tft); // redraw swaying grass + trees + open-sky clouds/birds (each frame)

// Composite the sky band (sky + in-box clouds/birds + top giraffe rows) into an
// off-screen sprite. Caller draws any eat item, then pushSprite(spriteX(), spriteY()).
void composeSkyBand(TFT_eSprite& band, uint16_t* gbuf);
bool cloudOrBirdInBox();            // true if a cloud/bird overlaps the giraffe x-range

void drawGiraffe(TFT_eSPI& tft, Emotion emotion);
bool renderGiraffeToBuffer(uint16_t* dst, Emotion emotion);  // decode into the active spriteW()*spriteH() buffer
bool renderSpriteToBuffer(uint16_t* dst, const char* path, int w = -1);  // decode a w-wide sprite PNG by path (w<0 => active width)
bool renderPoseToBuffer(uint16_t* dst, const char* pose, int w = -1);    // decode a pose sprite for the active species (w<0 => active width)

// Top row of four care meters, and the bottom row of five action buttons.
void drawMeters(TFT_eSPI& tft, uint8_t hunger, uint8_t thirst, uint8_t fun, uint8_t hygiene);
void drawButtons(TFT_eSPI& tft);

// Draw `count` poop blobs in the lower side background (clears empty slots).
void drawPoops(TFT_eSPI& tft, uint8_t count);

// Action-animation primitives.
void drawFood(TFT_eSPI& tft, int x, int y, int r);              // eating: apple
void drawDrink(TFT_eSPI& tft, int cx, int cy, int fillPct);     // drinking: glass of water
void drawBall(TFT_eSPI& tft, int x, int y, int r);             // playing: bouncing ball
void drawSparkle(TFT_eSPI& tft, int x, int y, int s, uint16_t color);  // cleaning: twinkle
void drawButterfly(TFT_eSPI& c, int x, int y, bool flapOpen);  // play: fluttering butterfly
void drawBubble(TFT_eSPI& c, int x, int y, int r);             // play: rising soap bubble
void drawKite(TFT_eSPI& c, int x, int y, int tailPhase);       // play: swooping kite
