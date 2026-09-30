#ifndef MAIN_RANDOM_H
#define MAIN_RANDOM_H

#include "types.h"

/// Coefficients of the shared 32-bit linear congruential random sequence.
enum {
    RANDOM_LCG_MULTIPLIER = 5,
    RANDOM_LCG_INCREMENT  = 0x71357911,
};

/// Shared pseudo-random state for gameplay and loaded overlays.
///
/// Advance once per draw as `state * RANDOM_LCG_MULTIPLIER +
/// RANDOM_LCG_INCREMENT`, with unsigned 32-bit wraparound. The upper 16 bits
/// provide a draw in 0..65535; consumers mask or reduce it for their own range.
/// Zero is a valid seed, used by replay and deterministic scene playback.
/// Scene playback saves and restores the full state so its draws do not
/// consume the suspended sequence. Storage lives in the resident image and
/// starts at zero; this sequence is separate from the SDK's `rand()` / `srand()`
/// sequence.
extern u32 gRandomLcgState;

#endif // MAIN_RANDOM_H
