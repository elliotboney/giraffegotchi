#include "pet.h"

static uint32_t satAdd(uint32_t a, uint32_t b) {
  return (a > UINT32_MAX - b) ? UINT32_MAX : a + b;  // saturate, no overflow
}

Pet::Pet(uint8_t hunger, uint8_t thirst, uint8_t fun, uint8_t hygiene) {
  stats_[int(StatId::Hunger)]  = hunger;
  stats_[int(StatId::Thirst)]  = thirst;
  stats_[int(StatId::Fun)]     = fun;
  stats_[int(StatId::Hygiene)] = hygiene;
}

void Pet::load(uint8_t hunger, uint8_t thirst, uint8_t fun, uint8_t hygiene, uint8_t poop) {
  stats_[int(StatId::Hunger)]  = hunger  > 100 ? 100 : hunger;
  stats_[int(StatId::Thirst)]  = thirst  > 100 ? 100 : thirst;
  stats_[int(StatId::Fun)]     = fun     > 100 ? 100 : fun;
  stats_[int(StatId::Hygiene)] = hygiene > 100 ? 100 : hygiene;
  poop_ = poop > MAX_POOP ? MAX_POOP : poop;
}

void Pet::add(StatId s, uint8_t amount) {
  uint16_t v = (uint16_t)stats_[int(s)] + amount;
  stats_[int(s)] = v > 100 ? 100 : (uint8_t)v;
  sinceFeed_ = 0;   // any care action triggers the Excited reaction
}

void Pet::feed()  { add(StatId::Hunger, FEED_AMOUNT); }
void Pet::drink() { add(StatId::Thirst, DRINK_AMOUNT); }
void Pet::play()  { add(StatId::Fun,    PLAY_AMOUNT); }

void Pet::clean() {
  poop_ = 0;
  poopAccum_ = 0;                         // next poop is a full interval away
  add(StatId::Hygiene, CLEAN_AMOUNT);
}

void Pet::read() { sinceRead_ = 0; }

void Pet::update(uint32_t elapsedMs) {
  // These always advance — they track the wake window and reading/excited state.
  sinceFeed_ = satAdd(sinceFeed_, elapsedMs);
  sinceRead_ = satAdd(sinceRead_, elapsedMs);

  // While sleeping for the night, the world pauses: no poop, no decay, no
  // sickness — so the owner never wakes to a sick giraffe.
  if (nightSleeping()) return;

  // Poop spawns on its own timer; each appearance knocks Hygiene down once.
  poopAccum_ += elapsedMs;
  while (poopAccum_ >= POOP_INTERVAL_MS) {
    poopAccum_ -= POOP_INTERVAL_MS;
    if (poop_ < MAX_POOP) {
      poop_++;
      uint8_t& hy = stats_[int(StatId::Hygiene)];
      hy = hy > POOP_SPAWN_PENALTY ? (uint8_t)(hy - POOP_SPAWN_PENALTY) : 0;
    }
  }

  // Hunger/Thirst/Fun decay on a shared accumulator. Hygiene is NOT timed —
  // it only drops from poop, so its meter moves at a similar pace to the rest.
  accum_ += elapsedMs;
  while (accum_ >= DECAY_INTERVAL_MS) {
    accum_ -= DECAY_INTERVAL_MS;
    for (uint8_t i = 0; i < STAT_COUNT; i++) {
      if (i == int(StatId::Hygiene)) continue;
      stats_[i] = stats_[i] > DECAY_STEP ? (uint8_t)(stats_[i] - DECAY_STEP) : 0;
    }
  }

  // Sick builds while hunger or hygiene is fully empty; resets when neither is.
  const bool starving = stats_[int(StatId::Hunger)]  == 0;
  const bool filthy   = stats_[int(StatId::Hygiene)] == 0;
  sickTimer_ = (starving || filthy) ? satAdd(sickTimer_, elapsedMs) : 0;
}

// Asleep for the night = it's the night window AND no care action recently
// (a feed/drink/play/clean wakes it for NIGHT_WAKE_MS, then it sleeps again).
bool Pet::nightSleeping() const {
  return nightMode_ && sinceFeed_ >= NIGHT_WAKE_MS;
}

Emotion Pet::emotion() const {
  if (sinceRead_ < READING_MS)       return Emotion::Reading;  // user-initiated, top priority
  if (sinceFeed_ < EXCITED_MS)       return Emotion::Excited;
  if (nightSleeping())               return Emotion::Sleepy;   // night sleep outranks needs/sick
  if (sickTimer_ >= SICK_MS)         return Emotion::Sick;
  if (hunger() < CRITICAL_THRESHOLD) return Emotion::Sad;      // hunger keeps its critical tier

  // Otherwise the lowest stat below LOW_THRESHOLD wins (ties -> earlier StatId).
  int worst = -1;
  uint8_t worstVal = LOW_THRESHOLD;
  for (uint8_t i = 0; i < STAT_COUNT; i++) {
    if (stats_[i] < worstVal) { worstVal = stats_[i]; worst = i; }
  }
  switch (worst) {
    case int(StatId::Hunger):  return Emotion::Hungry;
    case int(StatId::Thirst):  return Emotion::Thirsty;
    case int(StatId::Fun):     return Emotion::Bored;
    case int(StatId::Hygiene): return Emotion::Dirty;
    default: break;  // no stat below threshold
  }

  return Emotion::Happy;   // idle by day = content (Sleepy is night-only now)
}
