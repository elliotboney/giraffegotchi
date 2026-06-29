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

// Grass that sits BEHIND the giraffe (x within the box 85..235). Drawn only into
// the band sprite, so the legs occlude it and it peeks through between/around them.
// Same three depth rows as the surrounding scene (back/mid/front) so it matches.
static const Blade GRASS_BOX[] = {
  // back row (short, dark, gentle)
  {100,172,5,1,0x2A40}, {140,173,4,1,0x2A40}, {176,172,5,2,0x2A40}, {214,171,4,1,0x2A40},
  // mid row (medium)
  {110,183,8,2,0x3B80}, {158,184,7,2,0x3B80}, {200,182,8,2,0x3B80},
  // front row (tall, bright)
  { 96,193,10,3,0x4DA0}, {130,192,11,4,0x4DA0}, {164,193,11,4,0x4DA0},
  {198,192,10,3,0x4DA0}, {226,191, 9,3,0x4DA0},
};
static const int GRASS_BOX_N = sizeof(GRASS_BOX) / sizeof(GRASS_BOX[0]);

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

// Clouds + birds drift through the sky. In open sky (left/right of the giraffe)
// they draw directly to the panel. Where they cross the giraffe x-range they are
// clipped out of the direct pass and instead composited into the sky-band sprite
// (composeSkyBand) so the giraffe silhouette occludes them flicker-free.
static int  s_cloudX[2] = {-999, -999};
static int  s_birdX[3]  = {-999, -999, -999};
static bool s_birdUp[3] = {false, false, false};
static const int CLOUD_Y[2] = {50, 72};
static const int BIRD_Y[3]  = {58, 86, 66};

// Giraffe box x-edges (screen coords) — the direct cloud/bird pass clips to these.
static const int BOX_L = GIRAFFE_X, BOX_R = GIRAFFE_X + GIRAFFE_W;  // 85, 235

static void drawCloud(TFT_eSPI& tft, int x, int y) {
  const int dx[4] = {7, 18, 28, 17}, dy[4] = {4, 3, 4, 7};
  const int rx[4] = {8, 10, 7, 20},  ry[4] = {4, 5, 4, 2};
  for (int k = 0; k < 4; k++)
    tft.fillEllipse(x + dx[k], y + dy[k], rx[k], ry[k], 0xF79E);
}

// Direct (panel) cloud draw, pixel-clipped to OUTSIDE the giraffe box so the
// in-box portion is left to the band sprite. A cloud is only ~44px wide so it
// straddles at most one box edge; a viewport (vpDatum=false → absolute coords)
// clips the draw to the outside sliver without dropping whole puffs.
static void drawCloudDirect(TFT_eSPI& tft, int x, int y) {
  const int l = x - 4, r = x + 40;
  if (r <= BOX_L || l >= BOX_R) { drawCloud(tft, x, y); return; }  // fully outside
  if (l < BOX_L) {                                                 // straddles left edge
    tft.setViewport(0, 0, BOX_L, HORIZON_Y, false);
    drawCloud(tft, x, y);
    tft.resetViewport();
  } else if (r > BOX_R) {                                          // straddles right edge
    tft.setViewport(BOX_R, 0, 320 - BOX_R, HORIZON_Y, false);
    drawCloud(tft, x, y);
    tft.resetViewport();
  }
  // else fully inside the box → the band sprite owns it
}

// Erase the parts of an old cloud that lie OUTSIDE the giraffe box (the in-box
// part is owned by the band sprite, which recomposites it).
static void eraseCloud(TFT_eSPI& tft, int x, int y) {
  const int l0 = x - 4, l1 = min(x + 40, BOX_L);
  if (l1 > l0) restoreBg(tft, l0, y - 3, l1 - l0, 16);
  const int r0 = max(x - 4, BOX_R), r1 = x + 40;
  if (r1 > r0) restoreBg(tft, r0, y - 3, r1 - r0, 16);
}

static void drawBird(TFT_eSPI& tft, int x, int y, bool up) {
  const uint16_t c = 0x4208;
  if (up) { tft.drawLine(x, y + 2, x + 3, y - 1, c); tft.drawLine(x + 3, y - 1, x + 6, y + 2, c); }
  else    { tft.drawLine(x, y,     x + 3, y + 2, c); tft.drawLine(x + 3, y + 2, x + 6, y,     c); }
}

