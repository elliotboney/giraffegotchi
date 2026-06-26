#pragma once
#include <stdint.h>

// Hardware-free pet state. No Arduino/TFT includes so it builds in the
// `native` test environment.

enum class Emotion { Happy, Hungry, Sad, Excited, Sleepy, Sick, Reading };

class Pet {
public:
  static constexpr uint8_t  HUNGRY_THRESHOLD  = 30;    // hunger < this => Hungry
  static constexpr uint8_t  SAD_THRESHOLD     = 15;    // hunger < this => Sad
  static constexpr uint8_t  FEED_AMOUNT       = 25;    // hunger gained per feed
  static constexpr uint8_t  DECAY_STEP        = 1;     // hunger lost per interval
  static constexpr uint32_t DECAY_INTERVAL_MS = 3000;  // time per decay step
  static constexpr uint32_t EXCITED_MS        = 2500;  // excited window after a feed
  static constexpr uint32_t SLEEPY_MS         = 60000; // idle this long => Sleepy
  static constexpr uint32_t SICK_MS           = 20000; // starving (hunger 0) this long => Sick
  static constexpr uint32_t READING_MS        = 5000;  // how long a "read" lasts

  Pet() = default;
  explicit Pet(uint8_t initialHunger) : hunger_(initialHunger) {}

  uint8_t hunger() const { return hunger_; }

  // Current emotion, resolved by priority (see implementation).
  Emotion emotion() const;

  // Add FEED_AMOUNT, clamped at 100 (no overflow/wrap). Resets the feed timer.
  void feed();

  // Make the pet read for READING_MS (user-initiated; outranks other states).
  void read();

  // Accumulate elapsed time: advance the interaction/starvation timers and
  // decrement hunger one DECAY_STEP per full DECAY_INTERVAL_MS (clamped at 0).
  void update(uint32_t elapsedMs);

private:
  uint8_t  hunger_    = 100;          // 0..100, starts full
  uint32_t accum_     = 0;            // carried elapsed time toward next decay step
  uint32_t sinceFeed_ = EXCITED_MS;   // ms since last feed (starts past the excited window)
  uint32_t sinceRead_ = READING_MS;   // ms since last read (starts past the reading window)
  uint32_t atZero_    = 0;            // ms hunger has been continuously 0
};
