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
uint8_t  lastHunger = 255;  // force first bar draw

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
  drawFeedButton(tft);
  drawBookButton(tft);
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
  drawFeedButton(tft);
  drawBookButton(tft);
  drawHungerBar(tft, pet.hunger());
  lastHunger = pet.hunger();
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
      if (FEED_BTN.contains(sx, sy)) { pet.feed(); startEat(now); }
      else if (BOOK_BTN.contains(sx, sy)) pet.read();
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
      drawGiraffe(tft, e);
      drawFeedButton(tft);
      drawBookButton(tft);
      lastEmotion = e;
    }
  }

  // Redraw the hunger bar the instant the value changes (feed or decay) —
  // no lag, no redundant overdraw.
  const uint8_t h = pet.hunger();
  if (h != lastHunger) {
    drawHungerBar(tft, h);
    lastHunger = h;
  }

  delay(10);
}
