#include <Arduino.h>
#include <SPI.h>
#include <LittleFS.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include <WiFi.h>
#include <time.h>
#include "pet.h"
#include "ui.h"
#include "io/save.h"
#include "anim/engine.h"

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
anim::Engine engine;   // data-driven animation engine (owns the emotion-base pose floor, AD-5/AD-12)

// Prank-death state (persisted via io/save). Owned here (orchestration); the
// save module takes it by value/ref — a power cut doesn't age the giraffe.
bool s_dead = false;
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

// Eating animation state (the composer + anchors live in anim/engine). The item
// travels entirely within the sky band, drawn after the giraffe overlay each
// frame and pushed atomically — no trail, no flicker, even with a cloud overhead.
static const uint32_t EAT_TOTAL = EAT_DROP_MS + EAT_BITE_MS * EAT_BITES;  // consts from engine.h
EatAnim eat;

// Push the persistent giraffe buffer to screen. buf[0] (top-left = magenta key)
// is used as the transparent colour so background pixels are skipped, letting
// the scene and any clouds/birds drawn before this call show through.
static void pushGiraffe() {
  if (!giraffeBuf) return;
  tft.pushImage(spriteX(), spriteY(), spriteW(), spriteH(), giraffeBuf, giraffeBuf[0]);
}

// Refresh the persistent buffer for the given emotion and push to screen.
// restoreBg the full rect FIRST so a shrinking silhouette (e.g. excited ears
// drop back down) doesn't leave ghost pixels in newly-transparent areas — the
// transparent push can only overwrite the solid body, never erase it.
static void updateGiraffe(Emotion e) {
  engine.forcePose(e, giraffeBuf);   // engine owns the emotion-base pose write (decodes into giraffeBuf)
  restoreBg(tft, spriteX(), spriteY(), spriteW(), spriteH());
  pushGiraffe();
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

// Happy-face rotation + idle tics (blink/ears/tail) are now DATA in the giraffe
// descriptor (species/giraffe.cpp) and played by anim::Engine (enterIdle/tickIdle).

static void startEat(uint32_t now, int kind) {
  eat.kind = kind;
  const Emotion e = pet.emotion();     // Excited after feed/drink
  if (e != engine.emotion()) updateGiraffe(e);
  eat.active = true;
  eat.start = now;
}


// Sleep + daydream STATE + scheduling (the composers + anchors live in anim/engine).
SleepAnim slp;
static void startSleep(uint32_t now) { slp.active = true; slp.start = now; }
static void stopSleep() { slp.active = false; }

// Daydream: an occasional thought bubble above the head while idle + content.
static const uint32_t DAYDREAM_SHOW_MS = 3800;
static const uint32_t DAYDREAM_GAP_MS  = 20000;    // quiet gap between daydreams
static const int      DREAM_ICONS      = 4;
DaydreamAnim dream;

// Play animations rotate through a list on each PLAY press. Butterfly & bubbles
// are universal band effects; the KITE (direct swoop) and KICK (ball physics) are
// SIGNATURE CAPABILITY HOOKS (AD-12/FR6) — they only cycle + run when the active
// descriptor declares CAP_KITE / CAP_KICK, so a species without them plays only
// butterfly/bubbles and none of the giraffe kick/kite code runs (FR10).
enum PlayKind { PLAY_BUTTERFLY, PLAY_BUBBLES, PLAY_KITE, PLAY_KICK, PLAY_KINDS };
static const uint32_t PLAY_MS[PLAY_KINDS] = {2200, 2400, 2600, 2400};

// Is this play kind available for the active species? Butterfly/bubbles always;
// kite/kick are opt-in capability hooks.
static bool playKindSupported(int kind) {
  if (kind == PLAY_KITE) return activeSpecies().caps & CAP_KITE;
  if (kind == PLAY_KICK) return activeSpecies().caps & CAP_KICK;
  return true;   // butterfly, bubbles — universal
}

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
  // Pick the next SUPPORTED kind starting at s_playKind (skips undeclared hooks).
  int k = s_playKind;
  for (int i = 0; i < PLAY_KINDS && !playKindSupported(k); i++) k = (k + 1) % PLAY_KINDS;
  play_.kind  = k;
  s_playKind  = (k + 1) % PLAY_KINDS;
  play_.active = true;
  play_.start  = now;
  play_.kx = play_.ky = -999;
  play_.bx = play_.by = -999;
  s_kickPose = -1;
}

