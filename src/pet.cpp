#include "pet.h"

static uint32_t satAdd(uint32_t a, uint32_t b) {
  return (a > UINT32_MAX - b) ? UINT32_MAX : a + b;  // saturate, no overflow
}

void Pet::feed() {
  uint16_t h = (uint16_t)hunger_ + FEED_AMOUNT;
  hunger_ = h > 100 ? 100 : (uint8_t)h;
  sinceFeed_ = 0;
}

void Pet::read() {
  sinceRead_ = 0;
}

void Pet::update(uint32_t elapsedMs) {
  sinceFeed_ = satAdd(sinceFeed_, elapsedMs);
  sinceRead_ = satAdd(sinceRead_, elapsedMs);

  accum_ += elapsedMs;
  while (accum_ >= DECAY_INTERVAL_MS) {
    accum_ -= DECAY_INTERVAL_MS;
    hunger_ = hunger_ > DECAY_STEP ? (uint8_t)(hunger_ - DECAY_STEP) : 0;
  }

  atZero_ = (hunger_ == 0) ? satAdd(atZero_, elapsedMs) : 0;
}

Emotion Pet::emotion() const {
  if (sinceRead_ < READING_MS)        return Emotion::Reading;  // user-initiated, top priority
  if (sinceFeed_ < EXCITED_MS)        return Emotion::Excited;
  if (atZero_ >= SICK_MS)             return Emotion::Sick;
  if (hunger_ < SAD_THRESHOLD)        return Emotion::Sad;
  if (hunger_ < HUNGRY_THRESHOLD)     return Emotion::Hungry;
  if (sinceFeed_ >= SLEEPY_MS)        return Emotion::Sleepy;
  return Emotion::Happy;
}
