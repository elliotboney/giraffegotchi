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
static uint16_t giraffeBuf[GIRAFFE_W * GIRAFFE_H];  // persistent sprite buffer (~48 KB)
TFT_eSprite skyBand = TFT_eSprite(&tft);            // off-screen sky-band compositor
bool bandOk = false;                                // false if createSprite failed
bool wasDown = false;
uint32_t lastTick = 0;
uint8_t  lastStats[4] = {255, 255, 255, 255};  // force first meter draw
uint8_t  lastPoop = 255;                        // force first poop draw

// Eating animation: an apple/glass drops into the giraffe's mouth and is eaten in
// shrinking bites. The item travels entirely within the sky band, so it is drawn
// INTO the band sprite (after the giraffe overlay) each frame and pushed atomically
// — no trail, no flicker, even with a cloud overhead.
static const int      MOUTH_X      = 160;   // giraffe mouth (screen coords)
static const int      DROP_Y0      = 52;    // item start (above the head)
static const int      MOUTH_Y      = 101;   // item rest (at the mouth)
static const int      FOOD_R       = 13;
static const uint32_t EAT_DROP_MS  = 450;
static const uint32_t EAT_BITE_MS  = 200;
static const int      EAT_BITES    = 3;
static const uint32_t EAT_TOTAL    = EAT_DROP_MS + EAT_BITE_MS * EAT_BITES;

enum ConsumeKind { CONSUME_APPLE, CONSUME_WATER };
struct EatAnim { bool active = false; uint32_t start = 0; int kind = CONSUME_APPLE; };
EatAnim eat;

// Push the persistent giraffe buffer to screen. buf[0] (top-left = magenta key)
// is used as the transparent colour so background pixels are skipped, letting
// the scene and any clouds/birds drawn before this call show through.
static void pushGiraffe() {
  tft.pushImage(GIRAFFE_X, GIRAFFE_Y, GIRAFFE_W, GIRAFFE_H, giraffeBuf, giraffeBuf[0]);
}

// Refresh the persistent buffer for the given emotion and push to screen.
// restoreBg the full rect FIRST so a shrinking silhouette (e.g. excited ears
// drop back down) doesn't leave ghost pixels in newly-transparent areas — the
// transparent push can only overwrite the solid body, never erase it.
static void updateGiraffe(Emotion e) {
  renderGiraffeToBuffer(giraffeBuf, e);
  restoreBg(tft, GIRAFFE_X, GIRAFFE_Y, GIRAFFE_W, GIRAFFE_H);
  pushGiraffe();
  lastEmotion = e;
}

// Repaint the scene after an action (play / clean) resets the display.
static void redrawScene() {
  pushGiraffe();
  drawButtons(tft);
}

static void startEat(uint32_t now, int kind) {
  eat.kind = kind;
  const Emotion e = pet.emotion();     // Excited after feed/drink
  if (e != lastEmotion) updateGiraffe(e);
  eat.active = true;
  eat.start = now;
}

// Draw the current eat item (apple shrinking / glass draining) into `c` at the
// offset (ox,oy) — (0,0) for the screen, (GIRAFFE_X,GIRAFFE_Y) for the band sprite.
static void drawEatItem(TFT_eSPI& c, int ox, int oy, uint32_t t) {
  const bool dropping = t < EAT_DROP_MS;
  int y = MOUTH_Y;
  if (dropping) {                      // drop into the mouth
    const float p = (float)t / EAT_DROP_MS;
    y = DROP_Y0 + (int)((MOUTH_Y - DROP_Y0) * p);
  }
  const int step = dropping ? -1 : (int)((t - EAT_DROP_MS) / EAT_BITE_MS);  // 0..EAT_BITES-1

  if (eat.kind == CONSUME_WATER) {     // glass drains in gulps
    int fill = dropping ? 100 : 100 - (step + 1) * 100 / EAT_BITES;
    if (fill < 0) fill = 0;
    drawDrink(c, MOUTH_X - ox, y - oy, fill);
  } else {                             // apple shrinks in bites
    int r = dropping ? FOOD_R : FOOD_R - step * (FOOD_R / EAT_BITES + 1);
    if (r < 2) r = 2;
    drawFood(c, MOUTH_X - ox, y - oy, r);
  }
}

