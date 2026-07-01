#pragma once
#include <stdint.h>

// NVS persistence for the pet's care state + the prank-death flag (AD-8).
// Fixed struct guarded by SAVE_MAGIC, throttled to <= once / 5 s, restore-as-is
// (no decay advance). The orchestration layer owns the dead flag and passes it
// in; this module owns only the NVS blob and the dirty/throttle bookkeeping.

class Pet;

namespace save {
  void begin();                                       // open the NVS namespace
  void restore(Pet& pet, bool& dead);                 // load stats + dead flag (no-op if absent/invalid)
  void writeNow(const Pet& pet, bool dead);           // immediate write (death/revive)
  void markDirty();                                   // note state changed since last write
  void tick(uint32_t now, const Pet& pet, bool dead); // throttled write (<= once / 5 s, only when dirty)
}
