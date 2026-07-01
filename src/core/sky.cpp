#include "sky.h"
#include <math.h>

// Eight colour bands, warm-cool-warm, keyed off sunrise (R) and sunset (S).
SkyPhase skyPhaseFor(int t, int R, int S) {
  if (t < R - 60 || t >= S + 60) return PHASE_NIGHT;
  if (t < R - 15)                return PHASE_DAWN;       // R-60 .. R-15  (cool first light)
  if (t < R + 25)                return PHASE_SUNRISE;    // R-15 .. R+25  (orange)
  if (t < R + 105)               return PHASE_MORNING;    // R+25 .. R+105 (gold)
  if (t < S - 105)               return PHASE_DAY;        //              (blue)
  if (t < S - 25)                return PHASE_AFTERNOON;  // S-105 .. S-25 (gold)
  if (t < S + 15)                return PHASE_SUNSET;     // S-25 .. S+15  (orange)
  return PHASE_DUSK;                                      // S+15 .. S+60  (purple)
}

// Position the sun (daytime) or moon (night) on a left→right arc. The body sits
// low at the horizon at rise/set (open sky, east/west) and peaks high+centre at
// midday — where it passes behind the giraffe. u is fraction across the span.
void celestialPos(int t, int R, int S, int& cx, int& cy, bool& isSun) {
  const int X_L = 20, X_R = 300, Y_BASE = 125, ARC = 75;
  float u;
  if (t >= R && t < S) {                       // daytime → sun, rise→set
    isSun = true;
    u = (float)(t - R) / (float)(S - R);
  } else {                                     // night → moon, set→next rise
    isSun = false;
    const int span = (R + 1440) - S;
    const int into = (t >= S) ? (t - S) : (t + 1440 - S);
    u = (float)into / (float)span;
  }
  if (u < 0) u = 0; if (u > 1) u = 1;
  cx = X_L + (int)(u * (X_R - X_L));
  cy = Y_BASE - (int)(sinf(u * 3.14159265f) * ARC);
}

// Sunrise/sunset via the "Almanac for Computers" (1990) algorithm. Self-
// contained — no network after the clock is set. Accurate to ~1 min for our
// purposes. tzOffsetMin is minutes east of UTC (negative for US), DST included.
static float d2r(float d) { return d * 0.01745329252f; }
static float r2d(float r) { return r * 57.2957795131f; }

static int dayOfYear(int y, int m, int d) {
  static const int cum[12] = {0,31,59,90,120,151,181,212,243,273,304,334};
  int n = cum[m - 1] + d;
  if (m > 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) n++;
  return n;
}

void solarTimes(int year, int month, int day, float lat, float lon,
                int tzOffsetMin, int& sunriseMin, int& sunsetMin) {
  const int N = dayOfYear(year, month, day);
  const float zenith = 90.833f;          // official sunrise (includes refraction)
  const float lngHour = lon / 15.0f;
  for (int pass = 0; pass < 2; pass++) {  // 0 = rise, 1 = set
    int& out = (pass == 0) ? sunriseMin : sunsetMin;
    const float t = N + (((pass == 0 ? 6.0f : 18.0f) - lngHour) / 24.0f);
    const float M = (0.9856f * t) - 3.289f;
    float L = M + (1.916f * sinf(d2r(M))) + (0.020f * sinf(d2r(2 * M))) + 282.634f;
    L = fmodf(L, 360.0f); if (L < 0) L += 360.0f;
    float RA = r2d(atanf(0.91764f * tanf(d2r(L))));
    RA = fmodf(RA, 360.0f); if (RA < 0) RA += 360.0f;
    RA += (floorf(L / 90.0f) * 90.0f) - (floorf(RA / 90.0f) * 90.0f);  // same quadrant as L
    RA /= 15.0f;
    const float sinDec = 0.39782f * sinf(d2r(L));
    const float cosDec = cosf(asinf(sinDec));
    const float cosH = (cosf(d2r(zenith)) - (sinDec * sinf(d2r(lat)))) /
                       (cosDec * cosf(d2r(lat)));
    if (cosH > 1.0f || cosH < -1.0f) {    // polar day/night — never happens here
      out = (pass == 0) ? 0 : 24 * 60;
      continue;
    }
    float H = (pass == 0) ? 360.0f - r2d(acosf(cosH)) : r2d(acosf(cosH));
    H /= 15.0f;
    const float T  = H + RA - (0.06571f * t) - 6.622f;
    float UT = fmodf(T - lngHour, 24.0f); if (UT < 0) UT += 24.0f;
    float localH = UT + tzOffsetMin / 60.0f;
    localH = fmodf(localH, 24.0f); if (localH < 0) localH += 24.0f;
    out = (int)lroundf(localH * 60.0f);
  }
}
