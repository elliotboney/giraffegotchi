#include <unity.h>
#include "pet.h"

void setUp(void) {}
void tearDown(void) {}

// Decay tick: hunger 80, one decay interval -> 79
void test_decay_tick(void) {
  Pet p(80);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(79, p.hunger());
}

// Cross threshold: 31 (Happy) -> two intervals -> 29 (Hungry)
void test_threshold_flip(void) {
  Pet p(31);
  TEST_ASSERT_EQUAL(int(Mood::Happy), int(p.mood()));
  p.update(Pet::DECAY_INTERVAL_MS * 2);
  TEST_ASSERT_EQUAL_UINT8(29, p.hunger());
  TEST_ASSERT_EQUAL(int(Mood::Hungry), int(p.mood()));
}

// Feed: 40 -> 65, mood Happy
void test_feed(void) {
  Pet p(40);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(65, p.hunger());
  TEST_ASSERT_EQUAL(int(Mood::Happy), int(p.mood()));
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

// Exact threshold boundary: hunger==30 is Happy (Hungry only when < 30)
void test_threshold_boundary(void) {
  TEST_ASSERT_EQUAL(int(Mood::Happy), int(Pet(30).mood()));
  TEST_ASSERT_EQUAL(int(Mood::Hungry), int(Pet(29).mood()));
}

// Decay clamps at 0 (no underflow)
void test_decay_clamp_zero(void) {
  Pet p(0);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(0, p.hunger());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_decay_tick);
  RUN_TEST(test_threshold_flip);
  RUN_TEST(test_feed);
  RUN_TEST(test_feed_clamp);
  RUN_TEST(test_feed_partial_clamp);
  RUN_TEST(test_threshold_boundary);
  RUN_TEST(test_decay_clamp_zero);
  return UNITY_END();
}
