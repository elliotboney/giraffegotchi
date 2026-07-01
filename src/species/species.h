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

// --- Animation data model (AD-12) — pure data, played by the anim engine ---
// An animation is data: a variable-length frame list (pose names resolved from
// assetFolder), a per-frame cadence, a layer tag, and (for foreground anims) the
// anchor it attaches to. Pose-layer anims write the pose buffer under the AD-5
// priority; foreground-layer anims compose into the band and never touch it.
enum class AnimLayer : uint8_t { Pose, Foreground };
enum class AnchorRef : uint8_t { None, Mouth, SleepZ, Daydream };

struct AnimSpec {
  const char* const* frames;   // pose names (variable length — never assume 3)
  uint8_t   n;                 // frame count
  uint32_t  cadenceMs;         // per-frame advance interval
  AnimLayer layer;
  AnchorRef anchor;            // foreground only; Pose anims use None
};

struct AnimSet {
  AnimSpec        idle;        // content-state pose rotation (played while Happy), variable length
  const AnimSpec* tics;        // idle tics interjected during the idle state (each variable length)
  uint8_t         ticN;
  // foreground specs (eat / sleep-Z / daydream / play) arrive in Story 2.3
};

// Capability hooks (AD-12 / FR6): signature moves a species opts into. Non-data
// behaviors (kick physics, kite swoop) run only when the active descriptor
// declares them, so they never leak into a data-only animal (FR10).
enum Capability : uint16_t { CAP_NONE = 0, CAP_KITE = 1 << 0, CAP_KICK = 1 << 1 };

// Optional per-species food (FR11). When a descriptor supplies one, the eat
// animation draws this sprite at the mouth anchor; otherwise it falls back to the
// drawn apple primitive. Drink/water is universal (the glass), never per-species.
struct FoodItem {
  const char* sprite;   // pose name in the species folder (decoded like other poses)
  int w, h;             // sprite dimensions
};

// --- Biome data (AD-15) — the world a species lives in ---
// The biome owns the sky/ground palette TABLE + the scene props (grass, trees,
// stars, ambient fireflies). render/scene reads these; it stays the sole writer
// of the live SKY_COLOR/GROUND_COLOR + phase id (AD-10). No prop array lives in
// scene code any more.
struct SkyColors { uint16_t sky, ground; };
struct Blade   { int16_t x, y, h, amp; uint16_t c; };  // grass blade: pos, height, sway amp, colour
struct Star    { int16_t x, y; };                      // night star
struct TreePos { int16_t x, baseY; };                  // tree trunk base
struct Firefly { int16_t x, y; };                      // firefly home position (drifts around it)

// Biome-specific prop ART that can't be data (acacia vs pine vs coral) is a named
// draw hook the biome opts into — the scene invokes it by pointer, like AD-12's
// capability hooks. The hook draws to a surface; species.h stays hardware-free via
// this forward declaration (impl lives in render).
class TFT_eSPI;
typedef void (*TreeDrawFn)(TFT_eSPI& tft, int x, int baseY);

struct Biome {
  SkyColors palette[8];   // per-phase sky/ground, indexed by SkyPhase (NIGHT..DUSK)
  const Blade*   grass;     uint8_t grassN;
  const Star*    stars;     uint8_t starN;
  const TreePos* trees;     uint8_t treeN;
  TreeDrawFn     treeDraw;  // how to draw one tree (null => this world has no trees)
  const Firefly* fireflies; uint8_t ffN;
};

struct Species {
  const char* name;         // internal id + registry key (findSpecies); lowercase ("giraffe")
  const char* displayName;  // the pet's given name, shown in the picker ("george")
  const char* assetFolder;  // LittleFS asset root; sprite paths resolve from it + pose name
  SpeciesGeometry geom;
  SpeciesAnchors  anchors;
  uint16_t        caps;     // OR of Capability flags — the signature moves this species has

  // --- structure only (later epics populate these; not read yet, AD-11) ---
  const AnimSet*  anims;    // Epic 2
  const Biome*    biome;    // Epic 3
  const FoodItem* food;     // Epic 2/5 (null => drawn apple)
  const char*     icon;     // Epic 4: picker icon sprite name (null => scaled happy)
  uint8_t         dreamN;   // daydream wish-objects in <folder>/objects/ (dream1..dreamN.png; 0 => no daydreams)
};
