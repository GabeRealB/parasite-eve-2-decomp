#include "common.h"

#include "main/display.h"
#include "main/session.h"
#include "main/stage.h"
#include "main/task.h"

extern Task*    D_mist_parking_8019532C;
extern TaskDesc RoomsShared8017daf0Desc;

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_8018451C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_8018459C);

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_801845D0);

void func_mist_parking_80184624(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&RoomsShared8017daf0Desc, 2U), arg0, 0, 0);
}

void func_mist_parking_80184668(Task* arg0)
{
    s32 temp_v0;

    temp_v0         = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = temp_v0;
    if (temp_v0 < 0) {
        Task_Kill(arg0);
        Stage_SetEndingFlag();
    }
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_801846A4);

void func_mist_parking_8018471C(void)
{
    D_mist_parking_8019532C = NULL;
}