#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"
#include "main/mc.h"
#include "main/task.h"

#include "rooms/shelter_b6_training_room.h"

extern s8 D_801153F1;

void func_shelter_b6_training_room_8017DAF8(s32 arg0)
{
    gGameSession->flowFlags |= 0x80;
    if (arg0 != 0) {
        D_801153F1 = arg0;
    }
}

/// Sets the saved location to area 0x16, warp 1, room 1 and spawns task 0x11.
/// Actor 503500 carries the same body.
void func_shelter_b6_training_room_8017DB28(void)
{
    Mc_SaveData.at4.loc.area = 0x16;
    Mc_SaveData.at4.loc.warp = 1;
    Mc_SaveData.at4.loc.room = 1;
    Task_Spawn(0, 0x11, 0, 0);
}

void func_shelter_b6_training_room_8017DB70(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
    SndEvt_EnqueueType7(0x80000000, 1);
}

void func_shelter_b6_training_room_8017DBB0(s32 arg0)
{
    D_shelter_b6_training_room_80185C5C = NULL;
}