// Draw a blade at its screen position minus (xo,yo) — (0,0) for the panel, or
// (GIRAFFE_X,GIRAFFE_Y) to draw into the band sprite's local coordinates.
static void drawBlade(TFT_eSPI& tft, const Blade& b, int xo = 0, int yo = 0) {
  // tip bends by the breeze; x-shifted phase makes the blades ripple
  const int dx = (int)(b.amp * sinf((s_phase + b.x * 9) / 360.0f));
  const int bx = b.x - xo, by = b.y - yo;
  tft.drawLine(bx, by, bx - 2 + dx, by - b.h + 2, b.c);
  tft.drawLine(bx, by, bx     + dx, by - b.h,     b.c);
  tft.drawLine(bx, by, bx + 2 + dx, by - b.h + 2, b.c);
}

// True if a cloud or bird currently overlaps the giraffe x-range (so the caller
// knows to compose+push the sky band this frame).
static bool cloudInBox(int x)  { return x + 40 > BOX_L && x - 4 < BOX_R; }
static bool birdInBox(int x)   { return x + 9  > BOX_L && x - 1 < BOX_R; }

bool cloudOrBirdInBox() {
  for (int i = 0; i < 2; i++) if (s_cloudX[i] > -60  && cloudInBox(s_cloudX[i])) return true;
  for (int i = 0; i < 3; i++) if (s_birdX[i]  > -900 && birdInBox(s_birdX[i]))   return true;
  return false;
}

// Re-draw the swaying scenery at the current phase (each frame). Clouds/birds are
// advanced here and drawn DIRECTLY only in open sky (clipped out of the giraffe
// box); their in-box portion is handled by composeSkyBand().
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

  // Clouds drift slowly; erase old + draw new, both clipped to outside the box.
  for (int i = 0; i < 2; i++) {
    const int nx = (int)((s_phase / 70 + i * 190) % 400) - 40;
    if (nx != s_cloudX[i]) {
      if (s_cloudX[i] > -60) eraseCloud(tft, s_cloudX[i], CLOUD_Y[i]);
      s_cloudX[i] = nx;
      drawCloudDirect(tft, nx, CLOUD_Y[i]);
    }
  }

  // Birds fly and flap; draw direct only while their center is outside the box.
  for (int i = 0; i < 3; i++) {
    const int  nx = (int)((s_phase / 28 + i * 140) % 380) - 20;
    const bool up = ((s_phase / 160 + i) & 1);
    if (nx != s_birdX[i] || up != s_birdUp[i]) {
      const bool prevDirect = s_birdX[i] > -900 && !birdInBox(s_birdX[i]);
      if (prevDirect) restoreBg(tft, s_birdX[i] - 1, BIRD_Y[i] - 3, 10, 8);
      if (!birdInBox(nx)) drawBird(tft, nx, BIRD_Y[i], up);
      s_birdX[i] = nx;
      s_birdUp[i] = up;
    }
  }
}

