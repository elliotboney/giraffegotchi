#include "species.h"

// The flamingo — "frances", the third species (Epic 5+). Same 150x160 footprint
// as the giraffe (tall, head-at-top), so it exercises the geometry path without a
// new buffer size. It brings its OWN food (shrimp — the reason flamingos are pink)
// and its OWN biome (a tropical lagoon), the two things every animal should define
// rather than fall back to the drawn apple / a borrowed world. Signature move is
// the kick (a flamingo standing on one leg); no kite art, so no CAP_KITE.

// --- animation data (pose names resolve from assetFolder /flamingo) ---
static const char* FL_IDLE[]  = {"happy", "happy2", "happy3"};   // 3-frame idle rotation
static const char* FL_BLINK[] = {"blink"};                       // single-frame blink (n=1)
static const char* FL_EARS[]  = {"ears_up", "ears_down"};
static const char* FL_TAIL[]  = {"tail_left"};
static const AnimSpec FL_TICS[] = {
  { FL_BLINK, 1,  90, AnimLayer::Pose, AnchorRef::None },
  { FL_EARS,  2, 150, AnimLayer::Pose, AnchorRef::None },
  { FL_TAIL,  1, 220, AnimLayer::Pose, AnchorRef::None },
};
static const AnimSet FL_ANIMS = {
  /* idle */ { FL_IDLE, 3, 3500, AnimLayer::Pose, AnchorRef::None },
  /* tics */ FL_TICS, 3,
};

// The flamingo's own food: shrimp (FR11). Decoded from data/flamingo/food.png.
// If that sprite isn't on the FS yet the eat anim just draws nothing (renderPose
// fails -> buffer freed -> no-op); no crash, no apple fallback.
static const FoodItem FL_FOOD = { "food", 40, 40 };

// The lagoon — the flamingo's biome. Tropical: turquoise water for the ground
// band, flamingo-pink sunrise/sunset skies, teal reeds instead of grass, and no
// trees (treeDraw=null). All props stay clear of the pet box (x 85..235). Palette
// is plausible-tropical and tunable on device.
static const Blade LAG_REEDS[] = {
  {  6,176,12,3,0x2A8B}, { 22,178,11,2,0x2A8B}, { 40,175,13,3,0x2A8B}, { 58,179,10,2,0x2A8B}, { 74,176,12,3,0x2A8B},
  {246,175,13,3,0x2A8B}, {264,178,11,2,0x2A8B}, {284,176,12,3,0x2A8B}, {302,179,10,2,0x2A8B}, {314,176,12,3,0x2A8B},
  { 12,192,14,4,0x3D75}, { 34,194,13,3,0x3D75}, { 60,192,14,4,0x3D75},
  {250,193,14,4,0x3D75}, {286,192,13,3,0x3D75}, {312,194,14,4,0x3D75},
};
static const Star LAG_STARS[] = { {34,54}, {58,86}, {268,96}, {298,62}, {312,88} };
static const Firefly LAG_FF[] = {
  {22,148}, {42,138}, {66,126}, {80,146}, {248,136}, {282,148}, {298,142}, {308,130},
};
static const Biome LAGOON = {
  { /* NIGHT     */ {0x08C6, 0x0946},   // deep teal-navy sky, dark water
    /* DAWN      */ {0x422D, 0x2A8B},   // cool blue-violet, teal water
    /* SUNRISE   */ {0xFCB4, 0x3CB2},   // flamingo pink-orange over turquoise
    /* MORNING   */ {0xE5B9, 0x45B5},   // soft warm pink, bright turquoise
    /* DAY       */ {0x5DFB, 0x4657},   // tropical blue sky, turquoise lagoon
    /* AFTERNOON */ {0xF656, 0x3D75},   // warm peach, teal water
    /* SUNSET    */ {0xF36F, 0x2B6E},   // hot flamingo pink, deep teal
    /* DUSK      */ {0x59ED, 0x19A8} }, // purple-pink, deep teal
  LAG_REEDS, 16,
  LAG_STARS, 5,
  nullptr,   0, nullptr,   // no trees in the lagoon (opt out of the tree hook)
  LAG_FF,    8,
};

extern const Species FLAMINGO = {   // `extern` for external linkage (const is internal by default)
  "flamingo",
  "frances",
  "/flamingo",
  /* geom    */ { 150, 160, 85, 34, 165 },   // same footprint as the giraffe (head at top)
  /* anchors */ { 155, 101, 52, 202, 86, 216, 42, 230, 42 },  // copied from giraffe; refine on device
  /* caps    */ CAP_KICK,                     // one-leg kick; no kite art -> no CAP_KITE
  /* anims   */ &FL_ANIMS,
  /* biome   */ &LAGOON,
  /* food    */ &FL_FOOD,
  /* icon    */ "icon",
};
