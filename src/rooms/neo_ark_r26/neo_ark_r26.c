#include "common.h"

#include "main/fs.h"
#include "main/mc.h"
#include "main/task.h"

extern s16 D_80071076;

void func_neo_ark_r26_8017D5D0(void)
{
    if (Mc_SaveData.field_23 != 9) {
        Mc_SaveData.at4.loc.stage = 5;
        Mc_SaveData.at4.loc.area  = 0x1C;
        Mc_SaveData.at4.loc.warp  = 1;
        Mc_SaveData.at4.loc.room  = 1;
        D_80071076                = 1;
        Task_Spawn(0, 0x11, 0, 0);
        Fs_BeginBootLoad(&Mc_SaveData.at4.loc.view, 1);
    }
}

s32 func_neo_ark_r26_8017D648(void)
{
    return 0;
}

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_r26/neo_ark_r26", D_neo_ark_r26_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/neo_ark_r26/neo_ark_r26", RoomsShared8017d878Table);
