#include "common.h"

#include "gameplay/3CD8.h"
#include "main/gameflag.h"
#include "main/session.h"
#include "main/task.h"

extern s32 D_shelter_r36_8017DF2C;
extern s32 D_shelter_r36_8017E5A4;
extern s32 D_shelter_r36_8017E664;
extern s32 D_shelter_r36_8017E8BC;

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

INCLUDE_ASM("rooms/nonmatchings/shelter_r36/shelter_r36", func_shelter_r36_8017D738);

INCLUDE_ASM("rooms/nonmatchings/shelter_r36/shelter_r36", func_shelter_r36_8017D7B4);

INCLUDE_ASM("rooms/nonmatchings/shelter_r36/shelter_r36", func_shelter_r36_8017D870);

INCLUDE_ASM("rooms/nonmatchings/shelter_r36/shelter_r36", func_shelter_r36_8017D8C8);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r36/shelter_r36", D_shelter_r36_8017D5C0);

INCLUDE_RODATA("rooms/nonmatchings/shelter_r36/shelter_r36", D_shelter_r36_8017D5C4);
