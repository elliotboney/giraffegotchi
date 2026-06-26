#include "pet.h"

void Pet::feed() {
  uint16_t h = (uint16_t)hunger_ + FEED_AMOUNT;
  hunger_ = h > 100 ? 100 : (uint8_t)h;
}

void Pet::update(uint32_t elapsedMs) {
  accum_ += elapsedMs;
  while (accum_ >= DECAY_INTERVAL_MS) {
    accum_ -= DECAY_INTERVAL_MS;
    hunger_ = hunger_ > DECAY_STEP ? (uint8_t)(hunger_ - DECAY_STEP) : 0;
  }
}
