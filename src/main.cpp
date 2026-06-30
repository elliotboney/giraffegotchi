#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>
#include <Preferences.h>
#include "pet.h"
#include "ui.h"

// Injected from .env by tools/load_env.py. Fallbacks keep the build working
// without a .env (firmware just stays in daytime — no WiFi/time).
#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PW
#define WIFI_PW ""
#endif
#ifndef TZ
#define TZ "CST6CDT,M3.2.0,M11.1.0"   // US Central w/ DST
#endif
#ifndef LAT
#define LAT 30.0858f
#endif
#ifndef LON
#define LON -97.8403f
#endif

// TEMP: cycle all 4 sky phases every few seconds, ignoring the real clock, so
// the day/night look can be verified without waiting for dusk. Set to 0 (then
// reflash) for normal time-driven behaviour.
#define DAYNIGHT_DEMO 0
#define DAYNIGHT_DEMO_MS 300   // ~29 s per simulated day at +15 min/step

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

// Persisted care state (survives power loss). Saved to NVS on change (throttled)
// and restored on boot. Restore is as-is — a power cut doesn't age the giraffe.
Preferences prefs;
struct PetSave { uint8_t magic, hu, th, fn, hy, poop; };
static const uint8_t  SAVE_MAGIC = 0x67;     // bump if PetSave layout changes
static bool     s_saveDirty = false;
static uint32_t s_lastSave  = 0;

static void saveState() {
  const PetSave s{ SAVE_MAGIC, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene(), pet.poopCount() };
  prefs.putBytes("pet", &s, sizeof(s));
}
static void loadState() {
  PetSave s;
  if (prefs.getBytes("pet", &s, sizeof(s)) == sizeof(s) && s.magic == SAVE_MAGIC) {
    pet.load(s.hu, s.th, s.fn, s.hy, s.poop);
    Serial.printf("[save] restored H%u T%u F%u C%u poop%u\n", s.hu, s.th, s.fn, s.hy, s.poop);
  }
}
static uint16_t* giraffeBuf = nullptr;  // persistent sprite buffer (~48 KB, heap — keeps it out of static DRAM so the WiFi stack fits)
static const int BALL_PX = 80;                      // beach-ball sprite size
TFT_eSprite ballSpr = TFT_eSprite(&tft);            // beach ball (rotatable sprite, heap-allocated)
static bool ballOk = false;
static const uint16_t BALL_KEY = 0xF81F;            // magenta transparent key
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

// Full-screen repaint after a day/night phase flip: sky, ground and the
// celestial body all change, so everything layered on top is redrawn.
static void repaintScene() {
  drawScene(tft);
  drawButtons(tft);
  drawMeters(tft, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene());
  drawPoops(tft, pet.poopCount());
  pushGiraffe();
  lastStats[0] = pet.hunger(); lastStats[1] = pet.thirst();
  lastStats[2] = pet.fun();    lastStats[3] = pet.hygiene();
  lastPoop = pet.poopCount();
}

// --- day/night cycle ---
static bool     s_timeSynced = false;
static int      s_lastYday   = -1;
static int      s_riseMin    = 6 * 60, s_setMin = 18 * 60;
static SkyPhase s_dayPhase   = PHASE_DAY;

// One-shot clock set: join WiFi (~10s cap), NTP-sync with the POSIX TZ, then
// drop WiFi — the ESP32 RTC keeps time after. Non-fatal; offline → daytime.
static void syncTime() {
  Serial.printf("[daynight] connecting to WiFi '%s' ...\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) delay(200);
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[daynight] WiFi connected (%s), syncing NTP ...\n",
                  WiFi.localIP().toString().c_str());
    configTzTime(TZ, "pool.ntp.org", "time.nist.gov");
    struct tm ti;
    s_timeSynced = getLocalTime(&ti, 8000);
    if (s_timeSynced)
      Serial.printf("[daynight] time = %04d-%02d-%02d %02d:%02d:%02d local\n",
                    ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
                    ti.tm_hour, ti.tm_min, ti.tm_sec);
  } else {
    Serial.println("[daynight] WiFi connect FAILED");
  }
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  if (!s_timeSynced) Serial.println("[daynight] time sync failed — staying in daytime");
}

