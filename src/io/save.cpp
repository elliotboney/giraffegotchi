#include "save.h"
#include "../pet.h"
#include "../species/registry.h"
#include <Arduino.h>
#include <Preferences.h>

// Per-species care state (survives power loss). One block per animal + the active
// id, versioned by SAVE_MAGIC. Restore is as-is — a power cut doesn't age the pet.
namespace {
  struct CareBlock { uint8_t hu, th, fn, hy, poop, dead; };
  const int     MAX_SP = 8;                 // fixed capacity (registry has fewer)
  struct PetSave { uint8_t magic; uint8_t activeIdx; CareBlock sp[MAX_SP]; };
  const uint8_t SAVE_MAGIC = 0x69;          // bump on layout change (was 0x68 single-block)

  Preferences prefs;
  PetSave  s_blob;
  bool     s_dirty    = false;
  uint32_t s_lastSave = 0;

  int activeSlot() {
    const int i = activeSpeciesIndex();
    return (i >= 0 && i < MAX_SP) ? i : 0;
  }
  void freshBlob() {
    s_blob.magic = SAVE_MAGIC;
    for (int i = 0; i < MAX_SP; i++) s_blob.sp[i] = {100, 100, 100, 100, 0, 0};  // never-visited = full
    s_blob.activeIdx = activeSpeciesIndex();   // compile-time default on first boot
  }
}

void save::begin() { prefs.begin("giraffe", false); }   // NVS namespace

void save::loadActive(Pet& pet, bool& dead) {
  const CareBlock& b = s_blob.sp[activeSlot()];
  pet.load(b.hu, b.th, b.fn, b.hy, b.poop);
  dead = b.dead != 0;
}

void save::captureActive(const Pet& pet, bool dead) {
  s_blob.sp[activeSlot()] = { pet.hunger(), pet.thirst(), pet.fun(),
                              pet.hygiene(), pet.poopCount(), (uint8_t)dead };
}

void save::restore(Pet& pet, bool& dead) {
  const bool valid = prefs.getBytes("pet", &s_blob, sizeof(s_blob)) == sizeof(s_blob)
                     && s_blob.magic == SAVE_MAGIC;
  if (!valid) freshBlob();                    // no/stale save -> fresh (no boot-loop)

  int idx = s_blob.activeIdx;
  if (idx < 0 || idx >= speciesCount()) idx = 0;   // unknown id -> default species (NFR7)
  setActiveSpecies(idx);
  loadActive(pet, dead);
  Serial.printf("[save] active=%s H%u T%u F%u C%u poop%u dead%u\n",
                activeSpecies().name, pet.hunger(), pet.thirst(), pet.fun(),
                pet.hygiene(), pet.poopCount(), dead);
}

void save::writeNow(const Pet& pet, bool dead) {
  captureActive(pet, dead);
  s_blob.activeIdx = activeSpeciesIndex();
  prefs.putBytes("pet", &s_blob, sizeof(s_blob));
}

void save::markDirty() { s_dirty = true; }

// Persist on change, throttled to spare the flash (<= once / 5 s).
void save::tick(uint32_t now, const Pet& pet, bool dead) {
  if (s_dirty && now - s_lastSave >= 5000) {
    writeNow(pet, dead);
    s_lastSave = now;
    s_dirty    = false;
  }
}
