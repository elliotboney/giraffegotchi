#pragma once
#include <TFT_eSPI.h>
#include "pet.h"

// Savanna scene (RGB565). Sky above the horizon, golden ground below. The
// giraffe sprites bake these same two bands behind the giraffe so they tile
// seamlessly. BG_COLOR aliases the sky for the (sky-region) meter labels.
#define SKY_COLOR    0x6DBC
#define GROUND_COLOR 0xCD4B
#define BG_COLOR     SKY_COLOR
static const int HORIZON_Y = 165;

// Giraffe sprite placement on screen (centered between meters and buttons).
static const int GIRAFFE_W = 150, GIRAFFE_H = 160;
static const int GIRAFFE_X = (320 - GIRAFFE_W) / 2;  // 85
static const int GIRAFFE_Y = 34;                     // y 34..194

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
void animateScenery(TFT_eSPI& tft); // redraw swaying grass + trees (call each frame)

void drawGiraffe(TFT_eSPI& tft, Emotion emotion);
bool renderGiraffeToBuffer(uint16_t* dst, Emotion emotion);  // decode into GIRAFFE_W*GIRAFFE_H buffer

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
