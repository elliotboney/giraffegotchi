#include "species.h"

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
  /* biome   */ nullptr,
  /* food    */ nullptr,
  /* icon    */ nullptr,
};
