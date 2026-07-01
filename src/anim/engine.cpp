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

void Engine::compose(TFT_eSprite& /*band*/, uint32_t /*now*/) {
  // Foreground-layer AnimSpecs (eat / sleep-Z / daydream / play) compose here in
  // Story 2.3, read from activeSpecies().anims. None are defined yet.
  const AnimSet* set = activeSpecies().anims;
  (void)set;
}

}  // namespace anim
