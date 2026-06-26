#include "ui.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include <math.h>

const Rect FEED_BTN  = {  4, 198, 60, 38};
const Rect DRINK_BTN = { 68, 198, 60, 38};
const Rect PLAY_BTN  = {132, 198, 60, 38};
const Rect CLEAN_BTN = {196, 198, 60, 38};
const Rect BOOK_BTN  = {260, 198, 60, 38};

// --- savanna scene ---
static const int SUN_X = 288, SUN_Y = 52, SUN_R = 16;
static const int TREE_LX = 22, TREE_RX = 298, TREE_BASEY = 172;

// Layered grass: back row sits near the horizon (small, dark, gentle sway),
// front row near the buttons (tall, bright, more sway) — gives depth. All x
// positions stay clear of the poop slots (left ~x48, right ~x264).
struct Blade { int16_t x, y, h, amp; uint16_t c; };
static const Blade GRASS[] = {
  // back row (short, dark, gentle) — scattered x, jittered height/y
  {  7,172,5,1,0x2A40}, { 22,171,4,1,0x2A40}, { 31,174,5,2,0x2A40},
  { 64,172,5,1,0x2A40}, { 76,173,4,1,0x2A40},
  {243,172,5,1,0x2A40}, {283,173,4,1,0x2A40}, {294,171,5,2,0x2A40}, {308,172,5,1,0x2A40},
  // mid row (medium)
  {  9,183,8,2,0x3B80}, { 27,182,7,2,0x3B80}, { 70,184,8,2,0x3B80}, { 79,182,7,2,0x3B80},
  {240,183,8,2,0x3B80}, {287,184,8,2,0x3B80}, {311,182,7,2,0x3B80},
  // front row (tall, bright, most sway)
  { 12,196,11,4,0x4DA0}, { 30,194,10,4,0x4DA0}, { 73,196,12,4,0x4DA0},
  {245,195,11,4,0x4DA0}, {289,196,11,4,0x4DA0}, {313,194,10,4,0x4DA0},
};
static const int GRASS_N = sizeof(GRASS) / sizeof(GRASS[0]);

static uint32_t s_phase = 0;                 // breeze phase (ms); set each frame by main
void uiSetPhase(uint32_t now) { s_phase = now; }

static bool overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
  return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void drawSun(TFT_eSPI& tft) {
  tft.fillCircle(SUN_X, SUN_Y, SUN_R, 0xFEC0);             // warm yellow
  tft.fillCircle(SUN_X - 4, SUN_Y - 4, SUN_R / 2, 0xFF30); // highlight
}

static int treeSway(int bx) {
  return (int)(2.0f * sinf((s_phase + bx * 40) / 900.0f));   // slow canopy sway, -2..2
}

static void drawTree(TFT_eSPI& tft, int bx, int by) {
  const int sw = treeSway(bx);
  tft.fillRect(bx - 2, by - 42, 4, 42, 0x6A40);                 // trunk (still)
  tft.drawLine(bx, by - 40, bx - 9 + sw, by - 48, 0x6A40);      // branches follow canopy
  tft.drawLine(bx, by - 40, bx + 9 + sw, by - 48, 0x6A40);
  tft.fillRect(bx - 22 + sw, by - 54, 44, 9, 0x33A0);           // flat-top canopy
  tft.fillRect(bx - 17 + sw, by - 58, 34, 6, 0x3BC0);
  tft.fillRect(bx - 10 + sw, by - 61, 20, 4, 0x4440);
}

static void drawBlade(TFT_eSPI& tft, const Blade& b) {
  // tip bends by the breeze; x-shifted phase makes the blades ripple
  const int dx = (int)(b.amp * sinf((s_phase + b.x * 9) / 360.0f));
  tft.drawLine(b.x, b.y, b.x - 2 + dx, b.y - b.h + 2, b.c);
  tft.drawLine(b.x, b.y, b.x     + dx, b.y - b.h,     b.c);
  tft.drawLine(b.x, b.y, b.x + 2 + dx, b.y - b.h + 2, b.c);
}

