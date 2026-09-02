#ifndef ROOMS_SHELTER_B6_NURSERY_H
#define ROOMS_SHELTER_B6_NURSERY_H

#include "common.h"

#include <psyq/libgte.h>

#include "rooms/room_common.h"

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

/// Spawn-parameter block handed to the `D_shelter_b6_nursery_80184FDC` cutscene
/// task as `Task::spawnArg2`. `func_shelter_b6_nursery_8017FA54` fills it before
/// `Task_SpawnFromTable`; `func_shelter_b6_nursery_8017F4E8` (the task body)
/// reads `field_0` as a destination room id (`Mc_SaveData.field_4`, negative =
/// stay), `field_1`/`field_2`/`field_3` as signed selectors, `field_4`..
/// `field_10` as sound-event ids and `field_14`/`field_16` as a coordinate pair.
typedef struct ShelterB6NurseryEvt {
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
} ShelterB6NurseryEvt;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryEvt, 0x18);

extern ShelterB6NurseryEvt D_shelter_b6_nursery_80187980;

#endif // ROOMS_SHELTER_B6_NURSERY_H
