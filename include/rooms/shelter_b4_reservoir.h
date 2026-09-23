#ifndef ROOMS_SHELTER_B4_RESERVOIR_H
#define ROOMS_SHELTER_B4_RESERVOIR_H

#include "common.h"

/// Parameters for the periodic `Gp_SpawnEff` burst driven by unit 2 of the
/// overlay. `func_shelter_b4_reservoir_80182B04` writes all three halves at
/// once; `func_shelter_b4_reservoir_8017FB84` zeroes them on entry, skips the
/// burst entirely while `field_0` or `field_2` is zero, then loops `field_0`
/// times, spawning an effect on each iteration whose pseudo-random draw is
/// below `field_2` and biasing the spawn coordinate by `field_4`.
typedef struct ShelterB4ReservoirBurst {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} ShelterB4ReservoirBurst;
STATIC_ASSERT_SIZEOF(ShelterB4ReservoirBurst, 0x6);

extern ShelterB4ReservoirBurst D_shelter_b4_reservoir_80187684;

/// Work block `func_shelter_b4_reservoir_8017FB84` reaches through its task's
/// `spawnArg2`. Only the three halves it touches are known; the bytes before
/// them are not read by that function.
typedef struct ShelterB4ReservoirWork {
    byte pad_0[0x22];
    s16  field_22; // Frames counted while game flag 0xB7 is set; the splash pass is skipped while it is zero
    s16  field_24; // Radius of the last randomised burst point, scaled into the `rsin` / `rcos` offsets
    s16  field_26; // Scratch: the angle of that point, then the splash chance for the frame's movement
} ShelterB4ReservoirWork;

void func_shelter_b4_reservoir_80182B04(s16 arg0, u16 arg1, s16 arg2);

#endif // ROOMS_SHELTER_B4_RESERVOIR_H
