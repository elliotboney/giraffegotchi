#pragma once

// Pure solar / sky-phase math — no Arduino, TFT or FS headers, so this compiles
// and unit-tests under env:native (AD-1). The render layer (ui) owns the live
// palette + phase-id holder (AD-10); this module only computes the numbers.

// Eight discrete sky-COLOUR phases, warm-cool-warm across the day. The sun/moon
// position is a SEPARATE continuous arc (celestialPos), not tied to the phase.
enum SkyPhase {
  PHASE_NIGHT, PHASE_DAWN, PHASE_SUNRISE, PHASE_MORNING,
  PHASE_DAY, PHASE_AFTERNOON, PHASE_SUNSET, PHASE_DUSK,
};

// Map a local time (minutes from midnight) to a colour phase, using shoulders
// around sunrise/sunset.
SkyPhase skyPhaseFor(int nowMin, int sunriseMin, int sunsetMin);

// Celestial body arc. Computes screen (cx,cy) + sun/moon for a given local time
// (the render layer stores it via setCelestial and draws it).
void celestialPos(int nowMin, int sunriseMin, int sunsetMin,
                  int& cx, int& cy, bool& isSun);

// NOAA almanac sunrise/sunset -> local minutes from midnight. tzOffsetMin is
// minutes east of UTC (includes DST). Recompute once per day.
void solarTimes(int year, int month, int day, float lat, float lon,
                int tzOffsetMin, int& sunriseMin, int& sunsetMin);
