#pragma once
#include "species.h"

// The built-in species registry + the active-species accessor. The active
// species is chosen at compile time by default (ACTIVE_SPECIES) and switched at
// runtime by the swap (Epic 4). main is the only module that applies a swap
// (AD-13); render/anim read the active species through activeSpecies().

const Species& activeSpecies();
int  speciesCount();
const Species& speciesAt(int i);
int  activeSpeciesIndex();
void setActiveSpecies(int i);       // switch the active species (index into the registry)
int  findSpecies(const char* name); // registry index by name, or -1
