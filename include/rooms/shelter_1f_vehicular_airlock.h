#ifndef ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
#define ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Event parameters latched into the room's pending event when an event
/// starts, and read back by the room's event task. `field_0` is the CAP
/// command the event runs, `field_4` the stage sound played after it (0 for
/// none), `field_8` the game flag checked and set as the event starts (0 for
/// none), and a non-zero `field_A` makes the event task start helper task
/// 0x31 once the command finishes.
typedef struct Shelter1fVehicularAirlockEvent {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  field_A;
} Shelter1fVehicularAirlockEvent;
STATIC_ASSERT_SIZEOF(Shelter1fVehicularAirlockEvent, 0xC);

void func_shelter_1f_vehicular_airlock_8017DC80(SVECTOR* arg0, s32 arg1, s32 arg2, s32 arg3);
void func_shelter_1f_vehicular_airlock_8017E468(SVECTOR* arg0, s32 arg1, s32 arg2);
void func_shelter_1f_vehicular_airlock_8017EF60(GsCOORDINATE2* arg0, s32 arg1, s32 arg2, u8* rgb);
void func_shelter_1f_vehicular_airlock_8017F38C(GsCOORDINATE2* arg0, s32 arg1, u8* rgb);
void func_shelter_1f_vehicular_airlock_80180290(GsCOORDINATE2* arg0, s16 arg1, u8* arg2);

#endif // ROOMS_SHELTER_1F_VEHICULAR_AIRLOCK_H
