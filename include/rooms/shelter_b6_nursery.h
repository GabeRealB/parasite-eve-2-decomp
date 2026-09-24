#ifndef ROOMS_SHELTER_B6_NURSERY_H
#define ROOMS_SHELTER_B6_NURSERY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"
#include "rooms/room_common.h"

/// Two-`s16` pair shared by units 7 and 8 of the overlay.
/// `func_shelter_b6_nursery_80182D14` is the setter that writes both halves at
/// once; `func_shelter_b6_nursery_801800A0` clears the pair on entry, gates a
/// 16-iteration `Gp_SpawnEff` burst on `field_2` being non-zero, and clears it
/// again afterwards.
typedef struct ShelterB6NurseryPair {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} ShelterB6NurseryPair;
STATIC_ASSERT_SIZEOF(ShelterB6NurseryPair, 0x4);

extern ShelterB6NurseryPair D_shelter_b6_nursery_801879F0;

/// Spawn-parameter block handed to the `D_shelter_b6_nursery_80184FDC` cutscene
/// task as `Task::spawnArg2`. `func_shelter_b6_nursery_8017FA54` fills it before
/// `Task_SpawnFromTable`; `func_shelter_b6_nursery_8017F4E8` (the task body)
/// reads `field_0` as a destination room id (`Mc_SaveData.at4.loc.view`, negative =
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

/// States of the room's message task: install the message table, idle, die.
extern const TaskFuncTable3 D_shelter_b6_nursery_8017D6A4;

void func_shelter_b6_nursery_8017F4AC(Task* task);
void func_shelter_b6_nursery_80180518(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_nursery_8018098C(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_b6_nursery_80182FCC(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_b6_nursery_801833F8(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_b6_nursery_801842FC(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_B6_NURSERY_H
