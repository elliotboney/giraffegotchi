#include "ui.h"
#include <LittleFS.h>
#include <PNGdec.h>

const Rect FEED_BTN = {90, 196, 100, 38};
const Rect BOOK_BTN = {262, 194, 50, 42};

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

void drawGiraffe(TFT_eSPI& tft, Emotion emotion) {
  const char* path;
  switch (emotion) {
    case Emotion::Hungry:  path = "/giraffe_hungry.png";  break;
    case Emotion::Sad:     path = "/giraffe_sad.png";     break;
    case Emotion::Excited: path = "/giraffe_excited.png"; break;
    case Emotion::Sleepy:  path = "/giraffe_sleepy.png";  break;
    case Emotion::Sick:    path = "/giraffe_sick.png";    break;
    case Emotion::Reading: path = "/giraffe_reading.png"; break;
    case Emotion::Happy:
    default:               path = "/giraffe_happy.png";   break;
  }

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

void drawBookButton(TFT_eSPI& tft) {
  const Rect& b = BOOK_BTN;
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, TFT_NAVY);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, TFT_WHITE);

  // open-book glyph centered in the button
  const int cx = b.x + b.w / 2;       // spine x
  const int top = b.y + 11;
  const int bot = b.y + 30;
  const int pw = 17;                   // page half-width
  // two cream pages meeting at the spine
  tft.fillTriangle(cx, top, cx - pw, top + 3, cx - pw, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx - pw, bot, cx, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx + pw, top + 3, cx + pw, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx + pw, bot, cx, bot, TFT_SILVER);
  // spine + page text lines
  tft.drawFastVLine(cx, top, bot - top, TFT_DARKGREY);
  for (int i = 0; i < 3; i++) {
    const int ly = top + 6 + i * 5;
    tft.drawFastHLine(cx - pw + 3, ly, pw - 5, TFT_DARKGREY);
    tft.drawFastHLine(cx + 3, ly, pw - 5, TFT_DARKGREY);
  }
}
