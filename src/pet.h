#pragma once
#include <stdint.h>

// Hardware-free pet state. No Arduino/TFT includes so it builds in the
// `native` test environment.

enum class Emotion {
  Happy, Hungry, Sad, Excited, Sleepy, Sick, Reading,  // original
  Thirsty, Bored, Dirty                                // care-stat needs
};

// Care stats, all 0..100. Order is the emotion tie-break order (earlier wins).
enum class StatId : uint8_t { Hunger, Thirst, Fun, Hygiene };

class Pet {
public:
  static constexpr uint8_t  STAT_COUNT         = 4;
  static constexpr uint8_t  LOW_THRESHOLD      = 30;    // stat < this => that stat's need emotion
  static constexpr uint8_t  CRITICAL_THRESHOLD = 15;    // hunger < this => Sad
  static constexpr uint8_t  DECAY_STEP         = 1;     // each stat loses this per interval
  static constexpr uint32_t DECAY_INTERVAL_MS  = 60000; // 1 pt/min => ~1h40m to empty a full stat

  static constexpr uint8_t  FEED_AMOUNT        = 25;    // +Hunger
  static constexpr uint8_t  DRINK_AMOUNT       = 25;    // +Thirst
  static constexpr uint8_t  PLAY_AMOUNT        = 25;    // +Fun
  static constexpr uint8_t  CLEAN_AMOUNT       = 40;    // +Hygiene (clean is a bigger restore)

  static constexpr uint32_t EXCITED_MS         = 2500;  // reaction window after any care action
  static constexpr uint32_t READING_MS         = 5000;  // how long a "read" lasts
  static constexpr uint32_t SICK_MS            = 20000; // a need empty (hunger or hygiene) this long => Sick
  static constexpr uint32_t NIGHT_WAKE_MS      = 300000;// after a night-time care action, stay awake this long, then sleep again

  static constexpr uint8_t  MAX_POOP           = 4;     // on-screen poop cap
  static constexpr uint32_t POOP_INTERVAL_MS   = 900000; // 15 min => ~15 Hygiene/15min, on par with the 1/min decay
  static constexpr uint8_t  POOP_SPAWN_PENALTY = 15;    // Hygiene lost when a poop appears

  Pet() = default;
  explicit Pet(uint8_t hunger, uint8_t thirst = 100, uint8_t fun = 100, uint8_t hygiene = 100);

  uint8_t hunger()    const { return stats_[int(StatId::Hunger)];  }
  uint8_t thirst()    const { return stats_[int(StatId::Thirst)];  }
  uint8_t fun()       const { return stats_[int(StatId::Fun)];     }
  uint8_t hygiene()   const { return stats_[int(StatId::Hygiene)]; }
  uint8_t poopCount() const { return poop_; }

  // Current emotion, resolved by priority (see implementation).
  Emotion emotion() const;

  // Care actions. Each clamps its stat at 100 and triggers the Excited window.
  void feed();
  void drink();
  void play();
  void clean();   // removes all poop + restores Hygiene
  void read();    // Reading window (outranks the rest while active)

  // Advance decay, poop spawning, and the sick/idle timers.
  void update(uint32_t elapsedMs);

  // Driven by main from the day/night clock: true once it's ~30 min past sunset
  // until sunrise. While night-asleep, decay/poop/sickness pause.
  void setNight(bool night) { nightMode_ = night; }

  // True when the giraffe is sleeping for the night (night + no recent action).
  bool nightSleeping() const;

private:
  void add(StatId s, uint8_t amount);   // clamp to 100, trigger Excited

  uint8_t  stats_[STAT_COUNT] = {100, 100, 100, 100};  // Hunger, Thirst, Fun, Hygiene
  uint32_t accum_     = 0;          // carried time toward next decay step
  uint32_t poopAccum_ = 0;          // carried time toward next poop spawn
  uint8_t  poop_      = 0;          // poop objects on screen, 0..MAX_POOP
  uint32_t sinceFeed_ = EXCITED_MS; // ms since last care action (drives Excited + Sleepy)
  uint32_t sinceRead_ = READING_MS; // ms since last read
  uint32_t sickTimer_ = 0;          // ms hunger or hygiene has been continuously 0
  bool     nightMode_ = false;      // set by main; true during the night window
};
