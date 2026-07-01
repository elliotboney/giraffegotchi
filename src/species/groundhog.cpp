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

extern const Species GROUNDHOG = {   // `extern` for external linkage (const is internal by default)
  "groundhog",
  "/groundhog",
  /* geom    */ { 150, 150, 85, 44, 165 },   // squat: H 150, sits at y44..194 on the ground
  /* anchors */ { 160, 110, 60, 195, 90, 210, 54, 225, 54 },  // mouth / food-drop / sleep / dream
  /* caps    */ CAP_NONE,                     // no signature moves (PLAY = butterfly/bubbles only)
  /* anims   */ &GH_ANIMS,
  /* biome   */ nullptr,                      // Epic 3
  /* food    */ &GH_FOOD,
  /* icon    */ "icon",
};
