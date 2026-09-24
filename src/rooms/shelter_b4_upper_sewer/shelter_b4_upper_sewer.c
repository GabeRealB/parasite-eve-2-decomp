#include "common.h"

#include "main/gameflag.h"
#include "main/mc.h"
#include "main/task.h"

#include "gameplay/3CD8.h"

extern u8       D_8007216C;
extern u8       D_801153F4;
extern u8       D_shelter_b4_upper_sewer_80188D2C;
extern TaskDesc D_shelter_b4_upper_sewer_80186300;
extern void     func_shelter_b4_upper_sewer_8017E59C(s32);
INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", RoomsShared8017d878Table);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D660);

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D80C);

s32 func_shelter_b4_upper_sewer_8017D9BC(void)
{
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017D9C4);

s32 func_shelter_b4_upper_sewer_8017DAB0(Task* task, s32 msgId, s32 arg2)
{
    u8 temp_a1;

    if (arg2 == 1) {
        if (GameFlag_GetNibble(0xB8) == 0) {
            Gp_MsgPlayer3F3(0);
            Gp_MsgAlly3F3(0);
            Gp_MsgPlayerWeapon(0);
            Gp_MsgAllyWeapon(0);
            D_801153F4                        = 2;
            temp_a1                           = Mc_SaveData.at4.loc.view;
            Mc_SaveData.at4.loc.view          = 0xD;
            D_shelter_b4_upper_sewer_80188D2C = temp_a1;
            Task_SpawnFromTable(&D_shelter_b4_upper_sewer_80186300, 0, 0, 0);
        } else {
            Gp_RunCapCmd1(6);
        }
    }
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB50(void)
{
    return 0;
}

s32 func_shelter_b4_upper_sewer_8017DB58(s32 arg0, s32 arg1, s32 arg2)
{
    if (arg2 == 4) {
        func_shelter_b4_upper_sewer_8017E59C(1);
        SndEvt_EnqueueType6(0x542C0004, 0, 0);
    }
    return 0;
}

void func_shelter_b4_upper_sewer_8017DB94(void)
{
    D_8007216C = D_shelter_b4_upper_sewer_80188D2C;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", func_shelter_b4_upper_sewer_8017DBA8);

void func_shelter_b4_upper_sewer_8017DC28(void)
{
}

INCLUDE_RODATA("rooms/nonmatchings/shelter_b4_upper_sewer/shelter_b4_upper_sewer", jtbl_shelter_b4_upper_sewer_8017D604);
