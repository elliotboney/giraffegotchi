#include <unity.h>
#include "pet.h"

void setUp(void) {}
void tearDown(void) {}

#define ASSERT_EMOTION(expected, pet) \
  TEST_ASSERT_EQUAL(int(expected), int((pet).emotion()))

// ---- hunger mechanics (unchanged) ----

void test_decay_tick(void) {
  Pet p(80);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(79, p.hunger());
}

void test_feed(void) {
  Pet p(40);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(65, p.hunger());
}

void test_feed_clamp(void) {
  Pet p(100);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(100, p.hunger());
}

void test_feed_partial_clamp(void) {
  Pet p(90);
  p.feed();
  TEST_ASSERT_EQUAL_UINT8(100, p.hunger());
}

void test_decay_clamp_zero(void) {
  Pet p(0);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(0, p.hunger());
}

// ---- the other three stats decay + replenish the same way ----

void test_thirst_decay_tick(void) {
  Pet p(100, 80);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(79, p.thirst());
}

void test_fun_decay_tick(void) {
  Pet p(100, 100, 80);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(79, p.fun());
}

void test_hygiene_not_timed(void) {
  Pet p(100, 100, 100, 80);
  p.update(Pet::DECAY_INTERVAL_MS);   // hygiene is not on the decay timer, only poop
  TEST_ASSERT_EQUAL_UINT8(80, p.hygiene());
}

void test_all_stats_decay_together(void) {
  Pet p(50, 50, 50, 50);
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(49, p.hunger());
  TEST_ASSERT_EQUAL_UINT8(49, p.thirst());
  TEST_ASSERT_EQUAL_UINT8(49, p.fun());
  TEST_ASSERT_EQUAL_UINT8(50, p.hygiene());  // hygiene exempt from timed decay
}

void test_drink(void) {
  Pet p(100, 40);
  p.drink();
  TEST_ASSERT_EQUAL_UINT8(65, p.thirst());
}

void test_play(void) {
  Pet p(100, 100, 40);
  p.play();
  TEST_ASSERT_EQUAL_UINT8(65, p.fun());
}

void test_drink_play_clamp(void) {
  Pet a(100, 90);   a.drink(); TEST_ASSERT_EQUAL_UINT8(100, a.thirst());
  Pet b(100, 100, 90); b.play(); TEST_ASSERT_EQUAL_UINT8(100, b.fun());
}

void test_clean_restores_hygiene(void) {
  Pet p(100, 100, 100, 40);
  p.clean();
  TEST_ASSERT_EQUAL_UINT8(80, p.hygiene());  // 40 + CLEAN_AMOUNT
}

// ---- poop mechanic ----

void test_poop_spawns_on_timer(void) {
  Pet p;
  p.update(Pet::POOP_INTERVAL_MS);
  TEST_ASSERT_EQUAL_UINT8(1, p.poopCount());
}

void test_poop_count_caps(void) {
  Pet p;
  p.update(Pet::POOP_INTERVAL_MS * (Pet::MAX_POOP + 3));
  TEST_ASSERT_EQUAL_UINT8(Pet::MAX_POOP, p.poopCount());
}

void test_poop_spawn_drops_hygiene(void) {
  Pet p;                                  // start hygiene 100
  p.update(Pet::POOP_INTERVAL_MS);        // 1 poop; hygiene drops only by the poop penalty
  TEST_ASSERT_EQUAL_UINT8(100 - Pet::POOP_SPAWN_PENALTY, p.hygiene());
}

void test_clean_removes_all_poop(void) {
  Pet p;
  p.update(Pet::POOP_INTERVAL_MS * 3);    // a few poops
  TEST_ASSERT_TRUE(p.poopCount() > 0);
  p.clean();
  TEST_ASSERT_EQUAL_UINT8(0, p.poopCount());
}

void test_clean_resets_poop_timer(void) {
  Pet p;
  p.update(Pet::POOP_INTERVAL_MS);
  p.clean();
  p.update(Pet::POOP_INTERVAL_MS - 1);    // not quite a full interval since clean
  TEST_ASSERT_EQUAL_UINT8(0, p.poopCount());
}

// ---- emotions: existing ----

void test_emotion_happy_default(void) {
  ASSERT_EMOTION(Emotion::Happy, Pet(100));
}

void test_emotion_hungry_boundary(void) {
  ASSERT_EMOTION(Emotion::Happy, Pet(30));
  ASSERT_EMOTION(Emotion::Hungry, Pet(29));
}

void test_emotion_sad_boundary(void) {
  ASSERT_EMOTION(Emotion::Hungry, Pet(15));
  ASSERT_EMOTION(Emotion::Sad, Pet(14));
}

void test_emotion_excited_then_expires(void) {
  Pet p(50);
  p.feed();
  ASSERT_EMOTION(Emotion::Excited, p);
  p.update(Pet::EXCITED_MS);
  ASSERT_EMOTION(Emotion::Happy, p);
}

void test_emotion_excited_on_drink_and_play(void) {
  Pet a(100, 50); a.drink(); ASSERT_EMOTION(Emotion::Excited, a);
  Pet b(100, 100, 50); b.play(); ASSERT_EMOTION(Emotion::Excited, b);
}

