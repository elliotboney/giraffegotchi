#pragma once
#include <TFT_eSPI.h>
#include "pet.h"

// Landscape background color (RGB565, soft blue).
#define BG_COLOR 0xAEDC

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
void drawHungerBar(TFT_eSPI& tft, uint8_t hunger);
void drawFeedButton(TFT_eSPI& tft);
void drawBookButton(TFT_eSPI& tft);
