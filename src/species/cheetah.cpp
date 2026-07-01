#include "species.h"

// The cheetah — "spot", the fourth species, added by following
// docs/adding-an-animal-agent.md end-to-end (art was pre-built). Same 150x160
// footprint as the giraffe. Brings its OWN food and its OWN biome (a dry golden
// grassland — treeless open plains, distinct from the giraffe's tree'd savanna).
// Signature move is the kick (no kite art), so CAP_KICK only.

// --- animation data (pose names resolve from assetFolder /cheetah) ---
static const char* CH_IDLE[]  = {"happy", "happy2", "happy3"};   // 3-frame idle rotation
static const char* CH_BLINK[] = {"blink"};                       // single-frame blink (n=1)
static const char* CH_EARS[]  = {"ears_up", "ears_down"};
static const char* CH_TAIL[]  = {"tail_left"};
static const AnimSpec CH_TICS[] = {
  { CH_BLINK, 1,  90, AnimLayer::Pose, AnchorRef::None },
  { CH_EARS,  2, 150, AnimLayer::Pose, AnchorRef::None },
  { CH_TAIL,  1, 220, AnimLayer::Pose, AnchorRef::None },
};
static const AnimSet CH_ANIMS = {
  /* idle */ { CH_IDLE, 3, 3500, AnimLayer::Pose, AnchorRef::None },
  /* tics */ CH_TICS, 3,
};

// The cheetah's own food (FR11). Decoded from data/cheetah/food.png.
static const FoodItem CH_FOOD = { "food", 40, 40 };

// The plains — the cheetah's biome. Dry golden grassland: warm amber ground,
// tall golden grass, hot pale-blue day sky, no trees (treeDraw=null) so it reads
// as open hunting plains, not the giraffe's acacia savanna. Props stay clear of
// the pet box (x 85..235). Palette is plausible-golden and tunable on device.
static const Blade PLN_GRASS[] = {
  {  6,170,7,2,0x7AC3}, { 22,172,6,1,0x7AC3}, { 38,169,8,2,0x7AC3}, { 58,173,6,1,0x7AC3}, { 74,170,7,2,0x7AC3},
  {246,169,8,2,0x7AC3}, {264,172,6,1,0x7AC3}, {284,170,7,2,0x7AC3}, {302,173,6,1,0x7AC3}, {314,170,7,2,0x7AC3},
  { 10,182,11,3,0xAC06}, { 30,184,10,3,0xAC06}, { 66,182,12,3,0xAC06}, { 80,184,10,2,0xAC06},
  {244,182,12,3,0xAC06}, {286,184,10,3,0xAC06}, {312,182,11,3,0xAC06},
  { 14,196,14,4,0xD56A}, { 34,194,13,4,0xD56A}, { 70,196,15,4,0xD56A},
  {248,195,14,4,0xD56A}, {290,196,14,4,0xD56A}, {314,194,13,4,0xD56A},
};
static const Star PLN_STARS[] = { {32,52}, {56,84}, {266,96}, {296,60}, {312,86} };
static const Firefly PLN_FF[] = {
  {24,150}, {44,140}, {66,128}, {80,148}, {248,138}, {282,150}, {298,144}, {308,132},
};
static const Biome PLAINS = {
  { /* NIGHT     */ {0x0885, 0x20E2},   // deep navy sky, dark warm earth
    /* DAWN      */ {0x524D, 0x5A45},   // blue-violet, dim gold
    /* SUNRISE   */ {0xFD0F, 0x9386},   // pink-orange sky, warm gold ground
    /* MORNING   */ {0xDE56, 0xB468},   // soft warm sky, golden ground
    /* DAY       */ {0x7DBB, 0xCD2A},   // hot pale blue, bright golden savanna
    /* AFTERNOON */ {0xF694, 0xBCA9},   // warm sky, amber ground
    /* SUNSET    */ {0xE367, 0x7A85},   // deep orange, burnt gold
    /* DUSK      */ {0x41AB, 0x3143} }, // purple, dark gold-brown
  PLN_GRASS, 23,
  PLN_STARS, 5,
  nullptr,   0, nullptr,   // no trees on the open plains (opt out of the tree hook)
  PLN_FF,    8,
};

extern const Species CHEETAH = {   // `extern` for external linkage (const is internal by default)
  "cheetah",
  "spot",
  "/cheetah",
  /* geom    */ { 150, 160, 85, 34, 165 },   // same footprint as the giraffe (head at top)
  /* anchors */ { 155, 101, 52, 202, 86, 216, 42, 230, 42 },  // copied from giraffe; refine on device
  /* caps    */ CAP_KICK,                     // kick only; no kite art -> no CAP_KITE
  /* anims   */ &CH_ANIMS,
  /* biome   */ &PLAINS,
  /* food    */ &CH_FOOD,
  /* icon    */ "icon",
};
