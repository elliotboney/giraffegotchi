#include "species.h"

// Biome prop-draw hook, defined in render (ui.cpp). Declared here (species.h
// forward-declares TFT_eSPI) so the biome can point to it without pulling in TFT.
void drawAcaciaTree(TFT_eSPI& tft, int x, int baseY);

// --- Animation data (Story 2.2) — pose names resolve from assetFolder ---
// Idle rotation: alternate happy faces cycled so the resting face isn't static.
static const char* GIR_IDLE[]  = {"happy", "happy2", "happy3"};
// Idle tics: short pose sequences interjected during the idle state.
//   blink: open -> blink -> blink2(closed) -> blink3(opening)   ears: up -> down   tail: swish
static const char* GIR_BLINK[] = {"blink", "blink2", "blink3"};
static const char* GIR_EARS[]  = {"ears_up", "ears_down"};
static const char* GIR_TAIL[]  = {"tail_left"};
static const AnimSpec GIR_TICS[] = {
  { GIR_BLINK, 3,  90, AnimLayer::Pose, AnchorRef::None },
  { GIR_EARS,  2, 150, AnimLayer::Pose, AnchorRef::None },
  { GIR_TAIL,  1, 220, AnimLayer::Pose, AnchorRef::None },
};
static const AnimSet GIR_ANIMS = {
  /* idle */ { GIR_IDLE, 3, 3500, AnimLayer::Pose, AnchorRef::None },
  /* tics */ GIR_TICS, 3,
};

// The savanna — the giraffe's biome (AD-15). All values are the frozen pre-
// refactor scene literals (baseline §3.4 palette + the GRASS/STARS/tree/firefly
// arrays from ui.cpp), now data.
static const Blade SAV_GRASS[] = {
  {  7,172,5,1,0x2A40}, { 22,171,4,1,0x2A40}, { 31,174,5,2,0x2A40},
  { 64,172,5,1,0x2A40}, { 76,173,4,1,0x2A40},
  {243,172,5,1,0x2A40}, {283,173,4,1,0x2A40}, {294,171,5,2,0x2A40}, {308,172,5,1,0x2A40},
  {  9,183,8,2,0x3B80}, { 27,182,7,2,0x3B80}, { 70,184,8,2,0x3B80}, { 79,182,7,2,0x3B80},
  {240,183,8,2,0x3B80}, {287,184,8,2,0x3B80}, {311,182,7,2,0x3B80},
  { 12,196,11,4,0x4DA0}, { 30,194,10,4,0x4DA0}, { 73,196,12,4,0x4DA0},
  {245,195,11,4,0x4DA0}, {289,196,11,4,0x4DA0}, {313,194,10,4,0x4DA0},
};
static const Star SAV_STARS[] = {
  {28,52}, {52,84}, {74,112}, {248,64}, {276,98}, {300,120}, {312,72}, {40,128}, {64,150},
};
static const TreePos SAV_TREES[] = { {22, 172}, {298, 172} };
static const Firefly SAV_FF[] = {
  {30,136}, {55,150}, {72,126}, {250,142}, {286,130}, {306,152},
};
static const Biome SAVANNA = {
  { /* NIGHT     */ {0x0865, 0x2104},   // deep navy
    /* DAWN      */ {0x39ED, 0x2966},   // cool blue-violet, first light
    /* SUNRISE   */ {0xFC6D, 0x92E9},   // pink-orange
    /* MORNING   */ {0xE671, 0xBD2B},   // soft warm gold
    /* DAY       */ {0x6DBC, 0xCD4B},   // bright blue (original)
    /* AFTERNOON */ {0xF64F, 0xCD0A},   // warm gold
    /* SUNSET    */ {0xE36B, 0x6A25},   // deep orange
    /* DUSK      */ {0x5A2F, 0x2947} }, // purple
  SAV_GRASS, 22,
  SAV_STARS, 9,
  SAV_TREES, 2, drawAcaciaTree,
  SAV_FF,    6,
};

// The giraffe as DATA — the reference species entry. Values are the frozen
// pre-refactor literals (Story 1.1 baseline §3.1/§3.3): geometry 150x160 at
// (85,34), horizon 165; anchors for mouth/sleep/daydream. Asset paths resolve
// from assetFolder in Story 1.5 (flat filenames today, e.g. "/giraffe_happy.png";
// migrates to per-species folders in Epic 5). The anim/biome/food/icon fields
// stay null until their epics populate them.
extern const Species GIRAFFE = {   // `extern` for external linkage (const is internal by default)
  "giraffe",
  "/giraffe",
  /* geom    */ { 150, 160, 85, 34, 165 },
  /* anchors */ { 160, 101, 52, 202, 86, 216, 42, 230, 42 },
  /* caps    */ CAP_KITE | CAP_KICK,   // the giraffe's signature play moves
  /* anims   */ &GIR_ANIMS,
  /* biome   */ &SAVANNA,
  /* food    */ nullptr,
  /* icon    */ nullptr,
};
