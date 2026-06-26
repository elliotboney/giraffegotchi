#include <unity.h>
#include "pet.h"

void setUp(void) {}
void tearDown(void) {}

// ---- hunger mechanics ----

// Decay tick: hunger 80, one decay interval -> 79
void test_decay_tick(void) {
  Pet p(80);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(79, p.hunger());
}

// Feed: 40 -> 65
void test_feed(void) {
  Pet p(40);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(65, p.hunger());
}

// Feed at full clamps at 100 (no wrap)
void test_feed_clamp(void) {
  Pet p(100);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(100, p.hunger());
}

// Partial clamp: 90 + 25 would be 115, clamps to 100
void test_feed_partial_clamp(void) {
  Pet p(90);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(100, p.hunger());
}

// Decay clamps at 0 (no underflow)
void test_decay_clamp_zero(void) {
  Pet p(0);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(0, p.hunger());
}

// ---- emotion resolution ----

#define ASSERT_EMOTION(expected, pet) \
  TEST_ASSERT_EQUAL(int(expected), int((pet).emotion()))

// Well-fed and idle (within sleepy window) -> Happy
void test_emotion_happy_default(void) {
  ASSERT_EMOTION(Emotion::Happy, Pet(100));
}

// Boundary: 30 is Happy, 29 is Hungry
void test_emotion_hungry_boundary(void) {
  ASSERT_EMOTION(Emotion::Happy, Pet(30));
  ASSERT_EMOTION(Emotion::Hungry, Pet(29));
}

// Boundary: 15 is Hungry, 14 is Sad
void test_emotion_sad_boundary(void) {
  ASSERT_EMOTION(Emotion::Hungry, Pet(15));
  ASSERT_EMOTION(Emotion::Sad, Pet(14));
}

// Feeding triggers Excited, which expires after EXCITED_MS -> Happy
void test_emotion_excited_then_expires(void) {
  Pet p(50);
  p.feed();
  ASSERT_EMOTION(Emotion::Excited, p);
  p.update(Pet::EXCITED_MS);   // no decay (under one interval), excited window ends
  ASSERT_EMOTION(Emotion::Happy, p);
}

// Starving (hunger 0) past SICK_MS -> Sick (outranks Sad)
void test_emotion_sick(void) {
  Pet p(0);
  ASSERT_EMOTION(Emotion::Sad, p);      // at 0 but not yet long enough
  p.update(Pet::SICK_MS);
  ASSERT_EMOTION(Emotion::Sick, p);
}

// Idle past SLEEPY_MS while still well-fed -> Sleepy
void test_emotion_sleepy(void) {
  Pet p(100);
  p.update(Pet::SLEEPY_MS);              // ~20 hunger lost, still well above hungry
  ASSERT_EMOTION(Emotion::Sleepy, p);
}

// Reading is user-initiated and outranks everything (even hunger) for its window
void test_emotion_reading(void) {
  Pet p(5);                              // would be Sad
  p.read();
  ASSERT_EMOTION(Emotion::Reading, p);
  p.update(Pet::READING_MS);             // window ends -> falls back to hunger state
  ASSERT_EMOTION(Emotion::Sad, p);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_decay_tick);
  RUN_TEST(test_feed);
  RUN_TEST(test_feed_clamp);
  RUN_TEST(test_feed_partial_clamp);
  RUN_TEST(test_decay_clamp_zero);
  RUN_TEST(test_emotion_happy_default);
  RUN_TEST(test_emotion_hungry_boundary);
  RUN_TEST(test_emotion_sad_boundary);
  RUN_TEST(test_emotion_excited_then_expires);
  RUN_TEST(test_emotion_sick);
  RUN_TEST(test_emotion_sleepy);
  RUN_TEST(test_emotion_reading);
  return UNITY_END();
}