// UTC offset in minutes (DST included), derived by diffing local vs GMT for
// the current instant — this core's struct tm has no tm_gmtoff.
static int tzOffsetMinutes() {
  time_t now = time(nullptr);
  struct tm lt, gt;
  localtime_r(&now, &lt);
  gmtime_r(&now, &gt);
  int dayDiff = (lt.tm_year != gt.tm_year) ? (lt.tm_year > gt.tm_year ? 1 : -1)
                                           : (lt.tm_yday - gt.tm_yday);
  return dayDiff * 1440 + (lt.tm_hour * 60 + lt.tm_min) - (gt.tm_hour * 60 + gt.tm_min);
}

// Recompute the phase from the clock; on a change swap the palette + repaint.
// force=true applies the phase WITHOUT repainting (used once during setup,
// before the first drawScene).
static const char* PHASE_NAME[8] = {
  "NIGHT", "DAWN", "SUNRISE", "MORNING", "DAY", "AFTERNOON", "SUNSET", "DUSK"};

static void updateDayNight(bool force) {
  if (!s_timeSynced) {
    if (force) { setSkyPhase(PHASE_DAY); setCelestial(288, 52, true); }
    return;
  }
  struct tm ti;
  if (!getLocalTime(&ti, 50)) return;
  if (ti.tm_yday != s_lastYday) {            // new day → recompute sun times
    solarTimes(ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
               LAT, LON, tzOffsetMinutes(), s_riseMin, s_setMin);
    s_lastYday = ti.tm_yday;
  }
  const int nowMin = ti.tm_hour * 60 + ti.tm_min;

  // Night sleep window: ~30 min after sunset until sunrise (wraps midnight).
  const int nightStart = (s_setMin + 30) % 1440;
  const bool isNight = (nightStart < s_riseMin)
                         ? (nowMin >= nightStart && nowMin < s_riseMin)
                         : (nowMin >= nightStart || nowMin < s_riseMin);
  pet.setNight(isNight);

  // Move the sun/moon every tick (cheap; the scenery layer redraws it).
  int cx, cy; bool isSun;
  celestialPos(nowMin, s_riseMin, s_setMin, cx, cy, isSun);
  setCelestial(cx, cy, isSun);

  // Swap the sky colours only when the phase actually changes (full repaint).
  const SkyPhase p = skyPhaseFor(nowMin, s_riseMin, s_setMin);
  if (force || p != s_dayPhase) {
    Serial.printf("[daynight] now=%02d:%02d  sunrise=%02d:%02d sunset=%02d:%02d  phase=%s\n",
                  ti.tm_hour, ti.tm_min, s_riseMin / 60, s_riseMin % 60,
                  s_setMin / 60, s_setMin % 60, PHASE_NAME[p]);
    s_dayPhase = p;
    setSkyPhase(p);
    if (!force) repaintScene();
  }
}

// Alternate happy faces cycled on a timer so the idle face isn't static. Loading
// a new frame into giraffeBuf is enough — the band composites it next push.
static const char* HAPPY_FRAMES[] = {"/giraffe_happy.png", "/giraffe_happy2.png", "/giraffe_happy3.png"};
static const int      HAPPY_FRAME_N  = 3;
static const uint32_t HAPPY_FRAME_MS = 3500;
static int      s_happyIdx  = 0;
static uint32_t s_happyNext = 0;

