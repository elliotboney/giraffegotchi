#include "ui.h"
#include <LittleFS.h>
#include <PNGdec.h>

const Rect FEED_BTN = {90, 196, 100, 38};
const Rect BOOK_BTN = {262, 194, 50, 42};

// Image placement aliases (geometry is declared in ui.h so main.cpp can use it).
static const int IMG_W = GIRAFFE_W, IMG_H = GIRAFFE_H;
static const int IMG_X = GIRAFFE_X, IMG_Y = GIRAFFE_Y;

// PNGdec uses a C-style draw callback that can't capture, so targets are
// file-static. If g_buf is set, decode writes into that IMG_W*IMG_H buffer;
// otherwise it pushes straight to g_tft at the on-screen offset.
static PNG png;
static TFT_eSPI* g_tft = nullptr;
static uint16_t* g_buf = nullptr;

static int pngDraw(PNGDRAW* pDraw) {
  uint16_t line[IMG_W];
  png.getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  if (g_buf) memcpy(&g_buf[pDraw->y * IMG_W], line, pDraw->iWidth * sizeof(uint16_t));
  else       g_tft->pushImage(IMG_X, IMG_Y + pDraw->y, pDraw->iWidth, 1, line);
  return 1;  // continue decoding
}

static const char* emotionPath(Emotion emotion) {
  switch (emotion) {
    case Emotion::Hungry:  return "/giraffe_hungry.png";
    case Emotion::Sad:     return "/giraffe_sad.png";
    case Emotion::Excited: return "/giraffe_excited.png";
    case Emotion::Sleepy:  return "/giraffe_sleepy.png";
    case Emotion::Sick:    return "/giraffe_sick.png";
    case Emotion::Reading: return "/giraffe_reading.png";
    case Emotion::Happy:
    default:               return "/giraffe_happy.png";
  }
}

// Decode the emotion's PNG to the currently-selected target (g_tft or g_buf).
static bool decodeGiraffe(Emotion emotion) {
  File f = LittleFS.open(emotionPath(emotion), "r");
  if (!f) return false;
  const size_t sz = f.size();
  uint8_t* buf = (uint8_t*)malloc(sz);
  bool ok = false;
  if (buf) {
    f.read(buf, sz);
    f.close();
    if (png.openRAM(buf, sz, pngDraw) == PNG_SUCCESS) {
      png.decode(nullptr, 0);
      png.close();
      ok = true;
    }
    free(buf);
  } else {
    f.close();
  }
  return ok;
}

void drawGiraffe(TFT_eSPI& tft, Emotion emotion) {
  g_buf = nullptr;
  g_tft = &tft;
  if (decodeGiraffe(emotion)) return;

  // Fallback if the asset is missing/unreadable — visible, not a blank screen.
  tft.fillRect(IMG_X, IMG_Y, IMG_W, IMG_H, BG_COLOR);
  tft.setTextColor(TFT_RED, BG_COLOR);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("giraffe png missing", IMG_X + IMG_W / 2, IMG_Y + IMG_H / 2, 2);
}

// Decode the emotion into a caller-provided IMG_W*IMG_H buffer (same pixel
// data as the on-screen draw). Returns false if the asset can't be read.
bool renderGiraffeToBuffer(uint16_t* dst, Emotion emotion) {
  g_buf = dst;
  const bool ok = decodeGiraffe(emotion);
  g_buf = nullptr;
  return ok;
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

void drawFood(TFT_eSPI& tft, int x, int y, int r) {
  if (r <= 0) return;
  tft.fillCircle(x, y, r, TFT_RED);
  tft.fillCircle(x - r / 3, y - r / 3, r / 4 + 1, 0xFBCC);            // shine
  tft.drawFastVLine(x, y - r - 3, 4, 0x6300);                        // stem
  tft.fillTriangle(x + 1, y - r - 2, x + 7, y - r - 5, x + 4, y - r + 1, TFT_GREEN);  // leaf
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
