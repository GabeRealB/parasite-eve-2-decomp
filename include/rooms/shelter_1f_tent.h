#ifndef ROOMS_SHELTER_1F_TENT_H
#define ROOMS_SHELTER_1F_TENT_H

#include "common.h"

/// `Task::spawnArg2` of the cap (cutscene) task this room family spawns.
/// `field_0` is the area id forced for the duration of the scene (negative =
/// keep the current one); `field_1` selects the cap slot / command;
/// `field_2` skips straight to the abort state; `field_3` is the cap file to
/// load. The four s32s are sound-event ids, and `field_14` / `field_16` are
/// the `func_800E6D4C` fade pair.
typedef struct {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} Shelter1fTentCapScript;

STATIC_ASSERT_SIZEOF(Shelter1fTentCapScript, 0x18);

extern Shelter1fTentCapScript D_shelter_1f_tent_801843C4;

#endif // ROOMS_SHELTER_1F_TENT_H