// Idle tics: short sprite sequences that play every few seconds while content,
// layered over the happy-face rotation, then return to the current happy frame.
//   blink:  open -> blink -> blink2(closed) -> blink3(opening) -> open
//   ears:   perk up -> down -> back
//   tail:   swish to the left -> back
static const char* BLINK_FR[] = {"/giraffe_blink.png", "/giraffe_blink2.png", "/giraffe_blink3.png"};
static const char* EARS_FR[]  = {"/giraffe_ears_up.png", "/giraffe_ears_down.png"};
static const char* TAIL_FR[]  = {"/giraffe_tail_left.png"};
struct Tic { const char* const* frames; int n; uint32_t holdMs; };
static const Tic TICS[] = {
  {BLINK_FR, 3, 90},
  {EARS_FR,  2, 150},
  {TAIL_FR,  1, 220},
};
static const int TIC_N = 3;
static bool     s_ticActive   = false;
static int      s_ticKind     = 0;
static int      s_ticIdx      = 0;
static uint32_t s_ticStepNext = 0;
static uint32_t s_ticNext     = 5000;          // next tic start time

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

// Sleep animation: "Z" glyphs drift up-and-right beside the head while sleeping.
// Composited INTO the band (in front of the giraffe) each frame, so they need no
// manual erase and can sit right next to the head (inside the giraffe box).
static const int      SLEEP_X0 = 202, SLEEP_Y0 = 86;   // start (by the head, lower-left)
static const int      SLEEP_X1 = 216, SLEEP_Y1 = 42;   // end (upper-right)
static const uint32_t SLEEP_CYCLE_MS = 2400;
static const int      SLEEP_ZS = 3;

struct SleepAnim { bool active = false; uint32_t start = 0; };
SleepAnim slp;

static void startSleep(uint32_t now) { slp.active = true; slp.start = now; }
static void stopSleep() { slp.active = false; }

// Daydream: an occasional thought bubble above the head while the giraffe is
// idle and content — hints at what it's thinking about. Composited into the
// band (in front of the giraffe), like the sleep Z's.
static const uint32_t DAYDREAM_SHOW_MS = 3800;
static const uint32_t DAYDREAM_GAP_MS  = 20000;    // quiet gap between daydreams
static const int      DREAM_ICONS      = 4;
struct DaydreamAnim { bool active = false; uint32_t start = 0; int icon = 0; uint32_t next = 8000; };
DaydreamAnim dream;

static void drawHeart(TFT_eSPI& c, int x, int y) {
  c.fillCircle(x - 2, y - 1, 2, TFT_RED);
  c.fillCircle(x + 2, y - 1, 2, TFT_RED);
  c.fillTriangle(x - 4, y, x + 4, y, x, y + 5, TFT_RED);
}
static void drawNote(TFT_eSPI& c, int x, int y) {
  c.fillCircle(x - 2, y + 3, 2, TFT_NAVY);
  c.fillRect(x, y - 4, 2, 7, TFT_NAVY);
  c.fillRect(x, y - 4, 5, 2, TFT_NAVY);
}

// Drawn DIRECTLY to the panel in the open sky to the right of the giraffe (clear
// of its face), with connector dots pointing back toward the head.
static const int DREAM_CX = 230, DREAM_CY = 44;
static void drawDaydream(TFT_eSPI& c, uint32_t /*now*/) {
  const int x = DREAM_CX, y = DREAM_CY;
  c.fillCircle(x,     y,     9, TFT_WHITE);          // thought cloud
  c.fillCircle(x - 9, y + 3, 6, TFT_WHITE);
  c.fillCircle(x + 9, y + 3, 6, TFT_WHITE);
  c.fillCircle(x,     y + 6, 6, TFT_WHITE);
  c.fillCircle(x - 13, y + 13, 2, TFT_WHITE);        // connector dots down-left to head
  c.fillCircle(x - 18, y + 18, 1, TFT_WHITE);
  switch (dream.icon) {                              // the wish
    case 0:  drawFood(c, x, y, 5);          break;   // apple
    case 1:  drawHeart(c, x, y);            break;
    case 2:  drawNote(c, x, y);             break;
    default: drawButterfly(c, x, y, true);  break;
  }
}
static void eraseDaydream(TFT_eSPI& c) {
  restoreBg(c, DREAM_CX - 21, DREAM_CY - 10, 39, 31);
}

