#ifndef INCLUDE_ACTORS_ACTORS_SHARED_801673F8_H
#define INCLUDE_ACTORS_ACTORS_SHARED_801673F8_H

#include "common.h"

/// 8-byte spawn point in the absolute tables `D_shelter_b3_dumping_hole_8018B74C` (map 0x427) and
/// `D_shelter_b3_garbage_incinerator_801874C4` (map 0x428), indexed by bits 8..11 of the 0x2C00 message
/// halfword (`ActorsShared80168d3cWork::field_44C`). `ActorsShared801673f8`
/// places the model root at `x` / `y` / `z` facing `heading` + 0x800.
typedef struct ActorsShared801673f8Spot {
    /* 0x0 */ s16 x;
    /* 0x2 */ s16 y;
    /* 0x4 */ s16 z;
    /* 0x6 */ u16 heading;
} ActorsShared801673f8Spot;
STATIC_ASSERT_SIZEOF(ActorsShared801673f8Spot, 0x8);

#endif // INCLUDE_ACTORS_ACTORS_SHARED_801673F8_H