void test_emotion_sick_from_hunger(void) {
  Pet p(0);
  ASSERT_EMOTION(Emotion::Sad, p);
  p.update(Pet::SICK_MS);
  ASSERT_EMOTION(Emotion::Sick, p);
}

void test_idle_no_daytime_nap(void) {
  Pet p(100);
  p.update(Pet::NIGHT_WAKE_MS * 5);   // long idle by day → stays content, no nap
  ASSERT_EMOTION(Emotion::Happy, p);
}

void test_emotion_reading(void) {
  Pet p(5);
  p.read();
  ASSERT_EMOTION(Emotion::Reading, p);
  p.update(Pet::READING_MS);
  ASSERT_EMOTION(Emotion::Sad, p);
}

// ---- emotions: new needs ----

void test_emotion_thirsty(void) {
  ASSERT_EMOTION(Emotion::Happy, Pet(100, 30));
  ASSERT_EMOTION(Emotion::Thirsty, Pet(100, 29));
}

void test_emotion_bored(void) {
  ASSERT_EMOTION(Emotion::Bored, Pet(100, 100, 29));
}

void test_emotion_dirty(void) {
  ASSERT_EMOTION(Emotion::Dirty, Pet(100, 100, 100, 29));
}

void test_emotion_lowest_need_wins(void) {
  ASSERT_EMOTION(Emotion::Bored, Pet(100, 20, 10, 100));  // fun lower than thirst
}

void test_emotion_tie_breaks_by_statid(void) {
  ASSERT_EMOTION(Emotion::Thirsty, Pet(100, 20, 20, 100));  // Thirst precedes Fun
}

void test_emotion_sick_from_hygiene(void) {
  Pet p(100, 100, 100, 0);
  ASSERT_EMOTION(Emotion::Dirty, p);
  p.update(Pet::SICK_MS);
  ASSERT_EMOTION(Emotion::Sick, p);
}

// ---- night sleep ----

void test_night_sleep_pauses_world(void) {
  Pet p(50, 50, 50, 50);
  p.setNight(true);
  p.update(Pet::NIGHT_WAKE_MS + Pet::DECAY_INTERVAL_MS * 50);  // asleep, lots of time
  ASSERT_EMOTION(Emotion::Sleepy, p);
  TEST_ASSERT_EQUAL_UINT8(50, p.hunger());      // decay frozen while asleep
  TEST_ASSERT_EQUAL_UINT8(50, p.hygiene());
  TEST_ASSERT_EQUAL_UINT8(0, p.poopCount());    // no poop at night
}

void test_night_wake_on_action(void) {
  Pet p(50, 50, 50, 50);
  p.setNight(true);
  p.feed();                                     // care action opens the wake window
  TEST_ASSERT_FALSE(p.nightSleeping());
  p.update(Pet::DECAY_INTERVAL_MS);             // within the window → normal decay
  TEST_ASSERT_EQUAL_UINT8(74, p.hunger());      // 50 + 25 feed - 1 decay
  p.update(Pet::NIGHT_WAKE_MS);                 // window elapses → back to sleep
  TEST_ASSERT_TRUE(p.nightSleeping());
}

void test_night_off_is_normal(void) {
  Pet p(50, 50, 50, 50);                        // nightMode defaults false
  p.update(Pet::DECAY_INTERVAL_MS);
  TEST_ASSERT_FALSE(p.nightSleeping());
  TEST_ASSERT_EQUAL_UINT8(49, p.hunger());      // decays as usual
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_decay_tick);
  RUN_TEST(test_feed);
  RUN_TEST(test_feed_clamp);
  RUN_TEST(test_feed_partial_clamp);
  RUN_TEST(test_decay_clamp_zero);
  RUN_TEST(test_thirst_decay_tick);
  RUN_TEST(test_fun_decay_tick);
  RUN_TEST(test_hygiene_not_timed);
  RUN_TEST(test_all_stats_decay_together);
  RUN_TEST(test_drink);
  RUN_TEST(test_play);
  RUN_TEST(test_drink_play_clamp);
  RUN_TEST(test_clean_restores_hygiene);
  RUN_TEST(test_poop_spawns_on_timer);
  RUN_TEST(test_poop_count_caps);
  RUN_TEST(test_poop_spawn_drops_hygiene);
  RUN_TEST(test_clean_removes_all_poop);
  RUN_TEST(test_clean_resets_poop_timer);
  RUN_TEST(test_emotion_happy_default);
  RUN_TEST(test_emotion_hungry_boundary);
  RUN_TEST(test_emotion_sad_boundary);
  RUN_TEST(test_emotion_excited_then_expires);
  RUN_TEST(test_emotion_excited_on_drink_and_play);
  RUN_TEST(test_emotion_sick_from_hunger);
  RUN_TEST(test_idle_no_daytime_nap);
  RUN_TEST(test_emotion_reading);
  RUN_TEST(test_emotion_thirsty);
  RUN_TEST(test_emotion_bored);
  RUN_TEST(test_emotion_dirty);
  RUN_TEST(test_emotion_lowest_need_wins);
  RUN_TEST(test_emotion_tie_breaks_by_statid);
  RUN_TEST(test_emotion_sick_from_hygiene);
  RUN_TEST(test_night_sleep_pauses_world);
  RUN_TEST(test_night_wake_on_action);
  RUN_TEST(test_night_off_is_normal);
  return UNITY_END();
}
