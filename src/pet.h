#pragma once
#include <stdint.h>

// Hardware-free pet state. No Arduino/TFT includes so it builds in the
// `native` test environment.

enum class Mood { Happy, Hungry };

class Pet {
public:
  static constexpr uint8_t  HUNGRY_THRESHOLD  = 30;    // hunger < this => Hungry
  static constexpr uint8_t  FEED_AMOUNT       = 25;    // hunger gained per feed
  static constexpr uint8_t  DECAY_STEP        = 1;     // hunger lost per interval
  static constexpr uint32_t DECAY_INTERVAL_MS = 3000;  // time per decay step

  Pet() = default;
  explicit Pet(uint8_t initialHunger) : hunger_(initialHunger) {}

  uint8_t hunger() const { return hunger_; }
  Mood    mood()   const { return hunger_ < HUNGRY_THRESHOLD ? Mood::Hungry : Mood::Happy; }

  // Add FEED_AMOUNT, clamped at 100 (no overflow/wrap).
  void feed();

  // Accumulate elapsed time; decrement hunger one DECAY_STEP per full
  // DECAY_INTERVAL_MS, clamped at 0.
  void update(uint32_t elapsedMs);

private:
  uint8_t  hunger_ = 100;  // 0..100, starts full
  uint32_t accum_  = 0;    // carried elapsed time toward next decay step
};
