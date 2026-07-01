#include "species.h"

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
  /* anims   */ nullptr,
  /* biome   */ nullptr,
  /* food    */ nullptr,
  /* icon    */ nullptr,
};
