#pragma once
#include "species.h"

// The built-in species registry + the active-species accessor. Story 1.4 has a
// single entry (the giraffe) and a fixed active species; the runtime swap
// (Epic 4) adds selection + persistence on top of this accessor.

const Species& activeSpecies();
