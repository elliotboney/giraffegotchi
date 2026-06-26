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
      if (FEED_BTN.contains(sx, sy)) pet.feed();
      else if (BOOK_BTN.contains(sx, sy)) pet.read();
    }
  }
  wasDown = down;

  // Redraw the giraffe only when the emotion actually changes (covers both
  // hunger-driven and time-driven transitions, e.g. excited -> happy).
  const Emotion e = pet.emotion();
  if (e != lastEmotion) {
    drawGiraffe(tft, e);
    drawFeedButton(tft);
    drawBookButton(tft);
    lastEmotion = e;
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