// Draw the rising Z's into the band sprite at local coords (band clips to bounds).
static void drawSleepZ(TFT_eSPI& c, uint32_t now) {
  c.setTextColor(TFT_NAVY);   // transparent background
  for (int i = 0; i < SLEEP_ZS; i++) {
    const uint32_t ph = (now - slp.start + (uint32_t)i * (SLEEP_CYCLE_MS / SLEEP_ZS)) % SLEEP_CYCLE_MS;
    const float p = (float)ph / SLEEP_CYCLE_MS;
    const int x = SLEEP_X0 + (int)((SLEEP_X1 - SLEEP_X0) * p) - GIRAFFE_X;
    const int y = SLEEP_Y0 + (int)((SLEEP_Y1 - SLEEP_Y0) * p) - GIRAFFE_Y;
    const int s = 1 + (int)(p * 2.0f);          // grows 1 -> 3 as it rises
    c.setTextSize(s);
    c.setCursor(x, y);
    c.print("Z");
  }
  c.setTextSize(1);   // restore so other text (meters, buttons) is normal
}

// Play animations rotate through a list on each PLAY press. Butterfly & bubbles
// composite INTO the band (in front of the giraffe); the kite draws directly in
// the open left sky. A kick/nudge using a new sprite is reserved as a future
// 4th kind — add it before PLAY_KINDS and extend PLAY_MS / the band dispatch.
enum PlayKind { PLAY_BUTTERFLY, PLAY_BUBBLES, PLAY_KITE, PLAY_KICK, PLAY_KINDS };
static const uint32_t PLAY_MS[PLAY_KINDS] = {2200, 2400, 2600, 2400};

struct PlayAnim {
  bool active = false;
  uint32_t start = 0;
  int kind = 0;
  int kx = -999, ky = -999;        // last kite position (for direct-draw erase)
  int bx = -999, by = -999;        // last kick-ball direct position (for erase)
};
PlayAnim play_;
static int s_playKind = 0;         // advances each press so play varies
static int s_kickPose = -1;        // which pose is in giraffeBuf (-1 none, 0 normal, 1 kick1, 2 kick2)

static void startPlay(uint32_t now) {
  play_.kind  = s_playKind;
  s_playKind  = (s_playKind + 1) % PLAY_KINDS;
  play_.active = true;
  play_.start  = now;
  play_.kx = play_.ky = -999;
  play_.bx = play_.by = -999;
  s_kickPose = -1;
}

// Butterfly: a figure-8 flutter around the head, composited into the band.
static void drawPlayButterfly(TFT_eSPI& band, uint32_t t) {
  const float ph = (float)t / PLAY_MS[PLAY_BUTTERFLY] * 6.2832f;   // one loop
  const int x = 160 + (int)(42.0f * sinf(ph))         - GIRAFFE_X;
  const int y = 80  + (int)(26.0f * sinf(ph * 2.0f))  - GIRAFFE_Y; // figure-8
  drawButterfly(band, x, y, ((t / 110) & 1));
}

// Bubbles: rise from the mouth, wobble, pop near the top — composited into the band.
static void drawPlayBubbles(TFT_eSPI& band, uint32_t t) {
  for (int i = 0; i < 4; i++) {
    const int bt = (int)t - i * 480;                  // staggered spawn
    if (bt < 0) continue;
    const float life = (float)bt / 1500.0f;
    if (life >= 1.0f) continue;                       // popped / gone
    const int x = 160 + (int)(9.0f * sinf(bt / 180.0f + i)) - GIRAFFE_X;
    const int y = 101 - (int)(54.0f * life)                 - GIRAFFE_Y;
    if (life > 0.82f) drawSparkle(band, x, y, 3, TFT_WHITE); // pop
    else              drawBubble(band, x, y, 3 + (i & 1));
  }
}