// Butterfly + bubbles play effects compose into the band via anim:: (Story 2.3).

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
  if      (pose == 2) renderPoseToBuffer(giraffeBuf, "kick2");
  else if (pose == 1) renderPoseToBuffer(giraffeBuf, "kick1");
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
  const int lo = spriteX(), ro = spriteX() + spriteW();
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
  skyBand.setPivot(cx - spriteX(), cy - spriteY());
  ballSpr.pushRotated(&skyBand, angle, BALL_KEY);
}

// Rotate the ball directly to the screen, clipped to OUTSIDE the box. pushRotated
// pushes the sprite's native bytes raw, so swapBytes is turned off around it.
static void drawBallDirect(int cx, int cy, int angle) {
  if (!ballOk) return;
  const int lo = spriteX(), ro = spriteX() + spriteW();
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

// --- backlight dimming ---
// Full brightness normally; dims after a few minutes of no touch, back to full
// on any tap. PWM the backlight pin via ledc.
static const int      BL_CH        = 0;        // ledc channel
static const uint8_t  BL_FULL      = 255;
static const uint8_t  BL_DIM       = 28;       // ~11% — visibly dim, not off
static const uint32_t DIM_AFTER_MS = 5 * 60 * 1000;   // idle this long -> dim
static uint32_t s_lastInteract = 0;
static bool     s_dimmed       = false;

static void backlight(uint8_t level) { ledcWrite(BL_CH, level); }
static void wakeScreen(uint32_t now) {
  s_lastInteract = now;
  if (s_dimmed) { backlight(BL_FULL); s_dimmed = false; }
}

// --- runtime species swap (AD-13) ---
static int s_pendingSwap = -1;   // latched swap request (registry index); applied atop loop()

// Bytes for the pose buffer sized to the LARGEST species — allocated once so a
// swap never reallocates it (see applySpeciesSwap). The buffer is packed at the
// active width per decode; oversize is harmless.
static size_t maxPoseBytes() {
  size_t m = 0;
  for (int i = 0; i < speciesCount(); i++) {
    const size_t n = (size_t)speciesAt(i).geom.w * speciesAt(i).geom.h;
    if (n > m) m = n;
  }
  return m * sizeof(uint16_t);
}

// (Re)load the beach-ball sprite for the ACTIVE species (kick prop). ballOk stays
// false if the species has no ball. Used at boot and after a swap.
static void reloadBall() {
  ballOk = false;
  if (ballSpr.created()) ballSpr.deleteSprite();
  uint16_t* tmp = (uint16_t*)malloc(BALL_PX * BALL_PX * sizeof(uint16_t));
  if (tmp && renderPoseToBuffer(tmp, "beach_ball", BALL_PX) && ballSpr.createSprite(BALL_PX, BALL_PX)) {
    ballSpr.setSwapBytes(true);
    ballSpr.pushImage(0, 0, BALL_PX, BALL_PX, tmp);   // byte-swaps into the sprite's native order
    ballSpr.setPivot(BALL_PX / 2, BALL_PX / 2);
    ballOk = true;
  }
  if (tmp) free(tmp);
}

// Apply a latched species swap as ONE atomic sequence (AD-13). Only ever called
// from the top of loop(), never inline mid-frame, so no frame straddles two
// species. Care stats are NOT touched (FR12; per-animal state is Story 4.2).
static void applySpeciesSwap(int idx) {
  if (idx < 0 || idx >= speciesCount() || idx == activeSpeciesIndex()) return;
  const int prevIdx = activeSpeciesIndex();

  // 1. cancel every in-flight animation + reset pose ownership
  eat.active = play_.active = cln.active = slp.active = dream.active = false;
  s_kickPose = -1;

  // 2. preserve the CURRENT animal's care state, switch species, and re-create the
  //    compositing band to the new footprint. The pose buffer is allocated ONCE at
  //    the max species size (setup) and never reallocated — a swap only re-creates
  //    the band, which reuses its just-freed block. (Reallocating the pose buffer
  //    too held both species' large buffers at once, exhausting/fragmenting the
  //    heap so the fail-safe reverted the swap and left a black screen.)
  (void)prevIdx;
  save::captureActive(pet, s_dead);   // A's stats kept (FR12)
  setActiveSpecies(idx);
  skyBand.deleteSprite();
  bandOk = (skyBand.createSprite(spriteW(), bandH()) != nullptr);

  // 3. load the NEW animal's own care state, decode its sprites, reset anim state
  save::loadActive(pet, s_dead);      // B resumes its own last-saved stats (FR12)
  reloadBall();
  if (s_dead) renderPoseToBuffer(giraffeBuf, "dead");
  else        engine.forcePose(pet.emotion(), giraffeBuf);
  engine.enterIdle(millis());     // reset idle rotation/tic indices to the new anim set

  // 4. refresh the palette from the NEW biome at the current phase, then a full-
  //    screen repaint — clears any out-of-box element (a ball in flight leaves no
  //    orphan) and paints the new world.
  setSkyPhase(currentSkyPhase());
  repaintScene();
  save::writeNow(pet, s_dead);    // persist the new active id + both animals' blocks
  Serial.printf("[swap] now %s\n", activeSpecies().name);
}

// --- animal picker (long-press BOOK) ---
// Full-screen modal grid: one tile per species (icon sprite + lowercase name,
// current outlined) plus a back tile, on a neutral dark panel (UX-DR2/6).
static const uint32_t LONG_PRESS_MS = 800;   // BOOK held this long -> open the picker
static bool     s_pickerOpen = false;
static uint32_t s_pressStart = 0;            // when the current touch began
static bool     s_pressBook  = false;        // current press began inside the BOOK rect
static bool     s_longFired  = false;        // long-press already fired this hold

static const uint16_t PK_BG   = 0x2104;      // neutral dark panel
static const uint16_t PK_TILE = 0x3186;      // tile fill
static const int PK_COLS   = 3;
static const int PK_TW = 100, PK_TH = 108, PK_GAP = 4, PK_X0 = 9, PK_Y0 = 42;

// Tile rect for item i (0..speciesCount-1 = species, then the back tile). Targets
// are 100x108 >= 44px (UX-DR6).
static Rect pickerTile(int i) {
  const int col = i % PK_COLS, row = i / PK_COLS;
  return { (int16_t)(PK_X0 + col * (PK_TW + PK_GAP)), (int16_t)(PK_Y0 + row * (PK_TH + PK_GAP)),
           (int16_t)PK_TW, (int16_t)PK_TH };
}

// Draw a species' icon sprite centred in its tile (fallback: nothing, the name
// stands alone). Decodes <folder>/<icon>.png for that species (not the active one).
static void drawPickerIcon(const Species& s, const Rect& r) {
  if (!s.icon) return;
  static uint16_t icon[64 * 64];
  char path[48];
  snprintf(path, sizeof(path), "%s/%s.png", s.assetFolder, s.icon);
  if (renderSpriteToBuffer(icon, path, 64)) {
    const bool sw = tft.getSwapBytes();
    tft.setSwapBytes(true);
    tft.pushImage(r.x + (r.w - 64) / 2, r.y + 8, 64, 64, icon, icon[0]);
    tft.setSwapBytes(sw);
  }
}

static void drawPicker() {
  tft.fillScreen(PK_BG);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString("choose animal", 160, 22, 4);

  const int active = activeSpeciesIndex();
  for (int i = 0; i < speciesCount(); i++) {
    const Rect r = pickerTile(i);
    const Species& s = speciesAt(i);
    tft.fillRoundRect(r.x, r.y, r.w, r.h, 8, PK_TILE);
    if (i == active) {                       // outline the current animal
      tft.drawRoundRect(r.x, r.y, r.w, r.h, 8, TFT_WHITE);
      tft.drawRoundRect(r.x + 1, r.y + 1, r.w - 2, r.h - 2, 8, TFT_WHITE);
    }
    drawPickerIcon(s, r);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(s.name, r.x + r.w / 2, r.y + r.h - 12, s.icon ? 2 : 4);
  }
  const Rect b = pickerTile(speciesCount());
  tft.fillRoundRect(b.x, b.y, b.w, b.h, 8, 0x4208);
  tft.setTextColor(TFT_WHITE);
  tft.drawString("back", b.x + b.w / 2, b.y + b.h / 2, 4);
}

static void openPicker()  { s_pickerOpen = true; drawPicker(); }
static void closePicker() { s_pickerOpen = false; repaintScene(); }

// A tap inside the open picker: a non-current species commits the swap (via the
// atomic-swap latch + NVS persist), the current animal or the back tile closes it.
static void pickerHandleTap(int sx, int sy) {
  for (int i = 0; i < speciesCount(); i++) {
    if (!pickerTile(i).contains(sx, sy)) continue;
    if (i == activeSpeciesIndex()) { closePicker(); return; }   // current -> no change
    s_pickerOpen = false;
    tft.fillScreen(PK_BG);                                       // transient swapping... beat (UX-DR4)
    tft.setTextColor(TFT_WHITE); tft.setTextDatum(MC_DATUM);
    tft.drawString("swapping...", 160, 120, 4);
    s_pendingSwap = i;               // atomic swap + persist at the top of next loop (4.1/4.2)
    return;
  }
  if (pickerTile(speciesCount()).contains(sx, sy)) closePicker();  // back
}

void setup() {
  Serial.begin(115200);

  tft.init();
  tft.setRotation(3);            // landscape 320x240, flipped 180° for the mount
  tft.setSwapBytes(true);        // RGB565 byte order for pushImage (PNG decode)

  // Backlight PWM AFTER tft.init() — init() drives TFT_BL in digital mode, which
  // would otherwise override the ledc channel.
  ledcSetup(BL_CH, 5000, 8);          // 5 kHz, 8-bit
  ledcAttachPin(TFT_BL, BL_CH);
  backlight(BL_FULL);

  // Filesystem + persisted state FIRST: save::restore sets the ACTIVE species,
  // so the buffers below are sized to the right geometry (a persisted/swapped-in
  // species may differ from the compile-time default).
  if (!LittleFS.begin()) {
    Serial.println("LittleFS mount failed — run 'pio run -t uploadfs'");
  }
  save::begin();                   // open NVS
  save::restore(pet, s_dead);      // set active species + restore its care block

  giraffeBuf = (uint16_t*)malloc(maxPoseBytes());   // sized to the largest species; never realloc'd on swap
  if (!giraffeBuf) Serial.println("giraffeBuf malloc failed — giraffe won't render");

  // Off-screen sky-band compositor (~25 KB). The giraffe is composited into it by
  // hand (see composeSkyBand), so no swapBytes setting is needed on the sprite.
  skyBand.setColorDepth(16);
  bandOk = (skyBand.createSprite(spriteW(), bandH()) != nullptr);
  if (!bandOk) Serial.println("skyBand createSprite failed — clouds will clip at giraffe edge");

  syncTime();             // one-shot NTP clock set (non-fatal)
  updateDayNight(true);   // pick the starting phase before the first paint

  drawScene(tft);

  touchSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(touchSPI);
  ts.setRotation(1);

  engine.start(millis());
  if (s_dead) { renderPoseToBuffer(giraffeBuf, "dead"); pushGiraffe(); }
  else        updateGiraffe(pet.emotion());
  reloadBall();                   // beach-ball prop for the active species
  drawButtons(tft);
  drawMeters(tft, pet.hunger(), pet.thirst(), pet.fun(), pet.hygiene());
  drawPoops(tft, pet.poopCount());
  lastStats[0] = pet.hunger(); lastStats[1] = pet.thirst();
  lastStats[2] = pet.fun();    lastStats[3] = pet.hygiene();
  lastPoop = pet.poopCount();
  lastTick = millis();
  s_lastInteract = lastTick;
}

// --- prank: rapid-tap death ---
// Mashing the care buttons fast "kills" the giraffe (dead sprite); mashing the
// BOOK button fast brings it back, reset to full. A gag, not real pet logic.
static const uint32_t FAST_TAP_MS     = 350;   // taps closer than this count as "fast"
static const int      FAST_TAP_DIE    = 6;     // fast care-button taps in a row -> die
static const int      FAST_TAP_REVIVE = 6;     // fast BOOK taps in a row while dead -> revive
static uint32_t s_lastTap   = 0;
static int      s_tapStreak = 0;

static void die() {
  s_dead = true; s_tapStreak = 0;
  eat.active = play_.active = cln.active = slp.active = dream.active = false;
  renderPoseToBuffer(giraffeBuf, "dead");   // band shows it next frame
  save::writeNow(pet, s_dead);                             // death survives a power cycle
  Serial.println("[prank] the giraffe has perished — mash the book to revive");
}

static void revive() {
  s_dead = false; s_tapStreak = 0;
  pet.load(100, 100, 100, 100, 0);          // back to full
  updateGiraffe(pet.emotion());             // engine repaints the emotion pose
  lastStats[0] = lastStats[1] = lastStats[2] = lastStats[3] = 255;  // force meter redraw
  lastPoop = 255;
  save::writeNow(pet, s_dead);
  Serial.println("[prank] revived — good as new");
}

void loop() {
  const uint32_t now = millis();

  // Apply a latched species swap at exactly ONE point — the top of loop(), before
  // pet.update() and any animation tick (AD-13), so no frame straddles two species.
  if (s_pendingSwap >= 0) { applySpeciesSwap(s_pendingSwap); s_pendingSwap = -1; }

#ifdef DEBUG_SWAP
  // Dev-only serial swap trigger (build with -DDEBUG_SWAP): 's' cycles species,
  // 'g'/'h' pick giraffe/groundhog. The picker (long-press BOOK) is the shipping path.
  while (Serial.available()) {
    const int c = Serial.read();
    if      (c == 's') s_pendingSwap = (activeSpeciesIndex() + 1) % speciesCount();
    else if (c == 'g') s_pendingSwap = findSpecies("giraffe");
    else if (c == 'h') s_pendingSwap = findSpecies("groundhog");
  }
#endif

  if (!s_dead) pet.update(now - lastTick);   // frozen while dead
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

  // --- touch: taps + fast-mash prank + BOOK long-press + the picker modal ---
  // Care buttons act on the press edge (as before). BOOK is disambiguated: a
  // quick tap = read (fires on release), a ~800ms hold = open the picker, and a
  // fast mash while dead = revive (on press) — the three never overlap.
  const bool down = ts.touched();
  int sx = -1, sy = -1;
  if (down) {
    TS_Point p = ts.getPoint();
    if (p.z > 0) {  // ignore ghost / zero-pressure reads
      // NOTE: if the FEED button doesn't respond in landscape on your unit,
      // the touch axes are swapped — map p.y to sx and p.x to sy instead.
      sx = constrain(map(p.x, TS_MINX, TS_MAXX, 0, tft.width()),  0, tft.width()  - 1);
      sy = constrain(map(p.y, TS_MINY, TS_MAXY, 0, tft.height()), 0, tft.height() - 1);
      sx = tft.width()  - 1 - sx;   // touch inverted to match the 180° display flip
      sy = tft.height() - 1 - sy;
    }
  }
  const bool press   = down && !wasDown && sx >= 0;   // valid press edge
  const bool release = !down && wasDown;

  if (s_pickerOpen) {
    if (press) { wakeScreen(now); pickerHandleTap(sx, sy); }
    wasDown = down;
    delay(10);
    return;                          // modal: skip the pet render while the picker is up
  }

  if (press) {
    wakeScreen(now);                 // any tap restores full brightness
    const bool fast = (now - s_lastTap < FAST_TAP_MS);
    s_lastTap = now;
    s_pressStart = now; s_longFired = false;
    s_pressBook = BOOK_BTN.contains(sx, sy);

    if (s_dead) {
      if (BOOK_BTN.contains(sx, sy)) {         // fast mashing revives (on press)
        s_tapStreak = fast ? s_tapStreak + 1 : 1;
        if (s_tapStreak >= FAST_TAP_REVIVE) revive();
      }
    } else {
      bool care = true;
      if      (FEED_BTN.contains(sx, sy))  { pet.feed();  startEat(now, (int)Consume::Apple); }
      else if (DRINK_BTN.contains(sx, sy)) { pet.drink(); startEat(now, (int)Consume::Water); }
      else if (PLAY_BTN.contains(sx, sy))  { pet.play();  startPlay(now); }
      else if (CLEAN_BTN.contains(sx, sy)) { const uint8_t n = pet.poopCount(); pet.clean(); startClean(now, n); }
      else care = false;                       // BOOK: read on release, picker on hold
      if (care) {                              // fast care-button mashing kills it
        s_tapStreak = fast ? s_tapStreak + 1 : 1;
        if (s_tapStreak >= FAST_TAP_DIE) die();
      }
    }
  } else if (down && wasDown && s_pressBook && !s_dead && !s_longFired
             && now - s_pressStart >= LONG_PRESS_MS) {
    s_longFired = true;              // BOOK held: open the picker (once per hold, alive only)
    openPicker();
  } else if (release && s_pressBook && !s_longFired && !s_dead) {
    pet.read();                      // BOOK short tap = read
  }
  if (release) s_pressBook = false;
  wasDown = down;

  // Dim the backlight after a stretch of no touches.
  if (!s_dimmed && now - s_lastInteract > DIM_AFTER_MS) { backlight(BL_DIM); s_dimmed = true; }

  if (s_pickerOpen) { delay(10); return; }   // picker just opened this frame — skip the pet render

  animateScenery(tft);   // grass/trees + open-sky clouds/birds (clipped to box edges)

  // Giraffe ownership: handle eat expiry, emotion changes and ambient animations.
  // Skipped entirely while dead — the dead sprite stays in the buffer.
  if (s_dead) {
    // nothing: the band below composites the dead sprite each frame
  } else if (eat.active) {
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
    if (e != engine.emotion()) {
      if (slp.active) stopSleep();   // clear Z's before switching sprite
      updateGiraffe(e);              // loads happy.png (frame 0) when entering happy
      drawButtons(tft);
      if (e == Emotion::Happy) engine.enterIdle(now);  // reset rotation + tic timers
    }
    // While happy: the engine plays the idle rotation + occasional tics (data-driven,
    // one pose-writer per frame). No-op for any other emotion.
    engine.tickIdle(now, giraffeBuf);

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
  const bool showDream = dream.active && !eat.active && !play_.active && !cln.active && !slp.active;
  if (bandOk) {
    composeSkyBand(skyBand, giraffeBuf);
    if (eat.active) anim::composeEat(skyBand, spriteX(), spriteY(), now - eat.start, (Consume)eat.kind);
    if (eat.active && (Consume)eat.kind == Consume::Apple) anim::composeFoodBand(skyBand, now - eat.start);
    if (slp.active) anim::composeSleepZ(skyBand, now - slp.start);
    if (showDream)  anim::composeDaydreamBand(skyBand, dream.icon);   // in-box part, atomic with the push
    if (play_.active && play_.kind == PLAY_BUTTERFLY) anim::composeButterfly(skyBand, now - play_.start, PLAY_MS[PLAY_BUTTERFLY]);
    if (play_.active && play_.kind == PLAY_BUBBLES)   anim::composeBubbles(skyBand, now - play_.start);
    if (play_.active && play_.kind == PLAY_KICK) {    // in-box ball part -> band (clips)
      int bx, by;
      kickBallPos(now - play_.start, PLAY_MS[PLAY_KICK], bx, by);
      if (bx + KICK_BALL_R > spriteX() && bx - KICK_BALL_R < spriteX() + spriteW())
        drawBallBand(bx, by, kickBallAngle(bx));
    }
    skyBand.pushSprite(spriteX(), spriteY());
  } else if (eat.active) {
    pushGiraffe();
    anim::composeEat(tft, 0, 0, now - eat.start, (Consume)eat.kind);
  }

  // Daydream bubble: the open-sky part is drawn direct (the in-box part went into
  // the band above). Erase the open-sky part when the bubble ends.
  static bool dreamDrawn = false;
  if (showDream) { anim::composeDaydreamDirect(tft, dream.icon); dreamDrawn = true; }
  else if (dreamDrawn) { anim::eraseDaydreamDirect(tft); dreamDrawn = false; }

  // Redraw the meters the instant any stat changes (action or decay).
  const uint8_t hu = pet.hunger(), th = pet.thirst(), fn = pet.fun(), hy = pet.hygiene();
  if (hu != lastStats[0] || th != lastStats[1] || fn != lastStats[2] || hy != lastStats[3]) {
    drawMeters(tft, hu, th, fn, hy);
    lastStats[0] = hu; lastStats[1] = th; lastStats[2] = fn; lastStats[3] = hy;
    save::markDirty();
  }

  // Redraw poop when the count changes (spawn or clean).
  const uint8_t pc = pet.poopCount();
  if (pc != lastPoop) {
    drawPoops(tft, pc);
    lastPoop = pc;
    save::markDirty();
  }

  // Persist care state on change, throttled to spare the flash (≤ once / 5 s).
  save::tick(now, pet, s_dead);

  delay(10);
}
