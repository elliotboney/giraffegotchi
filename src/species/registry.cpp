#include "registry.h"
#include <string.h>

// Built-in species (defined in their own .cpp data files).
extern const Species GIRAFFE;
extern const Species GROUNDHOG;

static const Species* const SPECIES[] = { &GIRAFFE, &GROUNDHOG };
static const int SPECIES_N = sizeof(SPECIES) / sizeof(SPECIES[0]);

// Default species on a fresh device (no valid save). The on-device picker + NVS
// persistence (Epic 4) choose the active species thereafter; boot restores the
// saved one. (The Story 2.6 dev compile-time override is retired now that the
// real selector ships — the groundhog is reached only through it.)
static const Species* s_active = &GIRAFFE;

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
