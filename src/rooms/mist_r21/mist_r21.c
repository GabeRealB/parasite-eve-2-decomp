#include "common.h"
#include "gameplay/1BC.h"
#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_mist_r21_8017D770[];
extern TaskDesc   D_mist_r21_8017D798[];

void func_mist_r21_8017D61C(Task* arg0)
{
    arg0->field_24 = D_mist_r21_8017D770;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_mist_r21_8017D798, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_mist_r21_8017D678(Task* arg0)
{
    if ((Pad_CheckButtons(0, 0, 0x200) != 0) && (Pad_CheckButtons(0, 1, 0x40) != 0)) {
        Mc_SaveData.field_6 = 5;
        Mc_SaveData.field_8 = 1;
        Mc_SaveData.field_4 = 2;
        Fs_BeginBootLoad(&Mc_SaveData.field_4, 0);
        Task_Spawn(0, 0x11, 1, 0);
        Task_Kill(arg0);
    }
}

INCLUDE_RODATA("rooms/nonmatchings/mist_r21/mist_r21", D_mist_r21_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/mist_r21/mist_r21", RoomsShared8017d878Table);