// Kite: swoops side-to-side in the open left sky (x<85) at a FIXED height that
// keeps its erase box (y26..42) clear of the meters above and the clouds below.
// Direct draw, so it erases its old box each frame.
static const int KITE_Y = 32;
static void eraseKite() {
  if (play_.kx > -900) restoreBg(tft, play_.kx - 7, KITE_Y - 6, 14, 17);
  play_.kx = play_.ky = -999;
}

static void tickPlay(uint32_t now) {                  // direct-draw kinds + expiry
  const uint32_t t = now - play_.start;
  if (t >= PLAY_MS[play_.kind]) {
    if (play_.kind == PLAY_KITE) eraseKite();
    play_.active = false;
    return;
  }
  if (play_.kind == PLAY_KITE) {
    const int x = 56 + (int)(20.0f * sinf(t / 360.0f));   // swoop x 36..76
    eraseKite();
    drawKite(tft, x, KITE_Y, (int)(t / 200));
    play_.kx = x; play_.ky = KITE_Y;
  }
}

// Kick (play kind #4): a beach ball rolls in from the right and the giraffe
// volleys it up-and-right. The kick poses swap giraffeBuf directly so the kick
// "owns" the giraffe while active (the loop skips the normal emotion redraw).
// The ball composites into the band over the giraffe, draws direct in open sky.
static const int      KICK_BALL_R    = BALL_PX / 2;    // 40 — beach ball half-size (touches the foot)
static const int      KICK_CONTACT_X = 195;            // ball rest x (at the kicking foot)
static const int      KICK_REST_Y    = 150;            // ball center on the ground (190 - r)
static const uint32_t KICK_ROLL_END  = 600;            // ball has rolled in by here
static const uint32_t KICK_LAUNCH    = 1150;           // leg extends / ball launches (≈0.5s rest)

// Pose in giraffeBuf: 0 = normal (current emotion), 1 = kick1 (windup/recover),
// 2 = kick2 (full extend). Only re-decodes when the pose actually changes.
static void setKickPose(int pose) {
  if (pose == s_kickPose) return;
  if      (pose == 2) renderSpriteToBuffer(giraffeBuf, "/giraffe_kick2.png");
  else if (pose == 1) renderSpriteToBuffer(giraffeBuf, "/giraffe_kick1.png");
  else                renderGiraffeToBuffer(giraffeBuf, pet.emotion());
  s_kickPose = pose;
}

static int kickPose(uint32_t t) {
  if (t < KICK_LAUNCH - 150) return 0;   // roll-in + the ~1s delay (normal pose)
  if (t < KICK_LAUNCH)       return 1;   // windup cock
  if (t < KICK_LAUNCH + 200) return 2;   // extend (ball launches at KICK_LAUNCH)
  if (t < KICK_LAUNCH + 400) return 1;   // recover
  return 0;                              // settle (ball flying off)
}

// Deterministic ball screen position: rolls in, rests at the foot, then is
// volleyed up-and-right off screen.
static void kickBallPos(uint32_t t, uint32_t T, int& bx, int& by) {
  if (t < KICK_ROLL_END) {                               // roll in along the ground
    const float p = (float)t / KICK_ROLL_END;
    bx = 305 + (int)((KICK_CONTACT_X - 305) * p);
    by = KICK_REST_Y;
  } else if (t < KICK_LAUNCH) {                           // rest at the foot (delay + windup)
    bx = KICK_CONTACT_X; by = KICK_REST_Y;
  } else {                                                // volleyed up-and-right
    const float tt = (float)(t - KICK_LAUNCH) / (T - KICK_LAUNCH);
    bx = KICK_CONTACT_X + (int)(155.0f * tt);
    by = KICK_REST_Y - (int)(230.0f * tt) + (int)(120.0f * tt * tt);
  }
}