// Composite the sky band off-screen: sky fill, then all clouds/birds (sprite
// auto-clips to its bounds), then the top BAND_H giraffe rows overlaid with the
// magenta key skipped so the giraffe occludes whatever is behind its silhouette.
//
// TFT_eSprite::pushImage has NO transparent-colour overload, so we composite the
// giraffe by hand directly into the band buffer. pushSprite() outputs the buffer
// raw, so each kept pixel is byte-swapped into the sprite's native order (the
// same order fillSprite/drawCloud use) — matching gbuf's little-endian PNG bytes.
void composeSkyBand(TFT_eSprite& band, uint16_t* gbuf) {
  // sky above the horizon, golden ground below (horizon in band-local rows)
  const int horizonRow = HORIZON_Y - GIRAFFE_Y;
  band.fillRect(0, 0, GIRAFFE_W, horizonRow, SKY_COLOR);
  band.fillRect(0, horizonRow, GIRAFFE_W, BAND_H - horizonRow, GROUND_COLOR);

  for (int i = 0; i < 2; i++)
    drawCloud(band, s_cloudX[i] - GIRAFFE_X, CLOUD_Y[i] - GIRAFFE_Y);
  for (int i = 0; i < 3; i++)
    drawBird(band, s_birdX[i] - GIRAFFE_X, BIRD_Y[i] - GIRAFFE_Y, s_birdUp[i]);
  for (int i = 0; i < GRASS_BOX_N; i++)
    drawBlade(band, GRASS_BOX[i], GIRAFFE_X, GIRAFFE_Y);

  uint16_t* dst = (uint16_t*)band.getPointer();
  const uint16_t key = gbuf[0];
  const int n = GIRAFFE_W * BAND_H;
  for (int i = 0; i < n; i++) {
    const uint16_t p = gbuf[i];
    if (p != key) dst[i] = (uint16_t)((p << 8) | (p >> 8));
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
static int g_bufW = IMG_W;     // row stride for g_buf (sprites can be narrower than the giraffe)

static int pngDraw(PNGDRAW* pDraw) {
  uint16_t line[IMG_W];        // IMG_W is the widest sprite (the giraffe)
  png.getLineAsRGB565(pDraw, line, PNG_RGB565_LITTLE_ENDIAN, 0xffffffff);
  if (g_buf) memcpy(&g_buf[pDraw->y * g_bufW], line, pDraw->iWidth * sizeof(uint16_t));
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

// Decode a PNG at `path` to the currently-selected target (g_tft or g_buf).
static bool decodePng(const char* path) {
  File f = LittleFS.open(path, "r");
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

static bool decodeGiraffe(Emotion emotion) { return decodePng(emotionPath(emotion)); }

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
  g_buf = dst; g_bufW = IMG_W;
  const bool ok = decodeGiraffe(emotion);
  g_buf = nullptr;
  return ok;
}

// Decode an arbitrary sprite PNG (by path) into a w-wide buffer — used for frames
// that aren't pet emotions (alternate happy faces, kick poses, the beach ball).
bool renderSpriteToBuffer(uint16_t* dst, const char* path, int w) {
  g_buf = dst; g_bufW = w;
  const bool ok = decodePng(path);
  g_buf = nullptr; g_bufW = IMG_W;
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

void drawButterfly(TFT_eSPI& c, int x, int y, bool flapOpen) {
  const uint16_t wing = 0xFC60;          // warm orange
  const uint16_t edge = 0x6180;          // dark body/antennae
  const int wf = flapOpen ? 7 : 3;       // wing half-width (flaps wide/narrow)
  const int wh = flapOpen ? 4 : 6;       // taller when folded
  c.fillEllipse(x - wf, y - 1, wf, wh, wing);          // upper wings
  c.fillEllipse(x + wf, y - 1, wf, wh, wing);
  c.fillEllipse(x - wf + 1, y + 3, wf - 1, wh - 1, wing);  // lower wings
  c.fillEllipse(x + wf - 1, y + 3, wf - 1, wh - 1, wing);
  c.drawFastVLine(x, y - 5, 11, edge);                 // body
  c.drawPixel(x - 1, y - 6, edge);                     // antennae
  c.drawPixel(x + 1, y - 6, edge);
}

void drawBubble(TFT_eSPI& c, int x, int y, int r) {
  c.drawCircle(x, y, r, 0xCE79);                       // pale blue-white ring
  c.drawPixel(x - r / 2, y - r / 2, TFT_WHITE);        // highlight
}

// Compact kite (~15px tall) so it + its erase box fit the narrow sky gap between
// the meters (y<=24) and the clouds (y>=43).
void drawKite(TFT_eSPI& c, int x, int y, int tailPhase) {
  c.fillTriangle(x, y - 5, x - 5, y, x + 5, y, 0xF800);    // red upper sail
  c.fillTriangle(x - 5, y, x + 5, y, x, y + 7, 0xFFE0);    // yellow lower sail
  c.drawLine(x, y - 5, x, y + 7, 0x4208);                  // spine
  c.drawLine(x - 5, y, x + 5, y, 0x4208);                  // spar
  const int tx = x + ((tailPhase & 1) ? 2 : -2);           // single tail bow
  c.fillTriangle(x - 2, y + 7, x + 2, y + 7, tx, y + 10, 0xFB40);
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