// Re-draw the swaying scenery at the current phase (called each frame). Grass
// blades are thin lines so they redraw every frame; the trees only redraw when
// their (integer) sway changes, otherwise the big canopy fill flickers.
void animateScenery(TFT_eSPI& tft) {
  for (int i = 0; i < GRASS_N; i++) {
    const Blade& b = GRASS[i];
    tft.fillRect(b.x - (b.amp + 3), b.y - b.h - 1, 2 * (b.amp + 3), b.h + 2, GROUND_COLOR);
    drawBlade(tft, b);
  }
  static int lastL = 99, lastR = 99;
  const int swL = treeSway(TREE_LX), swR = treeSway(TREE_RX);
  if (swL != lastL) {
    tft.fillRect(TREE_LX - 25, TREE_BASEY - 62, 50, 19, SKY_COLOR);
    drawTree(tft, TREE_LX, TREE_BASEY);
    lastL = swL;
  }
  if (swR != lastR) {
    tft.fillRect(TREE_RX - 25, TREE_BASEY - 62, 50, 19, SKY_COLOR);
    drawTree(tft, TREE_RX, TREE_BASEY);
    lastR = swR;
  }
}

// Redraw scene props whose bounding box intersects the given rect.
static void drawProps(TFT_eSPI& tft, int x, int y, int w, int h) {
  if (overlap(x, y, w, h, SUN_X - SUN_R, SUN_Y - SUN_R, 2 * SUN_R, 2 * SUN_R)) drawSun(tft);
  if (overlap(x, y, w, h, TREE_LX - 25, TREE_BASEY - 62, 50, 62)) drawTree(tft, TREE_LX, TREE_BASEY);
  if (overlap(x, y, w, h, TREE_RX - 25, TREE_BASEY - 62, 50, 62)) drawTree(tft, TREE_RX, TREE_BASEY);
  for (int i = 0; i < GRASS_N; i++) {
    const Blade& b = GRASS[i];
    if (overlap(x, y, w, h, b.x - (b.amp + 3), b.y - b.h - 1, 2 * (b.amp + 3), b.h + 2))
      drawBlade(tft, b);
  }
}

void drawScene(TFT_eSPI& tft) {
  tft.fillRect(0, 0, 320, HORIZON_Y, SKY_COLOR);
  tft.fillRect(0, HORIZON_Y, 320, 240 - HORIZON_Y, GROUND_COLOR);
  drawProps(tft, 0, 0, 320, 240);
}

void restoreBg(TFT_eSPI& tft, int x, int y, int w, int h) {
  if (y >= HORIZON_Y) {
    tft.fillRect(x, y, w, h, GROUND_COLOR);
  } else if (y + h <= HORIZON_Y) {
    tft.fillRect(x, y, w, h, SKY_COLOR);
  } else {                                  // straddles the horizon
    tft.fillRect(x, y, w, HORIZON_Y - y, SKY_COLOR);
    tft.fillRect(x, HORIZON_Y, w, (y + h) - HORIZON_Y, GROUND_COLOR);
  }
  drawProps(tft, x, y, w, h);
}

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
    case Emotion::Thirsty: return "/giraffe_thirsty.png";
    case Emotion::Bored:   return "/giraffe_bored.png";
    case Emotion::Dirty:   return "/giraffe_dirty.png";
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
  restoreBg(tft, IMG_X, IMG_Y, IMG_W, IMG_H);
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

static void drawMeter(TFT_eSPI& tft, int cellX, uint8_t value, uint16_t color, const char* label) {
  const int y = 8, bx = cellX + 14, bw = 58, bh = 12, by = y + 2;
  tft.setTextColor(TFT_WHITE, BG_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(label, cellX, y, 2);
  tft.drawRect(bx - 1, by - 1, bw + 2, bh + 2, TFT_WHITE);
  const int fill = (int)value * bw / 100;
  tft.fillRect(bx, by, fill, bh, value < Pet::LOW_THRESHOLD ? TFT_RED : color);
  tft.fillRect(bx + fill, by, bw - fill, bh, TFT_DARKGREY);
}

void drawMeters(TFT_eSPI& tft, uint8_t hunger, uint8_t thirst, uint8_t fun, uint8_t hygiene) {
  drawMeter(tft, 2,   hunger,  TFT_GREEN,   "H");
  drawMeter(tft, 80,  thirst,  TFT_CYAN,    "T");
  drawMeter(tft, 158, fun,     TFT_YELLOW,  "F");
  drawMeter(tft, 236, hygiene, TFT_MAGENTA, "C");
}

static void drawTextBtn(TFT_eSPI& tft, const Rect& b, const char* label, uint16_t bg) {
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, bg);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 8, TFT_WHITE);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(label, b.x + b.w / 2, b.y + b.h / 2, 2);
}

