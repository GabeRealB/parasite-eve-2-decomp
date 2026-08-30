#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/sound.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern s16 D_80071076;
extern u8  D_acropolis_patio_80187064;
extern u8  D_acropolis_patio_80187065;

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio", func_acropolis_patio_8017D5EC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio", func_acropolis_patio_8017D7D0);

void func_acropolis_patio_8017DA5C(Task* task)
{
    s32 state;

    state = task->state;
    switch (state) {
        case 0:
            Gp_MsgPlayerWeapon(0);
            Gp_RunCapCmd1(3);
            goto advance;
        case 1:
            task->state = 2;
            return;
        case 2:
            if (Gp_GetCapEventKey() == state) {
                GameFlag_SetNibble(8, 3);
                SndEvt_EnqueueType6(0x51030004, 0, 0);
            advance:
                task->state = task->state + 1;
                return;
            }
            Gp_MsgPlayerWeapon(1);
            goto kill;
        case 3:
            if (SndVoice_HasActiveId(0x51030004) != 0) {
                return;
            }
            SndEvt_EnqueueType7(0x80000000, 0);
            Mc_SaveData.field_6 = 4;
            D_80071076          = 1;
            Mc_SaveData.field_8 = D_acropolis_patio_80187064;
            Mc_SaveData.field_5 = D_acropolis_patio_80187065;
            Task_Spawn(0, 0x11, 0, 0);
        kill:
            Task_Kill(task);
            return;
    }
}

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio", func_acropolis_patio_8017DBAC);

INCLUDE_ASM("rooms/nonmatchings/acropolis_patio/acropolis_patio", func_acropolis_patio_8017DCE4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", D_acropolis_patio_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", D_acropolis_patio_8017D5C4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", jtbl_acropolis_patio_8017D5D4);

INCLUDE_RODATA("rooms/nonmatchings/acropolis_patio/acropolis_patio", D_acropolis_patio_8017D5E8);