// Sleep animation: "Z" glyphs drift up-and-right from beside the head while
// sleeping. They start just RIGHT of the giraffe box (x>=236) so they live in
// open sky and the band-sprite push never clobbers them; each frame erases
// cleanly with a background fill.
static const int      SLEEP_X0 = 238, SLEEP_Y0 = 98;   // start (lower-left, by the head)
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
    restoreBg(tft, slp.lx[i] - 1, slp.ly[i] - 1, 6 * slp.ls[i] + 3, 8 * slp.ls[i] + 3);
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
  tft.setTextSize(1);   // restore so other text (meters, buttons) is normal
}

// Play animation: a ball bounces a few times in the left background lane
// (clear of the giraffe, meters and poop, so it erases with a plain fill).
static const int      BALL_X       = 42;
static const int      BALL_GROUND  = 138;
static const int      BALL_R       = 9;
static const uint32_t PLAY_MS      = 1400;
static const int      PLAY_BOUNCES = 3;

struct PlayAnim { bool active = false; uint32_t start = 0; int ly = -999; };
PlayAnim play_;

static void erasePlay() {
  if (play_.ly > -900)
    restoreBg(tft, BALL_X - BALL_R - 1, play_.ly - BALL_R - 1, 2 * BALL_R + 2, 2 * BALL_R + 2);
  play_.ly = -999;
}

static void startPlay(uint32_t now) {
  redrawScene();
  play_.active = true; play_.start = now; play_.ly = -999;
}

static void tickPlay(uint32_t now) {
  const uint32_t t = now - play_.start;
  if (t >= PLAY_MS) { erasePlay(); play_.active = false; return; }
  const float prog = (float)t / PLAY_MS;
  const uint32_t bounceMs = PLAY_MS / PLAY_BOUNCES;
  const float bp = (float)(t % bounceMs) / bounceMs;                 // 0..1 within a bounce
  const int amp = (int)(82.0f * (1.0f - prog * 0.55f));              // decaying height
  const int y = BALL_GROUND - (int)(amp * 4.0f * bp * (1.0f - bp));  // parabola arc
  erasePlay();
  drawBall(tft, BALL_X, y, BALL_R);
  play_.ly = y;
}

// Clean animation: sparkles twinkle over each poop slot as it is swept away.
static const int POOP_PX[Pet::MAX_POOP] = {48, 48, 264, 264};
static const int POOP_PY[Pet::MAX_POOP] = {162, 186, 162, 186};
static const uint32_t CLEAN_MS = 600;

struct CleanAnim { bool active = false; uint32_t start = 0; uint8_t n = 0; };
CleanAnim cln;

static void startClean(uint32_t now, uint8_t n) {
  redrawScene();
  cln.active = true; cln.start = now; cln.n = n;
}

