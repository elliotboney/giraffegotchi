#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "pet.h"
#include "ui.h"

// CYD resistive touch (XPT2046) on its own SPI bus
#define XPT2046_IRQ  36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK  25
#define XPT2046_CS   33

// Raw touch calibration — per-unit tunable. Adjust if the FEED hit-zone feels
// offset on your board (raw XPT2046 values run ~200..3800).
static const int TS_MINX = 200, TS_MAXX = 3700;
static const int TS_MINY = 240, TS_MAXY = 3800;

TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

Pet pet;
Emotion lastEmotion;
bool wasDown = false;
uint32_t lastTick = 0;
uint8_t  lastStats[4] = {255, 255, 255, 255};  // force first meter draw
uint8_t  lastPoop = 255;                        // force first poop draw

// Eating animation: an apple drops into the giraffe's mouth and is eaten in
// shrinking bites. We capture the face region once (readRect), then each frame
// restore just that box and redraw the apple on top -> drawn over the giraffe,
// flicker-free, no per-frame PNG decode.
static const int      MOUTH_X      = 160;   // giraffe mouth (screen coords)
static const int      DROP_Y0      = 52;    // apple start (above the head)
static const int      MOUTH_Y      = 101;   // apple rest (at the mouth)
static const int      FOOD_R       = 13;
static const int      CAP_X = 131, CAP_Y = 34, CAP_W = 60, CAP_H = 88;  // captured face box
static const uint32_t EAT_DROP_MS  = 450;
static const uint32_t EAT_BITE_MS  = 200;
static const int      EAT_BITES    = 3;
static const uint32_t EAT_TOTAL    = EAT_DROP_MS + EAT_BITE_MS * EAT_BITES;

struct EatAnim { bool active = false; uint32_t start = 0; uint16_t* bg = nullptr; };
EatAnim eat;

// Repaint the giraffe at its current emotion plus the buttons (used to seed
// the eating face and to clean up afterwards).
static void redrawScene() {
  const Emotion e = pet.emotion();
  drawGiraffe(tft, e);
  drawButtons(tft);
  lastEmotion = e;
}

static void startEat(uint32_t now) {
  redrawScene();             // show the eating (excited) face first

  // Capture the clean face box by decoding the giraffe into a RAM buffer
  // (same pixel data as the screen draw -> correct colors, no panel readback).
  eat.bg = nullptr;
  uint16_t* full = (uint16_t*)malloc((size_t)GIRAFFE_W * GIRAFFE_H * sizeof(uint16_t));
  if (full) {
    if (renderGiraffeToBuffer(full, Emotion::Excited)) {
      eat.bg = (uint16_t*)malloc((size_t)CAP_W * CAP_H * sizeof(uint16_t));
      if (eat.bg) {
        for (int row = 0; row < CAP_H; row++) {
          const int srcRow = (CAP_Y - GIRAFFE_Y) + row;
          memcpy(&eat.bg[row * CAP_W],
                 &full[srcRow * GIRAFFE_W + (CAP_X - GIRAFFE_X)],
                 CAP_W * sizeof(uint16_t));
        }
      }
    }
    free(full);
  }

  eat.active = true;
  eat.start = now;
}

static void tickEat(uint32_t now) {
  const uint32_t t = now - eat.start;
  if (t >= EAT_TOTAL) {
    eat.active = false;
    if (eat.bg) { free(eat.bg); eat.bg = nullptr; }
    redrawScene();
    return;
  }

  // restore the clean face (erases the previous apple under it)
  if (eat.bg) tft.pushImage(CAP_X, CAP_Y, CAP_W, CAP_H, eat.bg);
  else        drawGiraffe(tft, Emotion::Excited);  // fallback if capture failed

  int y, r;
  if (t < EAT_DROP_MS) {     // drop into the mouth
    const float p = (float)t / EAT_DROP_MS;
    y = DROP_Y0 + (int)((MOUTH_Y - DROP_Y0) * p);
    r = FOOD_R;
  } else {                   // bite down in steps
    y = MOUTH_Y;
    const int bite = (int)((t - EAT_DROP_MS) / EAT_BITE_MS);
    r = FOOD_R - bite * (FOOD_R / EAT_BITES + 1);
    if (r < 2) r = 2;
  }
  drawFood(tft, MOUTH_X, y, r);
}

// Sleep animation: "Z" glyphs drift up-and-right from beside the head while
// sleeping. They live in the background (right of the sprite) so each frame
// erases cleanly with a background fill.
static const int      SLEEP_X0 = 214, SLEEP_Y0 = 98;   // start (lower-left, by the head)
static const int      SLEEP_X1 = 276, SLEEP_Y1 = 46;   // end (upper-right)
static const uint32_t SLEEP_CYCLE_MS = 2400;
static const int      SLEEP_ZS = 3;

struct SleepAnim {
  bool active = false;
  uint32_t start = 0;
  int lx[SLEEP_ZS], ly[SLEEP_ZS], ls[SLEEP_ZS];   // last drawn glyph box per slot
};
SleepAnim slp;

