#pragma once
#include <stdint.h>
#include "../pet.h"               // Emotion
#include "../species/species.h"   // AnimSpec / AnimSet (played from the active descriptor)

class TFT_eSprite;                 // reference-only in the header (impl includes ui.h)

// The data-driven animation engine (AD-12). Species-agnostic: it plays the
// AnimSpecs of the active descriptor and resolves the single pose-buffer writer
// per frame by priority (dead > kick > tic > emotion, AD-5). It writes the pose
// buffer / composes the band only — it NEVER pushes to the panel (AD-3).
//
// Story 2.1 migrates the emotion base (the pose-layer floor). Timed pose overlays
// (happy rotation, idle tics) arrive in 2.2, foreground composers in 2.3, and the
// kick capability hook in 2.4 — all through this same engine.
namespace anim {

class Engine {
public:
  void start(uint32_t now);                    // (re)start play timers + force a pose redraw

  // Pose floor: ensure `buf` holds the sprite for emotion `e`. Decodes only when
  // the emotion actually changed (or after a forced repaint); returns true when
  // it (re)wrote the buffer. Exactly one pose writer per frame (AD-5).
  bool setPose(Emotion e, uint16_t* buf);

  // Set the emotion and decode it unconditionally — used on discrete events
  // (feed/eat-end, kick-end, revive, boot) where the buffer must be repainted
  // even if the emotion is unchanged (e.g. a kick pose is sitting in it).
  void forcePose(Emotion e, uint16_t* buf);

  // Idle pose overlays (played while the base emotion is Happy): the content
  // face-rotation + occasional tics, from the active descriptor's AnimSet.
  void enterIdle(uint32_t now);              // reset rotation + tic timers (on entering Happy)
  void tickIdle(uint32_t now, uint16_t* buf);// advance rotation/tics; writes buf on a frame change (one writer/frame, AD-5)

  // Compose the foreground-layer AnimSpecs into the band (Story 2.3). None yet.
  void compose(TFT_eSprite& band, uint32_t now);

  Emotion emotion() const { return emotion_; }

private:
  Emotion  emotion_ = Emotion::Happy;
  bool     loaded_  = false;   // is emotion_ currently decoded into the buffer?
  uint32_t started_ = 0;

  // Idle overlay state — defaults mirror the pre-refactor globals so boot timing
  // is identical (rotation advances immediately, first tic at 5 s).
  int      rotIdx_      = 0;
  uint32_t rotNext_     = 0;
  bool     ticActive_   = false;
  int      ticKind_     = 0;
  int      ticIdx_      = 0;
  uint32_t ticStepNext_ = 0;
  uint32_t ticNext_     = 5000;
};

}  // namespace anim
