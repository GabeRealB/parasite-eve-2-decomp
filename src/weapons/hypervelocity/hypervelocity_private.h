#ifndef SRC_WEAPONS_HYPERVELOCITY_HYPERVELOCITY_PRIVATE_H
#define SRC_WEAPONS_HYPERVELOCITY_HYPERVELOCITY_PRIVATE_H

#include "types.h"

/// Per-particle jitter of the hypervelocity trail, one 8-bit LCG roll each,
/// re-rolled as a block when the round is fired.
extern s16 D_hypervelocity_8012EF0C[16];

#endif // SRC_WEAPONS_HYPERVELOCITY_HYPERVELOCITY_PRIVATE_H