// Erase the parts of the old ball OUTSIDE the giraffe box (the in-box part is
// repainted by the band each frame).
static void eraseKickBall() {
  if (play_.bx <= -900) return;
  const int r = KICK_BALL_R, top = play_.by - r, h = 2 * r + 1;
  const int lo = GIRAFFE_X, ro = GIRAFFE_X + GIRAFFE_W;
  const int l = play_.bx - r, rr = play_.bx + r + 1;
  if (l < lo)  restoreBg(tft, l, top, min(rr, lo) - l, h);
  if (rr > ro) { const int r0 = max(l, ro); restoreBg(tft, r0, top, rr - r0, h); }
  play_.bx = play_.by = -999;
}

// Ball spins with horizontal travel so it looks like it's rolling (stops when it
// rests, reverses when it's kicked back the other way).
static int kickBallAngle(int bx) { return (bx - 305) * 3; }

// Rotate the ball into the band (in front of the giraffe) at screen (cx,cy); the
// band sprite clips it to the giraffe footprint.
static void drawBallBand(int cx, int cy, int angle) {
  if (!ballOk) return;
  skyBand.setPivot(cx - GIRAFFE_X, cy - GIRAFFE_Y);
  ballSpr.pushRotated(&skyBand, angle, BALL_KEY);
}

// Rotate the ball directly to the screen, clipped to OUTSIDE the box. pushRotated
// pushes the sprite's native bytes raw, so swapBytes is turned off around it.
static void drawBallDirect(int cx, int cy, int angle) {
  if (!ballOk) return;
  const int lo = GIRAFFE_X, ro = GIRAFFE_X + GIRAFFE_W;
  const bool sw = tft.getSwapBytes();
  tft.setSwapBytes(false);
  if (cx - KICK_BALL_R < lo) {
    tft.setViewport(0, 0, lo, 240, false); tft.setPivot(cx, cy);
    ballSpr.pushRotated(angle, BALL_KEY); tft.resetViewport();
  }
  if (cx + KICK_BALL_R > ro) {
    tft.setViewport(ro, 0, 320 - ro, 240, false); tft.setPivot(cx, cy);
    ballSpr.pushRotated(angle, BALL_KEY); tft.resetViewport();
  }
  tft.setSwapBytes(sw);
}

static void tickKick(uint32_t now) {
  const uint32_t t = now - play_.start, T = PLAY_MS[PLAY_KICK];
  if (t >= T) {                              // done: clean up, restore the giraffe
    eraseKickBall();
    drawPoops(tft, pet.poopCount());
    drawMeters(tft, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene());
    play_.active = false;
    s_kickPose   = -1;
    updateGiraffe(pet.emotion());
    drawButtons(tft);
    return;
  }
  setKickPose(kickPose(t));

  int bx, by;
  kickBallPos(t, T, bx, by);
  eraseKickBall();                           // restore out-of-box old-ball region
  drawPoops(tft, pet.poopCount());           // repair poop / meters the ball crossed
  drawMeters(tft, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene());
  drawBallDirect(bx, by, kickBallAngle(bx)); // out-of-box part direct (rotated)
  play_.bx = bx; play_.by = by;              // in-box part is drawn into the band
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

  giraffeBuf = (uint16_t*)malloc(GIRAFFE_W * GIRAFFE_H * sizeof(uint16_t));
  if (!giraffeBuf) Serial.println("giraffeBuf malloc failed — giraffe won't render");

  // Off-screen sky-band compositor (~25 KB). The giraffe is composited into it by
  // hand (see composeSkyBand), so no swapBytes setting is needed on the sprite.
  skyBand.setColorDepth(16);
  bandOk = (skyBand.createSprite(GIRAFFE_W, BAND_H) != nullptr);
  if (!bandOk) Serial.println("skyBand createSprite failed — clouds will clip at giraffe edge");

  syncTime();             // one-shot NTP clock set (non-fatal)
  updateDayNight(true);   // pick the starting phase before the first paint

  drawScene(tft);

  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed — run 'pio run -t uploadfs'");
  }

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  prefs.begin("giraffe", false);   // NVS namespace
  loadState();                     // restore care stats from before the power loss

  lastEmotion = pet.emotion();
  updateGiraffe(lastEmotion);
  uint16_t* tmp = (uint16_t*)malloc(BALL_PX * BALL_PX * sizeof(uint16_t));
  if (tmp && renderSpriteToBuffer(tmp, "/beach_ball.png", BALL_PX) && ballSpr.createSprite(BALL_PX, BALL_PX)) {
    ballSpr.setSwapBytes(true);
    ballSpr.pushImage(0, 0, BALL_PX, BALL_PX, tmp);   // byte-swaps into the sprite's native order
    ballSpr.setPivot(BALL_PX / 2, BALL_PX / 2);
    ballOk = true;
  }
  if (tmp) free(tmp);
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

