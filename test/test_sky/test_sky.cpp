#include <unity.h>
#include "core/sky.h"

// Golden-reference values captured from the pre-extraction code (Story 1.1
// baseline). solarTimes/celestialPos/skyPhaseFor were moved verbatim from
// ui.cpp into core/sky.cpp; these pins prove the move changed no math.

void setUp(void) {}
void tearDown(void) {}

// --- sunrise/sunset (Almanac algorithm) — project defaults (Austin-ish) ---
// lat 30.0858, lon -97.8403. tzOffsetMin is minutes east of UTC (DST included).

void test_solar_summer_solstice(void) {
  int rise, set;
  solarTimes(2026, 6, 21, 30.0858f, -97.8403f, -300, rise, set);  // CDT = UTC-5
  TEST_ASSERT_EQUAL_INT(390, rise);   // 06:30
  TEST_ASSERT_EQUAL_INT(1236, set);   // 20:36
}

void test_solar_winter_solstice(void) {
  int rise, set;
  solarTimes(2026, 12, 21, 30.0858f, -97.8403f, -360, rise, set);  // CST = UTC-6
  TEST_ASSERT_EQUAL_INT(443, rise);   // 07:23
  TEST_ASSERT_EQUAL_INT(1056, set);   // 17:36
}

// --- phase mapping across the 8 bands (R=390 6:30, S=1200 20:00) ---
void test_phase_bands(void) {
  const int R = 390, S = 1200;
  TEST_ASSERT_EQUAL_INT(PHASE_NIGHT,     skyPhaseFor(300,  R, S));  // before R-60
  TEST_ASSERT_EQUAL_INT(PHASE_DAWN,      skyPhaseFor(340,  R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_SUNRISE,   skyPhaseFor(380,  R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_MORNING,   skyPhaseFor(420,  R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_DAY,       skyPhaseFor(700,  R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_AFTERNOON, skyPhaseFor(1150, R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_SUNSET,    skyPhaseFor(1190, R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_DUSK,      skyPhaseFor(1230, R, S));
  TEST_ASSERT_EQUAL_INT(PHASE_NIGHT,     skyPhaseFor(1270, R, S));  // after S+60
}

// --- celestial arc: sun by day, moon by night; low at rise, high at midday ---
void test_celestial_midday_peak(void) {
  int cx, cy; bool sun;
  celestialPos(795, 390, 1200, cx, cy, sun);  // day midpoint
  TEST_ASSERT_TRUE(sun);
  TEST_ASSERT_EQUAL_INT(160, cx);   // centre
  TEST_ASSERT_EQUAL_INT(50, cy);    // peak (125 - 75)
}

void test_celestial_sunrise_low_east(void) {
  int cx, cy; bool sun;
  celestialPos(390, 390, 1200, cx, cy, sun);  // exactly at rise
  TEST_ASSERT_TRUE(sun);
  TEST_ASSERT_EQUAL_INT(20, cx);    // east edge
  TEST_ASSERT_EQUAL_INT(125, cy);   // horizon base
}

void test_celestial_night_is_moon(void) {
  int cx, cy; bool sun;
  celestialPos(0, 390, 1200, cx, cy, sun);  // deep night
  TEST_ASSERT_FALSE(sun);
  TEST_ASSERT_EQUAL_INT(126, cx);
  TEST_ASSERT_EQUAL_INT(56, cy);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_solar_summer_solstice);
  RUN_TEST(test_solar_winter_solstice);
  RUN_TEST(test_phase_bands);
  RUN_TEST(test_celestial_midday_peak);
  RUN_TEST(test_celestial_sunrise_low_east);
  RUN_TEST(test_celestial_night_is_moon);
  return UNITY_END();
}
