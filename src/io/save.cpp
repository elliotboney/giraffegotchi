#include "save.h"
#include "../pet.h"
#include <Arduino.h>
#include <Preferences.h>

// Persisted care state (survives power loss). Saved to NVS on change (throttled)
// and restored on boot. Restore is as-is — a power cut doesn't age the giraffe.
namespace {
  Preferences prefs;
  struct PetSave { uint8_t magic, hu, th, fn, hy, poop, dead; };
  const uint8_t  SAVE_MAGIC = 0x68;     // bump if PetSave layout changes
  bool     s_saveDirty = false;
  uint32_t s_lastSave  = 0;
}

void save::begin() { prefs.begin("giraffe", false); }   // NVS namespace

void save::writeNow(const Pet& pet, bool dead) {
  const PetSave s{ SAVE_MAGIC, pet.hunger(), pet.thirst(), pet.fun(),
                   pet.hygiene(), pet.poopCount(), (uint8_t)dead };
  prefs.putBytes("pet", &s, sizeof(s));
}

void save::restore(Pet& pet, bool& dead) {
  PetSave s;
  if (prefs.getBytes("pet", &s, sizeof(s)) == sizeof(s) && s.magic == SAVE_MAGIC) {
    pet.load(s.hu, s.th, s.fn, s.hy, s.poop);
    dead = s.dead != 0;
    Serial.printf("[save] restored H%u T%u F%u C%u poop%u dead%u\n",
                  s.hu, s.th, s.fn, s.hy, s.poop, s.dead);
  }
}

void save::markDirty() { s_saveDirty = true; }

// Persist care state on change, throttled to spare the flash (<= once / 5 s).
void save::tick(uint32_t now, const Pet& pet, bool dead) {
  if (s_saveDirty && now - s_lastSave >= 5000) {
    writeNow(pet, dead);
    s_lastSave  = now;
    s_saveDirty = false;
  }
}
