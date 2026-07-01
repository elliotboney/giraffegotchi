#pragma once
#include <stdint.h>

// NVS persistence (AD-8). Versioned fixed struct holding a PER-SPECIES care block
// (stats + poop + prank-death) for every animal plus the active-species id, so
// each animal keeps its own life across swaps and power loss (FR12). Throttled to
// <= once / 5 s, restore-as-is (no decay advance).

class Pet;

namespace save {
  void begin();                                       // open the NVS namespace
  void restore(Pet& pet, bool& dead);                 // load blob, set active species, load its block
  void captureActive(const Pet& pet, bool dead);      // store the active species' block (call before a swap)
  void loadActive(Pet& pet, bool& dead);              // load the active species' block into pet (after a swap)
  void writeNow(const Pet& pet, bool dead);           // capture active + persist immediately
  void markDirty();                                   // note state changed since last write
  void tick(uint32_t now, const Pet& pet, bool dead); // throttled write (<= once / 5 s, only when dirty)
}
