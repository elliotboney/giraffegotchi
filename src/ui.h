#pragma once
#include <TFT_eSPI.h>
#include "pet.h"
#include "core/sky.h"   // SkyPhase enum + pure solar/phase math (native-tested)

// Savanna scene (RGB565). Sky above the horizon, golden ground below. The
// giraffe sprites use a magenta key for the WHOLE background, so the runtime
// sky/ground show through behind the giraffe — both are swapped live by the
// day/night cycle (setSkyPhase). BG_COLOR aliases the sky for meter labels.
extern uint16_t SKY_COLOR;
extern uint16_t GROUND_COLOR;
#define BG_COLOR     SKY_COLOR
static const int HORIZON_Y = 165;

// Render-side day/night state (SkyPhase + the pure math live in core/sky.h).
void setSkyPhase(SkyPhase p);                      // apply the sky/ground colours
SkyPhase currentSkyPhase();

// setCelestial stores the sun/moon screen position (from celestialPos) for the
// scenery layer to draw (occluded behind the giraffe via the sky band at centre).
void setCelestial(int cx, int cy, bool isSun);

// Giraffe sprite placement on screen (centered between meters and buttons).
static const int GIRAFFE_W = 150, GIRAFFE_H = 160;
static const int GIRAFFE_X = (320 - GIRAFFE_W) / 2;  // 85
static const int GIRAFFE_Y = 34;                     // y 34..194

// Compositing band: covers the FULL giraffe footprint. Built off-screen each
// frame (sky+ground+clouds+birds+grass) and the giraffe is overlaid with the
// magenta key skipped, then pushed atomically so everything behind the giraffe
// (clouds up top, grass at the feet) is occluded by the silhouette, no flicker.
static const int BAND_H = GIRAFFE_H;

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
// off-screen sprite. Caller draws any eat item, then pushSprite(GIRAFFE_X, GIRAFFE_Y).
void composeSkyBand(TFT_eSprite& band, uint16_t* gbuf);
bool cloudOrBirdInBox();            // true if a cloud/bird overlaps the giraffe x-range

void drawGiraffe(TFT_eSPI& tft, Emotion emotion);
bool renderGiraffeToBuffer(uint16_t* dst, Emotion emotion);  // decode into GIRAFFE_W*GIRAFFE_H buffer
bool renderSpriteToBuffer(uint16_t* dst, const char* path, int w = GIRAFFE_W);  // decode a w-wide sprite PNG by path
bool renderPoseToBuffer(uint16_t* dst, const char* pose, int w = GIRAFFE_W);    // decode a pose sprite for the active species (folder from descriptor)

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
