#ifndef MAIN_SOUND_TYPES_H
#define MAIN_SOUND_TYPES_H

#include "common.h"

/// Scaling policies for a linear audio gain ramp.
enum {
    LINEAR_INTERPOLATOR_BYPASS = 0,
    LINEAR_INTERPOLATOR_SCALE  = 1
};

/// Caller-owned linear gain ramp for audio fade-in, fade-out and muting.
///
/// Gain is a normalized fraction with denominator 65535: zero is silence and
/// 65535 is unity. Setup uses only the ordering of two low-byte level selectors
/// to choose 0 -> 65535 or 65535 -> 0; their magnitudes are not stored.
/// Equal selectors or zero duration reset the ramp to bypass scaling.
///
/// For positive durations, the step is 65535 / requested updates, rounded down;
/// durations 1..65535 give a nonzero step, and rounding can extend the fade.
/// Each step call advances once: MIDI and scripts use audio updates (including
/// extra PAL ticks), while CD playback uses its main-loop update. Application
/// scales an independently supplied level by gain / 65535 when enabled.
/// Reaching the target retains that gain; application then clears the step,
/// leaving scaling enabled. The enable policy does not control stepping.
typedef struct {
    u32 gain;       // Current gain (0 silent, 65535 unity), clamped to targetGain
    u32 targetGain; // Endpoint (0 for fade-out, 65535 for fade-in; 0 when bypassed)
    s32 step;       // Gain units per step call; signed duration quotient (0 holds gain)
    s16 direction;  // Direction (-1 decreasing, 1 increasing); irrelevant when step is zero
    s16 enabled;    // Scaling policy (0 bypass, 1 apply gain, including a completed ramp)
} LinInterp;
STATIC_ASSERT_SIZEOF(LinInterp, 0x10);

/// Type-1 script request with only the bank-type key set.
///
/// Bits 16..31 hold the type-1 bank key (`SOUND_BANK_TYPE_1`, 0x1000) and the
/// low half is zero, so the word names the loaded type-1 bank rather than one
/// entry or instance. Bits 28..31 are that type's nibble. Masking any type-1
/// request with 0xF0000000 yields this value, including one that carries a
/// retail bank number and an entry.
enum { SOUND_SCRIPT_REQUEST_TYPE_1 = 0x10000000 };

#endif // MAIN_SOUND_TYPES_H
