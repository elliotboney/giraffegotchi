#include "species.h"

// The groundhog — the second species (Story 2.6 fixture; graduates to a full pet
// in Epic 5). Deliberately DIFFERENT from the giraffe to pressure-test the data
// model: squat 150x150 geometry (vs 150x160) validates buffer sizing (NFR3);
// a single-frame blink validates the variable-length engine (2.2); its own food
// sprite exercises the descriptor-food path (2.5, FR11); and it declares NO
// signature moves, so PLAY runs only the universal effects (FR6/FR10).

// --- animation data (pose names resolve from assetFolder /groundhog) ---
static const char* GH_IDLE[]  = {"happy", "happy2"};      // no happy3 (n=2)
static const char* GH_BLINK[] = {"blink"};                // single-frame blink (n=1)
static const char* GH_EARS[]  = {"ears_up", "ears_down"};
static const char* GH_TAIL[]  = {"tail_left"};
static const AnimSpec GH_TICS[] = {
  { GH_BLINK, 1,  90, AnimLayer::Pose, AnchorRef::None },
  { GH_EARS,  2, 150, AnimLayer::Pose, AnchorRef::None },
  { GH_TAIL,  1, 220, AnimLayer::Pose, AnchorRef::None },
};
static const AnimSet GH_ANIMS = {
  /* idle */ { GH_IDLE, 2, 3500, AnimLayer::Pose, AnchorRef::None },
  /* tics */ GH_TICS, 3,
};

// The groundhog eats its own food (FR11) instead of the drawn apple.
static const FoodItem GH_FOOD = { "food", 40, 40 };

// The meadow — the groundhog's biome (Story 3.4). Visibly different from the
// savanna: GREEN grounds (vs tan), its own denser grass, more fireflies, and NO
// trees (treeDraw=null exercises 3.3's opt-out). All props stay clear of the pet
// box (x 85..235). Refined when the groundhog graduates (Epic 5).
static const Blade MDW_GRASS[] = {
  {  8,180,7,2,0x3C4C}, { 24,182,6,1,0x3C4C}, { 40,181,7,2,0x3C4C}, { 58,183,6,1,0x3C4C}, { 72,180,7,2,0x3C4C},
  {248,181,7,2,0x3C4C}, {266,183,6,1,0x3C4C}, {284,180,7,2,0x3C4C}, {300,182,6,1,0x3C4C}, {314,181,7,2,0x3C4C},
  { 14,194,11,3,0x5E8C}, { 34,196,10,3,0x5E8C}, { 60,194,11,3,0x5E8C},
  {252,195,11,3,0x5E8C}, {286,194,10,3,0x5E8C}, {312,196,11,3,0x5E8C},
};
static const Star MDW_STARS[] = { {30,50}, {60,80}, {270,100}, {300,60}, {312,90} };
static const Firefly MDW_FF[] = {
  {20,150}, {40,140}, {66,128}, {80,148}, {250,138}, {280,150}, {296,144}, {306,132},
};
static const Biome MEADOW = {
  { /* NIGHT     */ {0x0865, 0x0A22},
    /* DAWN      */ {0x39ED, 0x1A44},
    /* SUNRISE   */ {0xFC6D, 0x33A6},
    /* MORNING   */ {0xE671, 0x3D06},
    /* DAY       */ {0x6DBC, 0x4E4C},   // green ground (vs savanna tan)
    /* AFTERNOON */ {0xF64F, 0x4586},
    /* SUNSET    */ {0xE36B, 0x2A44},
    /* DUSK      */ {0x5A2F, 0x1263} },
  MDW_GRASS, 16,
  MDW_STARS, 5,
  nullptr,   0, nullptr,   // no trees in the meadow (opt out of the tree hook)
  MDW_FF,    8,
};

extern const Species GROUNDHOG = {   // `extern` for external linkage (const is internal by default)
  "groundhog",
  "/groundhog",
  /* geom    */ { 150, 150, 85, 44, 165 },   // squat: H 150, sits at y44..194 on the ground
  /* anchors */ { 170, 110, 60, 195, 90, 210, 54, 225, 54 },  // mouth / food-drop / sleep / dream
  /* caps    */ CAP_NONE,                     // no signature moves (PLAY = butterfly/bubbles only)
  /* anims   */ &GH_ANIMS,
  /* biome   */ &MEADOW,
  /* food    */ &GH_FOOD,
  /* icon    */ "icon",
};
