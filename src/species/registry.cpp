#include "registry.h"

// Built-in species (defined in their own .cpp data files).
extern const Species GIRAFFE;
extern const Species GROUNDHOG;

// Dev-only compile-time active-species override (Story 2.6) — lets a build boot
// straight into a species before the runtime swap (Epic 4) exists. Defaults to
// the giraffe. Pick another by naming its symbol, e.g.:
//   pio run -e esp32dev  (append)  -DACTIVE_SPECIES=GROUNDHOG
// Removed once the real selector ships (Story 5.2).
#ifndef ACTIVE_SPECIES
#define ACTIVE_SPECIES GIRAFFE
#endif

// The active species. Fixed until the runtime swap (Epic 4) makes it mutable.
static const Species* s_active = &ACTIVE_SPECIES;

const Species& activeSpecies() { return *s_active; }
