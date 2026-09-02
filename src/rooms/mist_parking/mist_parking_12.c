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

extern s16 D_80071076;
extern s8  D_80073BAE;

extern Task*    D_mist_parking_8019532C;
extern TaskDesc RoomsShared8017daf0Desc;

void func_mist_parking_8018451C(void)
{
    func_800BC4BC();
    D_80073BAE          = 1;
    Mc_SaveData.field_6 = 5;
    Mc_SaveData.field_7 = 1;
    Mc_SaveData.field_8 = 1;
    Mc_SaveData.field_5 = 1;
    D_80071076          = 1;
    SndEvt_EnqueueType7(0x80000000, 0);
    Task_Spawn(0, 0x11, 0, 0);
    Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
}

INCLUDE_ASM("rooms/nonmatchings/mist_parking/mist_parking_12", func_mist_parking_8018459C);

void func_mist_parking_801845D0(s32 arg0)
{
    Task* t = D_mist_parking_8019532C;

    if (t == NULL) {
        return;
    }
    if (arg0 >= 2) {
        goto kill;
    }
    if (arg0 < 0) {
        goto kill;
    }
    t->spawnArg1 = arg0;
    return;
kill:
    Task_Kill(D_mist_parking_8019532C);
    D_mist_parking_8019532C = NULL;
}

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