static void tickClean(uint32_t now) {
  const uint32_t t = now - cln.start;
  if (t >= CLEAN_MS) {
    for (int i = 0; i < cln.n; i++)
      restoreBg(tft, POOP_PX[i] - 14, POOP_PY[i] - 16, 28, 26);
    cln.active = false;
    return;
  }
  const int s = 10 - (int)(20.0f * ((float)t / CLEAN_MS < 0.5f
                  ? 0.5f - (float)t / CLEAN_MS : (float)t / CLEAN_MS - 0.5f));  // 0..10..0
  for (int i = 0; i < cln.n; i++) {
    restoreBg(tft, POOP_PX[i] - 14, POOP_PY[i] - 16, 28, 26);
    drawSparkle(tft, POOP_PX[i], POOP_PY[i] - 4, s, TFT_WHITE);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  tft.init();
  tft.setRotation(1);            // landscape 320x240
  tft.setSwapBytes(true);        // RGB565 byte order for pushImage (PNG decode)

  // Off-screen sky-band compositor (~25 KB). The giraffe is composited into it by
  // hand (see composeSkyBand), so no swapBytes setting is needed on the sprite.
  skyBand.setColorDepth(16);
  bandOk = (skyBand.createSprite(GIRAFFE_W, BAND_H) != nullptr);
  if (!bandOk) Serial.println("skyBand createSprite failed — clouds will clip at giraffe edge");

  drawScene(tft);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed — run 'pio run -t uploadfs'");
  }

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  lastEmotion = pet.emotion();
  updateGiraffe(lastEmotion);
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
  uiSetPhase(now);   // advance the breeze for the grass sway

  // Edge-triggered feed: one feed per press, not while held.
  const bool down = ts.touched();
  if (down && !wasDown) {
    TS_Point p = ts.getPoint();
    if (p.z > 0) {  // ignore ghost / zero-pressure reads
      // NOTE: if the FEED button doesn't respond in landscape on your unit,
      // the touch axes are swapped — map p.y to sx and p.x to sy instead.
      const int sx = constrain(map(p.x, TS_MINX, TS_MAXX, 0, tft.width()),  0, tft.width()  - 1);
      const int sy = constrain(map(p.y, TS_MINY, TS_MAXY, 0, tft.height()), 0, tft.height() - 1);
      if      (FEED_BTN.contains(sx, sy))  { pet.feed();  startEat(now, CONSUME_APPLE); }
      else if (DRINK_BTN.contains(sx, sy)) { pet.drink(); startEat(now, CONSUME_WATER); }
      else if (PLAY_BTN.contains(sx, sy))  { pet.play();  startPlay(now); }
      else if (CLEAN_BTN.contains(sx, sy)) { const uint8_t n = pet.poopCount(); pet.clean(); startClean(now, n); }
      else if (BOOK_BTN.contains(sx, sy))  pet.read();
    }
  }
  wasDown = down;

  animateScenery(tft);   // grass/trees + open-sky clouds/birds (clipped to box edges)

  // Giraffe ownership: handle eat expiry, emotion changes and ambient animations.
  if (eat.active) {
    if (now - eat.start >= EAT_TOTAL) {
      eat.active = false;
      updateGiraffe(pet.emotion());   // clean repaint, no food
    }
  } else {
    // Redraw the giraffe only when the emotion actually changes (covers both
    // hunger-driven and time-driven transitions, e.g. excited -> happy).
    const Emotion e = pet.emotion();
    if (e != lastEmotion) {
      if (slp.active) stopSleep();   // clear Z's before switching sprite
      updateGiraffe(e);
      drawButtons(tft);
    }
    // Run the ambient sleep animation while sleepy.
    if (e == Emotion::Sleepy) {
      if (!slp.active) startSleep(now);
      tickSleep(now);
    } else if (slp.active) {
      stopSleep();
    }

    // One-shot action animations (background-safe; run alongside the sprite).
    if (play_.active) tickPlay(now);
    if (cln.active)   tickClean(now);
  }

  // Composite + push the sky band whenever a cloud/bird overlaps the giraffe or
  // we're eating. One extra push after the last object leaves (wasBand) cleans
  // the band region. Falls back to a direct draw if the sprite failed to alloc.
  const bool bandNow = cloudOrBirdInBox() || eat.active;
  static bool wasBand = false;
  if (bandOk) {
    if (bandNow || wasBand) {
      composeSkyBand(skyBand, giraffeBuf);
      if (eat.active) drawEatItem(skyBand, GIRAFFE_X, GIRAFFE_Y, now - eat.start);
      skyBand.pushSprite(GIRAFFE_X, GIRAFFE_Y);
    }
  } else if (eat.active) {
    pushGiraffe();
    drawEatItem(tft, 0, 0, now - eat.start);
  }
  wasBand = bandNow;

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