static void eraseZ(int i) {
  if (slp.lx[i] > -900)
    tft.fillRect(slp.lx[i] - 1, slp.ly[i] - 1, 6 * slp.ls[i] + 3, 8 * slp.ls[i] + 3, BG_COLOR);
  slp.lx[i] = -999;
}

static void startSleep(uint32_t now) {
  slp.active = true;
  slp.start = now;
  for (int i = 0; i < SLEEP_ZS; i++) slp.lx[i] = -999;
}

static void stopSleep() {
  for (int i = 0; i < SLEEP_ZS; i++) eraseZ(i);
  slp.active = false;
}

static void tickSleep(uint32_t now) {
  tft.setTextColor(TFT_NAVY);   // transparent background
  for (int i = 0; i < SLEEP_ZS; i++) {
    const uint32_t ph = (now - slp.start + (uint32_t)i * (SLEEP_CYCLE_MS / SLEEP_ZS)) % SLEEP_CYCLE_MS;
    const float p = (float)ph / SLEEP_CYCLE_MS;
    const int x = SLEEP_X0 + (int)((SLEEP_X1 - SLEEP_X0) * p);
    const int y = SLEEP_Y0 + (int)((SLEEP_Y1 - SLEEP_Y0) * p);
    const int s = 1 + (int)(p * 2.0f);          // grows 1 -> 3 as it rises
    eraseZ(i);
    tft.setTextSize(s);
    tft.setCursor(x, y);
    tft.print("Z");
    slp.lx[i] = x; slp.ly[i] = y; slp.ls[i] = s;
  }
  tft.setTextSize(1);   // restore so other text (hunger bar, buttons) is normal
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);            // landscape 320x240
  tft.setSwapBytes(true);        // RGB565 byte order for pushImage (PNG decode)
  tft.fillScreen(BG_COLOR);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed — run 'pio run -t uploadfs'");
  }

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  lastEmotion = pet.emotion();
  drawGiraffe(tft, lastEmotion);
  drawButtons(tft);
  drawMeters(tft, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene());
  drawPoops(tft, pet.poopCount());
  lastStats[0] = pet.hunger(); lastStats[1] = pet.thirst();
  lastStats[2] = pet.fun();    lastStats[3] = pet.hygiene();
  lastPoop = pet.poopCount();
  lastTick = millis();
}

void loop() {
  const uint32_t now = millis();

  pet.update(now - lastTick);
  lastTick = now;

  // Edge-triggered feed: one feed per press, not while held.
  const bool down = ts.touched();
  if (down && !wasDown) {
    TS_Point p = ts.getPoint();
    if (p.z > 0) {  // ignore ghost / zero-pressure reads
      // NOTE: if the FEED button doesn't respond in landscape on your unit,
      // the touch axes are swapped — map p.y to sx and p.x to sy instead.
      const int sx = constrain(map(p.x, TS_MINX, TS_MAXX, 0, tft.width()),  0, tft.width()  - 1);
      const int sy = constrain(map(p.y, TS_MINY, TS_MAXY, 0, tft.height()), 0, tft.height() - 1);
      if      (FEED_BTN.contains(sx, sy))  { pet.feed(); startEat(now); }
      else if (DRINK_BTN.contains(sx, sy)) pet.drink();
      else if (PLAY_BTN.contains(sx, sy))  pet.play();
      else if (CLEAN_BTN.contains(sx, sy)) pet.clean();
      else if (BOOK_BTN.contains(sx, sy))  pet.read();
    }
  }
  wasDown = down;

  // While eating, the animation owns the giraffe area; skip emotion redraws.
  if (eat.active) {
    tickEat(now);
  } else {
    // Redraw the giraffe only when the emotion actually changes (covers both
    // hunger-driven and time-driven transitions, e.g. excited -> happy).
    const Emotion e = pet.emotion();
    if (e != lastEmotion) {
      if (slp.active) stopSleep();   // clear Z's before switching sprite
      drawGiraffe(tft, e);
      drawButtons(tft);
      lastEmotion = e;
    }
    // Run the ambient sleep animation while sleepy.
    if (e == Emotion::Sleepy) {
      if (!slp.active) startSleep(now);
      tickSleep(now);
    } else if (slp.active) {
      stopSleep();
    }
  }

  // Redraw the meters the instant any stat changes (action or decay).
  const uint8_t hu = pet.hunger(), th = pet.thirst(), fn = pet.fun(), hy = pet.hygiene();
  if (hu != lastStats[0] || th != lastStats[1] || fn != lastStats[2] || hy != lastStats[3]) {
    drawMeters(tft, hu, th, fn, hy);
    lastStats[0] = hu; lastStats[1] = th; lastStats[2] = fn; lastStats[3] = hy;
  }

  // Redraw poop when the count changes (spawn or clean).
  const uint8_t pc = pet.poopCount();
  if (pc != lastPoop) {
    drawPoops(tft, pc);
    lastPoop = pc;
  }

  delay(10);
}
