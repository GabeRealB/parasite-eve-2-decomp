#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"

#include "main/display.h"
#include "main/fs.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/sound.h"
#include "main/stage.h"
#include "main/task.h"

#include "rooms/mist_parking.h"

void func_mist_parking_80184624(s32 arg0)
{
    Display_InitModeObj(Task_GetDescAt(&D_mist_parking_80190824, 2U), arg0, 0, 0);
}

void func_mist_parking_80184668(Task* arg0)
{
    s32 temp_v0;

    temp_v0         = arg0->spawnArg1 - 1;
    arg0->spawnArg1 = temp_v0;
    if (temp_v0 < 0) {
        taskKill(arg0);
        Stage_SetEndingFlag();
    }
}

void func_mist_parking_801846A4(s32 arg0)
{
    Gp_ResetCap();
    switch (arg0) {
        case 1:
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x140, 0x100);
            break;
        case 2:
            Gp_CapFile = 0;
            Gp_LoadCapFile(2);
            func_800E6D4C(0x2C0, 0);
            break;
    }
}

void func_mist_parking_8018471C(void)
{
    D_mist_parking_8019532C = NULL;
}
