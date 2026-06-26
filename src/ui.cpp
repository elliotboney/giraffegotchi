#include "ui.h"
#include <LittleFS.h>
#include <PNGdec.h>

const Rect FEED_BTN = {110, 196, 100, 38};

// Giraffe PNG placement (image is 150x160, centered in the band between the
// top hunger bar and the bottom feed button).
static const int IMG_W = 150, IMG_H = 160;
static const int IMG_X = (320 - IMG_W) / 2;  // 85
static const int IMG_Y = 34;                 // y 34..194

// PNGdec uses a C-style draw callback that can't capture, so the target and
// offset are file-static.
static PNG png;
static TFT_eSPI* g_tft = nullptr;

static int pngDraw(PNGDRAW* pDraw) {
  uint16_t line[IMG_W];
  png.getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  g_tft->pushImage(IMG_X, IMG_Y + pDraw->y, pDraw->iWidth, 1, line);
  return 1;  // continue decoding
}

void drawGiraffe(TFT_eSPI& tft, Mood mood) {
  const char* path = (mood == Mood::Hungry) ? "/giraffe_hungry.png"
                                            : "/giraffe_happy.png";

  File f = LittleFS.open(path, "r");
  if (f) {
    const size_t sz = f.size();
    uint8_t* buf = (uint8_t*)malloc(sz);
    if (buf) {
      f.read(buf, sz);
      f.close();
      if (png.openRAM(buf, sz, pngDraw) == PNG_SUCCESS) {
        g_tft = &tft;
        png.decode(nullptr, 0);
        png.close();
      }
      free(buf);
      return;
    }
    f.close();
  }

  // Fallback if the asset is missing/unreadable — visible, not a blank screen.
  tft.fillRect(IMG_X, IMG_Y, IMG_W, IMG_H, BG_COLOR);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("giraffe png missing", IMG_X + IMG_W / 2, IMG_Y + IMG_H / 2, 2);
}

void drawHungerBar(TFT_eSPI& tft, uint8_t hunger) {
  const int x = 10, y = 10, w = 200, h = 18;
  tft.drawRect(x - 1, y - 1, w + 2, h + 2, TFT_WHITE);
  const int fill = map(hunger, 0, 100, 0, w);
  const uint16_t col = (hunger < Pet::HUNGRY_THRESHOLD) ? TFT_RED : TFT_GREEN;
  tft.fillRect(x, y, fill, h, col);
  tft.fillRect(x + fill, y, w - fill, h, TFT_DARKGREY);
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Hunger", x + w + 8, y + 2, 2);
}

void drawFeedButton(TFT_eSPI& tft) {
  tft.fillRoundRect(FEED_BTN.x, FEED_BTN.y, FEED_BTN.w, FEED_BTN.h, 8, TFT_DARKGREEN);
  tft.drawRoundRect(FEED_BTN.x, FEED_BTN.y, FEED_BTN.w, FEED_BTN.h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("FEED", FEED_BTN.x + FEED_BTN.w / 2, FEED_BTN.y + FEED_BTN.h / 2, 4);
}