void drawFood(TFT_eSPI& tft, int x, int y, int r) {
  if (r <= 0) return;
  tft.fillCircle(x, y, r, TFT_RED);
  tft.fillCircle(x - r / 3, y - r / 3, r / 4 + 1, 0xFBCC);            // shine
  tft.drawFastVLine(x, y - r - 3, 4, 0x6300);                        // stem
  tft.fillTriangle(x + 1, y - r - 2, x + 7, y - r - 5, x + 4, y - r + 1, TFT_GREEN);  // leaf
}

void drawDrink(TFT_eSPI& tft, int cx, int cy, int fillPct) {
  const int w = 18, h = 24;
  const int x = cx - w / 2, y = cy - h / 2;
  const int wh = (h - 4) * fillPct / 100;                            // water height
  tft.fillRect(x + 2, y + h - 2 - wh, w - 4, wh, 0x047F);            // water (blue)
  if (wh > 2) tft.drawFastHLine(x + 2, y + h - 2 - wh, w - 4, TFT_CYAN);  // surface
  tft.drawRect(x, y, w, h, TFT_WHITE);                               // glass
  tft.drawRect(x + 1, y, w - 2, h, 0xC618);
}

void drawBall(TFT_eSPI& tft, int x, int y, int r) {
  tft.fillCircle(x, y, r, TFT_RED);
  tft.drawFastHLine(x - r, y, 2 * r + 1, TFT_WHITE);                 // stripe
  tft.fillCircle(x - r / 3, y - r / 3, r / 4 + 1, 0xFFFF);           // shine
}

void drawSparkle(TFT_eSPI& tft, int x, int y, int s, uint16_t color) {
  if (s <= 0) return;
  tft.drawFastVLine(x, y - s, 2 * s + 1, color);
  tft.drawFastHLine(x - s, y, 2 * s + 1, color);
  const int d = s / 2;                                              // shorter diagonals
  tft.drawLine(x - d, y - d, x + d, y + d, color);
  tft.drawLine(x - d, y + d, x + d, y - d, color);
}

static void drawBookGlyph(TFT_eSPI& tft, const Rect& b) {
  // open-book glyph centered in the button
  const int cx = b.x + b.w / 2;       // spine x
  const int top = b.y + 11;
  const int bot = b.y + 30;
  const int pw = 13;                   // page half-width
  tft.fillTriangle(cx, top, cx - pw, top + 3, cx - pw, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx - pw, bot, cx, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx + pw, top + 3, cx + pw, bot, TFT_SILVER);
  tft.fillTriangle(cx, top, cx + pw, bot, cx, bot, TFT_SILVER);
  tft.drawFastVLine(cx, top, bot - top, TFT_DARKGREY);
  for (int i = 0; i < 3; i++) {
    const int ly = top + 6 + i * 5;
    tft.drawFastHLine(cx - pw + 3, ly, pw - 5, TFT_DARKGREY);
    tft.drawFastHLine(cx + 3, ly, pw - 5, TFT_DARKGREY);
  }
}

void drawButtons(TFT_eSPI& tft) {
  drawTextBtn(tft, FEED_BTN,  "FEED",  TFT_DARKGREEN);
  drawTextBtn(tft, DRINK_BTN, "DRINK", 0x019F);   // blue
  drawTextBtn(tft, PLAY_BTN,  "PLAY",  0xC300);   // orange
  drawTextBtn(tft, CLEAN_BTN, "CLEAN", 0x6810);   // purple
  tft.fillRoundRect(BOOK_BTN.x, BOOK_BTN.y, BOOK_BTN.w, BOOK_BTN.h, 8, TFT_NAVY);
  tft.drawRoundRect(BOOK_BTN.x, BOOK_BTN.y, BOOK_BTN.w, BOOK_BTN.h, 8, TFT_WHITE);
  drawBookGlyph(tft, BOOK_BTN);
}

static void drawPoop(TFT_eSPI& tft, int x, int y) {
  tft.fillEllipse(x,     y,     11, 5, 0x6B40);   // base coil (brown)
  tft.fillEllipse(x - 1, y - 6,  8, 4, 0x7BC0);
  tft.fillEllipse(x,     y - 11, 5, 3, 0x9CC0);
}

void drawPoops(TFT_eSPI& tft, uint8_t count) {
  static const int px[Pet::MAX_POOP] = {48, 48, 264, 264};
  static const int py[Pet::MAX_POOP] = {162, 186, 162, 186};
  for (int i = 0; i < Pet::MAX_POOP; i++) {
    if (i < count) drawPoop(tft, px[i], py[i]);
    else           restoreBg(tft, px[i] - 13, py[i] - 15, 26, 22);
  }
}
