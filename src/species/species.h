#pragma once
#include <stdint.h>

// The Species descriptor: the single source of truth for everything animal-
// specific (AD-11). Pure data — no Arduino/TFT/FS headers. The giraffe is the
// reference entry (species/giraffe.cpp). Later epics READ this; Story 1.4 only
// declares it, so nothing consumes it yet (behavior-neutral).

// On-screen sprite placement + world seams. The footprint is the box
// (x, y, w, h); the compositing band covers it (BAND_H == h). horizonY is the
// sky/ground split for this species' world.
struct SpeciesGeometry {
  int w, h;        // pose-buffer / band dimensions
  int x, y;        // top-left placement on the 320x240 screen
  int horizonY;    // sky/ground split
};

// Screen-coordinate anchors where animations attach.
struct SpeciesAnchors {
  int mouthX, mouthY;      // eat item rest + bubble origin
  int foodDropY;           // eat item start (above the head)
  int sleepX0, sleepY0;    // sleep-Z drift start (by the head)
  int sleepX1, sleepY1;    // sleep-Z drift end (upper-right)
  int dreamCx, dreamCy;    // daydream thought-bubble centre
};

// Populated by later epics — forward-declared so the descriptor can carry the
// fields now (structure only, no behavior). Null until their epic lands.
struct AnimSet;    // Epic 2: data-driven animation set
struct Biome;      // Epic 3: world palettes / props / critters
struct FoodItem;   // Epic 2/5: optional per-species food (else the drawn apple)

struct Species {
  const char* name;         // lowercase display name ("giraffe")
  const char* assetFolder;  // LittleFS asset root; sprite paths resolve from it + pose name
  SpeciesGeometry geom;
  SpeciesAnchors  anchors;

  // --- structure only (later epics populate these; not read yet, AD-11) ---
  const AnimSet*  anims;    // Epic 2
  const Biome*    biome;    // Epic 3
  const FoodItem* food;     // Epic 2/5 (null => drawn apple)
  const char*     icon;     // Epic 4: picker icon sprite name (null => scaled happy)
};
