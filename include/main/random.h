#ifndef MAIN_RANDOM_H
#define MAIN_RANDOM_H

#include "types.h"

/// Coefficients of the shared 32-bit linear congruential random sequence.
enum {
    /// Multiplicative coefficient of the game's 32-bit linear congruential sequences.
    ///
    /// Multiplies the previous state before adding `RANDOM_LCG_INCREMENT`;
    /// the next state is the low 32 bits of the result. Shared gameplay draws
    /// and independently seeded distortion grids use the same coefficient.
    /// This constant retains type `int`, including in signed intermediate
    /// expressions; unsigned state operands provide unsigned wraparound.
    RANDOM_LCG_MULTIPLIER = 5,

    /// Additive coefficient of the game's 32-bit linear congruential sequences.
    ///
    /// Added after multiplying the previous state by `RANDOM_LCG_MULTIPLIER`;
    /// the next state is the low 32 bits of the result. Shared gameplay draws
    /// and independently seeded distortion grids use the same coefficient.
    /// This constant retains type `int`, including in signed intermediate
    /// expressions; unsigned state operands provide unsigned wraparound.
    RANDOM_LCG_INCREMENT = 0x71357911,
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

/// The state that follows `state` in a sequence (the shared one, or one seeded on its own).
#define RANDOM_LCG_NEXT(state) ((state) * RANDOM_LCG_MULTIPLIER + RANDOM_LCG_INCREMENT)

/// Advances the shared sequence by one draw; its value is the new state.
#define RANDOM_LCG_ADVANCE() (gRandomLcgState = RANDOM_LCG_NEXT(gRandomLcgState))

/// The shared sequence's current draw: the state's upper 16 bits, 0..65535.
#define RANDOM_LCG_DRAW() (gRandomLcgState >> 16)

#endif // MAIN_RANDOM_H
