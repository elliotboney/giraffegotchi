#include "engine.h"
#include "../ui.h"                 // renderGiraffeToBuffer, TFT_eSprite
#include "../species/registry.h"   // activeSpecies() — specs come from the descriptor

namespace anim {

void Engine::start(uint32_t now) {
  started_ = now;
  loaded_  = false;   // force the pose to be (re)decoded on the next setPose/forcePose
}

bool Engine::setPose(Emotion e, uint16_t* buf) {
  if (e == emotion_ && loaded_) return false;   // unchanged floor — leave the buffer
  emotion_ = e;
  renderGiraffeToBuffer(buf, e);                // emotion -> pose -> active descriptor folder
  loaded_ = true;
  return true;
}

void Engine::forcePose(Emotion e, uint16_t* buf) {
  emotion_ = e;
  renderGiraffeToBuffer(buf, e);
  loaded_ = true;
}

// Reset the idle rotation + tic timers — called when the pet (re)enters Happy.
// Mirrors the pre-refactor emotion-change reset exactly.
void Engine::enterIdle(uint32_t now) {
  const AnimSet* set = activeSpecies().anims;
  rotIdx_    = 0;
  rotNext_   = now + (set && set->idle.n ? set->idle.cadenceMs : 3500);
  ticActive_ = false;
  ticNext_   = now + 4000;
}

// Advance the idle rotation + tics while Happy. Exactly one pose write per frame,
// by the same priority the pre-refactor loop used: step an active tic > start the
// next tic > advance the face rotation. Plays any frame-count (variable length).
void Engine::tickIdle(uint32_t now, uint16_t* buf) {
  if (emotion_ != Emotion::Happy) return;
  const AnimSet* set = activeSpecies().anims;
  if (!set) return;

  if (ticActive_) {                                   // step through the active tic
    const AnimSpec& t = set->tics[ticKind_];
    if (now >= ticStepNext_) {
      ticIdx_++;
      if (ticIdx_ < t.n) {
        renderPoseToBuffer(buf, t.frames[ticIdx_]);
        ticStepNext_ = now + t.cadenceMs;
      } else {                                        // tic done -> resume the current rotation frame
        ticActive_ = false;
        if (set->idle.n) renderPoseToBuffer(buf, set->idle.frames[rotIdx_]);
        ticNext_ = now + 3500 + (now % 5000);         // 3.5–8.5 s until the next tic
      }
    }
  } else if (set->ticN && now >= ticNext_) {          // start the next tic (cycles kinds)
    ticKind_   = (ticKind_ + 1) % set->ticN;
    ticActive_ = true; ticIdx_ = 0;
    renderPoseToBuffer(buf, set->tics[ticKind_].frames[0]);
    ticStepNext_ = now + set->tics[ticKind_].cadenceMs;
  } else if (set->idle.n && now >= rotNext_) {        // idle face rotation
    rotIdx_ = (rotIdx_ + 1) % set->idle.n;
    renderPoseToBuffer(buf, set->idle.frames[rotIdx_]);
    rotNext_ = now + set->idle.cadenceMs;
  }
}

void Engine::compose(TFT_eSprite& /*band*/, uint32_t /*now*/) {
  // Foreground-layer AnimSpecs (eat / sleep-Z / daydream / play) compose here in
  // Story 2.3, read from activeSpecies().anims. None are defined yet.
  const AnimSet* set = activeSpecies().anims;
  (void)set;
}

}  // namespace anim
