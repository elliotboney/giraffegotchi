#include "registry.h"
#include <string.h>

// Built-in species (defined in their own .cpp data files).
extern const Species GIRAFFE;
extern const Species GROUNDHOG;

static const Species* const SPECIES[] = { &GIRAFFE, &GROUNDHOG };
static const int SPECIES_N = sizeof(SPECIES) / sizeof(SPECIES[0]);

// Dev-only compile-time active-species override (Story 2.6) — the runtime swap
// (Epic 4) changes s_active after boot. Defaults to the giraffe. Pick another by
// naming its symbol, e.g. -DACTIVE_SPECIES=GROUNDHOG.
#ifndef ACTIVE_SPECIES
#define ACTIVE_SPECIES GIRAFFE
#endif

static const Species* s_active = &ACTIVE_SPECIES;

const Species& activeSpecies() { return *s_active; }
int  speciesCount()            { return SPECIES_N; }
const Species& speciesAt(int i){ return *SPECIES[i]; }

int activeSpeciesIndex() {
  for (int i = 0; i < SPECIES_N; i++)
    if (SPECIES[i] == s_active) return i;
  return 0;
}

void setActiveSpecies(int i) {
  if (i >= 0 && i < SPECIES_N) s_active = SPECIES[i];
}

int findSpecies(const char* name) {
  for (int i = 0; i < SPECIES_N; i++)
    if (strcmp(SPECIES[i]->name, name) == 0) return i;
  return -1;
}
