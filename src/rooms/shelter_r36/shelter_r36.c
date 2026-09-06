#include "common.h"

#include "gameplay/3CD8.h"
#include "main/fs.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_shelter_r36_8017DF2C;
extern s32 D_shelter_r36_8017E5A4;
extern s32 D_shelter_r36_8017E664;
extern s32 D_shelter_r36_8017E8BC;

extern s16      D_80071076;
extern TaskDesc RoomsShared8018397cDesc;

void func_shelter_r36_8017D5E8(Task* task)
{
    s32 state;
    s16 slot;

    state = task->state;
    switch (state) {
        case 0:
            if (GameFlag_GetNibble(0x113) == 0) {
                slot = 1;
            } else {
                Gp_CapFile = 0;
                Gp_LoadCapFile(3);
                func_800E6D4C(0x140, 0x100);
                slot = 2;
            }
            Gp_StartCapSlot(slot, 0, 0);
            func_800E8634((s32)&D_shelter_r36_8017DF2C, 0, (s32)&D_shelter_r36_8017E5A4);
            task->state++;
            break;
        case 1:
            if (Game_Session->field_1 == 0) {
                if (GameFlag_GetNibble(0x113) == 0) {
                    Game_Session->field_128 = 0xFF;
                    Game_Session->field_12E = state;
                    Task_Kill(task);
                } else {
                    Gp_ResetCap();
                    task->state++;
                }
            }
            break;
        case 2:
            func_800E8634((s32)&D_shelter_r36_8017E664, 0, (s32)&D_shelter_r36_8017E8BC);
            Task_Kill(task);
            break;
    }
}

void func_shelter_r36_8017D738(void)
{
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.field_7 = 4;
        Mc_SaveData.field_6 = 0x24;
        Mc_SaveData.field_8 = 2;
        Mc_SaveData.field_5 = 1;
        D_80071076          = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.field_4, 1);
    }
}

void func_shelter_r36_8017D7B4(Task* task)
{
    switch (task->state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Task_SpawnFromTable(&RoomsShared8018397cDesc, 0, 0, 0);
            task->state++;
            break;
        case 1:
            Gp_MsgPlayerWeapon(0);
            task->state++;
            break;
        case 2:
            Game_Session->field_128 = 0xFF;
            Game_Session->field_12E = 1;
            Task_Kill(task);
            break;
    }
}

void func_shelter_r36_8017D870(s32 arg0)
{
    s16 var_a0;

    if (arg0 != 0) {
        Gp_CapFile = 0;
        Gp_LoadCapFile(arg0);
        var_a0 = 0x2C0;
        if (arg0 == 1) {
            var_a0 = 0x280;
        }
        func_800E6D4C(var_a0, 0x100);
        return;
    }
    Gp_ResetCap();
}

s32 func_shelter_r36_8017D8C8(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_r36/shelter_r36", D_shelter_r36_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r36/shelter_r36", RoomsShared8017d878Table);
