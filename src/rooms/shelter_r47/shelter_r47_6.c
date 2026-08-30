#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/task.h"

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018061C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180650);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180714);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801807B4);

void func_shelter_r47_8018080C(Task* task)
{
    s32 nibble;

    switch (task->state) {
        case 0:
            Gp_ResetCap();
            Gp_CapFile = 0;
            Gp_LoadCapFile(1);
            func_800E6D4C(0x240, 0x100);
            Gp_RunCapCmd1(8);
            task->state++;
            break;
        case 1:
            if (Gp_CapBusy() != 0) {
                break;
            }
            Gp_ResetCap();
            Gp_MsgPlayerWeapon(1);
            nibble = GameFlag_GetNibble(0x165);
            if (nibble < 3) {
                GameFlag_SetNibble(0x165, nibble + 1);
            }
        default:
            Task_Kill(task);
            break;
    }
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801808D4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018097C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180C48);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80180F38);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181148);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018138C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181568);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801816CC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181914);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80181F14);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801820C0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182348);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182470);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801828D0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801829B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182AA0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182B18);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182B9C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182C78);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182CA4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182DAC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182E78);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182F18);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80182FDC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183068);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801830B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183170);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801831C8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183210);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183234);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183284);

void func_shelter_r47_801832E4(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801832EC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018337C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801833DC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183484);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183B84);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183E24);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183F0C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80183FF4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184124);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018431C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801844A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184658);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018489C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184AE0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80184F40);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185028);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185098);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801851B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185214);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801852A0);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185354);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185450);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_80185510);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801855B8);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801856AC);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018571C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018580C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_8018585C);

INCLUDE_ASM("rooms/nonmatchings/shelter_r47/shelter_r47_6", func_shelter_r47_801858BC);
