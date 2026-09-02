#include "common.h"

#include "gameplay/268.h"
#include "gameplay/3CD8.h"
#include "main/display.h"
#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

extern s16      D_80071076;
extern TaskDesc D_acropolis_helicopter_landing_pad_80184E68;

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D658);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D6E0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D7B0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D824);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D8E8);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5C0);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D964);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017D9BC);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", D_acropolis_helicopter_landing_pad_8017D5E4);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DA9C);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017DE78);

void func_acropolis_helicopter_landing_pad_8017DFCC(Task* arg0)
{
    s32 temp_v1;

    temp_v1 = arg0->state;
    switch (temp_v1) {
        case 0:
            Gp_FillPlayerHpMp();
            Gp_ClearCollectedBit(0x101);
            Gp_ClearCollectedBit(0x102);
            Gp_SetItemSeenBit(0x102, 1);
            GameFlag_SetNibble(3, 0);
            GameFlag_SetNibble(0x155, 7);
            Task_SpawnFromTable(&D_acropolis_helicopter_landing_pad_80184E68, 0, 0, 0);
            arg0->state = (s32)(arg0->state + 1);
            return;
        case 1:
            arg0->state = 2;
            return;
        case 2:
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_5C5 = 1;
            Mc_SaveData.field_7   = 1;
            Mc_SaveData.field_6   = 0x12;
            Mc_SaveData.field_8   = 1;
            Mc_SaveData.field_5   = 1;
            D_80071076            = 1;
            Task_Spawn(0, 0x11, 0, 0);
            Display_ReleaseRef();
            Task_Kill(arg0);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E0F8);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E270);

INCLUDE_ASM("rooms/nonmatchings/acropolis_helicopter_landing_pad/acropolis_helicopter_landing_pad", func_acropolis_helicopter_landing_pad_8017E3F0);
