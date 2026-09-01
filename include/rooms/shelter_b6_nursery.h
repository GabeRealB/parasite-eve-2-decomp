#ifndef ROOMS_SHELTER_B6_NURSERY_H
#define ROOMS_SHELTER_B6_NURSERY_H

#include "common.h"

/// Two-`s16` pair shared by units 7 and 8 of the overlay.
/// `func_shelter_b6_nursery_80182D14` is the setter that writes both halves at
/// once; `func_shelter_b6_nursery_801800A0` clears the pair on entry, gates a
/// 16-iteration `Gp_SpawnEff` burst on `field_2` being non-zero, and clears it
/// again afterwards.
typedef struct ShelterB6NurseryPair {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
} ShelterB6NurseryPair;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryPair, 0x4);

extern ShelterB6NurseryPair D_shelter_b6_nursery_801879F0;

#endif // ROOMS_SHELTER_B6_NURSERY_H
