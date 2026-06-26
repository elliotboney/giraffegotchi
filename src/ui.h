#pragma once
#include <TFT_eSPI.h>
#include "pet.h"

// Landscape background color (RGB565, soft blue).
#define BG_COLOR 0xAEDC

// Giraffe sprite placement on screen (centered between hunger bar and buttons).
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

// Touch hit-zones (screen coords, landscape 320x240).
extern const Rect FEED_BTN;
extern const Rect BOOK_BTN;

void drawGiraffe(TFT_eSPI& tft, Emotion emotion);
bool renderGiraffeToBuffer(uint16_t* dst, Emotion emotion);  // decode into GIRAFFE_W*GIRAFFE_H buffer
void drawHungerBar(TFT_eSPI& tft, uint8_t hunger);
void drawFeedButton(TFT_eSPI& tft);
void drawBookButton(TFT_eSPI& tft);

// Eating-animation food sprite (a little apple) drawn with primitives.
void drawFood(TFT_eSPI& tft, int x, int y, int r);