#if DAYNIGHT_DEMO
  // TEMP: fast-forward a simulated 24h so the arc + all 8 colours sweep by.
  static uint32_t nextDemo = 0; static int demoMin = 5 * 60;
  if (now >= nextDemo) {
    nextDemo = now + DAYNIGHT_DEMO_MS;
    demoMin = (demoMin + 15) % 1440;                 // +15 sim-minutes per step
    const int R = 6 * 60 + 33, S = 20 * 60 + 37;     // Buda-ish rise/set for demo
    int cx, cy; bool isSun;
    celestialPos(demoMin, R, S, cx, cy, isSun);
    setCelestial(cx, cy, isSun);
    const SkyPhase p = skyPhaseFor(demoMin, R, S);
    if (p != s_dayPhase) {
      Serial.printf("[daynight] DEMO %02d:%02d phase=%s\n", demoMin / 60, demoMin % 60, PHASE_NAME[p]);
      s_dayPhase = p;
      setSkyPhase(p);
      repaintScene();
    }
  }
#else
  // Day/night: cheap to check, only repaints on an actual phase change.
  static uint32_t nextDayNight = 0;
  if (now >= nextDayNight) { nextDayNight = now + 60000; updateDayNight(false); }
#endif

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
  } else if (play_.active && play_.kind == PLAY_KICK) {
    tickKick(now);                    // owns giraffeBuf (kick poses) + ball + expiry
  } else {
    // Redraw the giraffe only when the emotion actually changes (covers both
    // hunger-driven and time-driven transitions, e.g. excited -> happy).
    const Emotion e = pet.emotion();
    if (e != lastEmotion) {
      if (slp.active) stopSleep();   // clear Z's before switching sprite
      updateGiraffe(e);              // loads happy.png (frame 0) when entering happy
      drawButtons(tft);
      if (e == Emotion::Happy) {
        s_happyIdx = 0; s_happyNext = now + HAPPY_FRAME_MS;
        s_ticActive = false; s_ticNext = now + 4000;
      }
    }
    // While happy: play an occasional idle tic; otherwise rotate the faces.
    if (e == Emotion::Happy) {
      if (s_ticActive) {                           // step through the tic's frames
        const Tic& t = TICS[s_ticKind];
        if (now >= s_ticStepNext) {
          s_ticIdx++;
          if (s_ticIdx < t.n) {
            renderSpriteToBuffer(giraffeBuf, t.frames[s_ticIdx]);
            s_ticStepNext = now + t.holdMs;
          } else {                                 // tic done -> current happy face
            s_ticActive = false;
            renderSpriteToBuffer(giraffeBuf, HAPPY_FRAMES[s_happyIdx]);
            s_ticNext = now + 3500 + (now % 5000); // 3.5–8.5 s until the next tic
          }
        }
      } else if (now >= s_ticNext) {               // start the next tic (cycles kinds)
        s_ticKind = (s_ticKind + 1) % TIC_N;
        s_ticActive = true; s_ticIdx = 0;
        renderSpriteToBuffer(giraffeBuf, TICS[s_ticKind].frames[0]);
        s_ticStepNext = now + TICS[s_ticKind].holdMs;
      } else if (now >= s_happyNext) {             // idle face rotation
        s_happyIdx = (s_happyIdx + 1) % HAPPY_FRAME_N;
        renderSpriteToBuffer(giraffeBuf, HAPPY_FRAMES[s_happyIdx]);
        s_happyNext = now + HAPPY_FRAME_MS;
      }
    }
    // Sleep state (Z's are drawn into the band below while active).
    if (e == Emotion::Sleepy) {
      if (!slp.active) startSleep(now);
    } else if (slp.active) {
      stopSleep();
    }

    // Daydream while idle + content: a thought bubble pops up now and then.
    const bool idle = (e == Emotion::Happy && !play_.active && !cln.active && !slp.active);
    if (idle) {
      if (!dream.active && now >= dream.next) {
        dream.active = true;  dream.start = now;
        dream.icon = (dream.icon + 1) % DREAM_ICONS;
        dream.next = now + DAYDREAM_SHOW_MS + DAYDREAM_GAP_MS;
      }
      if (dream.active && now - dream.start >= DAYDREAM_SHOW_MS) dream.active = false;
    } else {
      dream.active = false;
    }

    // One-shot action animations (background-safe; run alongside the sprite).
    if (play_.active) tickPlay(now);
    if (cln.active)   tickClean(now);
  }

  // Composite + push the full giraffe footprint every frame: the in-box grass
  // sways continuously so the band is always refreshing. One atomic push keeps
  // clouds (top) and grass (feet) occluded by the silhouette, no flicker.
  if (bandOk) {
    composeSkyBand(skyBand, giraffeBuf);
    if (eat.active) drawEatItem(skyBand, GIRAFFE_X, GIRAFFE_Y, now - eat.start);
    if (slp.active) drawSleepZ(skyBand, now);
    if (play_.active && play_.kind == PLAY_BUTTERFLY) drawPlayButterfly(skyBand, now - play_.start);
    if (play_.active && play_.kind == PLAY_BUBBLES)   drawPlayBubbles(skyBand, now - play_.start);
    if (play_.active && play_.kind == PLAY_KICK) {    // in-box ball part -> band (clips)
      int bx, by;
      kickBallPos(now - play_.start, PLAY_MS[PLAY_KICK], bx, by);
      if (bx + KICK_BALL_R > GIRAFFE_X && bx - KICK_BALL_R < GIRAFFE_X + GIRAFFE_W)
        drawBallBand(bx, by, kickBallAngle(bx));
    }
    skyBand.pushSprite(GIRAFFE_X, GIRAFFE_Y);
  } else if (eat.active) {
    pushGiraffe();
    drawEatItem(tft, 0, 0, now - eat.start);
  }

  // Daydream bubble: drawn last, in the open sky beside the giraffe. Redraw it
  // every frame while active (stays on top of drifting birds); erase on end.
  static bool dreamDrawn = false;
  const bool showDream = dream.active && !eat.active && !play_.active && !cln.active && !slp.active;
  if (showDream) { drawDaydream(tft, now); dreamDrawn = true; }
  else if (dreamDrawn) { eraseDaydream(tft); dreamDrawn = false; }

  // Redraw the meters the instant any stat changes (action or decay).
  const uint8_t hu = pet.hunger(), th = pet.thirst(), fn = pet.fun(), hy = pet.hygiene();
  if (hu != lastStats[0] || th != lastStats[1] || fn != lastStats[2] || hy != lastStats[3]) {
    drawMeters(tft, hu, th, fn, hy);
    lastStats[0] = hu; lastStats[1] = th; lastStats[2] = fn; lastStats[3] = hy;
    s_saveDirty = true;
  }

  // Redraw poop when the count changes (spawn or clean).
  const uint8_t pc = pet.poopCount();
  if (pc != lastPoop) {
    drawPoops(tft, pc);
    lastPoop = pc;
    s_saveDirty = true;
  }

  // Persist care state on change, throttled to spare the flash (≤ once / 5 s).
  if (s_saveDirty && now - s_lastSave >= 5000) {
    saveState();
    s_lastSave = now;
    s_saveDirty = false;
  }

  delay(10);
}
