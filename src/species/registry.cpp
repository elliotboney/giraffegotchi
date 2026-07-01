#include "registry.h"

// Built-in species. The giraffe is the sole entry for now (Epic 5 adds the
// groundhog). Defined in giraffe.cpp.
extern const Species GIRAFFE;

// The active species. Fixed to the giraffe until the runtime swap (Epic 4)
// makes this mutable + persisted.
static const Species* s_active = &GIRAFFE;

const Species& activeSpecies() { return *s_active; }
