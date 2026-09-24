#ifndef ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
#define ROOMS_SHELTER_B1_STERILIZATION_ROOM_H

#include "common.h"

#include <psyq/libgte.h>

/// 0x14-byte scratch block `func_shelter_b1_sterilization_room_80183B8C` takes
/// from `G_SCRATCH_HEAD`. `otz`, `flag` and `sx`/`sy` receive the projection of
/// the glow's centre; `rOuter` and `rInner` are its two on-screen radii,
/// derived from that `otz`.
typedef struct {
    s32 otz;
    s32 rOuter;
    s32 rInner;
    s32 flag;
    u16 sx;
    u16 sy;
} ShelterB1SterilizationRoomGlowScratch;

STATIC_ASSERT_SIZEOF(ShelterB1SterilizationRoomGlowScratch, 0x14);

void func_shelter_b1_sterilization_room_80183B8C(SVECTOR* arg0, s32 arg1, s32 arg2);

#endif // ROOMS_SHELTER_B1_STERILIZATION_ROOM_H
