#pragma once
#include <stdint.h>
#include "../pet.h"               // Emotion
#include "../species/species.h"   // AnimSpec / AnimSet (played from the active descriptor)

class TFT_eSprite;                 // reference-only in the header (impl includes ui.h)
class TFT_eSPI;

// --- Foreground animation state (owned by orchestration; the engine draws it) ---
// These compose into the band IN FRONT of the pet and never touch the pose buffer
// (AD-5). Anchors (mouth / sleep-Z / daydream) come from the active descriptor.
enum class Consume : uint8_t { Apple, Water };

// Eat timing — shared: the composers use the breakdown, the loop uses the total.
static const uint32_t EAT_DROP_MS = 450;
static const uint32_t EAT_BITE_MS = 200;
static const int      EAT_BITES   = 3;

struct EatAnim      { bool active = false; uint32_t start = 0; int kind = (int)Consume::Apple; };
struct SleepAnim    { bool active = false; uint32_t start = 0; };
struct DaydreamAnim { bool active = false; uint32_t start = 0; int icon = 0; uint32_t next = 8000; };

namespace anim {
// Foreground composers (Story 2.3) — stateless given their inputs; draw into the
// given surface only. Used for the band (in-box) and, for eat/daydream, the
// direct panel path (out-of-box, viewport-clipped).
void composeEat(TFT_eSPI& c, int ox, int oy, uint32_t t, Consume kind);   // apple shrink / glass drain at the mouth
void composeSleepZ(TFT_eSprite& band, uint32_t sinceStart);               // rising Z's beside the head
void composeDaydreamBand(TFT_eSprite& band, int icon);                    // in-box thought bubble
void composeDaydreamDirect(TFT_eSPI& tft, int icon);                      // open-sky thought bubble (viewport-clipped)
void eraseDaydreamDirect(TFT_eSPI& tft);                                  // erase the open-sky part
void composeButterfly(TFT_eSprite& band, uint32_t t, uint32_t period);    // play: figure-8 flutter
void composeBubbles(TFT_eSprite& band, uint32_t t);                       // play: rising soap bubbles
}

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
