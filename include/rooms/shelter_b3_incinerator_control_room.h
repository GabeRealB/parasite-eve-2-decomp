#ifndef ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
#define ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H

#include "common.h"

#include "main/task.h"

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
} ShelterB3IncineratorControlRoomCapScript;

STATIC_ASSERT_SIZEOF(ShelterB3IncineratorControlRoomCapScript, 0x18);

extern TaskDesc                                 D_shelter_b3_incinerator_control_room_80181814;
extern ShelterB3IncineratorControlRoomCapScript D_shelter_b3_incinerator_control_room_80182A58;

#endif // ROOMS_SHELTER_B3_INCINERATOR_CONTROL_ROOM_H
